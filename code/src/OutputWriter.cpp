/**
 * @file OutputWriter.cpp
 * @brief Implementation of the allocation result file writer.
 */

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>
#include "OutputWriter.h"

std::string OutputWriter::formatWebPoints(const Web &web) {
    std::string result;
    bool first = true;
    for (int ln : web.lines) {
        bool isDef = web.defLines.count(ln) > 0;
        bool isUse = web.useLines.count(ln) > 0;

        // A line that is both def and use is a fusion point (e.g. i = i+1):
        // two live ranges of the same variable meet here. Per the spec (Figure 11),
        // such a point carries no marker — it appears as a plain line number.
        auto emit = [&](const std::string &token) {
            if (!first) result += ",";
            result += token;
            first = false;
        };

        if (isDef && isUse) {
            emit(std::to_string(ln));
        } else if (isDef) {
            emit(std::to_string(ln) + "+");
        } else if (isUse) {
            emit(std::to_string(ln) + "-");
        } else {
            emit(std::to_string(ln));
        }
    }
    return result;
}

void OutputWriter::write(const std::string &filename, const AllocationResult &result) {
    if (!result.feasible)
        std::cerr << "Warning: Register allocation was not possible with the given number of registers.\n";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open output file: " << filename << "\n";
        return;
    }

    file << "# Total number of webs followed by the listing of the program points of each one\n";
    file << "# program points in each web are sorted in ascending order\n";
    file << "webs: " << result.webs.size() << "\n";
    for (const auto &w : result.webs)
        file << "web" << w.id << ": " << formatWebPoints(w) << "\n";

    file << "# Total number of registers used, followed by assignment to webs\n";
    file << "registers: " << result.registersUsed << "\n";

    if (result.registersUsed == 0) {
        for (const auto &w : result.webs)
            file << "M: web" << w.id << "\n";
    } else {
        std::map<int, std::vector<int>> regToWebs;
        std::vector<int> memWebs;
        for (const auto &w : result.webs) {
            if (w.assignedRegister >= 0) regToWebs[w.assignedRegister].push_back(w.id);
            else                         memWebs.push_back(w.id);
        }
        for (auto &[reg, ids] : regToWebs)
            for (int id : ids)
                file << "r" << reg << ": web" << id << "\n";
        for (int id : memWebs)
            file << "M: web" << id << "\n";
    }
}
