#ifndef REGISTERALLOCATOR_H
#define REGISTERALLOCATOR_H

#include <vector>
#include <stack>
#include "models/AllocationData.h"
#include "InterferenceGraph.h"

/**
 * @brief Implements all four register allocation algorithms.
 *
 * Algorithms:
 *   - basic:    Greedy graph coloring as described in the project spec.
 *   - spilling: Greedy coloring with up to K webs spilled to memory.
 *   - splitting: Greedy coloring with up to K webs split into two sub-webs.
 *   - free:     DSATUR heuristic (color node with most constraints first).
 */
class RegisterAllocator {
public:
    /**
     * @brief Runs the algorithm specified in config on the given webs.
     *
     * @param webs   The webs to allocate (modified in-place with assignedRegister).
     * @param ig     The interference graph.
     * @param config The register configuration (numRegisters, algorithm, param).
     * @return AllocationResult with assignments and feasibility flag.
     */
    static AllocationResult allocate(std::vector<Web> &webs,
                                     InterferenceGraph &ig,
                                     const RegisterConfig &config);

private:
    /**
     * @brief Basic greedy graph coloring with N colors.
     *
     * Implements the Chaitin-style simplification + coloring described in Fig.9.
     * Does NOT spill — if a node cannot be colored, it is left unassigned.
     *
     * **Time Complexity:** O(W^2) where W = number of webs.
     *
     * @param webs      Webs (assignedRegister filled in on success).
     * @param ig        Interference graph (reset before use).
     * @param N         Number of colors (registers).
     * @param spilledIds Indices of nodes that were forced to spill during simplification.
     * @return true if no nodes were spilled (fully feasible coloring).
     */
    static bool basicColoring(std::vector<Web> &webs, InterferenceGraph &ig,
                               int N, std::vector<int> &spilledIds);

    /**
     * @brief Selects the best node to spill during the simplification phase.
     *
     * Heuristic: minimise spill cost = liveRangeLength / (degree + 1).
     * A web that is short and heavily connected is the cheapest to spill:
     * few program points are affected and register pressure is maximally relieved.
     * Ties are broken by highest effective degree.
     *
     * **Time Complexity:** O(W).
     *
     * @param ig   Interference graph.
     * @param webs All webs.
     * @return Index of the web to spill.
     */
    static int selectSpillCandidate(const InterferenceGraph &ig,
                                    const std::vector<Web> &webs);

    /**
     * @brief Splits web at index webIdx into two sub-webs at the midpoint of its line list.
     *
     * The original web is replaced by web_a (first half) and web_b (second half).
     * Both sub-webs get new unique IDs appended to the webs vector.
     *
     * **Time Complexity:** O(L) where L = size of the web's line list.
     *
     * @param webs   Web list (modified in-place).
     * @param webIdx Index of the web to split.
     */
    static void splitWeb(std::vector<Web> &webs, int webIdx);

    /**
     * @brief Selects the best web to split.
     *
     * Heuristic: maximise degree × liveRangeLength.
     * A long, high-degree web is the most likely to have its neighbours
     * concentrated in one half after splitting, reducing overall interference.
     * Webs with fewer than 2 lines are excluded (cannot be split).
     *
     * **Time Complexity:** O(W).
     *
     * @param ig   Interference graph.
     * @param webs All webs.
     * @return Index of the web to split, or -1 if no splittable web exists.
     */
    static int selectSplitCandidate(const InterferenceGraph &ig,
                                    const std::vector<Web> &webs);

    /**
     * @brief DSATUR heuristic: always color the web with the highest saturation degree first.
     *
     * Saturation degree = number of distinct colors used by neighbours.
     * Ties broken by highest effective degree.
     *
     * **Time Complexity:** O(W^2).
     *
     * @param webs  Webs (assignedRegister filled in on success).
     * @param ig    Interference graph.
     * @param N     Number of colors.
     * @return true if all webs were colored successfully.
     */
    static bool dsaturColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N);
};

#endif
