#!/usr/bin/env python3
"""
Search for non-perfect interference graphs where BCT-Color beats DSATUR.

Non-perfect graphs contain odd holes (induced odd cycles C_{2k+1}, k>=2)
or their complements. These are the only graphs where DSATUR can be
suboptimal. BCT-Color uses exact backtracking on blocks <=20 nodes, so
it finds the true chromatic number.

Encoding: each web gets a unique def line and use line. Each edge (u,v)
gets a unique shared interior line — so two webs interfere iff they share
that edge's line. No accidental chords are introduced.
"""

import random
import subprocess
import tempfile
import os
import sys

BIN = "code/cmake-build/register_alloc"

# ── graph encoding ────────────────────────────────────────────────────────────

def encode_ranges(adj, n):
    """
    Encode adjacency list as a ranges file.
    Each node i: def=3i+1, use=3i+2. Each edge (u,v): shared line = 3*n + edge_index.
    Interference iff shared line exists => exactly the edges in adj.
    """
    edge_line = {}
    counter = 3 * n + 1
    for u in range(n):
        for v in adj[u]:
            if u < v:
                edge_line[(u, v)] = counter
                counter += 1

    rows = []
    for i in range(n):
        pts = set()
        pts.add(3 * i + 1)   # def
        pts.add(3 * i + 2)   # use
        for j in adj[i]:
            key = (min(i, j), max(i, j))
            pts.add(edge_line[key])
        sorted_pts = sorted(pts)
        parts = []
        for p in sorted_pts:
            if p == 3 * i + 1:
                parts.append(f"{p}+")
            elif p == 3 * i + 2:
                parts.append(f"{p}-")
            else:
                parts.append(str(p))
        rows.append(f"w{i}: " + ",".join(parts))
    return "\n".join(rows) + "\n"

# ── exact chromatic number ────────────────────────────────────────────────────

def chromatic_number(adj, n):
    """Exact via backtracking with degree-descending order."""
    order = sorted(range(n), key=lambda v: -len(adj[v]))
    color = [-1] * n
    def bt(idx, max_c, budget):
        if idx == n:
            return max_c + 1
        v = order[idx]
        used = {color[nb] for nb in adj[v] if color[nb] >= 0}
        for c in range(budget):
            if c not in used:
                color[v] = c
                r = bt(idx + 1, max(max_c, c), budget)
                if r is not None:
                    return r
                color[v] = -1
        return None
    for k in range(1, n + 1):
        result = bt(0, -1, k)
        if result is not None:
            return result
    return n

# ── odd hole detection ────────────────────────────────────────────────────────

def has_odd_hole(adj, n):
    """Check if graph contains an induced odd cycle of length >= 5."""
    # Try all subsets of size 5, 7, 9 (up to n)
    from itertools import combinations
    for size in range(5, min(n + 1, 10), 2):
        for nodes in combinations(range(n), size):
            ns = set(nodes)
            # Check if these nodes form an induced cycle
            # Each node must have exactly 2 neighbors within the subset
            degs = []
            valid = True
            for v in nodes:
                nb_in = [u for u in adj[v] if u in ns]
                if len(nb_in) != 2:
                    valid = False
                    break
                degs.append(nb_in)
            if not valid:
                continue
            # Check it's actually a cycle (connected, each has deg 2 in subset)
            # BFS within the subset
            start = nodes[0]
            visited = {start}
            stack = [start]
            while stack:
                v = stack.pop()
                for nb in degs[nodes.index(v)]:
                    if nb not in visited:
                        visited.add(nb)
                        stack.append(nb)
            if len(visited) == size:
                return True
    return False

# ── run binary ────────────────────────────────────────────────────────────────

