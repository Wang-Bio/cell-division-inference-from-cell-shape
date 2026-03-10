#include "neighborgeometrycalculator.h"

#include "neighborpair.h"
#include "polygonItem.h"
#include "lineitem.h"
#include "vertexitem.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QStringList>
#include <algorithm>
#include <QPainter>
#include <QPixmap>
#include <QTableWidget>
#include <QHeaderView>
#include <QtMath>
#include <limits>
#include <cmath>
#include <QPainterPath>
#include <QSet>
#include <QLineF>
#include <utility>
#include <memory>

static NeighborPairGeometrySettings g_neighborPairGeometrySettings;

NeighborPairGeometrySettings NeighborPairGeometryCalculator::currentSettings(){
    return g_neighborPairGeometrySettings;
}

void NeighborPairGeometryCalculator::setCurrentSettings(const NeighborPairGeometrySettings &settings)
{
    g_neighborPairGeometrySettings = settings;
}

QPolygonF polygonToScene(const PolygonItem *item);
QPixmap buildContactPreviewPixmap(const PolygonItem *a, const PolygonItem *b);

bool NeighborPairGeometryCalculator::showSettingsDialog(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Geometry Calculation Settings");

    const NeighborPairGeometrySettings current = NeighborPairGeometryCalculator::currentSettings();

    auto *areaCheck = new QCheckBox("Calculate area ratio (smaller/bigger", &dialog);
    areaCheck->setChecked(current.computeAreaRatio);

    auto *areaStatsCheck = new QCheckBox("Calculate area statistics (mean/min/max/diff)", &dialog);
    areaStatsCheck->setChecked(current.computeAreaStats);

    auto *perimeterCheck = new QCheckBox("Calculate perimeter ratio (smaller/bigger", &dialog);
    perimeterCheck->setChecked(current.computePerimeterRatio);

    auto *perimeterStatsCheck = new QCheckBox("Calculate perimeter statistics (mean/min/max/diff)", &dialog);
    perimeterStatsCheck->setChecked(current.computePerimeterStats);

    auto *aspectRatioCheck = new QCheckBox("Calculate aspect ratio (smaller/bigger)", &dialog);
    aspectRatioCheck->setChecked(current.computeAspectRatio);

    auto *aspectStatsCheck = new QCheckBox("Calculate aspect ratio statistics (mean/min/max/diff)", &dialog);
    aspectStatsCheck->setChecked(current.computeAspectRatioStats);

    auto *circularityCheck = new QCheckBox("Calculate circularity ratio (smaller/bigger)", &dialog);
    circularityCheck->setChecked(current.computeCircularityRatio);

    auto *circularityStatsCheck = new QCheckBox("Calculate circularity statistics (mean/min/max/diff)", &dialog);
    circularityStatsCheck->setChecked(current.computeCircularityStats);

    auto *solidityCheck = new QCheckBox("Calculate solidity ratio (smaller/bigger)", &dialog);
    solidityCheck->setChecked(current.computeSolidityRatio);

    auto *solidityStatsCheck = new QCheckBox("Calculate solidity statistics (mean/min/max/diff)", &dialog);
    solidityStatsCheck->setChecked(current.computeSolidityStats);

    auto *vertexCountCheck = new QCheckBox("Calculate vertex count ratio (smaller/bigger)", &dialog);
    vertexCountCheck->setChecked(current.computeVertexCountRatio);

    auto *vertexCountStatsCheck = new QCheckBox("Calculate vertex count statistics (mean/min/max/diff)", &dialog);
    vertexCountStatsCheck->setChecked(current.computeVertexCountStats);

    auto *centroidDistanceCheck = new QCheckBox("Calculate centroid-centroid distance", &dialog);
    centroidDistanceCheck->setChecked(current.computeCentroidDistance);

    auto *unionAspectRatioCheck = new QCheckBox("Calculate aspect ratio of union", &dialog);
    unionAspectRatioCheck->setChecked(current.computeUnionAspectRatio);

    auto *unionCircularityCheck = new QCheckBox("Calculate circularity of union", &dialog);
    unionCircularityCheck->setChecked(current.computeUnionCircularity);

    auto *unionConvexDeficiencyCheck = new QCheckBox("Calculate convex deficiency of union", &dialog);
    unionConvexDeficiencyCheck->setChecked(current.computeUnionConvexDeficiency);

    auto *normalizedSharedEdgeCheck = new QCheckBox("Calculate normalized length of shared edge", &dialog);
    normalizedSharedEdgeCheck->setChecked(current.computeNormalizedSharedEdgeLength);

    auto *sharedEdgeUnsharedVerticesCheck = new QCheckBox("Distance between shared edge and unshared vertices", &dialog);
    sharedEdgeUnsharedVerticesCheck->setChecked(current.computeSharedEdgeUnsharedVerticesDistance);

    auto *sharedEdgeUnsharedVerticesNormCheck = new QCheckBox("Normalized distance between shared edge and unshared vertices", &dialog);
    sharedEdgeUnsharedVerticesNormCheck->setChecked(current.computeSharedEdgeUnsharedVerticesDistanceNormalized);

    auto *centroidSharedEdgeCheck = new QCheckBox("Centroid-shared edge distance", &dialog);
    centroidSharedEdgeCheck->setChecked(current.computeCentroidSharedEdgeDistance);

    auto *centroidSharedEdgeNormCheck = new QCheckBox("Normalized centroid-shared edge distance", &dialog);
    centroidSharedEdgeNormCheck->setChecked(current.computeCentroidSharedEdgeDistanceNormalized);

    auto *junctionAngleAverageCheck = new QCheckBox("Calculate average junction angle (degrees)", &dialog);
    junctionAngleAverageCheck->setChecked(current.computeJunctionAngleAverage);

    auto *junctionAngleMaxCheck = new QCheckBox("Calculate max junction angle (degrees)", &dialog);
    junctionAngleMaxCheck->setChecked(current.computeJunctionAngleMax);

    auto *junctionAngleMinCheck = new QCheckBox("Calculate min junction angle (degrees)", &dialog);
    junctionAngleMinCheck->setChecked(current.computeJunctionAngleMin);

    auto *junctionAngleDiffCheck = new QCheckBox("Calculate junction angle difference (deg, big - small)", &dialog);
    junctionAngleDiffCheck->setChecked(current.computeJunctionAngleDifference);

    auto *junctionAngleRatioCheck = new QCheckBox("Calculate junction angle ratio (small/big)", &dialog);
    junctionAngleRatioCheck->setChecked(current.computeJunctionAngleRatio);

    auto *edgeCentroidDistanceCheck = new QCheckBox("Calculate distance between shared edge and union centroid", &dialog);
    edgeCentroidDistanceCheck->setChecked(current.computeSharedEdgeUnionCentroidDistance);

    auto *edgeCentroidDistanceNormCheck = new QCheckBox("Calculate normalized distance between shared edge and union centroid", &dialog);
    edgeCentroidDistanceNormCheck->setChecked(current.computeSharedEdgeUnionCentroidDistanceNormalized);

    auto *edgeUnionAxisAngleCheck = new QCheckBox("Calculate angle between shared edge and union longest axis", &dialog);
    edgeUnionAxisAngleCheck->setChecked(current.computeSharedEdgeUnionAxisAngle);

    auto *infoLabel = new QLabel("Disable an optoin to skip that measurement during neighbor pair calculation.", &dialog);
    infoLabel->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(infoLabel);
    layout->addWidget(areaCheck);
    layout->addWidget(areaStatsCheck);
    layout->addWidget(perimeterCheck);
    layout->addWidget(perimeterStatsCheck);
    layout->addWidget(aspectRatioCheck);
    layout->addWidget(aspectStatsCheck);
    layout->addWidget(circularityCheck);
    layout->addWidget(circularityStatsCheck);
    layout->addWidget(solidityCheck);
    layout->addWidget(solidityStatsCheck);
    layout->addWidget(vertexCountCheck);
    layout->addWidget(vertexCountStatsCheck);
    layout->addWidget(centroidDistanceCheck);

    layout->addWidget(unionAspectRatioCheck);
    layout->addWidget(unionCircularityCheck);
    layout->addWidget(unionConvexDeficiencyCheck);

    layout->addWidget(normalizedSharedEdgeCheck);
    layout->addWidget(sharedEdgeUnsharedVerticesCheck);
    layout->addWidget(sharedEdgeUnsharedVerticesNormCheck);
    layout->addWidget(centroidSharedEdgeCheck);
    layout->addWidget(centroidSharedEdgeNormCheck);
    layout->addWidget(junctionAngleAverageCheck);
    layout->addWidget(junctionAngleMaxCheck);
    layout->addWidget(junctionAngleMinCheck);
    layout->addWidget(junctionAngleDiffCheck);
    layout->addWidget(junctionAngleRatioCheck);
    layout->addWidget(edgeCentroidDistanceCheck);
    layout->addWidget(edgeCentroidDistanceNormCheck);
    layout->addWidget(edgeUnionAxisAngleCheck);

    layout->addWidget(buttons);

    if(dialog.exec() != QDialog::Accepted) return false;

    NeighborPairGeometrySettings updated;
    updated.computeAreaRatio = areaCheck->isChecked();
    updated.computeAreaStats = areaStatsCheck->isChecked();
    updated.computePerimeterRatio = perimeterCheck->isChecked();
    updated.computePerimeterStats = perimeterStatsCheck->isChecked();
    updated.computeAspectRatio = aspectRatioCheck->isChecked();
    updated.computeAspectRatioStats = aspectStatsCheck->isChecked();
    updated.computeCircularityRatio = circularityCheck->isChecked();
    updated.computeCircularityStats = circularityStatsCheck->isChecked();
    updated.computeSolidityRatio = solidityCheck->isChecked();
    updated.computeSolidityStats = solidityStatsCheck->isChecked();
    updated.computeVertexCountRatio = vertexCountCheck->isChecked();
    updated.computeVertexCountStats = vertexCountStatsCheck->isChecked();
    updated.computeCentroidDistance = centroidDistanceCheck->isChecked();

    updated.computeUnionAspectRatio = unionAspectRatioCheck->isChecked();
    updated.computeUnionCircularity = unionCircularityCheck->isChecked();
    updated.computeUnionConvexDeficiency = unionConvexDeficiencyCheck->isChecked();

    updated.computeNormalizedSharedEdgeLength = normalizedSharedEdgeCheck->isChecked();
    updated.computeSharedEdgeUnsharedVerticesDistance = sharedEdgeUnsharedVerticesCheck->isChecked();
    updated.computeSharedEdgeUnsharedVerticesDistanceNormalized = sharedEdgeUnsharedVerticesNormCheck->isChecked();
    updated.computeCentroidSharedEdgeDistance = centroidSharedEdgeCheck->isChecked();
    updated.computeCentroidSharedEdgeDistanceNormalized = centroidSharedEdgeNormCheck->isChecked();
    updated.computeJunctionAngleAverage = junctionAngleAverageCheck->isChecked();
    updated.computeJunctionAngleMax = junctionAngleMaxCheck->isChecked();
    updated.computeJunctionAngleMin = junctionAngleMinCheck->isChecked();
    updated.computeJunctionAngleDifference = junctionAngleDiffCheck->isChecked();
    updated.computeJunctionAngleRatio = junctionAngleRatioCheck->isChecked();
    updated.computeSharedEdgeUnionCentroidDistance = edgeCentroidDistanceCheck->isChecked();
    updated.computeSharedEdgeUnionCentroidDistanceNormalized = edgeCentroidDistanceNormCheck->isChecked();
    updated.computeSharedEdgeUnionAxisAngle = edgeUnionAxisAngleCheck->isChecked();

    NeighborPairGeometryCalculator::setCurrentSettings(updated);
    return true;
}

