/**
 * @file RegisterAllocator.cpp
 * @brief Implementation of the four register allocation algorithms.
 *
 * Algorithms implemented:
 *  - **basic**:    Chaitin-style greedy simplification + stack-based coloring.
 *  - **spilling**: Basic coloring extended with up to K webs committed to memory.
 *  - **splitting**: Iterative web splitting to reduce interference, then basic coloring.
 *  - **free** (BCT-Color): biconnected-component decomposition with per-block adaptive coloring.
 */

#include <algorithm>
#include <stack>
#include <queue>
#include <set>
#include <map>
#include <unordered_map>
#include <iostream>
#include "core/RegisterAllocator.h"

// ─── Basic coloring ───────────────────────────────────────────────────────────

bool RegisterAllocator::basicColoring(std::vector<Web> &webs, InterferenceGraph &ig,
                                       int N, std::vector<int> &spilledIds) {
    ig.resetRemoved();
    spilledIds.clear();
    int total = ig.size();

    for (auto &w : webs) w.assignedRegister = -1;

    std::stack<int> colorStack;
    int remaining = total;

    // Phase 1: Simplification — push nodes with degree < N onto the stack.
    // If no such node exists, force-spill the highest-degree node.
    // Time complexity: O(W^2) — each iteration scans all nodes.
    while (remaining > 0) {
        bool progress = true;
        while (progress) {
            progress = false;
            for (int i = 0; i < total; i++) {
                if (ig.isRemoved(i)) continue;
                if (ig.effectiveDegree(i) < N) {
                    ig.setRemoved(i, true);
                    colorStack.push(i);
                    remaining--;
                    progress = true;
                }
            }
        }
        if (remaining > 0) {
            int spill = selectSpillCandidate(ig, webs);
            ig.setRemoved(spill, true);
            spilledIds.push_back(spill);
            remaining--;
        }
    }

    // Phase 2: Coloring — pop nodes and assign the lowest available color.
    // A valid color always exists because degree was < N when the node was pushed.
    // Time complexity: O(W * W) — for each node, scan its neighbours.
    while (!colorStack.empty()) {
        int id = colorStack.top();
        colorStack.pop();
        ig.setRemoved(id, false);

        std::set<int> usedColors;
        for (int nb : ig.neighbours(id)) {
            if (!ig.isRemoved(nb) && webs[nb].assignedRegister >= 0)
                usedColors.insert(webs[nb].assignedRegister);
        }
        for (int c = 0; c < N; c++) {
            if (!usedColors.count(c)) {
                webs[id].assignedRegister = c;
                break;
            }
        }
    }

    return spilledIds.empty();
}

int RegisterAllocator::selectSpillCandidate(const InterferenceGraph &ig,
                                             const std::vector<Web> &webs) {
    // Heuristic: minimise spill cost = liveRangeLength / (degree + 1).
    // A web that is short and highly connected is cheap to spill (few lines affected,
    // high relief in register pressure). We pick the web with the lowest cost.
    // Ties are broken by highest degree (most interference relief).
    int best = -1;
    double bestCost = 1e18;
    for (int i = 0; i < ig.size(); i++) {
        if (ig.isRemoved(i)) continue;
        int deg = ig.effectiveDegree(i);
        int len = (int)webs[i].lines.size();
        double cost = static_cast<double>(len) / (deg + 1);
        if (cost < bestCost || (cost == bestCost && deg > ig.effectiveDegree(best))) {
            bestCost = cost;
            best = i;
        }
    }
    return best;
}

// ─── Splitting ────────────────────────────────────────────────────────────────

void RegisterAllocator::splitWeb(std::vector<Web> &webs, int webIdx) {
    Web &orig = webs[webIdx];
    int sz = (int)orig.lines.size();
    if (sz < 2) return;

    int mid = sz / 2;
    Web a, b;
    a.varName = orig.varName;
    b.varName = orig.varName;

    for (int i = 0; i < mid; i++) {
        int ln = orig.lines[i];
        a.lines.push_back(ln);
        if (orig.defLines.count(ln)) a.defLines.insert(ln);
        if (orig.useLines.count(ln)) a.useLines.insert(ln);
    }
    for (int i = mid; i < sz; i++) {
        int ln = orig.lines[i];
        b.lines.push_back(ln);
        if (orig.defLines.count(ln)) b.defLines.insert(ln);
        if (orig.useLines.count(ln)) b.useLines.insert(ln);
    }

    // Mark the split boundary so the two sub-webs do not interfere at the cut point
    if (!a.lines.empty()) a.useLines.insert(a.lines.back());
    if (!b.lines.empty()) b.defLines.insert(b.lines.front());

    int nextId = (int)webs.size();
    a.id = webIdx;
    b.id = nextId;
    a.assignedRegister = -1;
    b.assignedRegister = -1;
    webs[webIdx] = a;
    webs.push_back(b);
}

int RegisterAllocator::selectSplitCandidate(const InterferenceGraph &ig,
                                             const std::vector<Web> &webs) {
    // Heuristic: pick the web most likely to reduce interference when split.
    // We prefer webs with:
    //   1. Size >= 2 (can actually be split)
    //   2. High degree (splitting relieves most pressure)
    //   3. Long live range (neighbours more likely to concentrate in one half)
    // Score = degree * liveRangeLength. Webs with size < 2 are excluded.
    int best = -1, bestScore = -1;
    for (int i = 0; i < ig.size(); i++) {
        if (ig.isRemoved(i)) continue;
        if ((int)webs[i].lines.size() < 2) continue;
        int score = ig.effectiveDegree(i) * (int)webs[i].lines.size();
        if (score > bestScore) { bestScore = score; best = i; }
    }
    return best;
}

// ─── DSATUR ───────────────────────────────────────────────────────────────────

