#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <string>
#include <vector>

/**
 * @brief Runs register allocation in batch mode and across full dataset suites.
 *
 * Three entry points:
 *   - run()                  — single-case batch mode (the -b CLI flag).
 *   - runBasicDatasetsToFile() — the 6 professor-provided basic cases.
 *   - runAllTestsToFile()      — all 75 test cases across 5 dataset folders.
 */
class BatchProcessor {
public:
    /**
     * @brief Parses, allocates, and writes a single input pair to an output file.
     *
     * Used by the -b command-line flag. Reads the ranges and registers files,
     * runs the allocation algorithm specified in the registers file, and writes
     * the result to @p outputFile.
     *
     * @param rangesFile    Path to the live-ranges input file.
     * @param registersFile Path to the register-configuration input file.
     * @param outputFile    Path to the output file to write.
     * @return 0 on success, 1 on parse or I/O error.
     *
     * @complexity O(W² · L) dominated by interference graph construction.
     */
    static int run(const std::string &rangesFile,
                   const std::string &registersFile,
                   const std::string &outputFile);

    /**
     * @brief Runs the 6 basic professor datasets and writes results to outputs/basic/.
     *
     * Creates outputs/basic/ if it does not exist. Writes one allocation file
     * per test case and a results.txt summary with PASS/FAIL per case.
     *
     * @return Display lines for the interactive menu box (summary + saved path).
     *
     * @complexity O(6 · W² · L).
     */
    static std::vector<std::string> runBasicDatasetsToFile();

    /**
     * @brief Runs all 75 test cases across 5 dataset folders and writes results to outputs/.
     *
     * Creates outputs/<folder>/ for each dataset category. Writes one allocation
     * file per test case, a per-folder results.txt with PASS/FAIL, and a global
     * outputs/results_report.txt summarising all categories.
     *
     * @return Display lines for the interactive menu box (pass count + saved path).
     *
     * @complexity O(75 · W² · L).
     */
    static std::vector<std::string> runAllTestsToFile();

    /**
     * @brief Returns summary lines for the basic datasets without writing any files.
     *
     * @return One summary string per test case.
     *
     * @complexity O(6 · W² · L).
     */
    static std::vector<std::string> runAllBasicDatasets();
};

#endif