NeighborPairGeometryCalculator::Result NeighborPairGeometryCalculator::calculateForPair(PolygonItem *first, PolygonItem* second, const NeighborPairGeometrySettings &settings)
{
    Result result;
    result.first = first;
    result.second = second;

    if(!first || !second) return result;

    const auto safeRatio = [](double a, double b) {
        if (!std::isfinite(a) || !std::isfinite(b) || a <= 0.0 || b <= 0.0)
            return -1.0;
        const double maxVal = std::max(a, b);
        if (maxVal <= 0.0)
            return -1.0;
        return std::min(a, b) / maxVal;
    };

    const auto aspectRatioForPolygon = [](const PolygonItem *item) {
        if (!item)
            return -1.0;
        const QRectF bounds = item->polygon().boundingRect();
        if (bounds.height() <= 0.0 || bounds.width() <= 0.0)
            return -1.0;
        const double ratio = bounds.width() / bounds.height();
        return ratio >= 1.0 ? ratio : 1.0 / ratio;
    };

    const auto circularityForPolygon = [](const PolygonItem *item) {
        if (!item)
            return -1.0;
        const double area = item->area();
        const double per = item->perimeter();
        if (area <= 0.0 || per <= 0.0)
            return -1.0;
        return (4.0 * M_PI * area) / (per * per);
    };

    const auto cross = [](const QPointF &a, const QPointF &b, const QPointF &c) {
        return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
    };

    const auto convexHull = [&cross](const QPolygonF &points) {
        QVector<QPointF> pts(points.begin(), points.end());
        if (pts.size() < 3)
            return QPolygonF(pts);

        std::sort(pts.begin(), pts.end(), [](const QPointF &lhs, const QPointF &rhs) {
            if (lhs.x() == rhs.x())
                return lhs.y() < rhs.y();
            return lhs.x() < rhs.x();
        });

        QVector<QPointF> lower;
        for (const QPointF &p : pts) {
            while (lower.size() >= 2 && cross(lower[lower.size() - 2], lower.last(), p) <= 0)
                lower.removeLast();
            lower.append(p);
        }

        QVector<QPointF> upper;
        for (int i = pts.size() - 1; i >= 0; --i) {
            const QPointF &p = pts[i];
            while (upper.size() >= 2 && cross(upper[upper.size() - 2], upper.last(), p) <= 0)
                upper.removeLast();
            upper.append(p);
        }

        lower.pop_back();
        upper.pop_back();
        lower += upper;
        return QPolygonF(lower);
    };

    const auto polygonArea = [](const QPolygonF &p) {
        const int n = p.size();
        if (n < 3)
            return 0.0;

        double area = 0.0;
        for (int i = 0; i < n; ++i) {
            const QPointF &a = p[i];
            const QPointF &b = p[(i + 1) % n];
            area += a.x() * b.y() - b.x() * a.y();
        }
        return std::abs(area) * 0.5;
    };

    const auto solidityForPolygon = [&convexHull, &polygonArea](const PolygonItem *item) {
        if (!item)
            return -1.0;
        const QPolygonF poly = item->polygon();
        if (poly.isEmpty())
            return -1.0;

        const double area = item->area();
        const double hullArea = polygonArea(convexHull(poly));
        if (area <= 0.0 || hullArea <= 0.0)
            return -1.0;
        return area / hullArea;
    };

    const auto polygonCentroid = [&polygonArea](const QPolygonF &p) {
        const int n = p.size();
        if (n < 3)
            return QPointF();

        double signedArea = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        for (int i = 0; i < n; ++i) {
            const QPointF &a = p[i];
            const QPointF &b = p[(i + 1) % n];
            const double cross = a.x() * b.y() - b.x() * a.y();
            signedArea += cross;
            cx += (a.x() + b.x()) * cross;
            cy += (a.y() + b.y()) * cross;
        }

        signedArea *= 0.5;
        if (std::abs(signedArea) < 1e-9)
            return QPointF();

        cx /= (6.0 * signedArea);
        cy /= (6.0 * signedArea);
        return QPointF(cx, cy);
    };

    const auto principalAxisForPoints = [](const QVector<QPointF> &pts) {
        if (pts.size() < 2)
            return QPointF();

        double meanX = 0.0;
        double meanY = 0.0;
        for (const QPointF &p : pts) {
            meanX += p.x();
            meanY += p.y();
        }
        meanX /= pts.size();
        meanY /= pts.size();

        double covXX = 0.0;
        double covXY = 0.0;
        double covYY = 0.0;
        for (const QPointF &p : pts) {
            const double dx = p.x() - meanX;
            const double dy = p.y() - meanY;
            covXX += dx * dx;
            covXY += dx * dy;
            covYY += dy * dy;
        }

        const double a = covXX;
        const double b = covXY;
        const double c = covYY;
        const double trace = a + c;
        const double detTerm = std::sqrt((a - c) * (a - c) + 4.0 * b * b);
        const double lambdaMax = 0.5 * (trace + detTerm);

        QPointF eigenVec(lambdaMax - c, b);
        if (std::abs(eigenVec.x()) < 1e-9 && std::abs(eigenVec.y()) < 1e-9)
            eigenVec = QPointF(b, lambdaMax - a);

        const double mag = std::hypot(eigenVec.x(), eigenVec.y());
        if (mag <= 0.0)
            return QPointF();
        return QPointF(eigenVec.x() / mag, eigenVec.y() / mag);
    };

    const bool computeAnyJunctionAngle = settings.computeJunctionAngleAverage
                                         || settings.computeJunctionAngleMax
                                         || settings.computeJunctionAngleMin
                                         || settings.computeJunctionAngleDifference
                                         || settings.computeJunctionAngleRatio;

    QPainterPath unionPath;
    double unionArea = std::numeric_limits<double>::quiet_NaN();
    double unionPerimeter = std::numeric_limits<double>::quiet_NaN();
    QPointF unionCentroid;
    bool hasUnionCentroid = false;
    QPointF unionPrincipalAxis;
    bool hasUnionPrincipalAxis = false;

    const bool needsUnionGeometry = settings.computeUnionAspectRatio
                                    || settings.computeUnionCircularity
                                    || settings.computeUnionConvexDeficiency
                                    || settings.computeSharedEdgeUnionCentroidDistance
                                    || settings.computeSharedEdgeUnionCentroidDistanceNormalized
                                    || settings.computeSharedEdgeUnionAxisAngle;

    if (needsUnionGeometry) {
        const QPolygonF polyA = polygonToScene(first);
        const QPolygonF polyB = polygonToScene(second);

        if (!polyA.isEmpty())
            unionPath.addPolygon(polyA);
        if (!polyB.isEmpty()) {
            QPainterPath pathB;
            pathB.addPolygon(polyB);
            unionPath = unionPath.isEmpty() ? pathB : unionPath.united(pathB);
        }

        if (!unionPath.isEmpty()) {
            const auto perimeterForPath = [](const QPainterPath &path) {
                if (path.isEmpty())
                    return 0.0;

                double perimeter = 0.0;
                const QPainterPath simplified = path.simplified();
                const auto polygons = simplified.toFillPolygons();
                for (const QPolygonF &p : polygons) {
                    const int n = p.size();
                    for (int i = 0; i < n; ++i)
                        perimeter += QLineF(p[i], p[(i + 1) % n]).length();
                }
                return perimeter;
            };

            const auto polygons = unionPath.simplified().toFillPolygons();
            unionArea = 0.0;
            QPointF centroidAccum(0.0, 0.0);
            QVector<QPointF> unionPoints;
            for (const QPolygonF &poly : polygons) {
                const double area = polygonArea(poly);
                unionArea += area;
                if (area > 0.0) {
                    centroidAccum += polygonCentroid(poly) * area;
                }
                for (const QPointF &p : poly)
                    unionPoints.append(p);
            }
            unionPerimeter = perimeterForPath(unionPath);

            if (unionArea > 0.0) {
                unionCentroid = QPointF(centroidAccum.x() / unionArea, centroidAccum.y() / unionArea);
                hasUnionCentroid = true;
            }

            const QPointF axis = principalAxisForPoints(unionPoints);
            if (!qFuzzyIsNull(axis.x()) || !qFuzzyIsNull(axis.y())) {
                unionPrincipalAxis = axis;
                hasUnionPrincipalAxis = true;
            }
        }
    }

    const double areaA = first->area();
    const double areaB = second->area();
    const double avgSqrtArea = 0.5 * (std::sqrt(areaA) + std::sqrt(areaB));

    const double perimeterA = first ? first->perimeter() : std::numeric_limits<double>::quiet_NaN();
    const double perimeterB = second ? second->perimeter() : std::numeric_limits<double>::quiet_NaN();

    const double aspectA = aspectRatioForPolygon(first);
    const double aspectB = aspectRatioForPolygon(second);

    const double circA = circularityForPolygon(first);
    const double circB = circularityForPolygon(second);

    const double solA = solidityForPolygon(first);
    const double solB = solidityForPolygon(second);

    const double vertexCountA = first ? double(first->polygon().size()) : std::numeric_limits<double>::quiet_NaN();
    const double vertexCountB = second ? double(second->polygon().size()) : std::numeric_limits<double>::quiet_NaN();

    const bool needsNeighborPair = settings.computeNormalizedSharedEdgeLength
                                   || settings.computeSharedEdgeUnsharedVerticesDistance
                                   || settings.computeSharedEdgeUnsharedVerticesDistanceNormalized
                                   || settings.computeCentroidSharedEdgeDistance
                                   || settings.computeCentroidSharedEdgeDistanceNormalized
                                   || settings.computeSharedEdgeUnionCentroidDistance
                                   || settings.computeSharedEdgeUnionCentroidDistanceNormalized
                                   || settings.computeSharedEdgeUnionAxisAngle
                                   || computeAnyJunctionAngle;

    std::unique_ptr<NeighborPair> pair;
    if (needsNeighborPair)
        pair = std::make_unique<NeighborPair>(first, second);

    double sharedEdgeTotalLength = 0.0;
    double recordedSharedEdgeLength = std::numeric_limits<double>::quiet_NaN();
    QLineF longestSharedEdge;
    VertexItem *longestSharedV1 = nullptr;
    VertexItem *longestSharedV2 = nullptr;
    bool hasSharedEdge = false;
    if (pair) {
        for (const NeighborPair::EdgeInfo &edge : pair->sharedEdges()) {
            if (!edge.v1 || !edge.v2)
                continue;
            const QLineF edgeLine(edge.v1->scenePos(), edge.v2->scenePos());
            const double len = edgeLine.length();
            sharedEdgeTotalLength += len;
            if (len > longestSharedEdge.length()) {
                longestSharedEdge = edgeLine;
                longestSharedV1 = edge.v1;
                longestSharedV2 = edge.v2;
                hasSharedEdge = true;
            }
        }
    }

    const auto pointToSegmentDistance = [](const QPointF &p, const QLineF &seg) {
        if (seg.length() <= 0.0)
            return std::numeric_limits<double>::quiet_NaN();

        const QPointF a = seg.p1();
        const QPointF b = seg.p2();
        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();
        const double denom = dx * dx + dy * dy;
        if (denom <= 0.0)
            return std::numeric_limits<double>::quiet_NaN();

        const double t = ((p.x() - a.x()) * dx + (p.y() - a.y()) * dy) / denom;
        const double clampedT = std::clamp(t, 0.0, 1.0);
        const QPointF proj(a.x() + clampedT * dx, a.y() + clampedT * dy);
        return std::hypot(p.x() - proj.x(), p.y() - proj.y());
    };

    double sharedEdgeUnsharedVerticesDistance = std::numeric_limits<double>::quiet_NaN();
    double centroidSharedEdgeDistance = std::numeric_limits<double>::quiet_NaN();

    if (hasSharedEdge && longestSharedEdge.length() > 0.0) {
        recordedSharedEdgeLength = sharedEdgeTotalLength;
        QVector<double> distances;
        distances.reserve((first ? first->vertices().size() : 0) + (second ? second->vertices().size() : 0));

        auto accumulateDistances = [&](const PolygonItem *poly) {
            if (!poly)
                return;

            for (VertexItem *v : poly->vertices()) {
                if (!v)
                    continue;
                if (v == longestSharedV1 || v == longestSharedV2)
                    continue;
                const double dist = pointToSegmentDistance(v->scenePos(), longestSharedEdge);
                if (std::isfinite(dist))
                    distances.append(dist);
            }
        };

        accumulateDistances(first);
        accumulateDistances(second);

        if (!distances.isEmpty()) {
            double sum = 0.0;
            for (double d : std::as_const(distances))
                sum += d;
            sharedEdgeUnsharedVerticesDistance = sum / distances.size();
        }

        if (first && second) {
            const QPointF centA = first->centroid();
            const QPointF centB = second->centroid();
            const double distA = pointToSegmentDistance(centA, longestSharedEdge);
            const double distB = pointToSegmentDistance(centB, longestSharedEdge);
            if (std::isfinite(distA) && std::isfinite(distB))
                centroidSharedEdgeDistance = 0.5 * (distA + distB);
        }
    }

    auto computeStats = [](double a, double b, double &mean, double &minVal, double &maxVal, double &diffVal) {
        if (!std::isfinite(a) || !std::isfinite(b))
            return;
        mean = 0.5 * (a + b);
        minVal = std::min(a, b);
        maxVal = std::max(a, b);
        diffVal = maxVal - minVal;
    };

    if(settings.computeAreaRatio){
        result.areaRatio = safeRatio(areaA, areaB);
    }

    if (settings.computeAreaStats)
        computeStats(areaA, areaB, result.areaMean, result.areaMin, result.areaMax, result.areaDiff);

    if(settings.computePerimeterRatio){
        result.perimeterRatio = safeRatio(perimeterA, perimeterB);
    }

    if (settings.computePerimeterStats)
        computeStats(perimeterA, perimeterB, result.perimeterMean, result.perimeterMin, result.perimeterMax, result.perimeterDiff);

    if (settings.computeAspectRatio) {
        result.aspectRatio = safeRatio(aspectA, aspectB);
    }

    if (settings.computeAspectRatioStats)
        computeStats(aspectA, aspectB, result.aspectMean, result.aspectMin, result.aspectMax, result.aspectDiff);

    if (settings.computeCircularityRatio) {
        result.circularityRatio = safeRatio(circA, circB);
    }

    if (settings.computeCircularityStats)
        computeStats(circA, circB, result.circularityMean, result.circularityMin, result.circularityMax, result.circularityDiff);

    if (settings.computeSolidityRatio) {
        result.solidityRatio = safeRatio(solA, solB);
    }

    if (settings.computeSolidityStats)
        computeStats(solA, solB, result.solidityMean, result.solidityMin, result.solidityMax, result.solidityDiff);

    if (settings.computeVertexCountRatio) {
        result.vertexCountRatio = safeRatio(vertexCountA, vertexCountB);
    }

    if (settings.computeVertexCountStats)
        computeStats(vertexCountA, vertexCountB, result.vertexCountMean, result.vertexCountMin, result.vertexCountMax, result.vertexCountDiff);

    if (settings.computeCentroidDistance) {
        const QPointF centA = first ? first->centroid() : QPointF();
        const QPointF centB = second ? second->centroid() : QPointF();
        if (first && second) {
            result.centroidDistance = QLineF(centA, centB).length();
            if (avgSqrtArea > 0.0 && std::isfinite(result.centroidDistance))
                result.centroidDistanceNormalized = result.centroidDistance / avgSqrtArea;
        }
    }

    if (settings.computeUnionAspectRatio) {
        const QRectF bounds = unionPath.boundingRect();
        if (!bounds.isNull() && bounds.width() > 0.0 && bounds.height() > 0.0) {
            const double ratio = bounds.width() / bounds.height();
            result.unionAspectRatio = ratio >= 1.0 ? ratio : 1.0 / ratio;
        }
    }

    if (settings.computeUnionCircularity) {
        if (unionArea > 0.0 && unionPerimeter > 0.0)
            result.unionCircularity = (4.0 * M_PI * unionArea) / (unionPerimeter * unionPerimeter);
    }

    if (settings.computeUnionConvexDeficiency) {
        const auto polygons = unionPath.simplified().toFillPolygons();
        QPolygonF combined;
        for (const QPolygonF &p : polygons)
            combined << p;

        if (!combined.isEmpty() && unionArea > 0.0) {
            const double hullArea = polygonArea(convexHull(combined));
            if (hullArea > 0.0)
                result.unionConvexDeficiency = (hullArea - unionArea) / unionArea;
        }
    }

    double sharedEdgeUnionCentroidDistance = std::numeric_limits<double>::quiet_NaN();
    if (hasUnionCentroid && hasSharedEdge && longestSharedEdge.length() > 0.0) {
        const QPointF p1 = longestSharedEdge.p1();
        const QPointF p2 = longestSharedEdge.p2();
        const double numerator = std::abs((p2.y() - p1.y()) * unionCentroid.x() - (p2.x() - p1.x()) * unionCentroid.y() + p2.x() * p1.y() - p2.y() * p1.x());
        sharedEdgeUnionCentroidDistance = numerator / longestSharedEdge.length();
    }

    if (settings.computeSharedEdgeUnionCentroidDistance && std::isfinite(sharedEdgeUnionCentroidDistance))
        result.sharedEdgeUnionCentroidDistance = sharedEdgeUnionCentroidDistance;

    if (settings.computeSharedEdgeUnionCentroidDistanceNormalized && std::isfinite(sharedEdgeUnionCentroidDistance) && avgSqrtArea > 0.0)
        result.sharedEdgeUnionCentroidDistanceNormalized = sharedEdgeUnionCentroidDistance / avgSqrtArea;

    if (settings.computeSharedEdgeUnionAxisAngle && hasSharedEdge && hasUnionPrincipalAxis && longestSharedEdge.length() > 0.0) {
        const QPointF edgeVec = longestSharedEdge.p2() - longestSharedEdge.p1();
        const double edgeMag = std::hypot(edgeVec.x(), edgeVec.y());
        const double axisMag = std::hypot(unionPrincipalAxis.x(), unionPrincipalAxis.y());
        if (edgeMag > 0.0 && axisMag > 0.0) {
            double dot = (edgeVec.x() * unionPrincipalAxis.x() + edgeVec.y() * unionPrincipalAxis.y()) / (edgeMag * axisMag);
            dot = std::clamp(dot, -1.0, 1.0);
            const double angle = std::acos(std::abs(dot));
            result.sharedEdgeUnionAxisAngleDegrees = qRadiansToDegrees(angle);
        }
    }

    if (settings.computeNormalizedSharedEdgeLength) {
        if (avgSqrtArea > 0.0 && sharedEdgeTotalLength > 0.0)
            result.normalizedSharedEdgeLength = sharedEdgeTotalLength / avgSqrtArea;
    }

    result.sharedEdgeLength = recordedSharedEdgeLength;

    if (settings.computeSharedEdgeUnsharedVerticesDistance && std::isfinite(sharedEdgeUnsharedVerticesDistance))
        result.sharedEdgeUnsharedVerticesDistance = sharedEdgeUnsharedVerticesDistance;

    if (settings.computeSharedEdgeUnsharedVerticesDistanceNormalized && std::isfinite(sharedEdgeUnsharedVerticesDistance) && avgSqrtArea > 0.0)
        result.sharedEdgeUnsharedVerticesDistanceNormalized = sharedEdgeUnsharedVerticesDistance / avgSqrtArea;

    if (settings.computeCentroidSharedEdgeDistance && std::isfinite(centroidSharedEdgeDistance))
        result.centroidSharedEdgeDistance = centroidSharedEdgeDistance;

    if (settings.computeCentroidSharedEdgeDistanceNormalized && std::isfinite(centroidSharedEdgeDistance) && avgSqrtArea > 0.0)
        result.centroidSharedEdgeDistanceNormalized = centroidSharedEdgeDistance / avgSqrtArea;

    if (computeAnyJunctionAngle && pair) {
        const QVector<VertexItem*> sharedVerts = pair->sharedVertices();
        if (!sharedVerts.isEmpty()) {
            QSet<int> sharedIds;
            for (VertexItem *v : sharedVerts) {
                if (v)
                    sharedIds.insert(v->id());
            }

            auto connectingVertexFor = [&sharedIds](const PolygonItem *poly, VertexItem *shared) -> VertexItem* {
                if (!poly || !shared)
                    return nullptr;

                const QVector<VertexItem*> verts = poly->vertices();
                if (verts.size() < 3)
                    return nullptr;

                int sharedIndex = -1;
                for (int i = 0; i < verts.size(); ++i) {
                    if (verts[i] == shared) {
                        sharedIndex = i;
                        break;
                    }
                }

                if (sharedIndex < 0)
                    return nullptr;

                const int n = verts.size();
                VertexItem *prev = verts[(sharedIndex - 1 + n) % n];
                VertexItem *next = verts[(sharedIndex + 1) % n];

                const bool prevShared = prev && sharedIds.contains(prev->id());
                const bool nextShared = next && sharedIds.contains(next->id());

                if (prevShared && !nextShared)
                    return next;
                if (nextShared && !prevShared)
                    return prev;

                return nullptr;
            };

            QVector<double> anglesDeg;
            for (VertexItem *shared : sharedVerts) {
                VertexItem *connA = connectingVertexFor(first, shared);
                VertexItem *connB = connectingVertexFor(second, shared);

                if (!connA || !connB)
                    continue;

                const QPointF base = shared->scenePos();
                const QPointF vecA = connA->scenePos() - base;
                const QPointF vecB = connB->scenePos() - base;

                const double crossProd = vecA.x() * vecB.y() - vecA.y() * vecB.x();
                const double dotProd = vecA.x() * vecB.x() + vecA.y() * vecB.y();
                const double angleRad = std::atan2(crossProd, dotProd);
                const double magnitude = std::abs(angleRad);
                anglesDeg.append(qRadiansToDegrees(magnitude));
            }

            if (!anglesDeg.isEmpty()) {
                double sum = 0.0;
                double maxAngle = -std::numeric_limits<double>::infinity();
                double minAngle = std::numeric_limits<double>::infinity();
                for (double a : anglesDeg) {
                    sum += a;
                    maxAngle = std::max(maxAngle, a);
                    minAngle = std::min(minAngle, a);
                }

                if (settings.computeJunctionAngleAverage && !anglesDeg.isEmpty())
                    result.junctionAngleAverageDegrees = sum / anglesDeg.size();

                if (settings.computeJunctionAngleMax)
                    result.junctionAngleMaxDegrees = maxAngle;

                if (settings.computeJunctionAngleMin)
                    result.junctionAngleMinDegrees = minAngle;

                if (settings.computeJunctionAngleDifference)
                    result.junctionAngleDifferenceDegrees = maxAngle - minAngle;

                if (settings.computeJunctionAngleRatio && maxAngle > 0.0)
                    result.junctionAngleRatio = minAngle / maxAngle;


            }
        }
    }

    return result;
}

