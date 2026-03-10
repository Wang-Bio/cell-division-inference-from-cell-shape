#ifndef IMAGEANALYSIS_H
#define IMAGEANALYSIS_H

#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace ImageAnalysis {

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

// Detect skeleton vertices: pixels with more than three 8-neighbors.
// Expects a single-channel (CV_8UC1) skeleton image where foreground pixels are non-zero.
// Returns zero-based pixel coordinates for each detected vertex.
std::vector<cv::Point2d> detectVertices(const cv::Mat &skeletonImage);

// Detect lines along the skeleton that connect clustered vertices. Starting from each
// vertex (with a tolerance radius of 1 pixel), track along the skeleton in every
// possible direction. When the traversal reaches another vertex within the tolerance,
// a connection between the two vertices is reported along with the traversed path.
// Each undirected connection is reported once.
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
