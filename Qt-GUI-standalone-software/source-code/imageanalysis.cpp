#include "imageanalysis.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <set>
#include <tuple>
#include <cmath>
#include <limits>
#include <utility>
#include <unordered_map>
#include <unordered_set>

namespace {
// Compute the number of 0->1 transitions in the ordered neighborhood.
int computeConnectivity(const cv::Mat &img, int r, int c) {
    uchar p2 = img.at<uchar>(r - 1, c);
    uchar p3 = img.at<uchar>(r - 1, c + 1);
    uchar p4 = img.at<uchar>(r, c + 1);
    uchar p5 = img.at<uchar>(r + 1, c + 1);
    uchar p6 = img.at<uchar>(r + 1, c);
    uchar p7 = img.at<uchar>(r + 1, c - 1);
    uchar p8 = img.at<uchar>(r, c - 1);
    uchar p9 = img.at<uchar>(r - 1, c - 1);

    int transitions = (!p2 && p3) + (!p3 && p4) + (!p4 && p5) + (!p5 && p6) +
                      (!p6 && p7) + (!p7 && p8) + (!p8 && p9) + (!p9 && p2);
    return transitions;
}

// Count the number of white neighbors (value 1) around the pixel.
int countNeighbors(const cv::Mat &img, int r, int c) {
    uchar p2 = img.at<uchar>(r - 1, c);
    uchar p3 = img.at<uchar>(r - 1, c + 1);
    uchar p4 = img.at<uchar>(r, c + 1);
    uchar p5 = img.at<uchar>(r + 1, c + 1);
    uchar p6 = img.at<uchar>(r + 1, c);
    uchar p7 = img.at<uchar>(r + 1, c - 1);
    uchar p8 = img.at<uchar>(r, c - 1);
    uchar p9 = img.at<uchar>(r - 1, c - 1);

    int n1 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9);
    int n2 = (p3 | p4) + (p5 | p6) + (p7 | p8) + (p9 | p2);
    return std::min(n1, n2);
}

// One of the two Guo-Hall sub-iterations.
void guoHallIteration(cv::Mat &img, int iter) {
    cv::Mat marker = cv::Mat::zeros(img.size(), CV_8UC1);

    for (int r = 1; r < img.rows - 1; ++r) {
        for (int c = 1; c < img.cols - 1; ++c) {
            uchar p1 = img.at<uchar>(r, c);
            if (p1 != 1)
                continue;

            int connectivity = computeConnectivity(img, r, c);
            int neighbors = countNeighbors(img, r, c);

            uchar p2 = img.at<uchar>(r - 1, c);
            uchar p4 = img.at<uchar>(r, c + 1);
            uchar p5 = img.at<uchar>(r + 1, c + 1);
            uchar p6 = img.at<uchar>(r + 1, c);
            uchar p8 = img.at<uchar>(r, c - 1);
            uchar p9 = img.at<uchar>(r - 1, c - 1);

            int m = (iter == 0)
                        ? ((p2 | p4 | !p6) & p8)
                        : ((p4 | p6 | !p8) & p2);

            if (connectivity == 1 && (neighbors >= 2 && neighbors <= 3) && m == 0) {
                marker.at<uchar>(r, c) = 1;
            }
        }
    }

    img &= ~marker;
}
} // namespace

