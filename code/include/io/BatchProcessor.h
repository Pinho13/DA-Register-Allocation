#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <string>
#include <vector>

/**
 * @brief Handles non-interactive batch execution.
 *
 * Invoked when the program is run as:
 * @code
 *   ./register_alloc -b ranges.txt registers.txt allocation.txt
 * @endcode
 */
class BatchProcessor {
public:
    /**
     * @brief Runs the full allocation pipeline in batch mode.
     *
     * Parses both input files, builds webs, builds interference graph,
     * runs the algorithm specified in the registers file, writes output.
     *
     * **Time Complexity:** O(W^2 * L) dominated by web building and coloring.
     *
     * @param rangesFile    Path to the live ranges input file.
     * @param registersFile Path to the registers configuration file.
     * @param outputFile    Path to the output allocation file.
     * @return 0 on success, 1 on error.
     */
    static int run(const std::string &rangesFile,
                   const std::string &registersFile,
                   const std::string &outputFile);

    /**
     * @brief Runs all basic test cases from the basic/ folder.
     *
     * Used by the interactive menu to demo all datasets at once.
     *
     * @return Lines to display in the menu box.
     */
    static std::vector<std::string> runAllBasicDatasets();
};

#endif
