/**
 * @file InterferenceGraph.cpp
 * @brief Implementation of the web interference graph.
 *
 * Wraps the TP-provided Graph<int> class. Each node is a web id (integer).
 * Edges are undirected and represent mutual interference between two webs.
 */

#include "InterferenceGraph.h"

InterferenceGraph::InterferenceGraph(const std::vector<Web> &webs)
    : numWebs_((int)webs.size()), removed_(webs.size(), false)
{
    for (const auto &w : webs)
        graph_.addVertex(w.id);

    for (int i = 0; i < numWebs_; i++)
        for (int j = i + 1; j < numWebs_; j++)
            if (computeInterference(webs[i], webs[j]))
                graph_.addBidirectionalEdge(webs[i].id, webs[j].id, 1.0);
}

bool InterferenceGraph::computeInterference(const Web &a, const Web &b) {
    std::set<int> setA(a.lines.begin(), a.lines.end());
    std::set<int> setB(b.lines.begin(), b.lines.end());

    for (int ln : setA) {
        if (!setB.count(ln)) continue;

        // Non-interference exception: A's last-use coincides with B's definition
        // at the same instruction — the value of A is consumed before B is born.
        bool aEndsHere  = a.useLines.count(ln) > 0;
        bool bStartsHere = b.defLines.count(ln) > 0;
        if (aEndsHere && bStartsHere) continue;

        bool bEndsHere  = b.useLines.count(ln) > 0;
        bool aStartsHere = a.defLines.count(ln) > 0;
        if (bEndsHere && aStartsHere) continue;

        return true;
    }
    return false;
}

int InterferenceGraph::effectiveDegree(int webId) const {
    auto *v = graph_.findVertex(webId);
    if (!v) return 0;
    int deg = 0;
    for (auto *e : v->getAdj())
        if (!removed_[e->getDest()->getInfo()]) deg++;
    return deg;
}

std::vector<int> InterferenceGraph::neighbours(int webId) const {
    auto *v = graph_.findVertex(webId);
    if (!v) return {};
    std::vector<int> result;
    for (auto *e : v->getAdj())
        result.push_back(e->getDest()->getInfo());
    return result;
}

bool InterferenceGraph::interferes(int a, int b) const {
    auto *v = graph_.findVertex(a);
    if (!v) return false;
    for (auto *e : v->getAdj())
        if (e->getDest()->getInfo() == b) return true;
    return false;
}

int InterferenceGraph::size() const { return numWebs_; }

void InterferenceGraph::setRemoved(int webId, bool removed) {
    if (webId >= 0 && webId < numWebs_) removed_[webId] = removed;
}

bool InterferenceGraph::isRemoved(int webId) const { return removed_[webId]; }

void InterferenceGraph::resetRemoved() {
    std::fill(removed_.begin(), removed_.end(), false);
}

const Graph<int> &InterferenceGraph::getGraph() const { return graph_; }
