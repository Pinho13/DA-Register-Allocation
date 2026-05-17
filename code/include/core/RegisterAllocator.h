#ifndef REGISTERALLOCATOR_H
#define REGISTERALLOCATOR_H

#include <vector>
#include <stack>
#include "models/AllocationData.h"
#include "core/InterferenceGraph.h"

/**
 * @brief Dispatcher for all register allocation algorithms.
 *
 * Selects and runs the algorithm specified in RegisterConfig::algorithm.
 * Implementations are split across:
 *   - Coloring_Basic.cpp  — basic greedy, spill/split candidate selection
 *   - Coloring_Dsatur.cpp — DSATUR heuristic + exact backtracking
 *   - Coloring_BCT.cpp    — Phantom (BCT-Color): biconnected-component decomposition
 *   - Coloring_Utils.cpp  — Kempe swap, colour normalisation, reduction, validator
 */
class RegisterAllocator {
public:
    /**
     * @brief Runs the register allocation algorithm specified in @p config.
     *
     * Dispatches to basicColoring, dsaturColoring, or partitionedColoring
     * depending on config.algorithm ("basic", "spilling", "splitting",
     * "dsatur", "phantom"). Applies a DSATUR floor for phantom: falls back
     * to DSATUR when it produces fewer spills.
     *
     * @param webs   Webs to colour; assignedRegister is set on each web.
     * @param ig     Interference graph over @p webs.
     * @param config Register count, algorithm name, and optional parameter K.
     * @return AllocationResult with feasibility flag, register count, and coloured webs.
     *
     * @complexity O(W²) for basic/spilling/splitting/dsatur; O(V+E + N^b) for phantom
     *             where b is the largest biconnected block size.
     */
    static AllocationResult allocate(std::vector<Web> &webs,
                                     InterferenceGraph &ig,
                                     const RegisterConfig &config);

private:
    // ── Basic / spilling / splitting (Coloring_Basic.cpp) ─────────────────────

    /**
     * @brief Chaitin-style simplify + select coloring.
     *
     * Phase 1: repeatedly removes nodes of degree < N onto a stack.
     * Phase 2: pops and assigns the first colour not used by any neighbour.
     *
     * @param webs      Webs to colour.
     * @param ig        Interference graph.
     * @param N         Number of available registers.
     * @param spilledIds Output: indices of webs that could not be simplified.
     * @return true if all webs were coloured without spilling.
     *
     * @complexity O(W²) — up to W simplification passes, each O(W).
     */
    static bool basicColoring(std::vector<Web> &webs, InterferenceGraph &ig,
                               int N, std::vector<int> &spilledIds);

    /**
     * @brief Selects the best spill candidate from the interference graph.
     *
     * Heuristic: minimise liveRangeLength / (degree + 1); tie-break by
     * highest degree to free the most edges per spill.
     *
     * @param ig    Current interference graph.
     * @param webs  Web vector for range-length lookup.
     * @return Index of the chosen web, or -1 if none available.
     *
     * @complexity O(W) — single pass over all webs.
     */
    static int selectSpillCandidate(const InterferenceGraph &ig,
                                    const std::vector<Web> &webs);

    /**
     * @brief Splits the web at @p webIdx into two halves.
     *
     * The first half retains lines up to the midpoint; the second half
     * (lines from midpoint onward) is appended to @p webs as a new web.
     * Both halves re-enter the interference graph on the next attempt.
     *
     * @param webs    Web vector; modified in place, new web appended.
     * @param webIdx  Index of the web to split.
     *
     * @complexity O(L) where L is the number of lines in the web.
     */
    static void splitWeb(std::vector<Web> &webs, int webIdx);

    /**
     * @brief Selects the best split candidate from the interference graph.
     *
     * Heuristic: maximise degree × liveRangeLength. Excludes webs with
     * fewer than 2 lines (cannot be split further).
     *
     * @param ig    Current interference graph.
     * @param webs  Web vector for range-length lookup.
     * @return Index of the chosen web, or -1 if none available.
     *
     * @complexity O(W) — single pass over all webs.
     */
    static int selectSplitCandidate(const InterferenceGraph &ig,
                                    const std::vector<Web> &webs);

    // ── DSATUR + exact backtracking (Coloring_Dsatur.cpp) ─────────────────────