bool RegisterAllocator::dsaturColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N) {
    int total = ig.size();
    for (auto &w : webs) w.assignedRegister = -1;

    // saturation[i] = set of distinct colors already used by neighbours of i
    std::vector<std::set<int>> saturation(total);
    std::vector<bool> colored(total, false);

    // Time complexity: O(W^2) — W iterations, each scanning all nodes for the maximum
    for (int step = 0; step < total; step++) {
        int chosen = -1, bestSat = -1, bestDeg = -1;
        for (int i = 0; i < total; i++) {
            if (colored[i]) continue;
            int sat = (int)saturation[i].size();
            int deg = ig.effectiveDegree(i);
            if (sat > bestSat || (sat == bestSat && deg > bestDeg)) {
                bestSat = sat; bestDeg = deg; chosen = i;
            }
        }
        if (chosen < 0) break;

        std::set<int> used;
        for (int nb : ig.neighbours(chosen))
            if (webs[nb].assignedRegister >= 0) used.insert(webs[nb].assignedRegister);

        int color = -1;
        for (int c = 0; c < N; c++) {
            if (!used.count(c)) { color = c; break; }
        }
        webs[chosen].assignedRegister = color;
        colored[chosen] = true;

        if (color >= 0)
            for (int nb : ig.neighbours(chosen))
                if (!colored[nb]) saturation[nb].insert(color);
    }

    for (const auto &w : webs)
        if (w.assignedRegister < 0) return false;
    return true;
}

// ─── Exact backtracking coloring (small blocks) ───────────────────────────────

// Tries to color blockNodes[idx..] given colors already assigned.
// Returns true if a valid coloring is found; writes into webs[*].assignedRegister.
static bool backtrack(const std::vector<int> &order, int idx,
                      std::vector<Web> &webs,
                      const InterferenceGraph &ig,
                      int N,
                      const std::map<int,int> &fixedColors) {
    if (idx == (int)order.size()) return true;
    int u = order[idx];

    // Fixed AP: must take exactly this color
    auto fit = fixedColors.find(u);
    if (fit != fixedColors.end()) {
        // Verify it doesn't conflict with already-colored neighbors
        // OR with other fixed-color neighbors not yet assigned
        for (int nb : ig.neighbours(u)) {
            if (webs[nb].assignedRegister == fit->second) return false;
            auto nfit = fixedColors.find(nb);
            if (nfit != fixedColors.end() && nfit->second == fit->second) return false;
        }
        webs[u].assignedRegister = fit->second;
        if (backtrack(order, idx + 1, webs, ig, N, fixedColors)) return true;
        webs[u].assignedRegister = -1;
        return false;
    }

    // Build forbidden set from already-colored neighbors
    std::set<int> forbidden;
    for (int nb : ig.neighbours(u))
        if (webs[nb].assignedRegister >= 0) forbidden.insert(webs[nb].assignedRegister);

    for (int c = 0; c < N; c++) {
        if (forbidden.count(c)) continue;
        webs[u].assignedRegister = c;
        if (backtrack(order, idx + 1, webs, ig, N, fixedColors)) return true;
        webs[u].assignedRegister = -1;
    }
    return false;
}

// ─── Biconnected-partition coloring ──────────────────────────────────────────

void RegisterAllocator::bcDFS(int u, int parent,
                               int &timer,
                               std::vector<int> &disc,
                               std::vector<int> &low,
                               std::vector<bool> &visited,
                               std::stack<std::pair<int,int>> &edgeStack,
                               const InterferenceGraph &ig,
                               std::vector<std::vector<int>> &blocks,
                               std::set<int> &artPoints) {
    visited[u] = true;
    disc[u] = low[u] = timer++;
    int children = 0;

    for (int v : ig.neighbours(u)) {
        if (!visited[v]) {
            children++;
            edgeStack.push({u, v});
            bcDFS(v, u, timer, disc, low, visited, edgeStack, ig, blocks, artPoints);
            low[u] = std::min(low[u], low[v]);

            // u is an articulation point if:
            //   (a) it's the DFS root with >= 2 children, OR
            //   (b) it has a child v where low[v] >= disc[u]
            bool isAP = (parent == -1 && children > 1) ||
                        (parent != -1 && low[v] >= disc[u]);

            if (isAP) {
                artPoints.insert(u);
                // Pop the biconnected component containing edge (u,v)
                std::vector<int> blockNodes;
                std::set<int> seen;
                while (edgeStack.top() != std::make_pair(u, v)) {
                    auto [a, b] = edgeStack.top(); edgeStack.pop();
                    if (seen.insert(a).second) blockNodes.push_back(a);
                    if (seen.insert(b).second) blockNodes.push_back(b);
                }
                auto [a, b] = edgeStack.top(); edgeStack.pop();
                if (seen.insert(a).second) blockNodes.push_back(a);
                if (seen.insert(b).second) blockNodes.push_back(b);
                blocks.push_back(blockNodes);
            }
        } else if (v != parent && disc[v] < disc[u]) {
            // Back edge
            edgeStack.push({u, v});
            low[u] = std::min(low[u], disc[v]);
        }
    }
}

void RegisterAllocator::findBiconnectedComponents(const InterferenceGraph &ig,
                                                   std::vector<std::vector<int>> &blocks,
                                                   std::set<int> &artPoints) {
    int total = ig.size();
    std::vector<int> disc(total, -1), low(total, -1);
    std::vector<bool> visited(total, false);
    std::stack<std::pair<int,int>> edgeStack;
    int timer = 0;

    for (int u = 0; u < total; u++) {
        if (visited[u]) continue;
        bcDFS(u, -1, timer, disc, low, visited, edgeStack, ig, blocks, artPoints);
        // Remaining edges in stack form the last biconnected component
        if (!edgeStack.empty()) {
            std::vector<int> blockNodes;
            std::set<int> seen;
            while (!edgeStack.empty()) {
                auto [a, b] = edgeStack.top(); edgeStack.pop();
                if (seen.insert(a).second) blockNodes.push_back(a);
                if (seen.insert(b).second) blockNodes.push_back(b);
            }
            blocks.push_back(blockNodes);
        }
    }

    // Isolated vertices (degree 0) form their own trivial block
    for (int u = 0; u < total; u++) {
        if (ig.neighbours(u).empty()) {
            blocks.push_back({u});
        }
    }
}

