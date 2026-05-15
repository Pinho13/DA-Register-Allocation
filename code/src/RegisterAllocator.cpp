/**
 * @file RegisterAllocator.cpp
 * @brief Implementation of the four register allocation algorithms.
 *
 * Algorithms implemented:
 *  - **basic**:    Chaitin-style greedy simplification + stack-based coloring.
 *  - **spilling**: Basic coloring extended with up to K webs committed to memory.
 *  - **splitting**: Iterative web splitting to reduce interference, then basic coloring.
 *  - **free**:     DSATUR heuristic — always color the highest-saturation web first.
 */

#include <algorithm>
#include <stack>
#include <set>
#include <iostream>
#include "RegisterAllocator.h"

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
        // "free" — DSATUR
        // DSATUR assigns a color to every web. If N colors are insufficient,
        // some webs remain uncolored (assignedRegister == -1). Those are spilled.
        // Partial coloring is valid: colored webs get registers, uncolored get M.
        dsaturColoring(webs, ig, N);
        bool anyUncolored = false;
        for (auto &w : webs) {
            if (w.assignedRegister < 0) { w.assignedRegister = -2; anyUncolored = true; }
        }
        result.webs = webs;
        result.registersUsed = countRegisters(webs);
        // feasible = true only if every web got a register (no spills)
        result.feasible = !anyUncolored;
        if (!result.feasible && result.registersUsed == 0) {
            // Complete failure: nothing colored — treat same as basic infeasible
            markAllMemory(result.webs);
        }
    }

    return result;
}