QString NeighborPairGeometryCalculator::formatResultSummary(const QVector<Result> &results, const NeighborPairGeometrySettings &settings)
{
    QStringList lines;
    lines << "Neighbor Pair Geometry Results:";

    if (results.isEmpty()) {
        lines << "No neighboring polygons were found.";
        return lines.join('\n');
    }

    auto ratioText = [](double ratio) {
        if (!std::isfinite(ratio) || ratio < 0.0)
            return QString("N/A");
        return QString::number(ratio, 'f', 3);
    };

    auto valueText = [](double value) {
        if (!std::isfinite(value))
            return QString("N/A");
        return QString::number(value, 'f', 3);
    };

    auto appendStats = [&](QStringList &list, const QString &label, double mean, double minVal, double maxVal, double diffVal, bool enabled) {
        if (!enabled)
            return;
        list << QString("%1 (mean=%2, min=%3, max=%4, diff=%5)")
                    .arg(label)
                    .arg(valueText(mean))
                    .arg(valueText(minVal))
                    .arg(valueText(maxVal))
                    .arg(valueText(diffVal));
    };

    for (const Result &res : results) {
        const int idA = res.first ? res.first->id() : -1;
        const int idB = res.second ? res.second->id() : -1;

        QStringList parts;
        parts << QString("Pair (%1, %2)").arg(idA).arg(idB);

        if (settings.computeAreaRatio)
            parts << QString("area ratio = %1").arg(ratioText(res.areaRatio));

        appendStats(parts, "area", res.areaMean, res.areaMin, res.areaMax, res.areaDiff, settings.computeAreaStats);

        if (settings.computePerimeterRatio)
            parts << QString("perimeter ratio = %1").arg(ratioText(res.perimeterRatio));

        appendStats(parts, "perimeter", res.perimeterMean, res.perimeterMin, res.perimeterMax, res.perimeterDiff, settings.computePerimeterStats);

        if (settings.computeAspectRatio)
            parts << QString("aspect ratio = %1").arg(ratioText(res.aspectRatio));

        appendStats(parts, "aspect ratio", res.aspectMean, res.aspectMin, res.aspectMax, res.aspectDiff, settings.computeAspectRatioStats);

        if (settings.computeCircularityRatio)
            parts << QString("circularity ratio = %1").arg(ratioText(res.circularityRatio));

        appendStats(parts, "circularity", res.circularityMean, res.circularityMin, res.circularityMax, res.circularityDiff, settings.computeCircularityStats);

        if (settings.computeSolidityRatio)
            parts << QString("solidity ratio = %1").arg(ratioText(res.solidityRatio));

        appendStats(parts, "solidity", res.solidityMean, res.solidityMin, res.solidityMax, res.solidityDiff, settings.computeSolidityStats);

        if (settings.computeVertexCountRatio)
            parts << QString("vertex count ratio = %1").arg(ratioText(res.vertexCountRatio));

        appendStats(parts, "vertex count", res.vertexCountMean, res.vertexCountMin, res.vertexCountMax, res.vertexCountDiff, settings.computeVertexCountStats);

        if (settings.computeCentroidDistance)
            parts << QString("centroid distance = %1").arg(res.centroidDistance < 0.0 ? QString("N/A") : QString::number(res.centroidDistance, 'f', 3));

        if (settings.computeUnionAspectRatio)
            parts << QString("union aspect ratio = %1").arg(ratioText(res.unionAspectRatio));

        if (settings.computeUnionCircularity)
            parts << QString("union circularity = %1").arg(ratioText(res.unionCircularity));

        if (settings.computeUnionConvexDeficiency)
            parts << QString("union convex deficiency = %1").arg(valueText(res.unionConvexDeficiency));

        if (settings.computeNormalizedSharedEdgeLength)
            parts << QString("normalized shared edge length = %1").arg(valueText(res.normalizedSharedEdgeLength));

        if (settings.computeSharedEdgeUnsharedVerticesDistance)
            parts << QString("shared edge-unshared vertices distance = %1").arg(valueText(res.sharedEdgeUnsharedVerticesDistance));

        if (settings.computeSharedEdgeUnsharedVerticesDistanceNormalized)
            parts << QString("shared edge-unshared vertices distance (norm) = %1").arg(valueText(res.sharedEdgeUnsharedVerticesDistanceNormalized));

        if (settings.computeCentroidSharedEdgeDistance)
            parts << QString("centroid-shared edge distance = %1").arg(valueText(res.centroidSharedEdgeDistance));

        if (settings.computeCentroidSharedEdgeDistanceNormalized)
            parts << QString("centroid-shared edge distance (norm) = %1").arg(valueText(res.centroidSharedEdgeDistanceNormalized));

        if (settings.computeJunctionAngleAverage)
            parts << QString("avg junction angle = %1 deg").arg(valueText(res.junctionAngleAverageDegrees));

        if (settings.computeJunctionAngleMax)
            parts << QString("max junction angle = %1 deg").arg(valueText(res.junctionAngleMaxDegrees));

        if (settings.computeJunctionAngleMin)
            parts << QString("min junction angle = %1 deg").arg(valueText(res.junctionAngleMinDegrees));

        if (settings.computeJunctionAngleDifference)
            parts << QString("junction angle diff = %1 deg").arg(valueText(res.junctionAngleDifferenceDegrees));

        if (settings.computeJunctionAngleRatio)
            parts << QString("junction angle ratio = %1").arg(valueText(res.junctionAngleRatio));

        if (settings.computeSharedEdgeUnionCentroidDistance)
            parts << QString("shared edge-union centroid distance = %1").arg(valueText(res.sharedEdgeUnionCentroidDistance));

        if (settings.computeSharedEdgeUnionCentroidDistanceNormalized)
            parts << QString("shared edge-union centroid distance (norm) = %1").arg(valueText(res.sharedEdgeUnionCentroidDistanceNormalized));

        if (settings.computeSharedEdgeUnionAxisAngle)
            parts << QString("shared edge-union axis angle = %1 deg").arg(valueText(res.sharedEdgeUnionAxisAngleDegrees));

        lines << parts.join(", ");
    }

    return lines.join('\n');
}