bool RegisterAllocator::colorBlock(const std::vector<int> &blockNodes,
                                    std::vector<Web> &webs,
                                    const InterferenceGraph &ig,
                                    int N,
                                    const std::map<int,int> &fixedColors) {
    if (blockNodes.empty()) return true;

    // Apply fixed colors for articulation points first
    for (int u : blockNodes) {
        auto it = fixedColors.find(u);
        if (it != fixedColors.end())
            webs[u].assignedRegister = it->second;
    }

    int sz = (int)blockNodes.size();

    // ── Small block: exact backtracking (≤32 nodes) ──────────────────────────
    // For very small blocks (≤12 free nodes) we search for the minimum k
    // (chromatic number of the block) by trying k=minK..N in order.
    // For larger small blocks (13-32) we use N directly to avoid factorial blowup.
    if (sz <= 32) {
        std::vector<std::pair<int,int>> saved;
        for (int u : blockNodes)
            saved.push_back({u, webs[u].assignedRegister});

        // Build order: fixed nodes first, then uncolored by degree desc
        std::vector<int> order;
        for (int u : blockNodes)
            if (fixedColors.count(u))  order.push_back(u);
        std::vector<int> freeNodes;
        for (int u : blockNodes)
            if (!fixedColors.count(u)) freeNodes.push_back(u);
        std::sort(freeNodes.begin(), freeNodes.end(), [&](int a, int b){
            return ig.neighbours(a).size() > ig.neighbours(b).size();
        });
        for (int u : freeNodes) order.push_back(u);

        // Minimum k imposed by already-fixed AP colors
        int minK = 0;
        for (auto &[id, c] : fixedColors) minK = std::max(minK, c + 1);

        // Sweep k upward from minK to find χ(block) — gives the minimum registers
        // needed for this block. Safe for sparse blocks; dense ones are fast anyway
        // because backtracking prunes early. Jump to N only for very large free sets.
        int startK = ((int)freeNodes.size() <= 24) ? minK : N;

        std::vector<std::pair<int,int>> bestColoring;
        for (int k = startK; k <= N; k++) {
            for (int u : freeNodes) webs[u].assignedRegister = -1;
            if (backtrack(order, 0, webs, ig, k, fixedColors)) {
                for (int u : blockNodes)
                    bestColoring.push_back({u, webs[u].assignedRegister});
                break;
            }
        }

        if (!bestColoring.empty()) {
            for (auto &[u, c] : bestColoring) webs[u].assignedRegister = c;
            return true;
        }

        for (auto &[u, c] : saved) webs[u].assignedRegister = c;
        return false;
    }

    // ── Large block: classify and choose heuristic ─────────────────────────
    std::set<int> nodeSet(blockNodes.begin(), blockNodes.end());
    int edgeCount = 0, maxDeg = 0;
    for (int u : blockNodes) {
        int localDeg = 0;
        for (int nb : ig.neighbours(u))
            if (nodeSet.count(nb)) { localDeg++; edgeCount++; }
        maxDeg = std::max(maxDeg, localDeg);
    }
    edgeCount /= 2;
    double density = (sz > 1) ? (2.0 * edgeCount) / (sz * (sz - 1)) : 0.0;
    bool useDsatur = (density > 0.6) || (maxDeg >= N - 1);

    if (useDsatur) {
        // Best-of-5 DSATUR: run with different tie-breaking strategies, keep the
        // coloring that uses the fewest distinct colors (minimizes register pressure).
        //
        // Tie-break strategies (all sat-first, then secondary):
        //   0: highest degree (standard DSATUR)
        //   1: lowest degree  (color constrained nodes last)
        //   2: longest live range (spill-cost proxy)
        //   3: shortest live range (cheapest-to-spill first)
        //   4: most APs among neighbors (hardest to reconcile first)

        auto runDsatur = [&](int strategy) {
            // Save pre-colored state
            std::vector<int> saved(ig.size());
            for (int u : blockNodes) saved[u] = webs[u].assignedRegister;

            std::vector<std::set<int>> saturation(ig.size());
            std::vector<bool> colored(ig.size(), false);
            for (int u : blockNodes) {
                if (webs[u].assignedRegister >= 0) {
                    colored[u] = true;
                    for (int nb : ig.neighbours(u))
                        if (nodeSet.count(nb) && !colored[nb])
                            saturation[nb].insert(webs[u].assignedRegister);
                }
            }

            int remaining = (int)std::count_if(blockNodes.begin(), blockNodes.end(),
                                                [&](int u){ return !colored[u]; });
            while (remaining > 0) {
                int chosen = -1, bestSat = -1;
                double bestTie = -1e18;
                for (int u : blockNodes) {
                    if (colored[u]) continue;
                    int sat = (int)saturation[u].size();
                    int deg = (int)ig.neighbours(u).size();
                    int lrl = (int)webs[u].lines.size();
                    int precolored = 0;
                    for (int nb : ig.neighbours(u))
                        if (nodeSet.count(nb) && webs[nb].assignedRegister >= 0) precolored++;

                    double tie = 0;
                    switch (strategy) {
                        case 0:  tie =  deg; break;
                        case 1:  tie = -deg; break;
                        case 2:  tie =  lrl; break;
                        case 3:  tie = -lrl; break;
                        case 4:  tie =  precolored * 100 + deg; break;
                    }
                    if (sat > bestSat || (sat == bestSat && tie > bestTie)) {
                        bestSat = sat; bestTie = tie; chosen = u;
                    }
                }
                if (chosen < 0) break;

                std::set<int> used;
                for (int nb : ig.neighbours(chosen))
                    if (webs[nb].assignedRegister >= 0) used.insert(webs[nb].assignedRegister);

                int color = -1;
                for (int c = 0; c < N; c++)
                    if (!used.count(c)) { color = c; break; }

                webs[chosen].assignedRegister = color;
                colored[chosen] = true;
                remaining--;

                if (color >= 0)
                    for (int nb : ig.neighbours(chosen))
                        if (nodeSet.count(nb) && !colored[nb])
                            saturation[nb].insert(color);
            }

            // Collect result and restore
            std::vector<std::pair<int,int>> result;
            for (int u : blockNodes) {
                result.push_back({u, webs[u].assignedRegister});
                webs[u].assignedRegister = saved[u];
            }
            return result;
        };

        // Run all 5 strategies, keep the one using fewest distinct colors
        std::vector<std::pair<int,int>> best;
        int bestColors = N + 1;
        for (int s = 0; s < 5; s++) {
            auto coloring = runDsatur(s);
            std::set<int> usedColors;
            bool valid = true;
            for (auto &[u, c] : coloring) {
                if (c < 0) { valid = false; break; }
                usedColors.insert(c);
            }
            int nc = (int)usedColors.size();
            if (valid && nc < bestColors) {
                bestColors = nc;
                best = coloring;
            }
        }

        // Apply best coloring (or fallback: strategy 0)
        if (best.empty()) best = runDsatur(0);
        for (auto &[u, c] : best) webs[u].assignedRegister = c;
    } else {
        // Greedy degree-descending on uncolored nodes
        std::vector<int> order;
        for (int u : blockNodes)
            if (webs[u].assignedRegister < 0) order.push_back(u);
        std::sort(order.begin(), order.end(), [&](int a, int b){
            return ig.neighbours(a).size() > ig.neighbours(b).size();
        });
        for (int u : order) {
            std::set<int> used;
            for (int nb : ig.neighbours(u))
                if (webs[nb].assignedRegister >= 0) used.insert(webs[nb].assignedRegister);
            for (int c = 0; c < N; c++) {
                if (!used.count(c)) { webs[u].assignedRegister = c; break; }
            }
        }
    }

    for (int u : blockNodes)
        if (webs[u].assignedRegister < 0) return false;
    return true;
}

