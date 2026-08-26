#ifndef WEIGHTEDMATCHING_H
#define WEIGHTEDMATCHING_H

#include <QVector>

namespace WeightedMatching {

struct Edge {
    int first;
    int second;
    double weight;
    int sourceIndex;
};

// Returns canonical (first < second) edges, ordered lexicographically.  Invalid,
// non-positive and duplicate edges are removed before Edmonds' algorithm runs.
QVector<Edge> solve(int vertexCount, const QVector<Edge> &edges);

} // namespace WeightedMatching

#endif
