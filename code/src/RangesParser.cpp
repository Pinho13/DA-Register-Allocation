/**
 * @file RangesParser.cpp
 * @brief Implementation of the live-ranges input file parser.
 */

#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include "parser/RangesParser.h"

bool RangesParser::parseToken(const std::string &token, int &line, bool &isDef, bool &isUse) {
    if (token.empty()) return false;
    isDef = false;
    isUse = false;
    std::string t = token;
    t.erase(0, t.find_first_not_of(" \t\r\n"));
    t.erase(t.find_last_not_of(" \t\r\n") + 1);
    if (t.empty()) return false;

    char last = t.back();
    if (last == '+') { isDef = true; t.pop_back(); }
    else if (last == '-') { isUse = true; t.pop_back(); }

    try {
        line = std::stoi(t);
    } catch (...) {
        return false;
    }
    return true;
}

bool RangesParser::parse(const std::string &filename, std::vector<LiveRange> &ranges) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string lineStr;
    while (std::getline(file, lineStr)) {
        lineStr.erase(0, lineStr.find_first_not_of(" \t\r\n"));
        if (lineStr.empty() || lineStr[0] == '#') continue;

        auto colonPos = lineStr.find(':');
        if (colonPos == std::string::npos) continue;

        std::string varName = lineStr.substr(0, colonPos);
        varName.erase(varName.find_last_not_of(" \t") + 1);

        std::string rest = lineStr.substr(colonPos + 1);

        LiveRange lr;
        lr.varName = varName;

        std::stringstream ss(rest);
        std::string token;
        while (std::getline(ss, token, ',')) {
            int lineNum;
            bool isDef, isUse;
            if (!parseToken(token, lineNum, isDef, isUse)) continue;
            lr.lines.push_back(lineNum);
            if (isDef) lr.defLines.insert(lineNum);
            if (isUse) lr.useLines.insert(lineNum);
        }

        if (!lr.lines.empty()) {
            std::sort(lr.lines.begin(), lr.lines.end());
            lr.lines.erase(std::unique(lr.lines.begin(), lr.lines.end()), lr.lines.end());
            ranges.push_back(lr);
        }
    }
    return true;
}
