/**
 * @file BatchProcessor.cpp
 * @brief Implementation of the non-interactive batch execution mode.
 */

#include <iostream>
#include <filesystem>
#include "io/BatchProcessor.h"
#include "parser/RangesParser.h"
#include "parser/RegistersParser.h"
#include "core/WebBuilder.h"
#include "core/InterferenceGraph.h"
#include "core/RegisterAllocator.h"
#include "io/OutputWriter.h"

namespace fs = std::filesystem;

int BatchProcessor::run(const std::string &rangesFile,
                        const std::string &registersFile,
                        const std::string &outputFile) {
    std::vector<LiveRange> ranges;
    if (!RangesParser::parse(rangesFile, ranges)) {
        std::cerr << "Error: Failed to parse ranges file: " << rangesFile << "\n";
        return 1;
    }

    RegisterConfig config;
    if (!RegistersParser::parse(registersFile, config)) {
        std::cerr << "Error: Failed to parse registers file: " << registersFile << "\n";
        return 1;
    }

    std::vector<Web> webs;
    WebBuilder::build(ranges, webs);

    InterferenceGraph ig(webs);
    AllocationResult result = RegisterAllocator::allocate(webs, ig, config);

    OutputWriter::write(outputFile, result);
    return 0;
}

std::vector<std::string> BatchProcessor::runAllBasicDatasets() {
    struct TestCase {
        std::string rangesFile;
        std::string registersFile;
        std::string label;
    };

    const std::vector<TestCase> cases = {
        {"basic/ranges/ranges1.txt", "basic/registers/registers2.txt", "ranges1 + registers2 (expect 2 regs)"},
        {"basic/ranges/ranges2.txt", "basic/registers/registers2.txt", "ranges2 + registers2 (expect 2 regs)"},
        {"basic/ranges/ranges3.txt", "basic/registers/registers2.txt", "ranges3 + registers2 (expect 2 regs)"},
        {"basic/ranges/ranges4.txt", "basic/registers/registers1.txt", "ranges4 + registers1 (expect 1 reg)"},
        {"basic/ranges/ranges5.txt", "basic/registers/registers1.txt", "ranges5 + registers1 (expect 1 reg)"},
        {"basic/ranges/ranges6.txt", "basic/registers/registers3.txt", "ranges6 + registers3 (expect 3 regs)"},
    };

    std::vector<std::string> output;
    for (const auto &tc : cases) {
        std::vector<LiveRange> ranges;
        RegisterConfig config;
        if (!RangesParser::parse(tc.rangesFile, ranges) ||
            !RegistersParser::parse(tc.registersFile, config)) {
            output.push_back("!!" + tc.label + " — FAILED to load");
            continue;
        }
        std::vector<Web> webs;
        WebBuilder::build(ranges, webs);
        InterferenceGraph ig(webs);
        AllocationResult result = RegisterAllocator::allocate(webs, ig, config);

        std::string line = tc.label + " => " + std::to_string(result.registersUsed) + " reg(s)";
        if (!result.feasible) line = "!!" + line + " [INFEASIBLE]";
        output.push_back(line);
    }
    return output;
}