namespace ImageAnalysis {

cv::Mat toGrayscale(const cv::Mat &input) {
    if (input.channels() == 1) {
        return input.clone();
    }

    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat binarize(const cv::Mat &grayscale, double threshold, bool invert) {
    CV_Assert(grayscale.channels() == 1);

    cv::Mat binary;
    int threshType = invert ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;

    // When the caller does not provide a threshold, fall back to Otsu so the
    // whole image is not forced to white (threshold < 0 would otherwise make
    // every pixel pass the binary test).
    if (threshold < 0) {
        threshType |= cv::THRESH_OTSU;
        threshold = 0.0;
    }

    cv::threshold(grayscale, binary, threshold, 255, threshType);
    return binary;
}

cv::Mat guoHallThinning(const cv::Mat &binaryInput) {
    CV_Assert(binaryInput.type() == CV_8UC1);

    // Normalize to 0/1 so the logical operations match the algorithm description.
    cv::Mat img;
    cv::threshold(binaryInput, img, 0, 1, cv::THRESH_BINARY);

    cv::Mat prev;
    cv::Mat diff;
    do {
        img.copyTo(prev);
        guoHallIteration(img, 0);
        guoHallIteration(img, 1);
        cv::absdiff(img, prev, diff);
    } while (cv::countNonZero(diff) > 0);

    img *= 255;
    return img;
}

cv::Mat destairSkeleton(const cv::Mat &skeletonImage) {
    CV_Assert(skeletonImage.type() == CV_8UC1);

    const int width = skeletonImage.cols;
    const int height = skeletonImage.rows;

    if (width < 3 || height < 3)
        return skeletonImage.clone();

    std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    for (int y = 0; y < height; ++y) {
        const uchar *line = skeletonImage.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            pixels[index] = line[x] > 0 ? 1U : 0U;
        }
    }

    auto at = [&pixels, width](int y, int x) -> uint8_t & {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    };

    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::size_t> remove;
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                if (at(y, x) == 0U)
                    continue;

                const uint8_t e = at(y, x + 1);
                const uint8_t ne = at(y - 1, x + 1);
                const uint8_t n = at(y - 1, x);
                const uint8_t nw = at(y - 1, x - 1);
                const uint8_t w = at(y, x - 1);
                const uint8_t sw = at(y + 1, x - 1);
                const uint8_t s0 = at(y + 1, x);
                const uint8_t se = at(y + 1, x + 1);

                if (pass == 0) {
                    const bool case1 = n && (e && !ne && !sw && (!w || !s0));
                    const bool case2 = n && (w && !nw && !se && (!e || !s0));
                    if (case1 || case2)
                        remove.push_back(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                         + static_cast<std::size_t>(x));
                } else {
                    const bool case1 = s0 && (e && !se && !nw && (!w || !n));
                    const bool case2 = s0 && (w && !sw && !ne && (!e || !n));
                    if (case1 || case2)
                        remove.push_back(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                         + static_cast<std::size_t>(x));
                }
            }
        }

        if (remove.empty())
            break;

        for (std::size_t index : remove)
            pixels[index] = 0U;
    }

    cv::Mat destaired(height, width, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < height; ++y) {
        uchar *line = destaired.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            line[x] = pixels[index] ? 255 : 0;
        }
    }

    return destaired;
}

cv::Mat removeSpurs(const cv::Mat &skeletonImage) {
    CV_Assert(skeletonImage.type() == CV_8UC1);

    const int width = skeletonImage.cols;
    const int height = skeletonImage.rows;

    if (width == 0 || height == 0)
        return skeletonImage.clone();

    std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    for (int y = 0; y < height; ++y) {
        const uchar *line = skeletonImage.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            pixels[index] = line[x] > 0 ? 1U : 0U;
        }
    }

    auto at = [&pixels, width](int y, int x) -> uint8_t & {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    };

    const int offsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                               {0, 1},  {1, -1}, {1, 0},  {1, 1}};

    while (true) {
        std::vector<std::size_t> remove;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (at(y, x) == 0U)
                    continue;

                int neighborCount = 0;
                for (const auto &offset : offsets) {
                    const int ny = y + offset[0];
                    const int nx = x + offset[1];
                    if (ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;
                    neighborCount += at(ny, nx);
                }

                if (neighborCount == 1)
                    remove.push_back(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                     + static_cast<std::size_t>(x));
            }
        }

        if (remove.empty())
            break;

        for (std::size_t index : remove)
            pixels[index] = 0U;
    }

    cv::Mat cleaned(height, width, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < height; ++y) {
        uchar *line = cleaned.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            line[x] = pixels[index] ? 255 : 0;
        }
    }

    return cleaned;
}