bool RegisterAllocator::kempeSwap(int u, int targetColor,
                                   std::vector<Web> &webs,
                                   const InterferenceGraph &ig,
                                   const std::set<int> &coloredSet) {
    // Find a neighbor of u that has targetColor; try to Kempe-swap it away.
    for (int nb : ig.neighbours(u)) {
        if (!coloredSet.count(nb)) continue;
        if (webs[nb].assignedRegister != targetColor) continue;

        // nb has targetColor. Find a color 'alt' used by nb's neighbors (other than u)
        // to swap with. We want to swap targetColor <-> alt in the Kempe chain rooted at nb.
        std::set<int> nbUsed;
        for (int nb2 : ig.neighbours(nb))
            if (coloredSet.count(nb2) && nb2 != u && webs[nb2].assignedRegister >= 0)
                nbUsed.insert(webs[nb2].assignedRegister);

        // Try each alternate color
        for (int alt : nbUsed) {
            if (alt == targetColor) continue;

            // BFS/DFS to find the Kempe chain: nodes reachable from nb
            // through edges where both endpoints have color targetColor or alt
            std::vector<int> chain;
            std::set<int> inChain;
            std::stack<int> stk;
            stk.push(nb);
            inChain.insert(nb);

            while (!stk.empty()) {
                int cur = stk.top(); stk.pop();
                chain.push_back(cur);
                for (int adj : ig.neighbours(cur)) {
                    if (!coloredSet.count(adj) || inChain.count(adj)) continue;
                    int c = webs[adj].assignedRegister;
                    if (c == targetColor || c == alt) {
                        inChain.insert(adj);
                        stk.push(adj);
                    }
                }
            }

            // Verify the swap doesn't create a conflict for u:
            // After swapping, nb's color becomes alt. Check u's other neighbors for alt.
            bool uHasAlt = false;
            for (int nb3 : ig.neighbours(u)) {
                if (nb3 == nb || !coloredSet.count(nb3)) continue;
                if (inChain.count(nb3) && webs[nb3].assignedRegister == alt)
                    uHasAlt = true; // after swap this becomes targetColor, fine
                if (!inChain.count(nb3) && webs[nb3].assignedRegister == targetColor) {
                    // another neighbor outside chain keeps targetColor — still conflicts
                    // but this is about freeing targetColor at u from nb specifically
                }
            }

            // Do the swap
            for (int node : chain)
                webs[node].assignedRegister = (webs[node].assignedRegister == targetColor) ? alt : targetColor;

            // Verify nb now has alt (i.e., targetColor is freed from u's perspective at nb)
            if (webs[nb].assignedRegister != targetColor)
                return true; // swap succeeded, targetColor freed from this neighbor

            // Swap back if it didn't help
            for (int node : chain)
                webs[node].assignedRegister = (webs[node].assignedRegister == targetColor) ? alt : targetColor;
        }
    }
    return false;
}

void RegisterAllocator::normalizeColors(std::vector<Web> &webs,
                                         const InterferenceGraph &ig) {
    // Build a conflict graph over color indices: two colors conflict if some pair
    // of interfering webs uses them. Then greedily remap colors to 0..K-1.
    int maxColor = -1;
    for (const auto &w : webs)
        if (w.assignedRegister >= 0) maxColor = std::max(maxColor, w.assignedRegister);
    if (maxColor < 0) return;

    int C = maxColor + 1;
    // colorConflicts[i] = set of colors that conflict with color i
    std::vector<std::set<int>> colorConflicts(C);
    int total = ig.size();
    for (int i = 0; i < total; i++) {
        if (webs[i].assignedRegister < 0) continue;
        for (int j : ig.neighbours(i)) {
            if (webs[j].assignedRegister < 0) continue;
            int ci = webs[i].assignedRegister;
            int cj = webs[j].assignedRegister;
            if (ci != cj) {
                colorConflicts[ci].insert(cj);
                colorConflicts[cj].insert(ci);
            }
        }
    }

    // Greedy recoloring of colors themselves
    std::vector<int> remap(C, -1);
    for (int c = 0; c < C; c++) {
        std::set<int> forbidden;
        for (int conflict : colorConflicts[c])
            if (remap[conflict] >= 0) forbidden.insert(remap[conflict]);
        for (int nc = 0; ; nc++) {
            if (!forbidden.count(nc)) { remap[c] = nc; break; }
        }
    }

    for (auto &w : webs)
        if (w.assignedRegister >= 0) w.assignedRegister = remap[w.assignedRegister];
}

