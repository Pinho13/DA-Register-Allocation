#ifndef INTERFERENCEGRAPH_H
#define INTERFERENCEGRAPH_H

#include <vector>
#include <set>
#include <map>
#include "models/AllocationData.h"
#include "data_structures/Graph.h"

/**
 * @brief Builds and owns the interference graph for a set of webs.
 *
 * Each node in the graph corresponds to one web (identified by its id).
 * An undirected edge connects two nodes if their webs interfere.
 *
 * Two webs interfere if they are simultaneously alive at any program point,
 * with one exception: if web A ends at line N with '-' (last-use) and web B
 * starts at line N with '+' (definition), they do NOT interfere at that line.
 */
class InterferenceGraph {
public:
    /**
     * @brief Constructs the interference graph from the given webs.
     *
     * **Time Complexity:** O(W^2 * L) where W = number of webs, L = average web size.
     *
     * @param webs The list of webs to build the graph from.
     */
    explicit InterferenceGraph(const std::vector<Web> &webs);

    /**
     * @brief Returns the effective degree of node webId, counting only non-removed nodes.
     *
     * **Time Complexity:** O(W) where W = number of webs.
     *
     * @param webId Web index.
     */
    int effectiveDegree(int webId) const;

    /**
     * @brief Returns the neighbours of webId (all, including removed ones).
     */
    std::vector<int> neighbours(int webId) const;

    /**
     * @brief Returns true if the two webs are connected by an edge.
     */
    bool interferes(int a, int b) const;

    /** @brief Number of webs (nodes) in the graph. */
    int size() const;

    /**
     * @brief Marks a node as removed (soft-delete for the coloring algorithm).
     */
    void setRemoved(int webId, bool removed);

    /** @brief Returns true if the node has been soft-deleted. */
    bool isRemoved(int webId) const;

    /** @brief Resets all removed flags (for re-runs with different N). */
    void resetRemoved();

    /**
     * @brief Returns the underlying Graph object for display/inspection.
     */
    const Graph<int> &getGraph() const;

private:
    Graph<int> graph_;            /**< Underlying TP-provided graph (nodes are web ids) */
    int numWebs_;                 /**< Total number of webs */
    std::vector<bool> removed_;   /**< Soft-delete flags for coloring simplification */

    /** @brief Returns true if webs a and b interfere (using the interference rule). */
    static bool computeInterference(const Web &a, const Web &b);
};

#endif
