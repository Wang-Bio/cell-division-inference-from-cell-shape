#ifndef IMAGEANALYSIS_H
#define IMAGEANALYSIS_H

#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace ImageAnalysis {

// An enclosed background component is treated as a tissue void, rather than a
// normal cell face, when it is at least this many times the median enclosed
// face area. This lets internal void perimeters participate in outer-boundary
// vertex detection without classifying every cell wall as an outer boundary.
constexpr double kInternalVoidMedianAreaFactor = 4.0;
constexpr int kMinimumInternalVoidAreaPixels = 16;

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

enum class DetectedVertexKind { InteriorJunction, BoundaryJunction, ContourSupport, Ambiguous };

struct OuterDetectionParameters {
    double contourSampleSpacing = 1.0;
    int outerJunctionNeighborhood = 3;
    int frameGuard = 2;
    double junctionMergeRadius = 2.0;
    double anchorContourTolerance = 4.0;
    double curvatureSigma = 2.0;
    int curvatureWindow = 2;
    double curvatureThresholdDegrees = 5.0;
    int curvatureNmsRadius = 8;
    double fitSigma = 2.5;
    double maximumFitError = 1.0;
    double maxAreaErrorFraction = 0.02;
    double consensusRadius = 2.5;
    double junctionExclusion = 3.0;
    int skeletonSnapRadius = 3;
};

struct VertexDetection {
    cv::Point2d position;
    DetectedVertexKind kind = DetectedVertexKind::InteriorJunction;
    int contourId = -1;
    int arcId = -1;
};

struct VertexDetectionResult {
    std::vector<VertexDetection> vertices;
    std::vector<std::string> diagnostics;
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

// Classify branch and region-topology junctions, then find geometric supports
// by curvature/Imai-Iri consensus independently on each anchored outer-cell arc.
// Ordinary cell faces are excluded; unusually large tissue voids are included.
VertexDetectionResult detectCellArcVertices(const cv::Mat &skeletonImage,
        const OuterDetectionParameters &parameters = OuterDetectionParameters());
bool isOuterBoundaryPoint(const cv::Mat &skeletonImage, const cv::Point2d &point);

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
