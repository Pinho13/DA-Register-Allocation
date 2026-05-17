#ifndef REGISTERALLOCATOR_H
#define REGISTERALLOCATOR_H

#include <vector>
#include <stack>
#include "models/AllocationData.h"
#include "core/InterferenceGraph.h"

/**
 * Implements all register allocation algorithms:
 *   basic, spilling, splitting, free (BCT-Color).
 * Each algorithm is implemented in its own Coloring_*.cpp file.
 */
class RegisterAllocator {
public:
    // Runs the algorithm specified in config. See Coloring_*.cpp for implementations.
    static AllocationResult allocate(std::vector<Web> &webs,
                                     InterferenceGraph &ig,
                                     const RegisterConfig &config);

private:
    // ── Basic / spilling / splitting (Coloring_Basic.cpp) ─────────────────────

    // Chaitin-style simplification + stack coloring. O(W^2).
    // spilledIds receives nodes forced out during simplification.
    static bool basicColoring(std::vector<Web> &webs, InterferenceGraph &ig,
                               int N, std::vector<int> &spilledIds);

    // Heuristic: min liveRangeLength / (degree+1), tie-break highest degree.
    static int selectSpillCandidate(const InterferenceGraph &ig,
                                    const std::vector<Web> &webs);

    // Splits web at webIdx into two halves; appends the second half to webs.
    static void splitWeb(std::vector<Web> &webs, int webIdx);

    // Heuristic: max degree × liveRangeLength; excludes webs with < 2 lines.
    static int selectSplitCandidate(const InterferenceGraph &ig,
                                    const std::vector<Web> &webs);

    // ── DSATUR + exact backtracking (Coloring_Dsatur.cpp) ─────────────────────

    // Saturation-degree ordering; O(W^2).
    static bool dsaturColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N);

    // ── BCT-Color (Coloring_BCT.cpp) ──────────────────────────────────────────

    // BCC decomposition + block-cut tree DP + per-block coloring + normalization.
    static bool partitionedColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N);

    // Tarjan DFS — populates blocks and artPoints.
    static void findBiconnectedComponents(const InterferenceGraph &ig,
                                          std::vector<std::vector<int>> &blocks,
                                          std::set<int> &artPoints);

    static void bcDFS(int u, int parent,
                      int &timer,
                      std::vector<int> &disc,
                      std::vector<int> &low,
                      std::vector<bool> &visited,
                      std::stack<std::pair<int,int>> &edgeStack,
                      const InterferenceGraph &ig,
                      std::vector<std::vector<int>> &blocks,
                      std::set<int> &artPoints);

    // Colors one BCC block respecting pre-fixed articulation-point colors.
    static bool colorBlock(const std::vector<int> &blockNodes,
                           std::vector<Web> &webs,
                           const InterferenceGraph &ig,
                           int N,
                           const std::map<int,int> &fixedColors);

    // ── Color post-processing (Coloring_Utils.cpp) ────────────────────────────

    // Flips colors A↔B in the Kempe chain rooted at a neighbor of u.
    static bool kempeSwap(int u, int targetColor,
                          std::vector<Web> &webs,
                          const InterferenceGraph &ig,
                          const std::set<int> &coloredSet);

    // Greedy remap of color indices to 0..K-1 using the global conflict graph.
    static void normalizeColors(std::vector<Web> &webs, const InterferenceGraph &ig);

    // Iteratively eliminates the highest color via Kempe swaps. O(K * W).
    static void reduceColors(std::vector<Web> &webs, const InterferenceGraph &ig, int N);

    // Checks: no conflicts, no out-of-range colors, no uninitialized (-1) webs.
    static bool validateColoring(const std::vector<Web> &webs,
                                 const InterferenceGraph &ig,
                                 int N);
};

#endif
