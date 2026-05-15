#ifndef WEBBUILDER_H
#define WEBBUILDER_H

#include <vector>
#include "models/AllocationData.h"

/**
 * @brief Merges live ranges into webs using a greedy union approach.
 *
 * Two live ranges of the same variable are merged into one web if:
 *   1. They share at least one program line number, OR
 *   2. One ends at line N with '-' and the other starts at line N with '+' (same-line def-use fusion).
 *
 * Merging is repeated until no further merges are possible (fixed-point).
 */
class WebBuilder {
public:
    /**
     * @brief Builds webs from a list of live ranges.
     *
     * **Time Complexity:** O(R^2 * L) where R = number of ranges, L = average range size.
     * In practice R is small so this is fast.
     *
     * @param ranges Input live ranges (may contain multiple ranges per variable).
     * @param webs   Output vector of fully merged webs, each with a unique id.
     */
    static void build(const std::vector<LiveRange> &ranges, std::vector<Web> &webs);

private:
    /**
     * @brief Returns true if two live ranges should be merged.
     *
     * Merges when they share a line number, or when one ends at N- and the other starts at N+.
     */
    static bool shouldMerge(const LiveRange &a, const LiveRange &b);

    /**
     * @brief Merges live range b into live range a (in-place union).
     */
    static void mergeTo(LiveRange &a, const LiveRange &b);
};

#endif