// ─── Color reduction pass ─────────────────────────────────────────────────────
//
// After normalizeColors the assignment uses colors 0..K-1.  We try to eliminate
// color K-1 by Kempe-swapping each node that holds it into one of the K-1 lower
// colors.  A Kempe swap between colors A and B flips A<->B in the maximal
// connected subgraph of nodes colored A or B reachable from the target node.
// The swap is always conflict-free; we only apply it when it actually removes
// color K-1 from the target node.  If all nodes using K-1 are successfully
// recolored, K drops by one and we repeat.

void RegisterAllocator::reduceColors(std::vector<Web> &webs,
                                      const InterferenceGraph &ig,
                                      int N) {
    int total = (int)webs.size();

    auto currentMax = [&]() {
        int m = -1;
        for (const auto &w : webs)
            if (w.assignedRegister >= 0) m = std::max(m, w.assignedRegister);
        return m;
    };

    bool progress = true;
    while (progress) {
        progress = false;
        int top = currentMax();
        if (top <= 0) break;

        // Collect nodes that use the highest color.
        std::vector<int> targets;
        for (int u = 0; u < total; u++)
            if (webs[u].assignedRegister == top) targets.push_back(u);

        // Try to recolor each target into a lower color via Kempe swap.
        // We work on a copy so a failed attempt doesn't leave partial changes.
        std::vector<int> saved(total);
        for (int u = 0; u < total; u++) saved[u] = webs[u].assignedRegister;

        bool allMoved = true;
        for (int u : targets) {
            if (webs[u].assignedRegister != top) continue; // already moved by prior swap

            bool moved = false;
            // Try swapping color `top` with each lower color c at node u.
            for (int c = 0; c < top && !moved; c++) {
                // Check u's neighbors don't already use c (swap would give u color c).
                bool uCanTakeC = true;
                for (int nb : ig.neighbours(u)) {
                    if (webs[nb].assignedRegister == c) { uCanTakeC = false; break; }
                }
                if (!uCanTakeC) continue;

                // BFS Kempe chain: nodes reachable from u through {top, c}-colored edges.
                std::vector<int> chain;
                std::vector<bool> inChain(total, false);
                std::stack<int> stk;
                stk.push(u); inChain[u] = true;
                while (!stk.empty()) {
                    int cur = stk.top(); stk.pop();
                    chain.push_back(cur);
                    for (int nb : ig.neighbours(cur)) {
                        if (inChain[nb]) continue;
                        int nc = webs[nb].assignedRegister;
                        if (nc == top || nc == c) {
                            inChain[nb] = true;
                            stk.push(nb);
                        }
                    }
                }

                // Apply swap: top<->c within chain.
                for (int node : chain)
                    webs[node].assignedRegister =
                        (webs[node].assignedRegister == top) ? c : top;

                // Verify no conflicts introduced.
                bool valid = true;
                for (int node : chain) {
                    int cn = webs[node].assignedRegister;
                    for (int nb : ig.neighbours(node)) {
                        if (webs[nb].assignedRegister == cn) { valid = false; break; }
                    }
                    if (!valid) break;
                }

                if (valid && webs[u].assignedRegister != top) {
                    moved = true;
                } else {
                    // Rollback this swap.
                    for (int node : chain)
                        webs[node].assignedRegister =
                            (webs[node].assignedRegister == top) ? c : top;
                }
            }

            if (!moved) { allMoved = false; break; }
        }

        if (allMoved && !targets.empty()) {
            // Verify the top color is truly gone.
            bool gone = true;
            for (const auto &w : webs)
                if (w.assignedRegister == top) { gone = false; break; }
            if (gone) { progress = true; continue; }
        }

        // Restore saved state — this reduction attempt failed.
        for (int u = 0; u < total; u++) webs[u].assignedRegister = saved[u];
    }
}

// ─── Block-cut tree DP ────────────────────────────────────────────────────────
//
// We build the block-cut tree where:
//   - Each BCC block is a "B-node"
//   - Each articulation point is an "AP-node"
// Edges connect B-nodes to the AP-nodes they contain.
//
// The DP runs bottom-up on this tree.  For each AP-node we compute, for every
// candidate color c, the minimum number of additional registers needed in the
// subtree below it when the AP is forced to color c.  The root block then
// picks the AP colors that minimise the total.
//
// For BCT-Color we don't need a full cost model — we just need to pick
// AP colors that keep adjacent blocks colorable.  So the DP value is:
//   dp[ap][c] = 1  (feasible with AP=c)  or  INFCOST  (infeasible)
// and we propagate bottom-up picking the lexicographically smallest feasible c.

