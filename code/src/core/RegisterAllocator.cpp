/**
 * @file RegisterAllocator.cpp
 * @brief Dispatch implementation for RegisterAllocator::allocate.
 *
 * Routes to the correct coloring strategy based on RegisterConfig::algorithm.
 * Algorithm implementations live in:
 *   Coloring_Basic.cpp  — basic greedy, spill/split candidate selection
 *   Coloring_Dsatur.cpp — DSATUR heuristic + exact backtracking
 *   Coloring_BCT.cpp    — Phantom (BCT-Color): biconnected-component decomposition
 *   Coloring_Utils.cpp  — Kempe swap, colour normalisation, reduction, validator
 */

#include <set>
#include <iostream>
#include "core/RegisterAllocator.h"

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

    } else if (config.algorithm == "phantom") {
        partitionedColoring(webs, ig, N);

        if (!validateColoring(webs, ig, N))
            std::cerr << "[Phantom] coloring invariant violated after partitionedColoring\n";

        // DSATUR floor: keep whichever produces fewer spills
        {
            std::vector<Web> dsatW = webs;
            for (auto &w : dsatW) w.assignedRegister = -1;
            dsaturColoring(dsatW, ig, N);
            for (auto &w : dsatW) if (w.assignedRegister == -1) w.assignedRegister = -2;

            int bctSpills = 0, dsatSpills = 0;
            for (const auto &w : webs)  if (w.assignedRegister == -2) bctSpills++;
            for (const auto &w : dsatW) if (w.assignedRegister == -2) dsatSpills++;
            if (dsatSpills < bctSpills) webs = dsatW;
        }

        int colored = 0;
        for (auto &w : webs)
            if (w.assignedRegister >= 0) colored++;
        result.webs = webs;
        result.registersUsed = countRegisters(webs);
        result.feasible = (colored > 0 || webs.empty());
        if (!result.feasible) markAllMemory(result.webs);
    }

    return result;
}
