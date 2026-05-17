#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <string>
#include <vector>

class BatchProcessor {
public:
    // Batch mode (-b flag): parse, allocate, write one output file.
    static int run(const std::string &rangesFile,
                   const std::string &registersFile,
                   const std::string &outputFile);

    // Menu option 7: run 6 basic professor datasets, write to outputs/basic/.
    // Returns display lines for the menu box.
    static std::vector<std::string> runBasicDatasetsToFile();

    // Menu option 8: run all 28 tests, write report + per-case files to outputs/.
    // Returns display lines for the menu box.
    static std::vector<std::string> runAllTestsToFile();

    // Legacy: returns summary lines for the basic professor datasets (no file output).
    static std::vector<std::string> runAllBasicDatasets();
};

#endif