cv::Mat segmentWithGuoHall(const cv::Mat &input, double threshold, bool invertInput) {
    cv::Mat gray = toGrayscale(input);
    cv::Mat binary = binarize(gray, threshold, invertInput);
    cv::Mat skeleton = guoHallThinning(binary);
    cv::Mat destaired_skeleton = destairSkeleton(skeleton);
    cv::Mat despurred_skeleton = removeSpurs(destaired_skeleton);
    return despurred_skeleton;
}

std::vector<cv::Point2d> detectVertices(const cv::Mat &skeletonImage) {
    CV_Assert(skeletonImage.type() == CV_8UC1);

    const int rows = skeletonImage.rows;
    const int cols = skeletonImage.cols;
    if (rows < 3 || cols < 3)
        return {};

    std::vector<cv::Point> vertices;
    vertices.reserve(rows * cols / 16); // heuristic reserve

    for (int y = 1; y < rows - 1; ++y) {
        const uchar *row = skeletonImage.ptr<uchar>(y);
        for (int x = 1; x < cols - 1; ++x) {
            if (row[x] == 0)
                continue;

            int neighborCount = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const uchar *neighborRow = skeletonImage.ptr<uchar>(y + dy);
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0)
                        continue;
                    neighborCount += neighborRow[x + dx] > 0 ? 1 : 0;
                }
            }

            if (neighborCount >= 3)
                vertices.emplace_back(x, y);
        }
    }

    // Cluster nearby vertices together to avoid redundancy. Any vertices within
    // a Euclidean distance of 2.0 pixels are merged, and the cluster centroid
    // is returned as the representative vertex.
    const double radius = 2.0;
    const double radiusSquared = radius * radius;
    std::vector<cv::Point2d> clustered;
    std::vector<bool> visited(vertices.size(), false);

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (visited[i])
            continue;

        std::vector<std::size_t> stack = {i};
        std::vector<cv::Point2d> cluster;
        visited[i] = true;

        while (!stack.empty()) {
            const std::size_t idx = stack.back();
            stack.pop_back();

            cluster.push_back(vertices[idx]);

            for (std::size_t j = 0; j < vertices.size(); ++j) {
                if (visited[j])
                    continue;

                const double dx = static_cast<double>(vertices[j].x - vertices[idx].x);
                const double dy = static_cast<double>(vertices[j].y - vertices[idx].y);
                const double distSquared = dx * dx + dy * dy;

                if (distSquared <= radiusSquared) {
                    visited[j] = true;
                    stack.push_back(j);
                }
            }
        }

        double sumX = 0.0;
        double sumY = 0.0;
        for (const cv::Point &p : cluster) {
            sumX += static_cast<double>(p.x);
            sumY += static_cast<double>(p.y);
        }

        const double count = static_cast<double>(cluster.size());
        const cv::Point2d center(cvRound(sumX / count), cvRound(sumY / count));
        clustered.push_back(center);
    }

    return clustered;
}

