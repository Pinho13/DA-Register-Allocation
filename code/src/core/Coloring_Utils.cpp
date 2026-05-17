#include <set>
#include <stack>
#include <iostream>
#include "core/RegisterAllocator.h"

bool RegisterAllocator::kempeSwap(int u, int targetColor,
                                   std::vector<Web> &webs,
                                   const InterferenceGraph &ig,
                                   const std::set<int> &coloredSet) {
    for (int nb : ig.neighbours(u)) {
        if (!coloredSet.count(nb)) continue;
        if (webs[nb].assignedRegister != targetColor) continue;

        std::set<int> nbUsed;
        for (int nb2 : ig.neighbours(nb))
            if (coloredSet.count(nb2) && nb2 != u && webs[nb2].assignedRegister >= 0)
                nbUsed.insert(webs[nb2].assignedRegister);

        for (int alt : nbUsed) {
            if (alt == targetColor) continue;

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

            for (int node : chain)
                webs[node].assignedRegister = (webs[node].assignedRegister == targetColor) ? alt : targetColor;

            if (webs[nb].assignedRegister != targetColor)
                return true;

            for (int node : chain)
                webs[node].assignedRegister = (webs[node].assignedRegister == targetColor) ? alt : targetColor;
        }
    }
    return false;
}

void RegisterAllocator::normalizeColors(std::vector<Web> &webs,
                                         const InterferenceGraph &ig) {
    int maxColor = -1;
    for (const auto &w : webs)
        if (w.assignedRegister >= 0) maxColor = std::max(maxColor, w.assignedRegister);
    if (maxColor < 0) return;

    int C = maxColor + 1;
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

        std::vector<int> targets;
        for (int u = 0; u < total; u++)
            if (webs[u].assignedRegister == top) targets.push_back(u);

        std::vector<int> saved(total);
        for (int u = 0; u < total; u++) saved[u] = webs[u].assignedRegister;

        bool allMoved = true;
        for (int u : targets) {
            if (webs[u].assignedRegister != top) continue;

            bool moved = false;
            for (int c = 0; c < top && !moved; c++) {
                bool uCanTakeC = true;
                for (int nb : ig.neighbours(u)) {
                    if (webs[nb].assignedRegister == c) { uCanTakeC = false; break; }
                }
                if (!uCanTakeC) continue;

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

                for (int node : chain)
                    webs[node].assignedRegister =
                        (webs[node].assignedRegister == top) ? c : top;

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
                    for (int node : chain)
                        webs[node].assignedRegister =
                            (webs[node].assignedRegister == top) ? c : top;
                }
            }

            if (!moved) { allMoved = false; break; }
        }

        if (allMoved && !targets.empty()) {
            bool gone = true;
            for (const auto &w : webs)
                if (w.assignedRegister == top) { gone = false; break; }
            if (gone) { progress = true; continue; }
        }

        for (int u = 0; u < total; u++) webs[u].assignedRegister = saved[u];
    }
}

bool RegisterAllocator::validateColoring(const std::vector<Web> &webs,
                                          const InterferenceGraph &ig,
                                          int N) {
    bool valid = true;
    int total = (int)webs.size();

    for (int u = 0; u < total; u++) {
        int cu = webs[u].assignedRegister;

        if (cu == -1) {
            std::cerr << "[validateColoring] web " << u << " has uninitialized assignment (-1)\n";
            valid = false;
            continue;
        }
        if (cu == -2) continue;

        if (cu >= N) {
            std::cerr << "[validateColoring] web " << u
                      << " assigned register " << cu << " >= N=" << N << "\n";
            valid = false;
        }

        for (int v : ig.neighbours(u)) {
            int cv = webs[v].assignedRegister;
            if (cv >= 0 && cu == cv) {
                std::cerr << "[validateColoring] conflict: web " << u
                          << " and web " << v << " both assigned register " << cu << "\n";
                valid = false;
            }
        }
    }
    return valid;
}
