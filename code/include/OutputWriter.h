#ifndef OUTPUTWRITER_H
#define OUTPUTWRITER_H

#include <string>
#include "models/AllocationData.h"

/**
 * @brief Writes the allocation result to a text file in the required format.
 *
 * Output format (success):
 * @code
 *   # Total number of webs ...
 *   webs: N
 *   web0: 1+,2,3-
 *   ...
 *   registers: K
 *   r0: web0
 *   r0: web2
 *   r1: web1
 * @endcode
 *
 * Output format (failure — all to memory):
 * @code
 *   webs: N
 *   web0: ...
 *   registers: 0
 *   M: web0
 *   M: web1
 * @endcode
 */
class OutputWriter {
public:
    /**
     * @brief Writes the allocation result to the given file.
     *
     * Also prints a console warning if allocation was not feasible.
     *
     * **Time Complexity:** O(W * L) where W = number of webs, L = average web size.
     *
     * @param filename Path to the output text file.
     * @param result   The allocation result to write.
     */
    static void write(const std::string &filename, const AllocationResult &result);

private:
    /**
     * @brief Formats a single web's program points as a comma-separated string.
     *
     * Line numbers are output with their '+' or '-' markers where applicable,
     * sorted in ascending order.
     *
     * @param web The web to format.
     * @return Formatted string e.g. "1+,2,3,4,5,6-"
     */
    static std::string formatWebPoints(const Web &web);
};

#endif