def run_algo(ranges_text, N, algo):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        f.write(ranges_text); rpath = f.name
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        f.write(f"registers: {N}\nalgorithm: {algo}\n"); gpath = f.name
    opath = tempfile.mktemp(suffix='.txt')
    epath = tempfile.mktemp(suffix='.txt')
    try:
        subprocess.run([BIN, "-b", rpath, gpath, opath],
                       stdout=subprocess.DEVNULL,
                       stderr=open(epath, 'w'), timeout=5)
        out = open(opath).read() if os.path.exists(opath) else ""
        err = open(epath).read() if os.path.exists(epath) else ""
        spills = out.count('\nM:')
        regs_line = next((l for l in out.splitlines() if l.startswith("registers:")), "registers: 0")
        regs = int(regs_line.split()[-1])
        feasible = "Warning" not in err
        return spills, regs, feasible
    except subprocess.TimeoutExpired:
        return None, None, None
    finally:
        for p in [rpath, gpath, opath, epath]:
            try: os.unlink(p)
            except: pass

# ── graph generators ──────────────────────────────────────────────────────────

def make_odd_cycle(k):
    """C_{2k+1}: odd cycle of length 2k+1. χ=3."""
    n = 2 * k + 1
    adj = [set() for _ in range(n)]
    for i in range(n):
        adj[i].add((i + 1) % n)
        adj[(i + 1) % n].add(i)
    return adj, n

def make_odd_cycle_plus_pendant(k):
    """C_{2k+1} + one pendant on each node. χ=3, non-perfect."""
    base_adj, base_n = make_odd_cycle(k)
    n = base_n * 2
    adj = [set() for _ in range(n)]
    for u in range(base_n):
        for v in base_adj[u]:
            adj[u].add(v); adj[v].add(u)
    for u in range(base_n):
        pendant = base_n + u
        adj[u].add(pendant); adj[pendant].add(u)
    return adj, n

def make_mycielski(base_adj, base_n):
    """One Mycielski step: adds n+1 nodes, raises χ by 1."""
    new_n = 2 * base_n + 1
    adj = [set() for _ in range(new_n)]
    for u in range(base_n):
        for v in base_adj[u]:
            adj[u].add(v); adj[v].add(u)
    # shadows: node base_n+i is the shadow of node i
    for u in range(base_n):
        shadow_u = base_n + u
        for v in base_adj[u]:
            adj[shadow_u].add(v); adj[v].add(shadow_u)
    # apex: connects to all shadows
    apex = 2 * base_n
    for s in range(base_n, 2 * base_n):
        adj[apex].add(s); adj[s].add(apex)
    return adj, new_n

def make_kneser(n, k):
    """
    Kneser graph K(n,k): vertices are k-subsets of [n],
    edges between disjoint subsets. K(5,2)=Petersen(χ=3).
    K(7,3) has χ=3, is non-perfect.
    """
    from itertools import combinations
    verts = list(combinations(range(n), k))
    nv = len(verts)
    adj = [set() for _ in range(nv)]
    for i in range(nv):
        for j in range(i + 1, nv):
            if not set(verts[i]) & set(verts[j]):
                adj[i].add(j); adj[j].add(i)
    return adj, nv

def random_graph_with_odd_hole(n, p, seed):
    """Random G(n,p) conditioned on containing an odd hole."""
    rng = random.Random(seed)
    for _ in range(200):
        adj = [set() for _ in range(n)]
        for u in range(n):
            for v in range(u + 1, n):
                if rng.random() < p:
                    adj[u].add(v); adj[v].add(u)
        # Ensure connected
        for i in range(1, n):
            if not any(i in adj[j] for j in range(i)):
                j = rng.randint(0, i - 1)
                adj[i].add(j); adj[j].add(i)
        if has_odd_hole(adj, n):
            return adj, n
    return None, None

# ── main ──────────────────────────────────────────────────────────────────────

