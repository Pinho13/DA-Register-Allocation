#include <algorithm>
#include <set>
#include <map>
#include <stack>
#include <queue>
#include <iostream>
#include "core/RegisterAllocator.h"

// Defined in Coloring_Dsatur.cpp
bool backtrack(const std::vector<int> &order, int idx,
               std::vector<Web> &webs,
               const InterferenceGraph &ig,
               int N,
               const std::map<int,int> &fixedColors);

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

            bool isAP = (parent == -1 && children > 1) ||
                        (parent != -1 && low[v] >= disc[u]);

            if (isAP) {
                artPoints.insert(u);
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

    for (int u : blockNodes) {
        auto it = fixedColors.find(u);
        if (it != fixedColors.end())
            webs[u].assignedRegister = it->second;
    }

    int sz = (int)blockNodes.size();

    if (sz <= 32) {
        std::vector<std::pair<int,int>> saved;
        for (int u : blockNodes)
            saved.push_back({u, webs[u].assignedRegister});

        std::vector<int> order;
        for (int u : blockNodes)
            if (fixedColors.count(u)) order.push_back(u);
        std::vector<int> freeNodes;
        for (int u : blockNodes)
            if (!fixedColors.count(u)) freeNodes.push_back(u);
        std::sort(freeNodes.begin(), freeNodes.end(), [&](int a, int b){
            return ig.neighbours(a).size() > ig.neighbours(b).size();
        });
        for (int u : freeNodes) order.push_back(u);

        int minK = 0;
        for (auto &[id, c] : fixedColors) minK = std::max(minK, c + 1);

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
        auto runDsatur = [&](int strategy) {
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

            std::vector<std::pair<int,int>> result;
            for (int u : blockNodes) {
                result.push_back({u, webs[u].assignedRegister});
                webs[u].assignedRegister = saved[u];
            }
            return result;
        };

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

        if (best.empty()) best = runDsatur(0);
        for (auto &[u, c] : best) webs[u].assignedRegister = c;
    } else {
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

bool RegisterAllocator::partitionedColoring(std::vector<Web> &webs,
                                             InterferenceGraph &ig, int N) {
    int total = ig.size();
    for (auto &w : webs) w.assignedRegister = -1;
    if (total == 0) return true;

    std::vector<std::vector<int>> blocks;
    std::set<int> artPoints;
    findBiconnectedComponents(ig, blocks, artPoints);

    int B = (int)blocks.size();
    if (B == 0) return true;

    std::vector<int> apWebId;
    std::map<int,int> apIndex;
    for (int id : artPoints) {
        apIndex[id] = B + (int)apWebId.size();
        apWebId.push_back(id);
    }
    int treeSize = B + (int)apWebId.size();

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

    const int INFCOST = 1000;
    int numAPs = (int)apWebId.size();
    std::vector<std::vector<int>> dp(numAPs, std::vector<int>(N, 0));
    std::vector<int> chosen(numAPs, -1);

    std::vector<int> treeParent(treeSize, -1);
    std::vector<bool> vis(treeSize, false);
    std::vector<int> postOrder;
    {
        std::stack<std::pair<int,bool>> dfsStk;
        dfsStk.push({0, false});
        vis[0] = true;
        while (!dfsStk.empty()) {
            auto [u, processed] = dfsStk.top(); dfsStk.pop();
            if (processed) { postOrder.push_back(u); continue; }
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

    for (int u : postOrder) {
        if (u < B) {
            int bi = u;
            for (int v : treeAdj[bi]) {
                if (treeParent[v] != bi) continue;
                if (v < B) continue;
                int ai = v - B;
                int apId = apWebId[ai];

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
                        std::set<int> usedC;
                        for (int n : blocks[bi])
                            if (webs[n].assignedRegister >= 0) usedC.insert(webs[n].assignedRegister);
                        dp[ai][c] += (int)usedC.size();
                    }

                    for (auto &[n, col] : saved) webs[n].assignedRegister = col;
                }
            }
        } else {
            int ai = u - B;
            int apId = apWebId[ai];
            std::set<int> apForbidden;
            for (int nb : ig.neighbours(apId)) {
                if (artPoints.count(nb) && apIndex.count(nb)) {
                    int nai = apIndex.at(nb) - B;
                    if (chosen[nai] >= 0) apForbidden.insert(chosen[nai]);
                }
            }
            int bestCost = INFCOST + 1;
            for (int c = 0; c < N; c++) {
                if (dp[ai][c] < INFCOST && !apForbidden.count(c)) {
                    if (dp[ai][c] < bestCost) { bestCost = dp[ai][c]; chosen[ai] = c; }
                }
            }
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
            int old = chosen[ai];
            chosen[ai] = -1;
            int bestC = INFCOST + 1;
            for (int c = 0; c < N; c++) {
                if (!forbidden.count(c) && dp[ai][c] < INFCOST && dp[ai][c] < bestC) {
                    bestC = dp[ai][c]; chosen[ai] = c;
                }
            }
            if (chosen[ai] < 0) {
                for (int c = 0; c < N; c++) {
                    if (!forbidden.count(c)) { chosen[ai] = c; break; }
                }
            }
            if (chosen[ai] < 0) chosen[ai] = -2;
            if (chosen[ai] != old) changed = true;
        }
    }

    std::map<int,int> fixedColors;
    for (int ai = 0; ai < numAPs; ai++) {
        if (chosen[ai] >= 0) fixedColors[apWebId[ai]] = chosen[ai];
        else if (chosen[ai] == -2) webs[apWebId[ai]].assignedRegister = -2;
    }

    std::vector<int> blockOrder;
    {
        std::vector<bool> bvis(treeSize, false);
        std::queue<int> q;
        q.push(0); bvis[0] = true;
        while (!q.empty()) {
            int u = q.front(); q.pop();
            if (u < B) blockOrder.push_back(u);
            for (int v : treeAdj[u]) {
                if (!bvis[v]) { bvis[v] = true; q.push(v); }
            }
        }
        for (int bi = 0; bi < B; bi++)
            if (!bvis[bi]) blockOrder.push_back(bi);
    }

    bool allColored = true;
    std::set<int> coloredGlobal;

    for (int ai = 0; ai < numAPs; ai++)
        if (webs[apWebId[ai]].assignedRegister == -2) coloredGlobal.insert(apWebId[ai]);

    for (int bi : blockOrder) {
        std::map<int,int> blockFixed;
        for (int u : blocks[bi]) {
            if (fixedColors.count(u)) blockFixed[u] = fixedColors[u];
        }

        std::vector<int> toColor;
        for (int u : blocks[bi]) {
            if (!coloredGlobal.count(u)) toColor.push_back(u);
            else if (artPoints.count(u) && fixedColors.count(u)) toColor.push_back(u);
        }
        if (toColor.empty()) continue;

        for (int u : toColor)
            if (!artPoints.count(u)) webs[u].assignedRegister = -1;

        bool ok = colorBlock(toColor, webs, ig, N, blockFixed);

        if (!ok) {
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

    normalizeColors(webs, ig);
    reduceColors(webs, ig, N);

    for (auto &w : webs)
        if (w.assignedRegister == -1) { w.assignedRegister = -2; allColored = false; }

    return allColored;
}
