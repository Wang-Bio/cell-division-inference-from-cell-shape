#include "weightedmatching.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/maximum_weighted_matching.hpp>
#include <algorithm>
#include <cmath>
#include <map>

namespace WeightedMatching {

QVector<Edge> solve(int vertexCount, const QVector<Edge> &input)
{
    QVector<Edge> answer;
    if (vertexCount <= 1)
        return answer;

    // Canonical insertion order is also the deterministic tie rule: among equal
    // optima Edmonds' search encounters smaller polygon pairs first.
    std::map<std::pair<int, int>, Edge> unique;
    for (Edge edge : input) {
        if (edge.first > edge.second)
            std::swap(edge.first, edge.second);
        if (edge.first < 0 || edge.second >= vertexCount || edge.first == edge.second
            || !std::isfinite(edge.weight) || edge.weight <= 0.0)
            continue;
        const auto key = std::make_pair(edge.first, edge.second);
        const auto found = unique.find(key);
        if (found == unique.end() || edge.weight > found->second.weight)
            unique[key] = edge;
    }

    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS,
        boost::no_property, boost::property<boost::edge_weight_t, double>>;
    Graph graph(static_cast<std::size_t>(vertexCount));
    std::map<std::pair<int, int>, Edge> byPair;
    for (const auto &item : unique) {
        boost::add_edge(item.second.first, item.second.second, item.second.weight, graph);
        byPair[item.first] = item.second;
    }
    std::vector<Graph::vertex_descriptor> mate(static_cast<std::size_t>(vertexCount));
    boost::maximum_weighted_matching(graph,
        boost::make_iterator_property_map(mate.begin(), boost::get(boost::vertex_index, graph)));
    for (int vertex = 0; vertex < vertexCount; ++vertex) {
        const auto other = mate[static_cast<std::size_t>(vertex)];
        if (other != boost::graph_traits<Graph>::null_vertex() && vertex < static_cast<int>(other))
            answer.append(byPair.at({vertex, static_cast<int>(other)}));
    }
    return answer;
}

} // namespace WeightedMatching