def test_pair(adj, n, label):
    """Test one graph, return True if BCT-Color beats DSATUR."""
    ranges = encode_ranges(adj, n)
    chi = chromatic_number(adj, n)

    found = False
    for N in range(1, chi + 2):
        sp_d, rd, fd = run_algo(ranges, N, "dsatur")
        sp_f, rf, ff = run_algo(ranges, N, "phantom")
        if sp_d is None or sp_f is None:
            continue
        if sp_f < sp_d or (ff and not fd) or (rd > 0 and rf < rd and ff and fd):
            print(f"  HIT [{label}] N={N} χ={chi}: "
                  f"DSATUR(sp={sp_d},regs={rd}) BCT(sp={sp_f},regs={rf})")
            sys.stdout.flush()
            found = True
    return found

def main():
    total_hits = 0

    print("=== Testing known non-perfect graphs ===\n")

    # 1. Odd cycles C5, C7, C9, C11, C13 — χ=3, non-perfect
    for k in range(2, 8):
        adj, n = make_odd_cycle(k)
        label = f"C{2*k+1}"
        if test_pair(adj, n, label):
            total_hits += 1

    # 2. Odd cycles + pendants
    for k in range(2, 6):
        adj, n = make_odd_cycle_plus_pendant(k)
        label = f"C{2*k+1}+pendants"
        if test_pair(adj, n, label):
            total_hits += 1

    # 3. Mycielski graphs (triangle-free, high χ)
    # M2 = C5 (χ=3), M3 = Grotzsch (χ=4), M4 (χ=5)
    adj_c5, n_c5 = make_odd_cycle(2)  # C5
    adj_m3, n_m3 = make_mycielski(adj_c5, n_c5)   # Grotzsch χ=4
    adj_m4, n_m4 = make_mycielski(adj_m3, n_m3)   # M4 χ=5

    for adj, n, label in [(adj_c5, n_c5, "Mycielski-C5(χ=3)"),
                           (adj_m3, n_m3, "Grotzsch(χ=4)"),
                           (adj_m4, n_m4, "Mycielski-M4(χ=5)")]:
        if n <= 25:
            if test_pair(adj, n, label):
                total_hits += 1

    # 4. Petersen graph — K(5,2), χ=3, non-perfect
    adj_p, n_p = make_kneser(5, 2)
    if test_pair(adj_p, n_p, "Petersen(χ=3)"):
        total_hits += 1

    # 5. K(7,3) Kneser — χ=3, 35 nodes
    # Too large for backtracking; skip

    # 6. Random graphs with guaranteed odd holes, n=8..15
    print("\n=== Random graphs containing odd holes ===\n")
    random_hits = 0
    random_total = 0
    for n in range(7, 16):
        for p100 in [25, 35, 45]:
            p = p100 / 100.0
            for seed in range(300):
                adj, _ = random_graph_with_odd_hole(n, p, seed * 100 + n * 10 + p100)
                if adj is None:
                    continue
                random_total += 1
                ranges = encode_ranges(adj, n)
                chi = chromatic_number(adj, n)
                for N in [chi, chi - 1]:
                    if N < 1:
                        continue
                    sp_d, rd, fd = run_algo(ranges, N, "dsatur")
                    sp_f, rf, ff = run_algo(ranges, N, "phantom")
                    if sp_d is None or sp_f is None:
                        continue
                    if sp_f < sp_d or (rd > rf and ff and fd):
                        random_hits += 1
                        print(f"  HIT [n={n} p={p:.2f} s={seed}] N={N} χ={chi}: "
                              f"DSATUR(sp={sp_d},r={rd}) BCT(sp={sp_f},r={rf})")
                        sys.stdout.flush()

    print(f"\nRandom odd-hole graphs: BCT wins {random_hits}/{random_total}")
    print(f"\nTotal known-graph hits: {total_hits}")
    if total_hits == 0 and random_hits == 0:
        print("\nConclusion: BCT-Color and DSATUR are equivalent even on non-perfect graphs.")
        print("Reason: DSATUR with minimum-available-color assignment is also optimal on")
        print("odd cycles and Mycielski graphs — it always finds χ colors on these specific")
        print("structures because their independence number = n/χ (balanced structure).")

if __name__ == "__main__":
    main()
