#!/usr/bin/env python3
"""
Compare pure Python DSATUR vs BCT-Color binary.

DSATUR (Brelaz 1979): at each step color the uncolored vertex with the
highest saturation degree (distinct colors in its neighbourhood).
Ties broken by highest degree, then lowest index.

BCT-Color is invoked via the register_alloc binary with algorithm=free.

The interference graph is encoded as a ranges file using the shared-line
trick: nodes u and v interfere iff they share an edge-specific program point.
"""

import random
import subprocess
import tempfile
import os
import sys

BIN = "code/cmake-build/register_alloc"

# ── graph encoding ────────────────────────────────────────────────────────────

def encode_ranges(adj, n):
    edge_line = {}
    counter = 3 * n + 1
    for u in range(n):
        for v in adj[u]:
            if u < v:
                edge_line[(u, v)] = counter
                counter += 1
    rows = []
    for i in range(n):
        pts = {3 * i + 1, 3 * i + 2}
        for j in adj[i]:
            pts.add(edge_line[(min(i, j), max(i, j))])
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

# ── pure Python DSATUR ────────────────────────────────────────────────────────

def dsatur(adj, n, num_colors):
    """
    Textbook DSATUR (Brelaz 1979).
    Returns (spills, colors_used) where spills = nodes that couldn't be colored.
    """
    color = [-1] * n
    sat = [set() for _ in range(n)]         # sat[v] = set of neighbor colors
    degree = [len(adj[v]) for v in range(n)]
    colored = [False] * n

    for _ in range(n):
        # pick uncolored vertex with max saturation, break ties by degree then index
        best = max(
            (v for v in range(n) if not colored[v]),
            key=lambda v: (len(sat[v]), degree[v], -v)
        )
        used = {color[nb] for nb in adj[best] if color[nb] >= 0}
        chosen = next((c for c in range(num_colors) if c not in used), None)
        if chosen is not None:
            color[best] = chosen
            for nb in adj[best]:
                if not colored[nb]:
                    sat[nb].add(chosen)
        # else: no color available — node is spilled (color stays -1)
        colored[best] = True

    spills = sum(1 for c in color if c == -1)
    colors_used = len(set(c for c in color if c >= 0))
    return spills, colors_used

# ── exact chromatic number ────────────────────────────────────────────────────

def chromatic_number(adj, n):
    order = sorted(range(n), key=lambda v: -len(adj[v]))
    color = [-1] * n
    def bt(idx, budget):
        if idx == n:
            return max(color) + 1
        v = order[idx]
        used = {color[nb] for nb in adj[v] if color[nb] >= 0}
        for c in range(budget):
            if c not in used:
                color[v] = c
                r = bt(idx + 1, budget)
                if r is not None:
                    return r
                color[v] = -1
        return None
    for k in range(1, n + 1):
        color = [-1] * n
        r = bt(0, k)
        if r is not None:
            return r
    return n

# ── run BCT-Color binary ──────────────────────────────────────────────────────

def run_bct(ranges_text, N):
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        f.write(ranges_text); rpath = f.name
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        f.write(f"registers: {N}\nalgorithm: phantom\n"); gpath = f.name
    opath = tempfile.mktemp(suffix='.txt')
    epath = tempfile.mktemp(suffix='.txt')
    try:
        subprocess.run([BIN, "-b", rpath, gpath, opath],
                       stdout=subprocess.DEVNULL,
                       stderr=open(epath, 'w'), timeout=5)
        out = open(opath).read() if os.path.exists(opath) else ""
        spills = out.count('\nM:')
        regs_line = next((l for l in out.splitlines() if l.startswith("registers:")), "registers: 0")
        regs = int(regs_line.split()[-1])
        return spills, regs
    except subprocess.TimeoutExpired:
        return None, None
    finally:
        for p in [rpath, gpath, opath, epath]:
            try: os.unlink(p)
            except: pass

# ── graph generators ──────────────────────────────────────────────────────────

def random_graph(n, p, seed):
    rng = random.Random(seed)
    adj = [set() for _ in range(n)]
    for u in range(n):
        for v in range(u + 1, n):
            if rng.random() < p:
                adj[u].add(v); adj[v].add(u)
    # ensure connected
    for i in range(1, n):
        if not any(i in adj[j] for j in range(i)):
            j = rng.randint(0, i - 1)
            adj[i].add(j); adj[j].add(i)
    return adj

def make_odd_cycle(k):
    n = 2 * k + 1
    adj = [set() for _ in range(n)]
    for i in range(n):
        adj[i].add((i + 1) % n); adj[(i + 1) % n].add(i)
    return adj, n

