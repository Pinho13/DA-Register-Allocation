/**
 * @file WebBuilder.cpp
 * @brief Implementation of the live-range to web merging algorithm.
 */

#include <algorithm>
#include "WebBuilder.h"

bool WebBuilder::shouldMerge(const LiveRange &a, const LiveRange &b) {
    if (a.varName != b.varName) return false;

    std::set<int> setA(a.lines.begin(), a.lines.end());
    std::set<int> setB(b.lines.begin(), b.lines.end());

    for (int ln : setA) {
        if (setB.count(ln)) return true;
    }

    // Same-line def-use fusion: a ends at N- and b starts at N+
    for (int n : a.useLines) {
        if (b.defLines.count(n)) return true;
    }
    for (int n : b.useLines) {
        if (a.defLines.count(n)) return true;
    }

    return false;
}

void WebBuilder::mergeTo(LiveRange &a, const LiveRange &b) {
    for (int ln : b.lines)    a.lines.push_back(ln);
    for (int ln : b.defLines) a.defLines.insert(ln);
    for (int ln : b.useLines) a.useLines.insert(ln);

    std::sort(a.lines.begin(), a.lines.end());
    a.lines.erase(std::unique(a.lines.begin(), a.lines.end()), a.lines.end());
}

void WebBuilder::build(const std::vector<LiveRange> &ranges, std::vector<Web> &webs) {
    std::vector<LiveRange> pool = ranges;

    // Fixed-point merge: repeat until no two ranges can be merged
    bool merged = true;
    while (merged) {
        merged = false;
        for (int i = 0; i < (int)pool.size(); i++) {
            for (int j = i + 1; j < (int)pool.size(); j++) {
                if (shouldMerge(pool[i], pool[j])) {
                    mergeTo(pool[i], pool[j]);
                    pool.erase(pool.begin() + j);
                    merged = true;
                    j--;
                }
            }
        }
    }

    webs.clear();
    for (int i = 0; i < (int)pool.size(); i++) {
        Web w;
        w.id       = i;
        w.varName  = pool[i].varName;
        w.lines    = pool[i].lines;
        w.defLines = pool[i].defLines;
        w.useLines = pool[i].useLines;
        w.assignedRegister = -1;
        webs.push_back(w);
    }
}
