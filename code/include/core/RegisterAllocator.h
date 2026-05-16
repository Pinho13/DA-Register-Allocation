#ifndef REGISTERALLOCATOR_H
#define REGISTERALLOCATOR_H

#include <vector>
#include <stack>
#include "models/AllocationData.h"
#include "core/InterferenceGraph.h"

/**
 * @brief Implements all four register allocation algorithms.
 *
 * Algorithms:
 *   - basic:    Greedy graph coloring as described in the project spec.
 *   - spilling: Greedy coloring with up to K webs spilled to memory.
 *   - splitting: Greedy coloring with up to K webs split into two sub-webs.
 *   - free (BCT-Color): biconnected-component decomposition with adaptive per-block coloring.
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

    /**
     * @brief Biconnected-partition coloring: decomposes the interference graph into
     *        biconnected components via Tarjan's algorithm, dispatches the locally
     *        optimal coloring algorithm to each component, then reconciles color
     *        assignments at articulation points.
     *
     * Each biconnected component is classified by its density and max degree:
     *   - Dense (density > 0.6 or max_degree >= N-1): solved with DSATUR.
     *   - Sparse: solved with basic greedy coloring.
     * Articulation points shared between components are pre-colored as fixed
     * constraints when solving adjacent components. Irreconcilable conflicts
     * at articulation points are resolved by spilling that web.
     * A final color-normalization pass minimizes total registers used.
     *
     * **Time Complexity:** O(W^2) — dominated by per-block coloring.
     *
     * @param webs  Webs (assignedRegister filled in on success).
     * @param ig    Interference graph.
     * @param N     Number of available registers.
     * @return true if all webs were colored (no spills).
     */
    static bool partitionedColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N);

    // ── Biconnected component helpers ──────────────────────────────────────────

    struct Block {
        std::vector<int> nodes;   // web ids in this biconnected component
        bool isArticulationPoint; // true if this "block" is a single AP placeholder
    };

    /**
     * @brief Finds all biconnected components and articulation points via Tarjan DFS.
     *
     * @param ig            Interference graph.
     * @param blocks        Output: list of biconnected components (sets of web ids).
     * @param artPoints     Output: set of articulation point web ids.
     */
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

    /**
     * @brief Colors one biconnected block, respecting pre-colored articulation points.
     *
     * @param blockNodes    Web ids in this block.
     * @param webs          All webs (colors written here).
     * @param ig            Full interference graph.
     * @param N             Number of registers.
     * @param fixedColors   Pre-assigned colors for articulation points (must be respected).
     * @return true if all nodes in the block were colored.
     */
    static bool colorBlock(const std::vector<int> &blockNodes,
                           std::vector<Web> &webs,
                           const InterferenceGraph &ig,
                           int N,
                           const std::map<int,int> &fixedColors);

    /**
     * @brief Attempts a Kempe chain swap to free up targetColor at node u.
     *
     * A Kempe chain between colors A and B rooted at u is the maximal connected
     * subgraph of already-colored nodes whose colors are in {A, B}. Swapping A↔B
     * within that chain is always valid (no new conflicts). We use this to try to
     * recolor a neighbor of an articulation point so the AP can take its required color.
     *
     * @param u             Node that needs targetColor freed among its neighbors.
     * @param targetColor   The color we need to free at u's neighbors.
     * @param webs          All webs (colors modified in-place on success).
     * @param ig            Interference graph.
     * @param coloredSet    Set of currently-colored web ids.
     * @return true if the swap succeeded and targetColor is now free at u.
     */
    static bool kempeSwap(int u, int targetColor,
                          std::vector<Web> &webs,
                          const InterferenceGraph &ig,
                          const std::set<int> &coloredSet);

    /**
     * @brief Remaps colors globally to minimize total registers used.
     *
     * Different blocks may have used disjoint color indices for webs that don't
     * actually interfere globally. This pass builds the true global conflict graph
     * over color indices and greedily remaps to 0..K-1.
     *
     * @param webs  All webs (colors remapped in-place).
     * @param ig    Interference graph (used to find actual conflicts).
     */
    static void normalizeColors(std::vector<Web> &webs, const InterferenceGraph &ig);

    /**
     * @brief Attempts to reduce the number of colors used via iterated Kempe-chain recoloring.
     *
     * After normalization colors are 0..K-1. This pass repeatedly tries to eliminate
     * the highest color (K-1) by Kempe-swapping each node that uses it into a lower
     * color. If all such nodes can be recolored, K shrinks by 1. Repeats until no
     * further reduction is possible.
     *
     * @param webs  All webs (colors modified in-place).
     * @param ig    Interference graph.
     * @param N     Number of available registers (hard upper bound — never exceeded).
     */
    static void reduceColors(std::vector<Web> &webs, const InterferenceGraph &ig, int N);

    /**
     * @brief Validates that the coloring produced by partitionedColoring is conflict-free.
     *
     * Checks three invariants:
     *   1. No two interfering webs share the same register (color conflict).
     *   2. No colored web was assigned a register index >= N.
     *   3. No web has an uninitialized assignment (-1 sentinel).
     *
     * Spilled webs (-2) are skipped — they are valid by definition.
     * Emits a diagnostic to std::cerr for each violation found.
     *
     * @param webs  All webs after coloring.
     * @param ig    Interference graph.
     * @param N     Number of available registers.
     * @return true if the coloring is valid, false if any invariant is violated.
     */
    static bool validateColoring(const std::vector<Web> &webs,
                                 const InterferenceGraph &ig,
                                 int N);
};

#endif