    /**
     * @brief DSATUR graph colouring heuristic.
     *
     * At each step picks the uncoloured web with the highest saturation
     * degree (number of distinct colours among its neighbours), breaking
     * ties by highest degree. Falls back to exact backtracking for small
     * blocks where DSATUR may not find the chromatic number.
     *
     * @param webs  Webs to colour; assignedRegister set on success.
     * @param ig    Interference graph.
     * @param N     Number of available registers.
     * @return true if all webs were coloured within N colours.
     *
     * @complexity O(W²) — argmax re-scan after each assignment.
     */
    static bool dsaturColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N);

    // ── Phantom / BCT-Color (Coloring_BCT.cpp) ────────────────────────────────

    /**
     * @brief Phantom algorithm: BCC decomposition + per-block exact colouring.
     *
     * Decomposes the interference graph into biconnected components (BCCs)
     * via Tarjan's DFS, builds a block-cut tree, and colours each block
     * independently with exact backtracking (cutoff ≤ 32 nodes) or DSATUR.
     * Articulation-point colours are fixed across adjacent blocks.
     * Post-processing: Kempe-chain reduction and colour normalisation.
     *
     * @param webs  Webs to colour.
     * @param ig    Interference graph.
     * @param N     Number of available registers.
     * @return true if a valid colouring within N colours was found.
     *
     * @complexity O(V + E) for decomposition; O(N^b) per block where b is
     *             block size (small in practice due to BCC sparsity).
     */
    static bool partitionedColoring(std::vector<Web> &webs, InterferenceGraph &ig, int N);

    /**
     * @brief Finds all biconnected components via Tarjan's DFS.
     *
     * Populates @p blocks with node sets for each BCC and @p artPoints
     * with the indices of all articulation points.
     *
     * @param ig         Interference graph.
     * @param blocks     Output: one vector of node indices per BCC.
     * @param artPoints  Output: set of articulation-point indices.
     *
     * @complexity O(V + E) — single DFS traversal.
     */
    static void findBiconnectedComponents(const InterferenceGraph &ig,
                                          std::vector<std::vector<int>> &blocks,
                                          std::set<int> &artPoints);

    /**
     * @brief Recursive DFS kernel for biconnected-component extraction.
     *
     * Tracks discovery times and low-link values. Pops edge stack segments
     * into blocks at each articulation point. Root cut-vertex check is
     * performed after the neighbour loop completes.
     *
     * @param u,parent    Current node and its DFS parent (-1 for root).
     * @param timer       Shared discovery-time counter.
     * @param disc,low    Discovery and low-link arrays.
     * @param visited     Visited flag array.
     * @param edgeStack   Stack of traversed edges for block extraction.
     * @param ig          Interference graph.
     * @param blocks      Accumulated BCC node sets.
     * @param artPoints   Accumulated articulation points.
     */
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
     * @brief Colours one BCC block respecting pre-fixed articulation-point colours.
     *
     * Tries exact backtracking for blocks of ≤ 32 free nodes; falls back to
     * DSATUR otherwise. Nodes in @p fixedColors are treated as already coloured
     * and are not reassigned.
     *
     * @param blockNodes  Nodes belonging to this BCC.
     * @param webs        Web vector; assignedRegister updated in place.
     * @param ig          Interference graph.
     * @param N           Number of available registers.
     * @param fixedColors Map of node index → pre-assigned colour for cut vertices.
     * @return true if a valid N-colouring of the block was found.
     *
     * @complexity O(N^b) exact where b = |blockNodes \ fixedColors|; O(b²) DSATUR fallback.
     */
    static bool colorBlock(const std::vector<int> &blockNodes,
                           std::vector<Web> &webs,
                           const InterferenceGraph &ig,
                           int N,
                           const std::map<int,int> &fixedColors);

    // ── Colour post-processing (Coloring_Utils.cpp) ───────────────────────────

    /**
     * @brief Flips colours A↔B along the Kempe chain containing a neighbour of @p u.
     *
     * Performs a BFS restricted to nodes coloured A or B reachable from
     * u's neighbour. If the chain does not conflict with u's other neighbours,
     * commits the flip — freeing colour A or B for u to use.
     *
     * @param u           Web index to recolour.
     * @param targetColor Desired new colour for u (B in the A↔B swap).
     * @param webs        Web vector; colours updated in place on success.
     * @param ig          Interference graph.
     * @param coloredSet  Set of currently coloured web indices.
     * @return true if the swap was committed successfully.
     *
     * @complexity O(V + E) — one BFS over the chain.
     */
    static bool kempeSwap(int u, int targetColor,
                          std::vector<Web> &webs,
                          const InterferenceGraph &ig,
                          const std::set<int> &coloredSet);

    /**
     * @brief Remaps colour indices to a compact 0..K-1 range.
     *
     * Greedily reassigns colours so that the set of used colour indices
     * is contiguous, minimising the reported register count without
     * altering the validity of the colouring.
     *
     * @param webs  Web vector; assignedRegister updated in place.
     * @param ig    Interference graph (used to verify no conflicts after remap).
     *
     * @complexity O(W · K) where K is the number of distinct colours used.
     */
    static void normalizeColors(std::vector<Web> &webs, const InterferenceGraph &ig);

    /**
     * @brief Iteratively eliminates the highest colour via Kempe swaps.
     *
     * Repeatedly attempts to recolour every web holding the maximum colour
     * index using kempeSwap until the colour count drops to N or no further
     * swap is possible (infeasible).
     *
     * @param webs  Web vector; colours updated in place.
     * @param ig    Interference graph.
     * @param N     Target maximum colour index (0-based, so N-1 is the max allowed).
     *
     * @complexity O(K · (V + E)) — at most K swap rounds, each O(V + E).
     */
    static void reduceColors(std::vector<Web> &webs, const InterferenceGraph &ig, int N);

    /**
     * @brief Validates that the current colouring is conflict-free and complete.
     *
     * Checks three invariants:
     *   1. No two adjacent webs share the same colour.
     *   2. No web holds a colour index ≥ N (out of range).
     *   3. No web remains unassigned (assignedRegister == -1).
     *
     * @param webs  Web vector to validate.
     * @param ig    Interference graph.
     * @param N     Number of available registers.
     * @return true if all three invariants hold.
     *
     * @complexity O(W + E) — one pass over webs and edges.
     */
    static bool validateColoring(const std::vector<Web> &webs,
                                 const InterferenceGraph &ig,
                                 int N);
};

#endif
