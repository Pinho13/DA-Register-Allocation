#ifndef RANGESPARSER_H
#define RANGESPARSER_H

#include <string>
#include <vector>
#include "../models/AllocationData.h"

/**
 * @brief Parses the live-ranges input file into a list of LiveRange objects.
 *
 * File format (lines starting with '#' are ignored):
 * @code
 *   varName: line+, line, line-
 *   varName: line+, line-
 * @endcode
 *
 * A '+' suffix on a line number marks a definition point.
 * A '-' suffix marks a last-use point.
 */
class RangesParser {
public:
    /**
     * @brief Parses the given file and fills the output vector.
     *
     * **Time Complexity:** O(L) where L = total number of line-number tokens across all ranges.
     *
     * @param filename Path to the ranges text file.
     * @param ranges   Output vector of LiveRange objects.
     * @return true on success, false if the file cannot be opened or is malformed.
     */
    static bool parse(const std::string &filename, std::vector<LiveRange> &ranges);

private:
    /**
     * @brief Parses a single token such as "7+", "8", "10-" into its numeric value and marker.
     *
     * @param token     The raw token string.
     * @param line      Output: the integer line number.
     * @param isDef     Output: true if the token ends with '+'.
     * @param isUse     Output: true if the token ends with '-'.
     * @return true if the token was valid.
     */
    static bool parseToken(const std::string &token, int &line, bool &isDef, bool &isUse);
};

#endif