std::vector<LineConnection> detectLines(const cv::Mat &skeletonImage,
                                        const std::vector<cv::Point> &vertices,
                                        int tolerance) {
    CV_Assert(skeletonImage.type() == CV_8UC1);
    CV_Assert(tolerance >= 0);

    const int rows = skeletonImage.rows;
    const int cols = skeletonImage.cols;
    if (rows == 0 || cols == 0 || vertices.size() < 2)
        return {};

    const int tolSquared = tolerance * tolerance;
    const int neighborOffsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                       {0, 1},  {1, -1}, {1, 0},  {1, 1}};

    auto vertexIndexAt = [&vertices, tolSquared](const cv::Point &p) -> int {
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const int dx = vertices[i].x - p.x;
            const int dy = vertices[i].y - p.y;
            if (dx * dx + dy * dy <= tolSquared)
                return static_cast<int>(i);
        }
        return -1;
    };

    struct PointPairLess {
        bool operator()(const std::pair<cv::Point, cv::Point> &a,
                        const std::pair<cv::Point, cv::Point> &b) const {
            return std::tie(a.first.y, a.first.x, a.second.y, a.second.x) <
                   std::tie(b.first.y, b.first.x, b.second.y, b.second.x);
        }
    };

    struct EdgeLess {
        bool operator()(const std::pair<int, int> &a, const std::pair<int, int> &b) const {
            return a < b;
        }
    };

    std::set<std::pair<cv::Point, cv::Point>, PointPairLess> visitedPixelEdges;
    std::set<std::pair<int, int>, EdgeLess> reportedVertexEdges;
    std::vector<LineConnection> connections;

    auto addVisited = [&visitedPixelEdges](const cv::Point &from, const cv::Point &to) {
        visitedPixelEdges.insert({from, to});
        visitedPixelEdges.insert({to, from});
    };

    auto wasVisited = [&visitedPixelEdges](const cv::Point &from, const cv::Point &to) {
        return visitedPixelEdges.find({from, to}) != visitedPixelEdges.end();
    };

    for (std::size_t vIdx = 0; vIdx < vertices.size(); ++vIdx) {
        const cv::Point &vertex = vertices[vIdx];
        for (const auto &offset : neighborOffsets) {
            cv::Point next(vertex.x + offset[1], vertex.y + offset[0]);
            if (next.x < 0 || next.x >= cols || next.y < 0 || next.y >= rows)
                continue;

            if (skeletonImage.at<uchar>(next) == 0)
                continue;

            if (wasVisited(vertex, next))
                continue;

            struct WalkState {
                cv::Point prev;
                cv::Point current;
                std::vector<cv::Point> path;
            };

            std::vector<WalkState> stack;
            stack.push_back({vertex, next, {vertex, next}});

            while (!stack.empty()) {
                WalkState state = stack.back();
                stack.pop_back();

                const cv::Point &prev = state.prev;
                const cv::Point &current = state.current;
                std::vector<cv::Point> path = state.path;

                if (wasVisited(prev, current))
                    continue;

                addVisited(prev, current);
                const int foundVertex = vertexIndexAt(current);
                if (foundVertex >= 0 && foundVertex != static_cast<int>(vIdx)) {
                    const int a = static_cast<int>(vIdx);
                    const int b = foundVertex;
                    const std::pair<int, int> edgeKey = a < b ? std::make_pair(a, b) : std::make_pair(b, a);

                    if (reportedVertexEdges.insert(edgeKey).second) {
                        connections.push_back({a, b, path});
                    }
                    continue;
                }

                for (const auto &innerOffset : neighborOffsets) {
                    cv::Point neighbor(current.x + innerOffset[1], current.y + innerOffset[0]);
                    if (neighbor.x < 0 || neighbor.x >= cols || neighbor.y < 0 || neighbor.y >= rows)
                        continue;

                    if (neighbor == prev)
                        continue;

                    // If the next point is already within tolerance of a vertex, treat it as the
                    // ending vertex even if the skeleton does not extend exactly to that pixel.
                    const int neighborVertex = vertexIndexAt(neighbor);
                    if (neighborVertex >= 0 && neighborVertex != static_cast<int>(vIdx)) {
                        const int a = static_cast<int>(vIdx);
                        const int b = neighborVertex;
                        const std::pair<int, int> edgeKey =
                            a < b ? std::make_pair(a, b) : std::make_pair(b, a);

                        if (reportedVertexEdges.insert(edgeKey).second) {
                            std::vector<cv::Point> finalPath = path;
                            if (finalPath.empty() || finalPath.back() != current)
                                finalPath.push_back(current);
                            finalPath.push_back(neighbor);

                            connections.push_back({a, b, std::move(finalPath)});
                        }

                        addVisited(current, neighbor);
                        continue;
                    }

                    if (skeletonImage.at<uchar>(neighbor) == 0)
                        continue;

                    if (wasVisited(current, neighbor))
                        continue;

                    std::vector<cv::Point> nextPath = path;
                    if (nextPath.empty() || nextPath.back() != current)
                        nextPath.push_back(current);
                    nextPath.push_back(neighbor);

                    stack.push_back({current, neighbor, std::move(nextPath)});
                }
            }
        }
    }

    return connections;
}

