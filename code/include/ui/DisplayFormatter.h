#ifndef DISPLAYFORMATTER_H
#define DISPLAYFORMATTER_H

#include <string>
#include <vector>
#include "models/AllocationData.h"

/**
 * @brief Converts domain data into display-ready string lines for the Menu.
 *
 * Pure formatting layer — receives domain objects, returns vectors of strings.
 * Knows nothing about terminal rendering or ANSI codes.
 */
class DisplayFormatter {
public:
    /**
     * @brief Formats the list of webs showing variable name, lines, and assignment.
     *
     * @param webs Web vector to format.
     * @return One string per web, e.g. "web0 [sum]: 7+,8,9,10- → r0".
     *
     * @complexity O(W · L) where W = number of webs, L = average line count.
     */
    static std::vector<std::string> formatWebs(const std::vector<Web> &webs);

    /**
     * @brief Formats the full allocation result: webs, register assignments, and summary.
     *
     * Lines prefixed with "!!" signal an infeasible allocation and are rendered
     * in red by the Menu.
     *
     * @param result Allocation result to format.
     * @return Vector of display lines covering all webs and the register summary.
     *
     * @complexity O(W · L).
     */
    static std::vector<std::string> formatAllocationResult(const AllocationResult &result);

    /**
     * @brief Formats the register configuration (register count, algorithm, parameter).
     *
     * @param config RegisterConfig to format.
     * @return Vector of display lines, one per config field.
     *
     * @complexity O(1).
     */
    static std::vector<std::string> formatConfig(const RegisterConfig &config);

    /**
     * @brief Formats the raw live ranges as loaded from the ranges file.
     *
     * @param ranges Live range vector to format.
     * @return One string per range showing variable name and program points.
     *
     * @complexity O(R · L) where R = number of ranges, L = average line count.
     */
    static std::vector<std::string> formatRanges(const std::vector<LiveRange> &ranges);

    /**
     * @brief Wraps a long string into multiple lines fitting within @p maxWidth characters.
     *
     * If @p prefix starts with "!!" all continuation lines also get "!!" so the
     * Menu renders them in red.
     *
     * @param prefix   Prefix for the first line (e.g. "- " or "!!").
     * @param text     Text to wrap.
     * @param maxWidth Maximum line width in characters including the prefix.
     * @return Vector of wrapped lines, each no longer than @p maxWidth.
     *
     * @complexity O(N) where N = length of @p text.
     */
    static std::vector<std::string> wrapLine(const std::string &prefix,
                                              const std::string &text,
                                              int maxWidth);
};

#endif