def make_petersen():
    # outer cycle 0-4, inner pentagram 5-9
    adj = [set() for _ in range(10)]
    outer = [(0,1),(1,2),(2,3),(3,4),(4,0)]
    spokes = [(0,5),(1,6),(2,7),(3,8),(4,9)]
    inner  = [(5,7),(7,9),(9,6),(6,8),(8,5)]
    for u, v in outer + spokes + inner:
        adj[u].add(v); adj[v].add(u)
    return adj, 10

def make_grotzsch():
    # Grotzsch graph: 11 nodes, triangle-free, χ=4
    adj = [set() for _ in range(11)]
    # inner 5-cycle: 0-4
    for i in range(5):
        adj[i].add((i+1)%5); adj[(i+1)%5].add(i)
    # outer 5-cycle: 5-9
    for i in range(5):
        adj[5+i].add(5+(i+1)%5); adj[5+(i+1)%5].add(5+i)
    # spokes: each outer connects to two non-adjacent inner
    # standard Grotzsch construction
    for i in range(5):
        adj[5+i].add((i+1)%5); adj[(i+1)%5].add(5+i)
        adj[5+i].add((i+2)%5); adj[(i+2)%5].add(5+i)
    # apex: 10 connects to all outer
    for i in range(5):
        adj[10].add(5+i); adj[5+i].add(10)
    return adj, 11

# ── main ──────────────────────────────────────────────────────────────────────

def main():
    bct_better = 0
    bct_worse  = 0
    tied       = 0
    total      = 0

    print("=" * 64)
    print("  Pure Python DSATUR  vs  BCT-Color binary")
    print("  (tested at N = χ and N = χ-1 when χ > 1)")
    print("=" * 64)

    # ── known structured graphs ───────────────────────────────────────────────
    named = []
    for k in range(2, 8):
        adj, n = make_odd_cycle(k)
        named.append((adj, n, f"C{2*k+1}"))
    named.append((*make_petersen(), "Petersen"))
    named.append((*make_grotzsch(), "Grotzsch"))

    print("\n── Named graphs ──")
    for adj, n, label in named:
        chi = chromatic_number(adj, n)
        ranges = encode_ranges(adj, n)
        for N in ([chi] if chi == 1 else [chi, chi - 1]):
            sp_d, cr_d = dsatur(adj, n, N)
            sp_b, cr_b = run_bct(ranges, N)
            if sp_b is None:
                print(f"  {label:20s} N={N} χ={chi}  BCT timeout"); continue
            total += 1
            tag = ""
            if sp_b < sp_d:
                bct_better += 1; tag = "  ← BCT wins"
            elif sp_b > sp_d:
                bct_worse  += 1; tag = "  ← DSATUR wins"
            else:
                tied += 1
            print(f"  {label:20s} N={N} χ={chi}  "
                  f"DSATUR spills={sp_d} regs={cr_d}  "
                  f"BCT spills={sp_b} regs={cr_b}{tag}")

    # ── random graphs ─────────────────────────────────────────────────────────
    print("\n── Random G(n,p) graphs ──")
    configs = [(n, p) for n in range(6, 16) for p in [0.3, 0.4, 0.5]]
    seeds   = range(200)
    hits_detail = []

    for n, p in configs:
        for seed in seeds:
            adj = random_graph(n, p, seed)
            chi = chromatic_number(adj, n)
            ranges = encode_ranges(adj, n)
            for N in ([chi] if chi == 1 else [chi, chi - 1]):
                sp_d, cr_d = dsatur(adj, n, N)
                sp_b, cr_b = run_bct(ranges, N)
                if sp_b is None:
                    continue
                total += 1
                if sp_b < sp_d:
                    bct_better += 1
                    hits_detail.append(
                        f"  n={n} p={p} seed={seed} N={N} χ={chi}: "
                        f"DSATUR spills={sp_d}  BCT spills={sp_b}"
                    )
                elif sp_b > sp_d:
                    bct_worse += 1
                else:
                    tied += 1

    if hits_detail:
        print("\n  BCT-Color wins:")
        for h in hits_detail[:20]:
            print(h)
        if len(hits_detail) > 20:
            print(f"  ... and {len(hits_detail)-20} more")
    else:
        print("  (no wins for BCT-Color on random graphs)")

    print("\n" + "=" * 64)
    print(f"  Total cases : {total}")
    print(f"  BCT better  : {bct_better}")
    print(f"  Tied        : {tied}")
    print(f"  DSATUR better: {bct_worse}")
    print("=" * 64)

if __name__ == "__main__":
    main()
