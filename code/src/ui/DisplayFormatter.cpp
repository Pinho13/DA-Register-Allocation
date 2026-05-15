/**
 * @file DisplayFormatter.cpp
 * @brief Implementation of domain-to-string formatting utilities for the menu UI.
 */

#include <sstream>
#include <map>
#include "ui/DisplayFormatter.h"

/// Returns a formatted "line+/-" string for a single web's program points.
static std::string formatPoints(const Web &w) {
    std::string s;
    for (int i = 0; i < (int)w.lines.size(); i++) {
        if (i > 0) s += ",";
        s += std::to_string(w.lines[i]);
        if      (w.defLines.count(w.lines[i])) s += "+";
        else if (w.useLines.count(w.lines[i])) s += "-";
    }
    return s;
}

std::vector<std::string> DisplayFormatter::formatWebs(const std::vector<Web> &webs) {
    std::vector<std::string> lines;
    if (webs.empty()) { lines.push_back("No webs built yet."); return lines; }
    lines.push_back("Total webs: " + std::to_string(webs.size()));
    lines.push_back("");
    for (const auto &w : webs)
        lines.push_back("web" + std::to_string(w.id) + " [" + w.varName + "]: " + formatPoints(w));
    return lines;
}

std::vector<std::string> DisplayFormatter::formatAllocationResult(const AllocationResult &result) {
    std::vector<std::string> lines;
    if (result.webs.empty()) { lines.push_back("No allocation performed yet."); return lines; }

    lines.push_back("Webs: " + std::to_string(result.webs.size()));
    lines.push_back("Registers used: " + std::to_string(result.registersUsed));
    if (!result.feasible) lines.push_back("!!Warning: Full allocation not feasible.");
    lines.push_back("");

    std::map<int, std::vector<int>> regMap;
    std::vector<int> memWebs;
    for (const auto &w : result.webs) {
        if (w.assignedRegister >= 0) regMap[w.assignedRegister].push_back(w.id);
        else                         memWebs.push_back(w.id);
    }
    for (auto &[reg, ids] : regMap)
        for (int id : ids)
            lines.push_back("r" + std::to_string(reg) + " <- web" + std::to_string(id)
                            + "  [" + formatPoints(result.webs[id]) + "]");
    for (int id : memWebs)
        lines.push_back("M  <- web" + std::to_string(id)
                        + "  [" + formatPoints(result.webs[id]) + "]");
    return lines;
}

std::vector<std::string> DisplayFormatter::formatConfig(const RegisterConfig &config) {
    std::vector<std::string> lines;
    lines.push_back("Registers: " + std::to_string(config.numRegisters));
    std::string alg = "Algorithm: " + config.algorithm;
    if (config.algorithmParam > 0) alg += ", K=" + std::to_string(config.algorithmParam);
    lines.push_back(alg);
    return lines;
}

std::vector<std::string> DisplayFormatter::formatRanges(const std::vector<LiveRange> &ranges) {
    std::vector<std::string> lines;
    if (ranges.empty()) { lines.push_back("No ranges loaded."); return lines; }
    lines.push_back("Live ranges loaded: " + std::to_string(ranges.size()));
    lines.push_back("");
    for (const auto &r : ranges) {
        std::string s = r.varName + ": ";
        for (int i = 0; i < (int)r.lines.size(); i++) {
            if (i > 0) s += ",";
            s += std::to_string(r.lines[i]);
            if      (r.defLines.count(r.lines[i])) s += "+";
            else if (r.useLines.count(r.lines[i])) s += "-";
        }
        lines.push_back(s);
    }
    return lines;
}

std::vector<std::string> DisplayFormatter::wrapLine(const std::string &prefix,
                                                      const std::string &text,
                                                      int maxWidth) {
    bool isError = (prefix.size() >= 2 && prefix.substr(0, 2) == "!!");
    std::string contPrefix = isError ? "!!" : std::string(prefix.size(), ' ');
    int firstWidth = maxWidth - (int)prefix.size();
    int contWidth  = maxWidth - (int)contPrefix.size();
    std::vector<std::string> result;
    std::string remaining = text;
    bool first = true;
    while (!remaining.empty()) {
        int width       = first ? firstWidth : contWidth;
        std::string pre = first ? prefix : contPrefix;
        if ((int)remaining.size() <= width) { result.push_back(pre + remaining); break; }
        int cut = remaining.rfind(' ', width);
        if (cut == (int)std::string::npos || cut == 0) cut = width;
        result.push_back(pre + remaining.substr(0, cut));
        remaining = remaining.substr(cut);
        remaining.erase(0, remaining.find_first_not_of(' '));
        first = false;
    }
    return result;
}