QPolygonF polygonToScene(const PolygonItem *item)
{
    QPolygonF mapped;
    if (!item)
        return mapped;

    const QPolygonF poly = item->polygon();
    mapped.reserve(poly.size());
    for (const QPointF &p : poly)
        mapped << item->mapToScene(p);
    return mapped;
}

QPixmap buildPreviewPixmap(const PolygonItem *a, const PolygonItem *b)
{
    constexpr int kSize = 120;
    constexpr qreal kMargin = 6.0;

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::white);

    const QPolygonF polyA = polygonToScene(a);
    const QPolygonF polyB = polygonToScene(b);

    if (polyA.isEmpty() && polyB.isEmpty())
        return pixmap;

    QRectF bounds = polyA.boundingRect();
    if (!polyB.isEmpty())
        bounds = bounds.isNull() ? polyB.boundingRect() : bounds.united(polyB.boundingRect());

    if (bounds.isNull())
        bounds = QRectF(QPointF(-1, -1), QSizeF(2, 2));

    const qreal scaleX = (kSize - 2 * kMargin) / bounds.width();
    const qreal scaleY = (kSize - 2 * kMargin) / bounds.height();
    const qreal scale = std::min(scaleX, scaleY);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.translate(kSize / 2.0, kSize / 2.0);
    painter.scale(scale, scale);
    painter.translate(-bounds.center());

    auto drawPoly = [&painter](const QPolygonF &poly, const QColor &color) {
        if (poly.isEmpty())
            return;

        QPen pen(color.darker(), 0);
        pen.setWidthF(0.0);
        painter.setPen(pen);
        painter.setBrush(QBrush(color, Qt::SolidPattern));
        painter.drawPolygon(poly, Qt::OddEvenFill);
    };

    drawPoly(polyA, QColor(76, 132, 255, 120));
    drawPoly(polyB, QColor(255, 112, 67, 120));

    auto drawLabel = [&painter](const QPolygonF &poly, const QString &text) {
        if (poly.isEmpty() || text.isEmpty())
            return;

        const QSizeF boxSize(26.0, 16.0);
        const QPointF center = poly.boundingRect().center();
        const QRectF rect(center - QPointF(boxSize.width() / 2.0, boxSize.height() / 2.0), boxSize);

        QFont font = painter.font();
        font.setPointSize(8);
        font.setBold(true);
        painter.setFont(font);

        painter.setPen(Qt::black);
        painter.drawText(rect, Qt::AlignCenter, text);
    };

    drawLabel(polyA, a ? QString::number(a->id()) : QString());
    drawLabel(polyB, b ? QString::number(b->id()) : QString());

    return pixmap;
}