double normalizedAngle(const cv::Point2d &origin, const cv::Point2d &dest) {
    double angle = std::atan2(dest.y - origin.y, dest.x - origin.x);
    if (angle < 0)
        angle += 2.0 * CV_PI;
    return angle;
}

double signedArea(const std::vector<int> &cycle, const std::unordered_map<int, cv::Point2d> &positions) {
    if (cycle.size() < 3)
        return 0.0;

    double area = 0.0;
    for (std::size_t i = 0; i < cycle.size(); ++i) {
        const int idA = cycle[i];
        const int idB = cycle[(i + 1) % cycle.size()];

        const auto itA = positions.find(idA);
        const auto itB = positions.find(idB);
        if (itA == positions.end() || itB == positions.end())
            return 0.0;

        const cv::Point2d &a = itA->second;
        const cv::Point2d &b = itB->second;
        area += a.x * b.y - a.y * b.x;
    }

    return 0.5 * area;
}

std::vector<std::vector<int>> detectPolygonsCCW(const std::vector<VertexGeometry> &vertices,
                                                const std::vector<GraphEdge> &edges) {
    if (vertices.size() < 3 || edges.size() < 3)
        return {};

    std::unordered_map<int, cv::Point2d> positions;
    positions.reserve(vertices.size());
    for (const auto &v : vertices)
        positions.emplace(v.id, v.position);

    struct HalfEdge {
        int origin = -1;
        int dest = -1;
        int twin = -1;
        int next = -1;
    };

    std::vector<HalfEdge> halfEdges;
    halfEdges.reserve(edges.size() * 2);

    for (const auto &edge : edges) {
        if (edge.startId == edge.endId)
            continue;

        if (!positions.count(edge.startId) || !positions.count(edge.endId))
            continue;

        const int id1 = static_cast<int>(halfEdges.size());
        halfEdges.push_back({edge.startId, edge.endId, -1, -1});
        const int id2 = static_cast<int>(halfEdges.size());
        halfEdges.push_back({edge.endId, edge.startId, -1, -1});

        halfEdges[id1].twin = id2;
        halfEdges[id2].twin = id1;
    }

    if (halfEdges.size() < 6)
        return {};

    struct OutgoingEdge {
        int halfEdgeId = -1;
        double angle = 0.0;
    };

    std::unordered_map<int, std::vector<OutgoingEdge>> vertexStars;
    vertexStars.reserve(positions.size());

    for (int hid = 0; hid < static_cast<int>(halfEdges.size()); ++hid) {
        const HalfEdge &edge = halfEdges[hid];
        const auto originIt = positions.find(edge.origin);
        const auto destIt = positions.find(edge.dest);
        if (originIt == positions.end() || destIt == positions.end())
            continue;

        vertexStars[edge.origin].push_back({hid, normalizedAngle(originIt->second, destIt->second)});
    }

    for (auto &entry : vertexStars) {
        auto &outgoing = entry.second;
        std::sort(outgoing.begin(), outgoing.end(), [](const OutgoingEdge &lhs, const OutgoingEdge &rhs) {
            if (lhs.angle == rhs.angle)
                return lhs.halfEdgeId < rhs.halfEdgeId;
            return lhs.angle < rhs.angle;
        });
    }

    for (int hid = 0; hid < static_cast<int>(halfEdges.size()); ++hid) {
        HalfEdge &edge = halfEdges[hid];
        if (edge.twin < 0)
            continue;

        const auto starIt = vertexStars.find(edge.dest);
        if (starIt == vertexStars.end())
            continue;

        const auto &outgoing = starIt->second;
        const auto it = std::find_if(outgoing.begin(), outgoing.end(), [twinId = edge.twin](const OutgoingEdge &out) {
            return out.halfEdgeId == twinId;
        });
        if (it == outgoing.end())
            continue;

        const int index = static_cast<int>(std::distance(outgoing.begin(), it));
        const int prevIndex = (index - 1 + static_cast<int>(outgoing.size())) % static_cast<int>(outgoing.size());
        edge.next = outgoing[prevIndex].halfEdgeId;
    }

    std::vector<char> visited(halfEdges.size(), 0);
    std::vector<std::vector<int>> detectedPolygons;
    std::unordered_set<std::string> seenKeys;

    for (int startId = 0; startId < static_cast<int>(halfEdges.size()); ++startId) {
        if (visited[startId])
            continue;

        std::vector<int> traversal;
        traversal.reserve(8);
        int current = startId;
        bool closed = false;

        while (!visited[current]) {
            visited[current] = 1;
            traversal.push_back(current);

            const HalfEdge &edge = halfEdges[current];
            if (edge.next < 0) {
                current = -1;
                break;
            }

            current = edge.next;
            if (current == startId) {
                closed = true;
                break;
            }

            if (current < 0 || current >= static_cast<int>(halfEdges.size())) {
                current = -1;
                break;
            }
        }

        if (!closed)
            continue;

        std::vector<int> cycle;
        cycle.reserve(traversal.size());
        bool validCycle = true;
        for (int hid : traversal) {
            const HalfEdge &edge = halfEdges[hid];
            if (!positions.count(edge.origin)) {
                validCycle = false;
                break;
            }
            cycle.push_back(edge.origin);
        }

        if (!validCycle || cycle.size() < 3)
            continue;

        const double area = signedArea(cycle, positions);
        if (area <= 1e-6)
            continue;

        const std::string key = polygonKey(cycle);
        if (key.empty() || seenKeys.count(key))
            continue;

        seenKeys.insert(key);
        detectedPolygons.push_back(std::move(cycle));
    }

    const auto polygonMinY = [&positions](const std::vector<int> &polygon) {
        double minY = std::numeric_limits<double>::max();
        double minX = std::numeric_limits<double>::max();

        for (int vid : polygon) {
            const auto posIt = positions.find(vid);
            if (posIt == positions.end())
                continue;

            const cv::Point2d &pt = posIt->second;
            if (pt.y < minY || (pt.y == minY && pt.x < minX)) {
                minY = pt.y;
                minX = pt.x;
            }
        }

        return std::make_pair(minY, minX);
    };

    std::sort(detectedPolygons.begin(), detectedPolygons.end(), [&](const auto &lhs, const auto &rhs) {
        const auto lhsMin = polygonMinY(lhs);
        const auto rhsMin = polygonMinY(rhs);

        if (lhsMin.first == rhsMin.first)
            return lhsMin.second < rhsMin.second;
        return lhsMin.first < rhsMin.first;
    });

    return detectedPolygons;
}

std::vector<int> canonicalizePolygon(const std::vector<int> &ids) {
    if (ids.empty())
        return {};

    int minVal = ids.front();
    for (int id : ids)
        minVal = std::min(minVal, id);

    std::vector<int> best;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] != minVal)
            continue;

        std::vector<int> rotated;
        rotated.reserve(ids.size());
        for (std::size_t j = 0; j < ids.size(); ++j)
            rotated.push_back(ids[(i + j) % ids.size()]);

        if (best.empty() || rotated < best)
            best = std::move(rotated);
    }

    return best;
}

std::string polygonKey(const std::vector<int> &ids) {
    const std::vector<int> canonical = canonicalizePolygon(ids);
    if (canonical.empty())
        return {};

    std::string key;
    for (int id : canonical) {
        key += std::to_string(id);
        key.push_back(':');
    }

    return key;
}


} // namespace ImageAnalysis