bool RegisterAllocator::partitionedColoring(std::vector<Web> &webs,
                                             InterferenceGraph &ig, int N) {
    int total = ig.size();
    for (auto &w : webs) w.assignedRegister = -1;
    if (total == 0) return true;

    // ── Step 1: Find BCCs and articulation points ─────────────────────────────
    std::vector<std::vector<int>> blocks;
    std::set<int> artPoints;
    findBiconnectedComponents(ig, blocks, artPoints);

    int B = (int)blocks.size();
    if (B == 0) return true;

    // ── Step 2: Build block-cut tree ──────────────────────────────────────────
    // Node numbering:
    //   0..B-1       → B-nodes (blocks)
    //   B..B+|AP|-1  → AP-nodes (articulation points)
    //
    // apIndex[webId] → AP-node index in block-cut tree
    // apWebId[apIdx] → web id of this AP-node
    std::vector<int> apWebId;
    std::map<int,int> apIndex;
    for (int id : artPoints) {
        apIndex[id] = B + (int)apWebId.size();
        apWebId.push_back(id);
    }
    int treeSize = B + (int)apWebId.size();

    // Adjacency list of block-cut tree
    std::vector<std::vector<int>> treeAdj(treeSize);
    for (int bi = 0; bi < B; bi++) {
        for (int u : blocks[bi]) {
            if (artPoints.count(u)) {
                int ai = apIndex[u];
                treeAdj[bi].push_back(ai);
                treeAdj[ai].push_back(bi);
            }
        }
    }

    // ── Step 3: Tree DP — bottom-up feasibility per AP color choice ───────────
    //
    // dp[ap_tree_idx][color] = true if the subtree rooted at this AP can be
    // colored when the AP takes `color`, given that all APs above it are fixed.
    //
    // We root the tree at block-node 0 and do a DFS.
    // For each AP-node a adjacent to block-node b (b is the parent in the tree),
    // we compute which colors for `a` allow the child blocks of `a` to be colored.
    //
    // Implementation: iterative post-order DFS.

    // dp[ai][c] = minimum distinct colors used by the subtree when AP ai takes color c.
    // INFCOST means infeasible. We minimise total register pressure across all child blocks.
    const int INFCOST = 1000;
    int numAPs = (int)apWebId.size();
    std::vector<std::vector<int>> dp(numAPs, std::vector<int>(N, 0)); // 0 = no cost yet, feasible

    // chosen[ai] = color decided for this AP by the DP
    std::vector<int> chosen(numAPs, -1);

    // Post-order traversal
    std::vector<int> treeParent(treeSize, -1);
    std::vector<bool> vis(treeSize, false);
    std::vector<int> postOrder;
    {
        std::stack<int> stk;
        stk.push(0);
        vis[0] = true;
        // Iterative DFS — children pushed right-to-left so left child processed first
        // We need a proper post-order, so use the "push twice" trick.
        std::stack<std::pair<int,bool>> dfsStk;
        dfsStk.push({0, false});
        while (!dfsStk.empty()) {
            auto [u, processed] = dfsStk.top(); dfsStk.pop();
            if (processed) {
                postOrder.push_back(u);
                continue;
            }
            dfsStk.push({u, true});
            for (int v : treeAdj[u]) {
                if (!vis[v]) {
                    vis[v] = true;
                    treeParent[v] = u;
                    dfsStk.push({v, false});
                }
            }
        }
    }

    // Process in post-order
    // When we reach a B-node b, for each AP child a of b (a is below b in the tree),
    // we already know dp[a] (feasible colors for a). We now try to color block b
    // with each feasible AP color assignment for a, and update dp[a] to only include
    // colors that actually allow b to be colored.
    //
    // When we reach an AP-node a, we propagate: the intersection of feasibility
    // from all child B-nodes is already in dp[a] from the B-node processing above.
    // We just pick the best color.

    for (int u : postOrder) {
        if (u < B) {
            // B-node: for each AP child, restrict dp[child] to colors that
            // allow this block to be colored.
            int bi = u;
            for (int v : treeAdj[bi]) {
                if (treeParent[v] != bi) continue; // v is child of bi
                // v must be an AP-node (B-node → AP-node edges only)
                if (v < B) continue;
                int ai = v - B;
                int apId = apWebId[ai];

                // For each candidate color for this AP, test if block bi is colorable
                // and record the number of distinct colors used (register cost).
                for (int c = 0; c < N; c++) {
                    if (dp[ai][c] == INFCOST) continue;

                    std::map<int,int> testFixed;
                    testFixed[apId] = c;
                    for (int w : blocks[bi]) {
                        if (artPoints.count(w) && w != apId) {
                            int wai = apIndex[w] - B;
                            if (chosen[wai] >= 0) testFixed[w] = chosen[wai];
                        }
                    }

                    std::vector<std::pair<int,int>> saved;
                    for (int n : blocks[bi]) { saved.push_back({n, webs[n].assignedRegister}); webs[n].assignedRegister = -1; }

                    bool ok = colorBlock(blocks[bi], webs, ig, N, testFixed);
                    if (!ok) {
                        dp[ai][c] = INFCOST;
                    } else {
                        // Count distinct colors used in this block
                        std::set<int> usedC;
                        for (int n : blocks[bi])
                            if (webs[n].assignedRegister >= 0) usedC.insert(webs[n].assignedRegister);
                        dp[ai][c] += (int)usedC.size(); // accumulate child block cost
                    }

                    for (auto &[n, col] : saved) webs[n].assignedRegister = col;
                }
            }
        } else {
            // AP-node: pick the lexicographically smallest feasible color,
            // also avoiding colors already chosen by interfering APs.
            int ai = u - B;
            int apId = apWebId[ai];
            std::set<int> apForbidden;
            for (int nb : ig.neighbours(apId)) {
                if (artPoints.count(nb) && apIndex.count(nb)) {
                    int nai = apIndex.at(nb) - B;
                    if (chosen[nai] >= 0) apForbidden.insert(chosen[nai]);
                }
            }
            // Pick the feasible color with minimum accumulated child-block cost.
            int bestCost = INFCOST + 1;
            for (int c = 0; c < N; c++) {
                if (dp[ai][c] < INFCOST && !apForbidden.count(c)) {
                    if (dp[ai][c] < bestCost) { bestCost = dp[ai][c]; chosen[ai] = c; }
                }
            }
            // Relax AP-AP constraint if no color found
            if (chosen[ai] < 0) {
                for (int c = 0; c < N; c++) {
                    if (dp[ai][c] < INFCOST && dp[ai][c] < bestCost) {
                        bestCost = dp[ai][c]; chosen[ai] = c;
                    }
                }
            }
            if (chosen[ai] < 0) chosen[ai] = 0;
        }
    }

    // ── Step 3b: Resolve AP-AP color conflicts before top-down coloring ─────────
    //
    // Two APs that directly interfere may have been assigned the same color by
    // the DP when they were processed in an order where the second hadn't yet
    // seen the first's chosen color. Run iterative greedy recoloring until stable:
    // treat the AP subgraph as a graph to color, reassigning any AP whose chosen
    // color conflicts with a neighbor's chosen color.
    bool changed = true;
    while (changed) {
        changed = false;
        for (int ai = 0; ai < numAPs; ai++) {
            int apId = apWebId[ai];
            std::set<int> forbidden;
            for (int nb : ig.neighbours(apId)) {
                if (!artPoints.count(nb) || !apIndex.count(nb)) continue;
                int nai = apIndex.at(nb) - B;
                if (chosen[nai] >= 0) forbidden.insert(chosen[nai]);
            }
            if (chosen[ai] >= 0 && !forbidden.count(chosen[ai])) continue;
            // Conflict or unassigned — pick min-cost non-conflicting DP-feasible color
            int old = chosen[ai];
            chosen[ai] = -1;
            int bestC = INFCOST + 1;
            for (int c = 0; c < N; c++) {
                if (!forbidden.count(c) && dp[ai][c] < INFCOST && dp[ai][c] < bestC) {
                    bestC = dp[ai][c]; chosen[ai] = c;
                }
            }
            if (chosen[ai] < 0) { // no DP-feasible color available — relax DP constraint
                for (int c = 0; c < N; c++) {
                    if (!forbidden.count(c)) { chosen[ai] = c; break; }
                }
            }
            // If truly no color is available, mark as unresolvable (-2 = spill)
            if (chosen[ai] < 0) chosen[ai] = -2;
            if (chosen[ai] != old) changed = true;
        }
    }

    // ── Step 4: Color all blocks top-down with fixed AP colors from DP ────────
    //
    // Pre-order traversal: root block first, propagate AP choices downward.
    std::map<int,int> fixedColors; // webId → color decided by DP
    for (int ai = 0; ai < numAPs; ai++) {
        if (chosen[ai] >= 0) fixedColors[apWebId[ai]] = chosen[ai];
        else if (chosen[ai] == -2) webs[apWebId[ai]].assignedRegister = -2; // spill unresolvable AP
    }

    // Topological order for blocks: BFS from root block (index 0)
    std::vector<int> blockOrder;
    {
        std::vector<bool> bvis(treeSize, false);
        std::queue<int> q;
        q.push(0);
        bvis[0] = true;
        while (!q.empty()) {
            int u = q.front(); q.pop();
            if (u < B) blockOrder.push_back(u);
            for (int v : treeAdj[u]) {
                if (!bvis[v]) { bvis[v] = true; q.push(v); }
            }
        }
        // Include any disconnected blocks not reachable from 0
        for (int bi = 0; bi < B; bi++) {
            if (!bvis[bi]) blockOrder.push_back(bi);
        }
    }

    bool allColored = true;
    std::set<int> coloredGlobal;

    // APs pre-marked as spilled (chosen=-2) are already resolved — treat as colored
    for (int ai = 0; ai < numAPs; ai++)
        if (webs[apWebId[ai]].assignedRegister == -2) coloredGlobal.insert(apWebId[ai]);

    for (int bi : blockOrder) {
        std::map<int,int> blockFixed;
        for (int u : blocks[bi]) {
            if (fixedColors.count(u)) blockFixed[u] = fixedColors[u];
        }

        // Reset block nodes not yet colored (APs may already be colored from prior block)
        std::vector<int> toColor;
        for (int u : blocks[bi]) {
            if (!coloredGlobal.count(u)) toColor.push_back(u);
            else if (artPoints.count(u) && fixedColors.count(u)) toColor.push_back(u); // re-apply fixed color
        }
        if (toColor.empty()) continue;

        // Clear non-AP nodes' assignments before coloring this block
        for (int u : toColor)
            if (!artPoints.count(u)) webs[u].assignedRegister = -1;

        bool ok = colorBlock(toColor, webs, ig, N, blockFixed);

        if (!ok) {
            // Fallback: Kempe swap on unresolved APs, then retry non-APs
            for (int u : toColor) {
                if (!artPoints.count(u) || webs[u].assignedRegister >= 0) continue;
                std::set<int> used;
                for (int nb : ig.neighbours(u))
                    if (webs[nb].assignedRegister >= 0) used.insert(webs[nb].assignedRegister);
                for (int c = 0; c < N; c++) {
                    if (!used.count(c)) continue;
                    if (kempeSwap(u, c, webs, ig, coloredGlobal)) break;
                }
                std::set<int> usedNow;
                for (int nb : ig.neighbours(u))
                    if (webs[nb].assignedRegister >= 0) usedNow.insert(webs[nb].assignedRegister);
                for (int c = 0; c < N; c++) {
                    if (!usedNow.count(c)) { webs[u].assignedRegister = c; break; }
                }
                if (webs[u].assignedRegister < 0) { webs[u].assignedRegister = -2; allColored = false; }
            }
            for (int u : toColor) {
                if (artPoints.count(u) || webs[u].assignedRegister >= 0) continue;
                std::set<int> used;
                for (int nb : ig.neighbours(u))
                    if (webs[nb].assignedRegister >= 0 && webs[nb].assignedRegister != -2)
                        used.insert(webs[nb].assignedRegister);
                bool assigned = false;
                for (int c = 0; c < N; c++) {
                    if (!used.count(c)) { webs[u].assignedRegister = c; assigned = true; break; }
                }
                if (!assigned) { webs[u].assignedRegister = -2; allColored = false; }
            }
        }

        for (int u : toColor) coloredGlobal.insert(u);
    }

    // ── Step 5: Global color normalization + Kempe-chain reduction ───────────
    normalizeColors(webs, ig);
    reduceColors(webs, ig, N);

    for (auto &w : webs)
        if (w.assignedRegister == -1) { w.assignedRegister = -2; allColored = false; }

    return allColored;
}