QPixmap buildUnionPreviewPixmap(const PolygonItem *a, const PolygonItem *b)
{
    constexpr int kSize = 120;
    constexpr qreal kMargin = 6.0;

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::white);

    const QPolygonF polyA = polygonToScene(a);
    const QPolygonF polyB = polygonToScene(b);

    QPainterPath unionPath;
    if (!polyA.isEmpty())
        unionPath.addPolygon(polyA);
    if (!polyB.isEmpty()) {
        QPainterPath pathB;
        pathB.addPolygon(polyB);
        unionPath = unionPath.isEmpty() ? pathB : unionPath.united(pathB);
    }

    if (unionPath.isEmpty())
        return pixmap;

    QRectF bounds = unionPath.boundingRect();
    if (bounds.isNull())
        bounds = QRectF(QPointF(-1, -1), QSizeF(2, 2));

    const qreal scaleX = (kSize - 2 * kMargin) / bounds.width();
    const qreal scaleY = (kSize - 2 * kMargin) / bounds.height();
    const qreal scale = std::min(scaleX, scaleY);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(kSize / 2.0, kSize / 2.0);
    painter.scale(scale, scale);
    painter.translate(-bounds.center());

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(76, 175, 80, 160));
    painter.drawPath(unionPath);

    struct EdgeToDraw {
        QString key;
        QLineF line;
    };

    auto edgeKey = [](int id1, int id2) {
        const int aId = std::min(id1, id2);
        const int bId = std::max(id1, id2);
        return QString("%1-%2").arg(aId).arg(bId);
    };

    auto buildEdges = [&](const PolygonItem *item) {
        QVector<EdgeToDraw> edges;
        if (!item)
            return edges;

        const QPolygonF poly = polygonToScene(item);
        const QVector<int> ids = item->vertexIds();
        const int count = std::min(poly.size(), ids.size());
        if (count < 2)
            return edges;

        edges.reserve(count);
        for (int i = 0; i < count; ++i) {
            const QPointF &p1 = poly[i];
            const QPointF &p2 = poly[(i + 1) % count];
            const int id1 = ids[i];
            const int id2 = ids[(i + 1) % count];
            edges.push_back(EdgeToDraw{edgeKey(id1, id2), QLineF(p1, p2)});
        }
        return edges;
    };

    const QVector<EdgeToDraw> edgesA = buildEdges(a);
    const QVector<EdgeToDraw> edgesB = buildEdges(b);

    QSet<QString> keysA;
    for (const EdgeToDraw &edge : edgesA)
        keysA.insert(edge.key);

    QSet<QString> keysB;
    for (const EdgeToDraw &edge : edgesB)
        keysB.insert(edge.key);

    QPen pen(QColor(46, 125, 50), 0);
    pen.setWidthF(0.0);
    painter.setPen(pen);

    auto drawEdges = [&](const QVector<EdgeToDraw> &edges, const QSet<QString> &otherKeys) {
        for (const EdgeToDraw &edge : edges) {
            if (otherKeys.contains(edge.key))
                continue; // shared edge, skip to avoid drawing internal boundary
            painter.drawLine(edge.line);
        }
    };

    drawEdges(edgesA, keysB);
    drawEdges(edgesB, keysA);

    return pixmap;
}

