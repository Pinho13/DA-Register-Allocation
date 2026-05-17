#include <set>
#include <map>
#include "core/RegisterAllocator.h"

bool RegisterAllocator::dsaturColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N) {
    int total = ig.size();
    for (auto &w : webs) w.assignedRegister = -1;

    std::vector<std::set<int>> saturation(total);
    std::vector<bool> colored(total, false);

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

// Used by colorBlock for exact small-block coloring.
bool backtrack(const std::vector<int> &order, int idx,
               std::vector<Web> &webs,
               const InterferenceGraph &ig,
               int N,
               const std::map<int,int> &fixedColors) {
    if (idx == (int)order.size()) return true;
    int u = order[idx];

    auto fit = fixedColors.find(u);
    if (fit != fixedColors.end()) {
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