// ─── Coloring validator ────────────────────────────────────────────────────────

bool RegisterAllocator::validateColoring(const std::vector<Web> &webs,
                                         const InterferenceGraph &ig,
                                         int N) {
    bool valid = true;
    int total = (int)webs.size();

    for (int u = 0; u < total; u++) {
        int cu = webs[u].assignedRegister;

        // Invariant 3: no uninitialized assignments
        if (cu == -1) {
            std::cerr << "[validateColoring] web " << u
                      << " has uninitialized assignment (-1)\n";
            valid = false;
            continue;
        }
        if (cu == -2) continue; // spilled — valid by definition

        // Invariant 2: color index in range
        if (cu >= N) {
            std::cerr << "[validateColoring] web " << u
                      << " assigned register " << cu
                      << " >= N=" << N << "\n";
            valid = false;
        }

        // Invariant 1: no conflict with interfering neighbors
        for (int v : ig.neighbours(u)) {
            int cv = webs[v].assignedRegister;
            if (cv >= 0 && cu == cv) {
                std::cerr << "[validateColoring] conflict: web " << u
                          << " and web " << v
                          << " both assigned register " << cu << "\n";
                valid = false;
            }
        }
    }
    return valid;
}

// ─── Main dispatch ─────────────────────────────────────────────────────────────