QPixmap buildContactPreviewPixmap(const PolygonItem *a, const PolygonItem *b)
{
    constexpr int kSize = 120;
    constexpr qreal kMargin = 6.0;

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::white);

    if (!a && !b)
        return pixmap;

    NeighborPair pair(const_cast<PolygonItem*>(a), const_cast<PolygonItem*>(b));

    const QPolygonF polyA = polygonToScene(a);
    const QPolygonF polyB = polygonToScene(b);

    QVector<QPointF> points;
    points.reserve(polyA.size() + polyB.size());
    for (const QPointF &p : polyA)
        points.push_back(p);
    for (const QPointF &p : polyB)
        points.push_back(p);

    auto addVertexPoints = [&](const QVector<VertexItem*> &verts) {
        for (VertexItem *v : verts) {
            if (v)
                points.push_back(v->scenePos());
        }
    };

    addVertexPoints(pair.sharedVertices());
    addVertexPoints(pair.connectingVerticesFirst());
    addVertexPoints(pair.connectingVerticesSecond());

    QRectF bounds;
    if (!polyA.isEmpty())
        bounds = polyA.boundingRect();

    if (!polyB.isEmpty())
        bounds = bounds.isNull() ? polyB.boundingRect() : bounds.united(polyB.boundingRect());

    for (const QPointF &p : std::as_const(points))
        bounds = bounds.isNull() ? QRectF(p, QSizeF(0.0, 0.0)) : bounds.united(QRectF(p, QSizeF(0.0, 0.0)));

    if (bounds.width() <= 0.0 || bounds.height() <= 0.0)
        bounds = QRectF(QPointF(-1, -1), QSizeF(2, 2));

    const qreal scaleX = (kSize - 2 * kMargin) / bounds.width();
    const qreal scaleY = (kSize - 2 * kMargin) / bounds.height();
    const qreal scale = std::min(scaleX, scaleY);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(kSize / 2.0, kSize / 2.0);
    painter.scale(scale, scale);
    painter.translate(-bounds.center());

    auto drawPoly = [&painter](const QPolygonF &poly, const QColor &color) {
        if (poly.isEmpty())
            return;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(color, Qt::SolidPattern));
        painter.drawPolygon(poly, Qt::OddEvenFill);
    };

    drawPoly(polyA, QColor(76, 132, 255, 80));
    drawPoly(polyB, QColor(255, 112, 67, 80));

    auto drawEdgeList = [&painter](const QVector<NeighborPair::EdgeInfo> &edges, const QColor &color) {
        QPen pen(color, 0);
        pen.setWidthF(0.0);
        painter.setPen(pen);
        for (const NeighborPair::EdgeInfo &edge : edges) {
            if (!edge.v1 || !edge.v2)
                continue;
            painter.drawLine(QLineF(edge.v1->scenePos(), edge.v2->scenePos()));
        }
    };

    drawEdgeList(pair.connectingEdgesFirst(), QColor(255, 152, 0));
    drawEdgeList(pair.connectingEdgesSecond(), QColor(255, 152, 0));
    drawEdgeList(pair.sharedEdges(), QColor(183, 28, 28));

    auto drawVertices = [&painter](const QVector<VertexItem*> &verts, const QColor &color) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        const qreal r = 3.0;
        for (VertexItem *v : verts) {
            if (!v)
                continue;
            const QPointF p = v->scenePos();
            painter.drawEllipse(QPointF(p.x(), p.y()), r, r);
        }
    };

    drawVertices(pair.connectingVerticesFirst(), QColor(255, 235, 59));
    drawVertices(pair.connectingVerticesSecond(), QColor(255, 235, 59));
    drawVertices(pair.sharedVertices(), QColor(244, 67, 54));

    return pixmap;
}

