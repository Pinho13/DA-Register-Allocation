#include <stack>
#include <set>
#include "core/RegisterAllocator.h"

bool RegisterAllocator::basicColoring(std::vector<Web> &webs, InterferenceGraph &ig,
                                       int N, std::vector<int> &spilledIds) {
    ig.resetRemoved();
    spilledIds.clear();
    int total = ig.size();

    for (auto &w : webs) w.assignedRegister = -1;

    std::stack<int> colorStack;
    int remaining = total;

    while (remaining > 0) {
        bool progress = true;
        while (progress) {
            progress = false;
            for (int i = 0; i < total; i++) {
                if (ig.isRemoved(i)) continue;
                if (ig.effectiveDegree(i) < N) {
                    ig.setRemoved(i, true);
                    colorStack.push(i);
                    remaining--;
                    progress = true;
                }
            }
        }
        if (remaining > 0) {
            int spill = selectSpillCandidate(ig, webs);
            ig.setRemoved(spill, true);
            spilledIds.push_back(spill);
            remaining--;
        }
    }

    while (!colorStack.empty()) {
        int id = colorStack.top();
        colorStack.pop();
        ig.setRemoved(id, false);

        std::set<int> usedColors;
        for (int nb : ig.neighbours(id)) {
            if (!ig.isRemoved(nb) && webs[nb].assignedRegister >= 0)
                usedColors.insert(webs[nb].assignedRegister);
        }
        for (int c = 0; c < N; c++) {
            if (!usedColors.count(c)) {
                webs[id].assignedRegister = c;
                break;
            }
        }
    }

    return spilledIds.empty();
}

int RegisterAllocator::selectSpillCandidate(const InterferenceGraph &ig,
                                             const std::vector<Web> &webs) {
    int best = -1;
    double bestCost = 1e18;
    for (int i = 0; i < ig.size(); i++) {
        if (ig.isRemoved(i)) continue;
        int deg = ig.effectiveDegree(i);
        int len = (int)webs[i].lines.size();
        double cost = static_cast<double>(len) / (deg + 1);
        if (cost < bestCost || (cost == bestCost && deg > ig.effectiveDegree(best))) {
            bestCost = cost;
            best = i;
        }
    }
    return best;
}

void RegisterAllocator::splitWeb(std::vector<Web> &webs, int webIdx) {
    Web &orig = webs[webIdx];
    int sz = (int)orig.lines.size();
    if (sz < 2) return;

    int mid = sz / 2;
    Web a, b;
    a.varName = orig.varName;
    b.varName = orig.varName;

    for (int i = 0; i < mid; i++) {
        int ln = orig.lines[i];
        a.lines.push_back(ln);
        if (orig.defLines.count(ln)) a.defLines.insert(ln);
        if (orig.useLines.count(ln)) a.useLines.insert(ln);
    }
    for (int i = mid; i < sz; i++) {
        int ln = orig.lines[i];
        b.lines.push_back(ln);
        if (orig.defLines.count(ln)) b.defLines.insert(ln);
        if (orig.useLines.count(ln)) b.useLines.insert(ln);
    }

    if (!a.lines.empty()) a.useLines.insert(a.lines.back());
    if (!b.lines.empty()) b.defLines.insert(b.lines.front());

    int nextId = (int)webs.size();
    a.id = webIdx;
    b.id = nextId;
    a.assignedRegister = -1;
    b.assignedRegister = -1;
    webs[webIdx] = a;
    webs.push_back(b);
}

int RegisterAllocator::selectSplitCandidate(const InterferenceGraph &ig,
                                             const std::vector<Web> &webs) {
    int best = -1, bestScore = -1;
    for (int i = 0; i < ig.size(); i++) {
        if (ig.isRemoved(i)) continue;
        if ((int)webs[i].lines.size() < 2) continue;
        int score = ig.effectiveDegree(i) * (int)webs[i].lines.size();
        if (score > bestScore) { bestScore = score; best = i; }
    }
    return best;
}