AllocationResult RegisterAllocator::allocate(std::vector<Web> &webs,
                                              InterferenceGraph &ig,
                                              const RegisterConfig &config) {
    AllocationResult result;
    int N = config.numRegisters;

    auto countRegisters = [&](const std::vector<Web> &ws) {
        std::set<int> used;
        for (const auto &w : ws) if (w.assignedRegister >= 0) used.insert(w.assignedRegister);
        return (int)used.size();
    };

    auto markAllMemory = [&](std::vector<Web> &ws) {
        for (auto &w : ws) w.assignedRegister = -2;
    };

    if (config.algorithm == "basic") {
        std::vector<int> spilled;
        bool ok = basicColoring(webs, ig, N, spilled);
        for (int id : spilled) webs[id].assignedRegister = -2;
        result.webs = webs;
        result.feasible = ok;
        result.registersUsed = ok ? countRegisters(webs) : 0;
        if (!ok) markAllMemory(result.webs);

    } else if (config.algorithm == "spilling") {
        int maxSpill = config.algorithmParam;
        for (auto &w : webs) w.assignedRegister = -1;
        std::vector<int> spilled;
        basicColoring(webs, ig, N, spilled);
        if ((int)spilled.size() <= maxSpill) {
            for (int id : spilled) webs[id].assignedRegister = -2;
            result.webs = webs;
            result.feasible = true;
            result.registersUsed = countRegisters(webs);
        } else {
            markAllMemory(webs);
            result.webs = webs;
            result.feasible = false;
            result.registersUsed = 0;
        }

    } else if (config.algorithm == "dsatur") {
        bool ok = dsaturColoring(webs, ig, N);
        // Spill uncolored webs (-1 → -2); keep partial colorings like BCT-Color does.
        int colored = 0;
        for (auto &w : webs) {
            if (w.assignedRegister == -1) w.assignedRegister = -2;
            if (w.assignedRegister >= 0) colored++;
        }
        result.webs = webs;
        result.feasible = ok || colored > 0;
        result.registersUsed = countRegisters(webs);

    } else if (config.algorithm == "splitting") {
        int maxSplit = config.algorithmParam;
        bool solved = false;
        for (int attempt = 0; attempt <= maxSplit && !solved; attempt++) {
            InterferenceGraph currentIg(webs);
            for (auto &w : webs) w.assignedRegister = -1;
            std::vector<int> spilled;
            bool ok = basicColoring(webs, currentIg, N, spilled);
            if (ok) {
                result.webs = webs;
                result.feasible = true;
                result.registersUsed = countRegisters(webs);
                solved = true;
            } else if (attempt < maxSplit) {
                for (auto &w : webs) w.assignedRegister = -1;
                InterferenceGraph igForSplit(webs);
                int candidate = selectSplitCandidate(igForSplit, webs);
                if (candidate >= 0) splitWeb(webs, candidate);
            }
        }
        if (!solved) {
            markAllMemory(webs);
            result.webs = webs;
            result.feasible = false;
            result.registersUsed = 0;
        }

    } else {
        // BCT-Color — biconnected-partition coloring with per-block algorithm dispatch.
        // Decomposes the interference graph into biconnected components (BCCs), solves
        // each with the locally optimal algorithm (DSATUR for dense/clique-like blocks,
        // basic greedy for sparse blocks), reconciles articulation-point colors via the
        // block-cut tree DP, and normalizes the global assignment to minimize registers.
        //
        // Feasibility: a partial coloring is feasible as long as no two interfering
        // webs share a register — spilled webs (-2) are acceptable. This matches the
        // spilling algorithm's contract: spills go to memory, colored webs are valid.
        // Infeasible only when ALL webs were forced to memory (no coloring found at all).
        partitionedColoring(webs, ig, N);

        if (!validateColoring(webs, ig, N))
            std::cerr << "[BCT-Color] coloring invariant violated after partitionedColoring\n";

        // DSATUR floor: run plain DSATUR on the same graph and keep whichever
        // produces fewer spills. BCT-Color is never worse than DSATUR this way.
        {
            std::vector<Web> dsatWebs = webs; // copy pre-colored state (all -1 initially)
            // Reset for fresh DSATUR run
            for (auto &w : dsatWebs) w.assignedRegister = -1;
            InterferenceGraph igCopy(dsatWebs); // rebuild from clean webs
            // Actually ig is already the right graph; just need to re-run dsatur on it.
            // Use original webs reset to -1.
            std::vector<Web> dsatW = webs;
            for (auto &w : dsatW) w.assignedRegister = -1;
            dsaturColoring(dsatW, ig, N);
            for (auto &w : dsatW) if (w.assignedRegister == -1) w.assignedRegister = -2;

            int bctSpills = 0, dsatSpills = 0;
            for (const auto &w : webs)    if (w.assignedRegister == -2) bctSpills++;
            for (const auto &w : dsatW)   if (w.assignedRegister == -2) dsatSpills++;

            if (dsatSpills < bctSpills) webs = dsatW;
        }

        int colored = 0;
        for (auto &w : webs) {
            if (w.assignedRegister >= 0) colored++;
        }
        result.webs = webs;
        result.registersUsed = countRegisters(webs);
        result.feasible = (colored > 0 || webs.empty());
        if (!result.feasible) markAllMemory(result.webs);
    }

    return result;
}
