#!/usr/bin/env python3
"""
Search for graphs where BCT-Color succeeds but DSATUR fails.

BCT-Color uses exact backtracking for biconnected components <= 20 nodes.
DSATUR is purely greedy and can fail on χ-colorable graphs.
We search for connected graphs where DSATUR spills at N=χ but BCT-Color doesn't.
"""

import random
import subprocess
import tempfile
import os
import sys
import itertools

BIN = "code/cmake-build/register_alloc"

# ── graph utilities ──────────────────────────────────────────────────────────

def exact_chromatic(adj, n):
    """Backtracking exact chromatic number. Returns χ(G)."""
    color = [-1] * n
    def bt(node, max_c):
        if node == n:
            return max_c + 1
        used = {color[nb] for nb in adj[node] if color[nb] >= 0}
        for c in range(max_c + 2):  # try existing colors + one new
            if c not in used:
                color[node] = c
                result = bt(node + 1, max(max_c, c))
                if result is not None:
                    return result
                color[node] = -1
        return None
    # try from χ=1 upwards with ordering
    order = sorted(range(n), key=lambda v: -sum(1 for _ in adj[v]))
    color_ord = [-1] * n
    def bt2(idx, max_c):
        if idx == n:
            return max_c + 1
        node = order[idx]
        used = {color_ord[nb] for nb in adj[node] if color_ord[nb] >= 0}
        for c in range(max_c + 2):
            if c not in used:
                color_ord[node] = c
                result = bt2(idx + 1, max(max_c, c))
                if result is not None:
                    return result
                color_ord[node] = -1
        return None
    return bt2(0, -1)

def is_connected(adj, n):
    if n == 0:
        return True
    visited = set()
    stack = [0]
    while stack:
        v = stack.pop()
        if v in visited:
            continue
        visited.add(v)
        for nb in adj[v]:
            if nb not in visited:
                stack.append(nb)
    return len(visited) == n

# ── ranges file generation ────────────────────────────────────────────────────

def graph_to_ranges(adj, n):
    """
    Convert adjacency list to ranges file format.
    Each node i lives at line i*10..(i*10+1), and shares a line with each neighbor.
    Strategy: assign each node a unique "def" line and "use" line far apart,
    and create interference by giving two nodes overlapping live ranges.

    Simpler approach: assign node i def=2i+1, use=2i+2.
    Make two nodes interfere by giving them identical lines in their range.

    We use the "shared program point" trick: nodes u and v interfere iff they
    share a live line. We use a unique shared line per edge (u,v).
    Node i: def at line start_i, use at line end_i, alive at all edge-shared lines.
    """
    lines_used = {}  # edge -> shared line
    line_counter = [1]
    def alloc_line():
        l = line_counter[0]
        line_counter[0] += 1
        return l

    # Each node gets a def line and a use line
    node_def = [alloc_line() for _ in range(n)]
    node_use = [alloc_line() for _ in range(n)]

    # Each edge gets a shared interior line
    edge_line = {}
    edges = []
    for u in range(n):
        for v in adj[u]:
            if u < v:
                l = alloc_line()
                edge_line[(u, v)] = l
                edges.append((u, v))

    # Build sorted line list per node: def, all edge-shared lines, use
    result = []
    for i in range(n):
        my_lines = set()
        my_lines.add(node_def[i])
        my_lines.add(node_use[i])
        for j in adj[i]:
            key = (min(i,j), max(i,j))
            my_lines.add(edge_line[key])
        sorted_lines = sorted(my_lines)
        # Format: first is def (+), last is use (-), middle are just line numbers
        parts = []
        for idx, l in enumerate(sorted_lines):
            if l == node_def[i]:
                parts.append(f"{l}+")
            elif l == node_use[i]:
                parts.append(f"{l}-")
            else:
                parts.append(str(l))
        result.append(f"w{i}: " + ",".join(parts))
    return "\n".join(result) + "\n"

# ── run one algorithm ─────────────────────────────────────────────────────────

def run_algo(ranges_text, N, algo):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as rf:
        rf.write(ranges_text)
        rpath = rf.name
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as gf:
        gf.write(f"registers: {N}\nalgorithm: {algo}\n")
        gpath = gf.name
    opath = tempfile.mktemp(suffix='.txt')
    epath = tempfile.mktemp(suffix='.txt')
    try:
        subprocess.run(
            [BIN, "-b", rpath, gpath, opath],
            capture_output=False,
            stdout=subprocess.DEVNULL,
            stderr=open(epath, 'w'),
            timeout=5
        )
        out = open(opath).read() if os.path.exists(opath) else ""
        err = open(epath).read() if os.path.exists(epath) else ""
        spills = out.count('\nM:')
        feasible = "Warning: Register allocation was not possible" not in err
        regs_line = next((l for l in out.splitlines() if l.startswith("registers:")), "registers: 0")
        regs = int(regs_line.split()[-1])
        return spills, regs, feasible
    finally:
        for p in [rpath, gpath, opath, epath]:
            try: os.unlink(p)
            except: pass

# ── main search ───────────────────────────────────────────────────────────────

def main():
    n_range = range(6, 16)   # biconnected graphs up to 15 nodes (BCT-Color backtracks ≤20)
    p_values = [0.3, 0.4, 0.5, 0.6]
    seeds_per = 300
    total = 0
    hits = 0

    print("Searching for graphs where BCT-Color beats DSATUR...")
    print(f"(n=6..15, p={{0.3,0.4,0.5,0.6}}, {seeds_per} seeds each = "
          f"{len(list(n_range))*len(p_values)*seeds_per} candidates)\n")

    for n in n_range:
        for p in p_values:
            for seed in range(seeds_per):
                rng = random.Random(seed * 10000 + n * 100 + int(p * 100))
                # generate random G(n,p)
                adj = [set() for _ in range(n)]
                for u in range(n):
                    for v in range(u+1, n):
                        if rng.random() < p:
                            adj[u].add(v)
                            adj[v].add(u)
                if not is_connected(adj, n):
                    continue

                total += 1
                chi = exact_chromatic(adj, n)
                N = chi  # test at exactly the chromatic number

                ranges = graph_to_ranges(adj, n)

                sp_dsatur, regs_dsatur, feas_dsatur = run_algo(ranges, N, "dsatur")
                sp_free,   regs_free,   feas_free   = run_algo(ranges, N, "phantom")

                if sp_free < sp_dsatur:
                    hits += 1
                    edges = sum(len(a) for a in adj) // 2
                    print(f"HIT! n={n} p={p} seed={seed} χ={chi} N={N}")
                    print(f"  DSATUR : spills={sp_dsatur} regs={regs_dsatur} feasible={feas_dsatur}")
                    print(f"  BCT-Color: spills={sp_free}   regs={regs_free}   feasible={feas_free}")
                    print(f"  Edges: {edges}")
                    print(f"  Ranges snippet:\n" + "\n".join("    " + l for l in ranges.strip().splitlines()[:6]))
                    print()
                    sys.stdout.flush()

    print(f"\nDone. Searched {total} connected graphs.")
    print(f"BCT-Color beat DSATUR: {hits}/{total}")
    if hits == 0:
        print("\nNo cases found — BCT-Color and DSATUR are tied on random graphs.")
        print("BCT-Color's backtracking advantage may only manifest on adversarially")
        print("constructed graphs or very specific interference structures.")

if __name__ == "__main__":
    main()