void NeighborPairGeometryCalculator::showResultsDialog(const QVector<Result> &results, const NeighborPairGeometrySettings &settings, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Neighbor Pair Geometry");

    auto *layout = new QVBoxLayout(&dialog);

    if (results.isEmpty()) {
        layout->addWidget(new QLabel("No neighboring polygons were found.", &dialog));
    } else {
        QVector<Result> sortedResults = results;
        std::sort(sortedResults.begin(), sortedResults.end(), [](const Result &lhs, const Result &rhs) {
            const int lhsIdA = lhs.first ? lhs.first->id() : std::numeric_limits<int>::max();
            const int lhsIdB = lhs.second ? lhs.second->id() : std::numeric_limits<int>::max();
            const int rhsIdA = rhs.first ? rhs.first->id() : std::numeric_limits<int>::max();
            const int rhsIdB = rhs.second ? rhs.second->id() : std::numeric_limits<int>::max();
            return std::tie(lhsIdA, lhsIdB) < std::tie(rhsIdA, rhsIdB);
        });

        const QStringList headers = {
            "Polygon 1", "Polygon 2", "Preview", "Union Preview", "Contact Preview",
            "Area Ratio", "Area Mean", "Area Min", "Area Max", "Area Diff",
            "Perimeter Ratio", "Perimeter Mean", "Perimeter Min", "Perimeter Max", "Perimeter Diff",
            "Aspect Ratio", "Aspect Mean", "Aspect Min", "Aspect Max", "Aspect Diff",
            "Circularity Ratio", "Circularity Mean", "Circularity Min", "Circularity Max", "Circularity Diff",
            "Solidity Ratio", "Solidity Mean", "Solidity Min", "Solidity Max", "Solidity Diff",
            "Vertex Count Ratio", "Vertex Count Mean", "Vertex Count Min", "Vertex Count Max", "Vertex Count Diff",
            "Centroid Distance", "Union Aspect Ratio", "Union Circularity", "Union Convex Deficiency", "Normalized Shared Edge Length",
            "Shared Edge-Unshared Vertices Distance", "Shared Edge-Unshared Vertices Distance (norm)",
            "Centroid-Shared Edge Distance", "Centroid-Shared Edge Distance (norm)",
            "Shared Edge-Centroid Distance", "Shared Edge-Centroid Distance (norm)", "Shared Edge-Union Axis Angle (deg)",
            "Avg Junction Angle (deg)", "Max Junction Angle (deg)", "Min Junction Angle (deg)", "Junction Angle Diff (deg)", "Junction Angle Ratio"
        };

        auto *table = new QTableWidget(sortedResults.size(), headers.size(), &dialog);
        table->setHorizontalHeaderLabels(headers);
        for (int i = 0; i < headers.size(); ++i)
            table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

        table->verticalHeader()->setVisible(false);

        auto ratioText = [](double ratio) {
            if (!std::isfinite(ratio) || ratio < 0.0)
                return QString("N/A");
            return QString::number(ratio, 'f', 3);
        };

        auto valueText = [](double value) {
            if (!std::isfinite(value))
                return QString("N/A");
            return QString::number(value, 'f', 3);
        };


        for (int row = 0; row < sortedResults.size(); ++row) {
            const Result &res = sortedResults[row];
            const int idA = res.first ? res.first->id() : -1;
            const int idB = res.second ? res.second->id() : -1;

            int col = 0;

            auto *idAItem = new QTableWidgetItem(QString::number(idA));
            idAItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, idAItem);

            auto *idBItem = new QTableWidgetItem(QString::number(idB));
            idBItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, idBItem);

            auto *previewLabel = new QLabel(&dialog);
            previewLabel->setPixmap(buildPreviewPixmap(res.first, res.second));
            previewLabel->setAlignment(Qt::AlignCenter);
            table->setCellWidget(row, col++, previewLabel);

            auto *unionPreviewLabel = new QLabel(&dialog);
            unionPreviewLabel->setPixmap(buildUnionPreviewPixmap(res.first, res.second));
            unionPreviewLabel->setAlignment(Qt::AlignCenter);
            table->setCellWidget(row, col++, unionPreviewLabel);

            auto *contactPreviewLabel = new QLabel(&dialog);
            contactPreviewLabel->setPixmap(buildContactPreviewPixmap(res.first, res.second));
            contactPreviewLabel->setAlignment(Qt::AlignCenter);
            table->setCellWidget(row, col++, contactPreviewLabel);

            auto *areaItem = new QTableWidgetItem(settings.computeAreaRatio ? ratioText(res.areaRatio) : QString("N/A"));
            areaItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, areaItem);

            auto *areaMeanItem = new QTableWidgetItem(settings.computeAreaStats ? valueText(res.areaMean) : QString("N/A"));
            areaMeanItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, areaMeanItem);

            auto *areaMinItem = new QTableWidgetItem(settings.computeAreaStats ? valueText(res.areaMin) : QString("N/A"));
            areaMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, areaMinItem);

            auto *areaMaxItem = new QTableWidgetItem(settings.computeAreaStats ? valueText(res.areaMax) : QString("N/A"));
            areaMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, areaMaxItem);

            auto *areaDiffItem = new QTableWidgetItem(settings.computeAreaStats ? valueText(res.areaDiff) : QString("N/A"));
            areaDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, areaDiffItem);

            auto *perimeterItem = new QTableWidgetItem(settings.computePerimeterRatio ? ratioText(res.perimeterRatio) : QString("N/A"));
            perimeterItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, perimeterItem);

            auto *perimeterMeanItem = new QTableWidgetItem(settings.computePerimeterStats ? valueText(res.perimeterMean) : QString("N/A"));
            perimeterMeanItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, perimeterMeanItem);

            auto *perimeterMinItem = new QTableWidgetItem(settings.computePerimeterStats ? valueText(res.perimeterMin) : QString("N/A"));
            perimeterMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, perimeterMinItem);

            auto *perimeterMaxItem = new QTableWidgetItem(settings.computePerimeterStats ? valueText(res.perimeterMax) : QString("N/A"));
            perimeterMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, perimeterMaxItem);

            auto *perimeterDiffItem = new QTableWidgetItem(settings.computePerimeterStats ? valueText(res.perimeterDiff) : QString("N/A"));
            perimeterDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, perimeterDiffItem);

            auto *aspectItem = new QTableWidgetItem(settings.computeAspectRatio ? ratioText(res.aspectRatio) : QString("N/A"));
            aspectItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, aspectItem);

            auto *aspectMeanItem = new QTableWidgetItem(settings.computeAspectRatioStats ? valueText(res.aspectMean) : QString("N/A"));
            aspectMeanItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, aspectMeanItem);

            auto *aspectMinItem = new QTableWidgetItem(settings.computeAspectRatioStats ? valueText(res.aspectMin) : QString("N/A"));
            aspectMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, aspectMinItem);

            auto *aspectMaxItem = new QTableWidgetItem(settings.computeAspectRatioStats ? valueText(res.aspectMax) : QString("N/A"));
            aspectMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, aspectMaxItem);

            auto *aspectDiffItem = new QTableWidgetItem(settings.computeAspectRatioStats ? valueText(res.aspectDiff) : QString("N/A"));
            aspectDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, aspectDiffItem);

            auto *circItem = new QTableWidgetItem(settings.computeCircularityRatio ? ratioText(res.circularityRatio) : QString("N/A"));
            circItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, circItem);

            auto *circMeanItem = new QTableWidgetItem(settings.computeCircularityStats ? valueText(res.circularityMean) : QString("N/A"));
            circMeanItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, circMeanItem);

            auto *circMinItem = new QTableWidgetItem(settings.computeCircularityStats ? valueText(res.circularityMin) : QString("N/A"));
            circMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, circMinItem);

            auto *circMaxItem = new QTableWidgetItem(settings.computeCircularityStats ? valueText(res.circularityMax) : QString("N/A"));
            circMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, circMaxItem);

            auto *circDiffItem = new QTableWidgetItem(settings.computeCircularityStats ? valueText(res.circularityDiff) : QString("N/A"));
            circDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, circDiffItem);

            auto *solidityItem = new QTableWidgetItem(settings.computeSolidityRatio ? ratioText(res.solidityRatio) : QString("N/A"));
            solidityItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, solidityItem);

            auto *solidityMeanItem = new QTableWidgetItem(settings.computeSolidityStats ? valueText(res.solidityMean) : QString("N/A"));
            solidityMeanItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, solidityMeanItem);

            auto *solidityMinItem = new QTableWidgetItem(settings.computeSolidityStats ? valueText(res.solidityMin) : QString("N/A"));
            solidityMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, solidityMinItem);

            auto *solidityMaxItem = new QTableWidgetItem(settings.computeSolidityStats ? valueText(res.solidityMax) : QString("N/A"));
            solidityMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, solidityMaxItem);

            auto *solidityDiffItem = new QTableWidgetItem(settings.computeSolidityStats ? valueText(res.solidityDiff) : QString("N/A"));
            solidityDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, solidityDiffItem);

            auto *vertexCountItem = new QTableWidgetItem(settings.computeVertexCountRatio ? ratioText(res.vertexCountRatio) : QString("N/A"));
            vertexCountItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, vertexCountItem);

            auto *vertexCountMeanItem = new QTableWidgetItem(settings.computeVertexCountStats ? valueText(res.vertexCountMean) : QString("N/A"));
            vertexCountMeanItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, vertexCountMeanItem);

            auto *vertexCountMinItem = new QTableWidgetItem(settings.computeVertexCountStats ? valueText(res.vertexCountMin) : QString("N/A"));
            vertexCountMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, vertexCountMinItem);

            auto *vertexCountMaxItem = new QTableWidgetItem(settings.computeVertexCountStats ? valueText(res.vertexCountMax) : QString("N/A"));
            vertexCountMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, vertexCountMaxItem);

            auto *vertexCountDiffItem = new QTableWidgetItem(settings.computeVertexCountStats ? valueText(res.vertexCountDiff) : QString("N/A"));
            vertexCountDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, vertexCountDiffItem);

            auto *centroidItem = new QTableWidgetItem(settings.computeCentroidDistance ? (res.centroidDistance < 0.0 ? QString("N/A") : QString::number(res.centroidDistance, 'f', 3)) : QString("N/A"));
            centroidItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, centroidItem);

            auto *unionAspectItem = new QTableWidgetItem(settings.computeUnionAspectRatio ? ratioText(res.unionAspectRatio) : QString("N/A"));
            unionAspectItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, unionAspectItem);

            auto *unionCircItem = new QTableWidgetItem(settings.computeUnionCircularity ? ratioText(res.unionCircularity) : QString("N/A"));
            unionCircItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, unionCircItem);

            auto *unionDeficiencyItem = new QTableWidgetItem(settings.computeUnionConvexDeficiency ? valueText(res.unionConvexDeficiency) : QString("N/A"));
            unionDeficiencyItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, unionDeficiencyItem);

            auto *normalizedSharedEdgeItem = new QTableWidgetItem(settings.computeNormalizedSharedEdgeLength ? valueText(res.normalizedSharedEdgeLength) : QString("N/A"));
            normalizedSharedEdgeItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, normalizedSharedEdgeItem);

            auto *sharedEdgeUnsharedVerticesItem = new QTableWidgetItem(settings.computeSharedEdgeUnsharedVerticesDistance ? valueText(res.sharedEdgeUnsharedVerticesDistance) : QString("N/A"));
            sharedEdgeUnsharedVerticesItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, sharedEdgeUnsharedVerticesItem);

            auto *sharedEdgeUnsharedVerticesNormItem = new QTableWidgetItem(settings.computeSharedEdgeUnsharedVerticesDistanceNormalized ? valueText(res.sharedEdgeUnsharedVerticesDistanceNormalized) : QString("N/A"));
            sharedEdgeUnsharedVerticesNormItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, sharedEdgeUnsharedVerticesNormItem);

            auto *centroidSharedEdgeItem = new QTableWidgetItem(settings.computeCentroidSharedEdgeDistance ? valueText(res.centroidSharedEdgeDistance) : QString("N/A"));
            centroidSharedEdgeItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, centroidSharedEdgeItem);

            auto *centroidSharedEdgeNormItem = new QTableWidgetItem(settings.computeCentroidSharedEdgeDistanceNormalized ? valueText(res.centroidSharedEdgeDistanceNormalized) : QString("N/A"));
            centroidSharedEdgeNormItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, centroidSharedEdgeNormItem);

            auto *edgeCentroidDistanceItem = new QTableWidgetItem(settings.computeSharedEdgeUnionCentroidDistance ? valueText(res.sharedEdgeUnionCentroidDistance) : QString("N/A"));
            edgeCentroidDistanceItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, edgeCentroidDistanceItem);

            auto *edgeCentroidDistanceNormItem = new QTableWidgetItem(settings.computeSharedEdgeUnionCentroidDistanceNormalized ? valueText(res.sharedEdgeUnionCentroidDistanceNormalized) : QString("N/A"));
            edgeCentroidDistanceNormItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, edgeCentroidDistanceNormItem);

            auto *edgeUnionAxisAngleItem = new QTableWidgetItem(settings.computeSharedEdgeUnionAxisAngle ? valueText(res.sharedEdgeUnionAxisAngleDegrees) : QString("N/A"));
            edgeUnionAxisAngleItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, edgeUnionAxisAngleItem);

            auto *junctionAngleAverageItem = new QTableWidgetItem(settings.computeJunctionAngleAverage ? valueText(res.junctionAngleAverageDegrees) : QString("N/A"));
            junctionAngleAverageItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, junctionAngleAverageItem);

            auto *junctionAngleMaxItem = new QTableWidgetItem(settings.computeJunctionAngleMax ? valueText(res.junctionAngleMaxDegrees) : QString("N/A"));
            junctionAngleMaxItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, junctionAngleMaxItem);

            auto *junctionAngleMinItem = new QTableWidgetItem(settings.computeJunctionAngleMin ? valueText(res.junctionAngleMinDegrees) : QString("N/A"));
            junctionAngleMinItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, junctionAngleMinItem);

            auto *junctionAngleDiffItem = new QTableWidgetItem(settings.computeJunctionAngleDifference ? valueText(res.junctionAngleDifferenceDegrees) : QString("N/A"));
            junctionAngleDiffItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, junctionAngleDiffItem);

            auto *junctionAngleRatioItem = new QTableWidgetItem(settings.computeJunctionAngleRatio ? valueText(res.junctionAngleRatio) : QString("N/A"));
            junctionAngleRatioItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, col++, junctionAngleRatioItem);

        }

        table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        table->resizeColumnsToContents();
        table->resizeRowsToContents();
        layout->addWidget(table);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}
