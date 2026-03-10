#ifndef NEIGHBORGEOMETRYCALCULATOR_H
#define NEIGHBORGEOMETRYCALCULATOR_H

#include <QVector>
#include <QString>
#include <limits>

class PolygonItem;
class QWidget;

struct NeighborPairGeometrySettings{
    bool computeAreaRatio = true;
    bool computeAreaStats = true;
    bool computePerimeterRatio = true;
    bool computePerimeterStats = true;
    bool computeAspectRatio = true;
    bool computeAspectRatioStats = true;
    bool computeCircularityRatio = true;
    bool computeCircularityStats = true;
    bool computeSolidityRatio = true;
    bool computeSolidityStats = true;
    bool computeVertexCountRatio = true;
    bool computeVertexCountStats = true;
    bool computeCentroidDistance = true;

    bool computeUnionAspectRatio = true;
    bool computeUnionCircularity = true;
    bool computeUnionConvexDeficiency = true;

    bool computeNormalizedSharedEdgeLength = true;
    bool computeSharedEdgeUnsharedVerticesDistance = true;
    bool computeSharedEdgeUnsharedVerticesDistanceNormalized = true;
    bool computeCentroidSharedEdgeDistance = true;
    bool computeCentroidSharedEdgeDistanceNormalized = true;
    bool computeJunctionAngleAverage = true;
    bool computeJunctionAngleMax = true;
    bool computeJunctionAngleMin = true;
    bool computeJunctionAngleDifference = true;
    bool computeJunctionAngleRatio = true;
    bool computeSharedEdgeUnionCentroidDistance = true;
    bool computeSharedEdgeUnionCentroidDistanceNormalized = true;
    bool computeSharedEdgeUnionAxisAngle = true;
};

class NeighborPairGeometryCalculator
{
public:

    struct Result{
        PolygonItem *first = nullptr;
        PolygonItem *second = nullptr;

        double areaRatio = -1.0;
        double areaMean = std::numeric_limits<double>::quiet_NaN();
        double areaMin = std::numeric_limits<double>::quiet_NaN();
        double areaMax = std::numeric_limits<double>::quiet_NaN();
        double areaDiff = std::numeric_limits<double>::quiet_NaN();
        double perimeterRatio = -1.0;
        double perimeterMean = std::numeric_limits<double>::quiet_NaN();
        double perimeterMin = std::numeric_limits<double>::quiet_NaN();
        double perimeterMax = std::numeric_limits<double>::quiet_NaN();
        double perimeterDiff = std::numeric_limits<double>::quiet_NaN();
        double aspectRatio = -1.0;
        double aspectMean = std::numeric_limits<double>::quiet_NaN();
        double aspectMin = std::numeric_limits<double>::quiet_NaN();
        double aspectMax = std::numeric_limits<double>::quiet_NaN();
        double aspectDiff = std::numeric_limits<double>::quiet_NaN();
        double circularityRatio = -1.0;
        double circularityMean = std::numeric_limits<double>::quiet_NaN();
        double circularityMin = std::numeric_limits<double>::quiet_NaN();
        double circularityMax = std::numeric_limits<double>::quiet_NaN();
        double circularityDiff = std::numeric_limits<double>::quiet_NaN();
        double solidityRatio = -1.0;
        double solidityMean = std::numeric_limits<double>::quiet_NaN();
        double solidityMin = std::numeric_limits<double>::quiet_NaN();
        double solidityMax = std::numeric_limits<double>::quiet_NaN();
        double solidityDiff = std::numeric_limits<double>::quiet_NaN();
        double vertexCountRatio = -1.0;
        double vertexCountMean = std::numeric_limits<double>::quiet_NaN();
        double vertexCountMin = std::numeric_limits<double>::quiet_NaN();
        double vertexCountMax = std::numeric_limits<double>::quiet_NaN();
        double vertexCountDiff = std::numeric_limits<double>::quiet_NaN();
        double centroidDistance = -1.0;

        double unionAspectRatio = std::numeric_limits<double>::quiet_NaN();
        double unionCircularity = std::numeric_limits<double>::quiet_NaN();
        double unionConvexDeficiency = std::numeric_limits<double>::quiet_NaN();

        double normalizedSharedEdgeLength = -1.0;
        double sharedEdgeLength = std::numeric_limits<double>::quiet_NaN();
        double sharedEdgeUnsharedVerticesDistance = std::numeric_limits<double>::quiet_NaN();
        double sharedEdgeUnsharedVerticesDistanceNormalized = std::numeric_limits<double>::quiet_NaN();
        double centroidSharedEdgeDistance = std::numeric_limits<double>::quiet_NaN();
        double centroidSharedEdgeDistanceNormalized = std::numeric_limits<double>::quiet_NaN();
        double centroidDistanceNormalized = std::numeric_limits<double>::quiet_NaN();
        double junctionAngleAverageDegrees = std::numeric_limits<double>::quiet_NaN();
        double junctionAngleMaxDegrees = std::numeric_limits<double>::quiet_NaN();
        double junctionAngleMinDegrees = std::numeric_limits<double>::quiet_NaN();
        double junctionAngleDifferenceDegrees = std::numeric_limits<double>::quiet_NaN();
        double junctionAngleRatio = std::numeric_limits<double>::quiet_NaN();
        double sharedEdgeUnionCentroidDistance = std::numeric_limits<double>::quiet_NaN();
        double sharedEdgeUnionCentroidDistanceNormalized = std::numeric_limits<double>::quiet_NaN();
        double sharedEdgeUnionAxisAngleDegrees = std::numeric_limits<double>::quiet_NaN();
    };

    static NeighborPairGeometrySettings currentSettings();
    static void setCurrentSettings(const NeighborPairGeometrySettings &settings);
    static bool showSettingsDialog(QWidget *parent);

    static Result calculateForPair(PolygonItem* first, PolygonItem *second, const NeighborPairGeometrySettings &settings);
    static void showResultsDialog(const QVector<Result> &results, const NeighborPairGeometrySettings &settings, QWidget *parent);
    static QString formatResultSummary(const QVector<Result> &results, const NeighborPairGeometrySettings &settings);

};

#endif // NEIGHBORGEOMETRYCALCULATOR_H
