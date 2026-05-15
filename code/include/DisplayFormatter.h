#ifndef DISPLAYFORMATTER_H
#define DISPLAYFORMATTER_H

#include <string>
#include <vector>
#include "models/AllocationData.h"

/**
 * @brief Converts domain data into display-ready string lines for the Menu.
 *
 * Pure formatting layer — receives domain objects, returns vectors of strings.
 * Knows nothing about terminal rendering.
 */
class DisplayFormatter {
public:
    /** @brief Formats the list of webs for display. */
    static std::vector<std::string> formatWebs(const std::vector<Web> &webs);

    /** @brief Formats the allocation result (webs + register assignments). */
    static std::vector<std::string> formatAllocationResult(const AllocationResult &result);

    /** @brief Formats the register configuration settings. */
    static std::vector<std::string> formatConfig(const RegisterConfig &config);

    /** @brief Formats the raw live ranges loaded from file. */
    static std::vector<std::string> formatRanges(const std::vector<LiveRange> &ranges);

    /**
     * @brief Wraps a long string into multiple lines fitting within maxWidth.
     *
     * If the prefix starts with "!!" all continuation lines also get "!!"
     * so the Menu renders them in red.
     *
     * @param prefix   Prefix for the first line (e.g. "- " or "!!")
     * @param text     Text to wrap.
     * @param maxWidth Maximum line width including prefix.
     */
    static std::vector<std::string> wrapLine(const std::string &prefix,
                                              const std::string &text,
                                              int maxWidth);
};

#endif
