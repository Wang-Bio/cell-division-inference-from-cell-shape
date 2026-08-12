#ifndef IMAGEANALYSIS_H
#define IMAGEANALYSIS_H

#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace ImageAnalysis {

// Minimum separation used by both automatic boundary-junction and contour-
// support placement.  The scale term is 0.08*S, where the no-face fallback
// S is 0.05 of the image diagonal.
constexpr double kMinimumOuterVertexSpacingPixels = 2.0;
constexpr double kOuterVertexSpacingScaleFraction = 0.04;
constexpr double kFallbackCharacteristicSizeDiagonalFraction = 0.05;

struct LineConnection {
    int startVertexIndex = -1;
    int endVertexIndex = -1;
    std::vector<cv::Point> path; // ordered pixels from start vertex to end vertex
};

struct VertexGeometry {
    int id = -1;
    cv::Point2d position;
};

struct GraphEdge {
    int startId = -1;
    int endId = -1;
};


// Convert input image to a single-channel grayscale representation.
cv::Mat toGrayscale(const cv::Mat &input);

// Threshold a grayscale image into a binary mask (0 or 255 values).
// Pass a negative threshold to enable automatic Otsu selection.
// If invert is true, the threshold is inverted so darker structures become white foreground.
cv::Mat binarize(const cv::Mat &grayscale, double threshold = -1.0, bool invert = false);

// Perform Guo-Hall thinning on a binary image (expects 0/255 values) and return the skeleton.
cv::Mat guoHallThinning(const cv::Mat &binaryInput);

//Destairing to make the skeleton 1-pixel thickness
cv::Mat destairSkeleton(const cv::Mat &skeletonImage);

// Remove spur pixels (with only one 8-neighbor) iteratively until no spurs remain.
cv::Mat removeSpurs(const cv::Mat &skeletonImage);

// High-level helper: convert the input image to grayscale, binarize it, and thin it using Guo-Hall.
cv::Mat segmentWithGuoHall(const cv::Mat &input, double threshold = -1.0, bool invertInput = true);

// Detect three- or four-branch skeleton vertices from crossing number and local branch topology.
// The local check recovers acute or stair-stepped junctions whose arms merge
// in the immediate 8-neighbour ring.
// Expects a single-channel (CV_8UC1) skeleton image where foreground pixels are non-zero.
// Returns zero-based pixel coordinates for each detected vertex.
std::vector<cv::Point2d> detectVertices(const cv::Mat &skeletonImage);

// Trace each disconnected foreground network as an independent outer polygon
// and return its simplified contour supports. Internal wall junctions do not
// invalidate a contour, and a frame-truncated or tiny network is skipped
// without discarding supports from other valid networks.
std::vector<cv::Point2d> detectOuterContourSupport(const cv::Mat &skeletonImage,
                                                   std::string *warning = nullptr);
bool isOuterBoundaryPoint(const cv::Mat &skeletonImage, const cv::Point2d &point);
double outerVertexSpacingThreshold(const cv::Size &imageSize, double characteristicSize = -1.0);

// Detect neighboring vertices with a multi-source traversal of the skeleton. Vertex
// labels act as barriers, while meeting wave fronts identify adjacent outline segments.
// Each undirected connection is reported once with its ordered skeleton path.
std::vector<LineConnection> detectLines(const cv::Mat &skeletonImage, const std::vector<cv::Point> &vertices, int tolerance = 1);

// Detect polygons by walking connected edges counterclockwise. Vertices are
// identified by their integer ids, and edges describe the graph connectivity.
// Returned polygons are represented as ordered vertex id loops (without
// repeating the start vertex at the end). Duplicate traversal directions are
// avoided using visited directed edges.
std::vector<std::vector<int>> detectPolygonsCCW(const std::vector<VertexGeometry> &vertices,
                                                const std::vector<GraphEdge> &edges);

// Rotate polygon vertex ids to a canonical lexicographic starting point
// (smallest vertex id, earliest rotation). Useful for deduplication.
std::vector<int> canonicalizePolygon(const std::vector<int> &ids);

// Generate a string key for a polygon using its canonicalized vertex order.
std::string polygonKey(const std::vector<int> &ids);


} // namespace ImageAnalysis

#endif // IMAGEANALYSIS_H
