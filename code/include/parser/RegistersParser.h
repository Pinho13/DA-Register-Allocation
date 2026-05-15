#ifndef REGISTERSPARSER_H
#define REGISTERSPARSER_H

#include <string>
#include "../models/AllocationData.h"

/**
 * @brief Parses the registers configuration input file.
 *
 * File format (lines starting with '#' are ignored):
 * @code
 *   registers: N
 *   algorithm: basic
 *   algorithm: spilling, 2
 *   algorithm: splitting, 2
 *   algorithm: free
 * @endcode
 */
class RegistersParser {
public:
    /**
     * @brief Parses the given file and fills the RegisterConfig struct.
     *
     * **Time Complexity:** O(1) — fixed number of lines.
     *
     * @param filename Path to the registers text file.
     * @param config   Output RegisterConfig struct.
     * @return true on success, false if the file cannot be opened or is malformed.
     */
    static bool parse(const std::string &filename, RegisterConfig &config);
};

#endif
