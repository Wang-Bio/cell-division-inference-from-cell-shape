#include "batchdivisionestimator.h"

#include "geometryio.h"
#include "neighborpair.h"
#include "polygonitem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace {
QVector<PolygonItem*> collectPolygons(QGraphicsScene *scene)
{
    QVector<PolygonItem*> polygons;
    if (!scene)
        return polygons;

    polygons.reserve(scene->items().size());
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type)
            polygons.append(static_cast<PolygonItem*>(item));
    }
    return polygons;
}

QHash<int, PolygonItem*> polygonMap(const QVector<PolygonItem*> &polygons)
{
    QHash<int, PolygonItem*> map;
    for (PolygonItem *poly : polygons) {
        if (poly)
            map.insert(poly->id(), poly);
    }
    return map;
}

QVector<QPair<int, int>> computeNeighborPairs(const QVector<PolygonItem*> &polygons)
{
    QVector<QPair<int, int>> pairs;
    for (int i = 0; i < polygons.size(); ++i) {
        for (int j = i + 1; j < polygons.size(); ++j) {
            NeighborPair pair(polygons[i], polygons[j]);
            if (pair.isNeighbor())
                pairs.append(QPair<int, int>(polygons[i]->id(), polygons[j]->id()));
        }
    }
    return pairs;
}

QStringList buildHeaders(const NeighborPairGeometrySettings &settings)
{
    QStringList headers = {
        "fileName",
        "pairIndex",
        "firstPolygonId",
        "secondPolygonId",
        "observed_division",
        "division_timing"
    };

    if (settings.computeAreaRatio)
        headers << "areaRatio";
    if (settings.computeAreaStats)
        headers << "areaMean" << "areaMin" << "areaMax" << "areaDiff";

    if (settings.computePerimeterRatio)
        headers << "perimeterRatio";
    if (settings.computePerimeterStats)
        headers << "perimeterMean" << "perimeterMin" << "perimeterMax" << "perimeterDiff";

    if (settings.computeAspectRatio)
        headers << "aspectRatio";
    if (settings.computeAspectRatioStats)
        headers << "aspectMean" << "aspectMin" << "aspectMax" << "aspectDiff";

    if (settings.computeCircularityRatio)
        headers << "circularityRatio";
    if (settings.computeCircularityStats)
        headers << "circularityMean" << "circularityMin" << "circularityMax" << "circularityDiff";

    if (settings.computeSolidityRatio)
        headers << "solidityRatio";
    if (settings.computeSolidityStats)
        headers << "solidityMean" << "solidityMin" << "solidityMax" << "solidityDiff";

    if (settings.computeVertexCountRatio)
        headers << "vertexCountRatio";
    if (settings.computeVertexCountStats)
        headers << "vertexCountMean" << "vertexCountMin" << "vertexCountMax" << "vertexCountDiff";

    if (settings.computeCentroidDistance)
        headers << "centroidDistance" << "centroidDistanceNormalized";

    if (settings.computeUnionAspectRatio)
        headers << "unionAspectRatio";
    if (settings.computeUnionCircularity)
        headers << "unionCircularity";
    if (settings.computeUnionConvexDeficiency)
        headers << "unionConvexDeficiency";

    if (settings.computeNormalizedSharedEdgeLength)
        headers << "normalizedSharedEdgeLength" << "sharedEdgeLength";

    if (settings.computeSharedEdgeUnsharedVerticesDistance)
        headers << "sharedEdgeUnsharedVerticesDistance";
    if (settings.computeSharedEdgeUnsharedVerticesDistanceNormalized)
        headers << "sharedEdgeUnsharedVerticesDistanceNormalized";

    if (settings.computeCentroidSharedEdgeDistance)
        headers << "centroidSharedEdgeDistance";
    if (settings.computeCentroidSharedEdgeDistanceNormalized)
        headers << "centroidSharedEdgeDistanceNormalized";

    if (settings.computeSharedEdgeUnionCentroidDistance)
        headers << "sharedEdgeUnionCentroidDistance";
    if (settings.computeSharedEdgeUnionCentroidDistanceNormalized)
        headers << "sharedEdgeUnionCentroidDistanceNormalized";
    if (settings.computeSharedEdgeUnionAxisAngle)
        headers << "sharedEdgeUnionAxisAngleDegrees";

    if (settings.computeJunctionAngleAverage)
        headers << "junctionAngleAverageDegrees";
    if (settings.computeJunctionAngleMax)
        headers << "junctionAngleMaxDegrees";
    if (settings.computeJunctionAngleMin)
        headers << "junctionAngleMinDegrees";
    if (settings.computeJunctionAngleDifference)
        headers << "junctionAngleDifferenceDegrees";
    if (settings.computeJunctionAngleRatio)
        headers << "junctionAngleRatio";

    return headers;
}

QString formatValue(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral("N/A");
    return QString::number(value, 'f', 6);
}

QStringList buildRow(const BatchDivisionEstimator::GeometryEntry &entry,
                     const NeighborPairGeometrySettings &settings)
{
    QStringList row = {
        entry.fileName,
        QString::number(entry.pairIndex),
        QString::number(entry.firstId),
        QString::number(entry.secondId),
        entry.observedDivision ? QStringLiteral("1") : QStringLiteral("0"),
        QString::number(entry.observedDivision ? entry.divisionTime : -1)
    };

    const auto &res = entry.geometry;

    if (settings.computeAreaRatio)
        row << formatValue(res.areaRatio);
    if (settings.computeAreaStats)
        row << formatValue(res.areaMean) << formatValue(res.areaMin) << formatValue(res.areaMax) << formatValue(res.areaDiff);

    if (settings.computePerimeterRatio)
        row << formatValue(res.perimeterRatio);
    if (settings.computePerimeterStats)
        row << formatValue(res.perimeterMean) << formatValue(res.perimeterMin) << formatValue(res.perimeterMax) << formatValue(res.perimeterDiff);

    if (settings.computeAspectRatio)
        row << formatValue(res.aspectRatio);
    if (settings.computeAspectRatioStats)
        row << formatValue(res.aspectMean) << formatValue(res.aspectMin) << formatValue(res.aspectMax) << formatValue(res.aspectDiff);

    if (settings.computeCircularityRatio)
        row << formatValue(res.circularityRatio);
    if (settings.computeCircularityStats)
        row << formatValue(res.circularityMean) << formatValue(res.circularityMin) << formatValue(res.circularityMax) << formatValue(res.circularityDiff);

    if (settings.computeSolidityRatio)
        row << formatValue(res.solidityRatio);
    if (settings.computeSolidityStats)
        row << formatValue(res.solidityMean) << formatValue(res.solidityMin) << formatValue(res.solidityMax) << formatValue(res.solidityDiff);

    if (settings.computeVertexCountRatio)
        row << formatValue(res.vertexCountRatio);
    if (settings.computeVertexCountStats)
        row << formatValue(res.vertexCountMean) << formatValue(res.vertexCountMin) << formatValue(res.vertexCountMax) << formatValue(res.vertexCountDiff);

    if (settings.computeCentroidDistance)
        row << formatValue(res.centroidDistance) << formatValue(res.centroidDistanceNormalized);

    if (settings.computeUnionAspectRatio)
        row << formatValue(res.unionAspectRatio);
    if (settings.computeUnionCircularity)
        row << formatValue(res.unionCircularity);
    if (settings.computeUnionConvexDeficiency)
        row << formatValue(res.unionConvexDeficiency);

    if (settings.computeNormalizedSharedEdgeLength)
        row << formatValue(res.normalizedSharedEdgeLength) << formatValue(res.sharedEdgeLength);

    if (settings.computeSharedEdgeUnsharedVerticesDistance)
        row << formatValue(res.sharedEdgeUnsharedVerticesDistance);
    if (settings.computeSharedEdgeUnsharedVerticesDistanceNormalized)
        row << formatValue(res.sharedEdgeUnsharedVerticesDistanceNormalized);

    if (settings.computeCentroidSharedEdgeDistance)
        row << formatValue(res.centroidSharedEdgeDistance);
    if (settings.computeCentroidSharedEdgeDistanceNormalized)
        row << formatValue(res.centroidSharedEdgeDistanceNormalized);

    if (settings.computeSharedEdgeUnionCentroidDistance)
        row << formatValue(res.sharedEdgeUnionCentroidDistance);
    if (settings.computeSharedEdgeUnionCentroidDistanceNormalized)
        row << formatValue(res.sharedEdgeUnionCentroidDistanceNormalized);
    if (settings.computeSharedEdgeUnionAxisAngle)
        row << formatValue(res.sharedEdgeUnionAxisAngleDegrees);

    if (settings.computeJunctionAngleAverage)
        row << formatValue(res.junctionAngleAverageDegrees);
    if (settings.computeJunctionAngleMax)
        row << formatValue(res.junctionAngleMaxDegrees);
    if (settings.computeJunctionAngleMin)
        row << formatValue(res.junctionAngleMinDegrees);
    if (settings.computeJunctionAngleDifference)
        row << formatValue(res.junctionAngleDifferenceDegrees);
    if (settings.computeJunctionAngleRatio)
        row << formatValue(res.junctionAngleRatio);

    return row;
}

QStringList buildSingleCellHeaders()
{
    return {
        "fileName",
        "polygonId",
        "inDividingPair",
        "area",
        "perimeter",
        "aspectRatio",
        "circularity",
        "solidity",
        "vertexCount"
    };
}

QStringList buildSingleCellRow(const BatchDivisionEstimator::SingleCellGeometryEntry &entry)
{
    return {
        entry.fileName,
        QString::number(entry.polygonId),
        entry.inDividingPair ? QStringLiteral("1") : QStringLiteral("0"),
        formatValue(entry.area),
        formatValue(entry.perimeter),
        formatValue(entry.aspectRatio),
        formatValue(entry.circularity),
        formatValue(entry.solidity),
        QString::number(entry.vertexCount)
    };
}

QString escapeForCsv(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n'))
        escaped = QStringLiteral("\"%1\"").arg(escaped);
    return escaped;
}

QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == '"') {
                current.append('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.append(current.trimmed());
            current.clear();
        } else {
            current.append(ch);
        }
    }
    fields.append(current.trimmed());
    return fields;
}

QStringList divisionCsvHeaders()
{
    return {
        "filename",
        "cell_1_id",
        "cell_2_id",
        "division_timing",
        "observed_division",
        "estimated_division",
        "distance",
        "distance_normalized",
        "area_ratio",
        "area_diff",
        "convex_deficiency",
        "area_min",
        "area_max",
        "area_mean",
        "perimeter_ratio",
        "perimeter_min",
        "perimeter_max",
        "perimeter_mean",
        "perimeter_diff",
        "aspect_ratio",
        "compactness_min",
        "compactness_max",
        "compactness_mean",
        "compactness_diff",
        "elongation_ratio",
        "elongation_min",
        "elongation_max",
        "elongation_mean",
        "elongation_diff",
        "solidity_ratio",
        "solidity_min",
        "solidity_max",
        "solidity_mean",
        "solidity_diff",
        "vertex_count_ratio",
        "vertex_count_min",
        "vertex_count_max",
        "vertex_count_mean",
        "vertex_count_diff",
        "union_aspect_ratio",
        "union_circularity",
        "shared_edge_length",
        "shared_edge_length_normalized",
        "shared_edge_vertex_distance",
        "shared_edge_vertex_distance_normalized",
        "centroid_shared_edge_distance",
        "centroid_shared_edge_distance_normalized",
        "union_centroid_edge_distance",
        "union_centroid_edge_distance_normalized",
        "union_axis_shared_edge_angle",
        "junction_angle_ratio",
        "junction_angle_diff",
        "junction_angle_min",
        "junction_angle_max",
        "junction_angle_mean"
    };
}

QStringList buildDivisionCsvRow(const BatchDivisionEstimator::DivisionPairRow &row)
{
    const auto &geom = row.geometry;
    QStringList values = {
        row.fileName,
        QString::number(row.firstId),
        QString::number(row.secondId),
        QString::number(row.divisionTime),
        row.observedDivision ? QStringLiteral("1") : QStringLiteral("0"),
        row.estimatedDivision ? QStringLiteral("1") : QStringLiteral("0"),
        formatValue(geom.centroidDistance),
        formatValue(geom.centroidDistanceNormalized),
        formatValue(geom.areaRatio),
        formatValue(geom.areaDiff),
        formatValue(geom.unionConvexDeficiency),
        formatValue(geom.areaMin),
        formatValue(geom.areaMax),
        formatValue(geom.areaMean),
        formatValue(geom.perimeterRatio),
        formatValue(geom.perimeterMin),
        formatValue(geom.perimeterMax),
        formatValue(geom.perimeterMean),
        formatValue(geom.perimeterDiff),
        formatValue(geom.aspectRatio),
        formatValue(geom.circularityMin),
        formatValue(geom.circularityMax),
        formatValue(geom.circularityMean),
        formatValue(geom.circularityDiff),
        formatValue(geom.aspectRatio),
        formatValue(geom.aspectMin),
        formatValue(geom.aspectMax),
        formatValue(geom.aspectMean),
        formatValue(geom.aspectDiff),
        formatValue(geom.solidityRatio),
        formatValue(geom.solidityMin),
        formatValue(geom.solidityMax),
        formatValue(geom.solidityMean),
        formatValue(geom.solidityDiff),
        formatValue(geom.vertexCountRatio),
        formatValue(geom.vertexCountMin),
        formatValue(geom.vertexCountMax),
        formatValue(geom.vertexCountMean),
        formatValue(geom.vertexCountDiff),
        formatValue(geom.unionAspectRatio),
        formatValue(geom.unionCircularity),
        formatValue(geom.sharedEdgeLength),
        formatValue(geom.normalizedSharedEdgeLength),
        formatValue(geom.sharedEdgeUnsharedVerticesDistance),
        formatValue(geom.sharedEdgeUnsharedVerticesDistanceNormalized),
        formatValue(geom.centroidSharedEdgeDistance),
        formatValue(geom.centroidSharedEdgeDistanceNormalized),
        formatValue(geom.sharedEdgeUnionCentroidDistance),
        formatValue(geom.sharedEdgeUnionCentroidDistanceNormalized),
        formatValue(geom.sharedEdgeUnionAxisAngleDegrees),
        formatValue(geom.junctionAngleRatio),
        formatValue(geom.junctionAngleDifferenceDegrees),
        formatValue(geom.junctionAngleMinDegrees),
        formatValue(geom.junctionAngleMaxDegrees),
        formatValue(geom.junctionAngleAverageDegrees)
    };
    return values;
}

QString normalizedPairKey(int a, int b)
{
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    return QString("%1-%2").arg(first).arg(second);
}

QSet<QString> pairKeys(const QVector<QPair<int, int>> &pairs)
{
    QSet<QString> keys;
    for (const auto &pair : pairs)
        keys.insert(normalizedPairKey(pair.first, pair.second));
    return keys;
}

QString normalizeFeatureName(const QString &value)
{
    QString normalized = value.trimmed().toLower();
    normalized.remove(QRegularExpression("[^a-z0-9]+"));
    return normalized;
}

QString featureKeyFromName(const QString &name)
{
    const QString normalized = normalizeFeatureName(name);
    if (normalized.isEmpty())
        return {};

    const QHash<QString, QString> manualMap = {
        {"junctionanglemean", "junctionAngleAverageDegrees"},
        {"junctionangleaverage", "junctionAngleAverageDegrees"},
        {"junctionangleavg", "junctionAngleAverageDegrees"},
        {"junctionanglemin", "junctionAngleMinDegrees"},
        {"junctionanglemax", "junctionAngleMaxDegrees"},
        {"junctionanglediff", "junctionAngleDifferenceDegrees"},
        {"junctionangledifference", "junctionAngleDifferenceDegrees"},
        {"junctionangleratio", "junctionAngleRatio"}
    };
    if (manualMap.contains(normalized))
        return manualMap.value(normalized);

    const QVector<DivisionEstimator::FeatureOption> options = DivisionEstimator::featureOptions();
    for (const auto &opt : options) {
        const QString normalizedKey = normalizeFeatureName(opt.key);
        const QString normalizedLabel = normalizeFeatureName(opt.label);
        if (normalizedKey == normalized || normalizedLabel == normalized)
            return opt.key;
        QString simplifiedLabel = normalizedLabel;
        simplifiedLabel.remove("degrees");
        simplifiedLabel.remove("degree");
        simplifiedLabel.remove("deg");
        if (simplifiedLabel == normalized)
            return opt.key;
    }

    return {};
}

QHash<QString, QSet<QString>> loadExceptionCsv(const QFileInfo &exceptionFile, QStringList *warnings)
{
    QHash<QString, QSet<QString>> map;
    QFile file(exceptionFile.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warnings)
            warnings->append(QString("Failed to open %1: %2").arg(exceptionFile.fileName(), file.errorString()));
        return map;
    }

    QTextStream in(&file);
    int lineNumber = 0;
    while (!in.atEnd()) {
        const QString rawLine = in.readLine();
        ++lineNumber;
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        const QStringList parts = line.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            if (warnings)
                warnings->append(QString("%1 line %2: expected filename and two ids").arg(exceptionFile.fileName()).arg(lineNumber));
            continue;
        }

        const QString fileName = parts.at(0).trimmed();
        bool okFirst = false;
        bool okSecond = false;
        const int firstId = parts.at(1).toInt(&okFirst);
        const int secondId = parts.at(2).toInt(&okSecond);
        if (fileName.isEmpty() || !okFirst || !okSecond) {
            if (warnings)
                warnings->append(QString("%1 line %2: invalid values").arg(exceptionFile.fileName()).arg(lineNumber));
            continue;
        }

        map[fileName].insert(normalizedPairKey(firstId, secondId));
    }

    return map;
}

NeighborPairGeometrySettings exportSettingsForDivisionCsv(const DivisionEstimator::Criterion &criterion)
{
    NeighborPairGeometrySettings settings = criterion.settings;
    settings.computeAreaRatio = true;
    settings.computeAreaStats = true;
    settings.computePerimeterRatio = true;
    settings.computePerimeterStats = true;
    settings.computeAspectRatio = true;
    settings.computeAspectRatioStats = true;
    settings.computeCircularityRatio = true;
    settings.computeCircularityStats = true;
    settings.computeSolidityRatio = true;
    settings.computeSolidityStats = true;
    settings.computeVertexCountRatio = true;
    settings.computeVertexCountStats = true;
    settings.computeCentroidDistance = true;
    settings.computeUnionAspectRatio = true;
    settings.computeUnionCircularity = true;
    settings.computeUnionConvexDeficiency = true;
    settings.computeNormalizedSharedEdgeLength = true;
    settings.computeSharedEdgeUnsharedVerticesDistance = true;
    settings.computeSharedEdgeUnsharedVerticesDistanceNormalized = true;
    settings.computeCentroidSharedEdgeDistance = true;
    settings.computeCentroidSharedEdgeDistanceNormalized = true;
    settings.computeSharedEdgeUnionCentroidDistance = true;
    settings.computeSharedEdgeUnionCentroidDistanceNormalized = true;
    settings.computeSharedEdgeUnionAxisAngle = true;
    settings.computeJunctionAngleAverage = true;
    settings.computeJunctionAngleMax = true;
    settings.computeJunctionAngleMin = true;
    settings.computeJunctionAngleDifference = true;
    settings.computeJunctionAngleRatio = true;
    return settings;
}

QString findRealDivisionFile(const QDir &dir, const QFileInfo &jsonFile)
{
    const QString baseName = jsonFile.completeBaseName();
    const QStringList candidates = {
        dir.filePath(baseName + "_real_division_pairs.csv"),
        dir.filePath(baseName + "_real_division_pairs.txt"),
        dir.filePath(baseName + ".csv")
    };

    for (const QString &path : candidates) {
        QFileInfo info(path);
        if (info.exists() && info.isFile())
            return info.absoluteFilePath();
    }

    return {};
}

struct RealDivisionEntry {
    int firstId = -1;
    int secondId = -1;
    int time = -1;
};

QVector<RealDivisionEntry> readDivisionPairs(const QString &filePath, QStringList &warnings)
{
    QVector<RealDivisionEntry> pairs;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        warnings << QString("Failed to open %1: %2").arg(QFileInfo(filePath).fileName(),
                                                        file.errorString());
        return pairs;
    }

    QTextStream in(&file);
    int lineNumber = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        ++lineNumber;

        if (line.isEmpty())
            continue;

        if (lineNumber == 1 && !line.at(0).isDigit() && line.at(0) != '-') {
            // Treat non-numeric first line as header
            continue;
        }

        const QStringList parts = line.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            warnings << QString("Line %1: Expected at least two integers.").arg(lineNumber);
            continue;
        }

        bool okFirst = false;
        bool okSecond = false;
        bool okTime = true;
        const int firstId = parts.at(0).toInt(&okFirst);
        const int secondId = parts.at(1).toInt(&okSecond);
        int time = -1;
        if (parts.size() >= 3)
            time = parts.at(2).toInt(&okTime);

        if (!okFirst || !okSecond) {
            warnings << QString("Line %1: Unable to parse integers.").arg(lineNumber);
            continue;
        }
        if (!okTime && parts.size() >= 3)
            warnings << QString("Line %1: Unable to parse division time value, defaulting to -1.").arg(lineNumber);

        RealDivisionEntry entry;
        entry.firstId = firstId;
        entry.secondId = secondId;
        entry.time = okTime ? time : -1;
        pairs.append(entry);
    }

    return pairs;
}

QVector<QPair<int, int>> neighborPairs(QGraphicsScene *scene, const QVector<PolygonItem*> &polygons)
{
    QVector<QPair<int, int>> pairs = GeometryIO::neighborPairsFromScene(scene);
    if (!pairs.isEmpty())
        return pairs;
    return computeNeighborPairs(polygons);
}

double aspectRatioForPolygon(const PolygonItem *item)
{
    if (!item)
        return -1.0;
    const QRectF bounds = item->polygon().boundingRect();
    if (bounds.height() <= 0.0 || bounds.width() <= 0.0)
        return -1.0;
    const double ratio = bounds.width() / bounds.height();
    return ratio >= 1.0 ? ratio : 1.0 / ratio;
}

double circularityForPolygon(const PolygonItem *item)
{
    if (!item)
        return -1.0;
    const double area = item->area();
    const double per = item->perimeter();
    if (area <= 0.0 || per <= 0.0)
        return -1.0;
    return (4.0 * M_PI * area) / (per * per);
}

double polygonArea(const QPolygonF &p)
{
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
}

QPolygonF convexHull(const QPolygonF &points)
{
    QVector<QPointF> pts(points.begin(), points.end());
    if (pts.size() < 3)
        return QPolygonF(pts);

    const auto cross = [](const QPointF &a, const QPointF &b, const QPointF &c) {
        return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
    };

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
}

double solidityForPolygon(const PolygonItem *item)
{
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
}
} // namespace

BatchDivisionEstimator::GeometrySummary BatchDivisionEstimator::processNeighborGeometryDirectory(
        const QString &directoryPath, const NeighborPairGeometrySettings &settings)
{
    GeometrySummary summary;
    QDir dir(directoryPath);
    if (!dir.exists()) {
        summary.errors << QString("Directory does not exist: %1").arg(directoryPath);
        return summary;
    }

    const QFileInfoList files = dir.entryInfoList(QStringList() << "*.json", QDir::Files | QDir::Readable, QDir::Name);
    if (files.isEmpty()) {
        summary.warnings << QString("No .json files found in %1").arg(directoryPath);
        return summary;
    }

    for (const QFileInfo &info : files) {
        ++summary.filesProcessed;
        GeometryImportResult importResult = GeometryIO::importFromJson(info.absoluteFilePath(), false);
        if (!importResult.success || !importResult.scene) {
            summary.errors << QString("Failed to import %1: %2")
                              .arg(info.fileName())
                              .arg(importResult.errorMessage.isEmpty() ? QStringLiteral("Unknown error") : importResult.errorMessage);
            continue;
        }

        std::unique_ptr<QGraphicsScene> scene(importResult.scene);
        const QVector<PolygonItem*> polygons = collectPolygons(scene.get());
        if (polygons.size() < 2) {
            summary.warnings << QString("%1: requires at least two polygons for neighbor calculation.").arg(info.fileName());
            continue;
        }

        const QHash<int, PolygonItem*> polygonsById = polygonMap(polygons);
        QVector<QPair<int, int>> pairs = neighborPairs(scene.get(), polygons);

        QStringList parseWarnings;
        QHash<QString, int> realPairTimes;
        QSet<QString> realPairKeys;
        const QString realDivisionFile = findRealDivisionFile(dir, info);
        if (realDivisionFile.isEmpty()) {
            summary.warnings << QString("%1: no matching *_real_division_pairs.csv file found.").arg(info.fileName());
        } else {
            const QVector<RealDivisionEntry> realPairs = readDivisionPairs(realDivisionFile, parseWarnings);
            for (const RealDivisionEntry &pair : realPairs) {
                const QString key = normalizedPairKey(pair.firstId, pair.secondId);
                realPairKeys.insert(key);
                realPairTimes.insert(key, pair.time);
            }
            if (realPairs.isEmpty()) {
                summary.warnings << QString("%1: no valid real division pairs found in %2")
                                    .arg(info.fileName())
                                    .arg(QFileInfo(realDivisionFile).fileName());
            }
        }

        if (pairs.isEmpty()) {
            summary.warnings << QString("%1: no neighboring polygons detected.").arg(info.fileName());
            continue;
        }

        int pairIndex = 0;
        for (const auto &pair : pairs) {
            PolygonItem *first = polygonsById.value(pair.first, nullptr);
            PolygonItem *second = polygonsById.value(pair.second, nullptr);
            if (!first || !second) {
                summary.warnings << QString("%1: neighbor pair (%2, %3) references missing polygons.")
                                    .arg(info.fileName())
                                    .arg(pair.first)
                                    .arg(pair.second);
                continue;
            }

            NeighborPairGeometryCalculator::Result result = NeighborPairGeometryCalculator::calculateForPair(first, second, settings);
            result.first = nullptr;
            result.second = nullptr;

            GeometryEntry entry;
            entry.fileName = info.fileName();
            entry.pairIndex = ++pairIndex;
            entry.firstId = first->id();
            entry.secondId = second->id();
            const QString neighborKey = normalizedPairKey(entry.firstId, entry.secondId);
            entry.observedDivision = realPairKeys.contains(neighborKey);
            entry.divisionTime = entry.observedDivision ? realPairTimes.value(neighborKey, -1) : -1;
            entry.geometry = result;

            summary.entries.append(entry);
        }

        if (pairIndex > 0)
            ++summary.filesWithResults;

        for (const QString &warn : std::as_const(parseWarnings))
            summary.warnings << QString("%1: %2").arg(info.fileName(), warn);
    }

    return summary;
}

bool BatchDivisionEstimator::exportNeighborGeometryToCsv(const QString &filePath,
                                                         const QVector<GeometryEntry> &entries,
                                                         const NeighborPairGeometrySettings &settings,
                                                         QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open %1 for writing").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    const QStringList headers = buildHeaders(settings);
    stream << headers.join(',') << '\n';

    for (const GeometryEntry &entry : entries) {
        QStringList row = buildRow(entry, settings);
        for (QString &field : row)
            field = escapeForCsv(field);
        stream << row.join(',') << '\n';
    }

    return true;
}

BatchDivisionEstimator::SingleCellGeometrySummary BatchDivisionEstimator::processSingleCellGeometryDirectory(const QString &directoryPath)
{
    SingleCellGeometrySummary summary;
    QDir dir(directoryPath);
    if (!dir.exists()) {
        summary.errors << QString("Directory does not exist: %1").arg(directoryPath);
        return summary;
    }

    const QFileInfoList files = dir.entryInfoList(QStringList() << "*.json", QDir::Files | QDir::Readable, QDir::Name);
    if (files.isEmpty()) {
        summary.warnings << QString("No .json files found in %1").arg(directoryPath);
        return summary;
    }

    for (const QFileInfo &info : files) {
        ++summary.filesProcessed;
        GeometryImportResult importResult = GeometryIO::importFromJson(info.absoluteFilePath(), false);
        if (!importResult.success || !importResult.scene) {
            summary.errors << QString("Failed to import %1: %2")
                              .arg(info.fileName())
                              .arg(importResult.errorMessage.isEmpty() ? QStringLiteral("Unknown error") : importResult.errorMessage);
            continue;
        }

        std::unique_ptr<QGraphicsScene> scene(importResult.scene);
        const QVector<PolygonItem*> polygons = collectPolygons(scene.get());
        if (polygons.isEmpty()) {
            summary.warnings << QString("%1: no polygons detected.").arg(info.fileName());
            continue;
        }

        QStringList parseWarnings;
        QSet<int> dividingCellIds;
        const QString realDivisionFile = findRealDivisionFile(dir, info);
        if (realDivisionFile.isEmpty()) {
            summary.warnings << QString("%1: no matching *_real_division_pairs.csv file found.").arg(info.fileName());
        } else {
            const QVector<RealDivisionEntry> realPairs = readDivisionPairs(realDivisionFile, parseWarnings);
            for (const RealDivisionEntry &pair : realPairs) {
                dividingCellIds.insert(pair.firstId);
                dividingCellIds.insert(pair.secondId);
            }
            if (realPairs.isEmpty()) {
                summary.warnings << QString("%1: no valid real division pairs found in %2")
                                    .arg(info.fileName())
                                    .arg(QFileInfo(realDivisionFile).fileName());
            }
        }

        int rowsAdded = 0;
        for (PolygonItem *poly : polygons) {
            if (!poly)
                continue;

            SingleCellGeometryEntry entry;
            entry.fileName = info.fileName();
            entry.polygonId = poly->id();
            entry.inDividingPair = dividingCellIds.contains(entry.polygonId);
            entry.area = poly->area();
            entry.perimeter = poly->perimeter();
            entry.aspectRatio = aspectRatioForPolygon(poly);
            entry.circularity = circularityForPolygon(poly);
            entry.solidity = solidityForPolygon(poly);
            entry.vertexCount = poly->vertices().size();

            summary.entries.append(entry);
            ++rowsAdded;
        }

        if (rowsAdded > 0)
            ++summary.filesWithResults;

        for (const QString &warn : std::as_const(parseWarnings))
            summary.warnings << QString("%1: %2").arg(info.fileName(), warn);
    }

    return summary;
}

bool BatchDivisionEstimator::exportSingleCellGeometryToCsv(const QString &filePath,
                                                           const QVector<SingleCellGeometryEntry> &entries,
                                                           QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open %1 for writing").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    QStringList headers = buildSingleCellHeaders();
    for (QString &header : headers)
        header = escapeForCsv(header);
    stream << headers.join(',') << '\n';

    for (const SingleCellGeometryEntry &entry : entries) {
        QStringList row = buildSingleCellRow(entry);
        for (QString &field : row)
            field = escapeForCsv(field);
        stream << row.join(',') << '\n';
    }

    return true;
}

bool BatchDivisionEstimator::exportDivisionEstimatesToCsv(const QString &filePath,
                                                          const QVector<DivisionPairRow> &rows,
                                                          QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open %1 for writing").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    QStringList headers = divisionCsvHeaders();
    for (QString &header : headers)
        header = escapeForCsv(header);
    stream << headers.join(',') << '\n';

    for (const DivisionPairRow &row : rows) {
        QStringList fields = buildDivisionCsvRow(row);
        for (QString &field : fields)
            field = escapeForCsv(field);
        stream << fields.join(',') << '\n';
    }

    return true;
}

BatchDivisionEstimator::DivisionMetrics BatchDivisionEstimator::metricsFromCounts(int neighborCount,
                                                                                  int truePositives,
                                                                                  int falsePositives,
                                                                                  int falseNegatives,
                                                                                  int estimatedCount,
                                                                                  int realCount)
{
    DivisionMetrics metrics;
    metrics.estimatedPairs = estimatedCount;
    metrics.realPairs = realCount;
    metrics.neighborPairs = neighborCount;
    metrics.truePositives = truePositives;
    metrics.falsePositives = falsePositives;
    metrics.falseNegatives = falseNegatives;
    metrics.trueNegatives = std::max(0, neighborCount - truePositives - falsePositives - falseNegatives);

    metrics.precision = (truePositives + falsePositives) > 0
            ? static_cast<double>(truePositives) / static_cast<double>(truePositives + falsePositives)
            : -1.0;
    metrics.recall = (truePositives + falseNegatives) > 0
            ? static_cast<double>(truePositives) / static_cast<double>(truePositives + falseNegatives)
            : -1.0;

    metrics.f1 = (metrics.precision >= 0.0 && metrics.recall >= 0.0 && (metrics.precision + metrics.recall) > 0.0)
            ? 2.0 * metrics.precision * metrics.recall / (metrics.precision + metrics.recall)
            : -1.0;

    metrics.specificity = (metrics.trueNegatives + falsePositives) > 0
            ? static_cast<double>(metrics.trueNegatives) / static_cast<double>(metrics.trueNegatives + falsePositives)
            : -1.0;

    metrics.accuracy = neighborCount > 0
            ? static_cast<double>(truePositives + metrics.trueNegatives) / static_cast<double>(neighborCount)
            : -1.0;

    return metrics;
}

BatchDivisionEstimator::DivisionMetrics BatchDivisionEstimator::calculateMetrics(const QVector<QPair<int, int>> &neighborPairs,
                                                                                 const QVector<QPair<int, int>> &estimatedPairs,
                                                                                 const QVector<QPair<int, int>> &realPairs)
{
    const QSet<QString> neighborKeys = pairKeys(neighborPairs);
    const QSet<QString> estimatedKeys = pairKeys(estimatedPairs);
    const QSet<QString> realKeys = pairKeys(realPairs);

    int truePositives = 0;
    int falsePositives = 0;
    int falseNegatives = 0;

    for (const QString &key : estimatedKeys) {
        if (realKeys.contains(key))
            ++truePositives;
        else
            ++falsePositives;
    }

    for (const QString &key : realKeys) {
        if (!estimatedKeys.contains(key))
            ++falseNegatives;
    }

    return metricsFromCounts(neighborKeys.size(),
                             truePositives,
                             falsePositives,
                             falseNegatives,
                             estimatedKeys.size(),
                             realKeys.size());
}

QHash<QString, QSet<QString>> BatchDivisionEstimator::loadExceptionPairs(const QDir &directory, QStringList *warnings)
{
    QFileInfo exceptionFile(directory.filePath("exception.csv"));
    if (!exceptionFile.exists() || !exceptionFile.isFile())
        return {};

    return loadExceptionCsv(exceptionFile, warnings);
}

QSet<QString> BatchDivisionEstimator::exceptionPairsForFile(const QFileInfo &fileInfo, const QHash<QString, QSet<QString>> &exceptionMap)
{
    QSet<QString> pairs = exceptionMap.value(fileInfo.fileName());
    if (pairs.isEmpty())
        pairs = exceptionMap.value(fileInfo.completeBaseName());
    return pairs;
}

BatchDivisionEstimator::BatchResult BatchDivisionEstimator::estimateDirectory(const QString &directoryPath,
                                                                              const DivisionEstimator::Criterion &criterion)
{
    BatchResult summary;
    int totalTruePositives = 0;
    int totalFalsePositives = 0;
    int totalFalseNegatives = 0;
    int totalNeighborPairs = 0;
    int totalEstimatedPairs = 0;
    int totalRealPairs = 0;
    const NeighborPairGeometrySettings geometrySettings = exportSettingsForDivisionCsv(criterion);

    QDir dir(directoryPath);
    if (!dir.exists()) {
        summary.errors << QString("Directory does not exist: %1").arg(directoryPath);
        return summary;
    }

    QStringList exceptionWarnings;
    const QHash<QString, QSet<QString>> exceptions = loadExceptionPairs(dir, &exceptionWarnings);
    summary.warnings.append(exceptionWarnings);

    const QFileInfoList files = dir.entryInfoList(QStringList() << "*.json", QDir::Files | QDir::Readable, QDir::Name);
    if (files.isEmpty()) {
        summary.warnings << QString("No .json files found in %1").arg(directoryPath);
        return summary;
    }

    for (const QFileInfo &info : files) {
        ++summary.filesProcessed;

        const QString realDivisionFile = findRealDivisionFile(dir, info);
        if (realDivisionFile.isEmpty()) {
            summary.warnings << QString("%1: no matching *_real_division_pairs.csv file found.").arg(info.fileName());
            continue;
        }

        GeometryImportResult importResult = GeometryIO::importFromJson(info.absoluteFilePath(), false);
        if (!importResult.success || !importResult.scene) {
            summary.errors << QString("Failed to import %1: %2")
                              .arg(info.fileName())
                              .arg(importResult.errorMessage.isEmpty() ? QStringLiteral("Unknown error") : importResult.errorMessage);
            continue;
        }

        std::unique_ptr<QGraphicsScene> scene(importResult.scene);
        const QVector<PolygonItem*> polygons = collectPolygons(scene.get());
        if (polygons.size() < 2) {
            summary.warnings << QString("%1: requires at least two polygons for division estimation.").arg(info.fileName());
            continue;
        }

        const QHash<int, PolygonItem*> polygonsById = polygonMap(polygons);
        QVector<QPair<int, int>> neighbors = neighborPairs(scene.get(), polygons);
        if (neighbors.isEmpty()) {
            summary.warnings << QString("%1: no neighboring polygons detected.").arg(info.fileName());
            continue;
        }

        const QSet<QString> exceptionKeys = exceptionPairsForFile(info, exceptions);
        QVector<QPair<int, int>> filteredNeighbors;
        filteredNeighbors.reserve(neighbors.size());
        for (const auto &neighbor : std::as_const(neighbors)) {
            if (exceptionKeys.contains(normalizedPairKey(neighbor.first, neighbor.second)))
                continue;
            filteredNeighbors.append(neighbor);
        }
        neighbors = filteredNeighbors;

        const DivisionEstimator::EstimationResult estimation = DivisionEstimator::estimatePairs(polygons, criterion, scene.get());
        QVector<QPair<int, int>> estimatedPairs = estimation.matchedPairIds;
        QVector<QPair<int, int>> filteredEstimatedPairs;
        filteredEstimatedPairs.reserve(estimatedPairs.size());
        for (const auto &pair : std::as_const(estimatedPairs)) {
            const QString key = normalizedPairKey(pair.first, pair.second);
            if (exceptionKeys.contains(key))
                continue;
            filteredEstimatedPairs.append(pair);
        }
        estimatedPairs = filteredEstimatedPairs;
        const QSet<QString> estimatedKeys = pairKeys(estimatedPairs);

        QStringList parseWarnings;
        QVector<RealDivisionEntry> realEntries = readDivisionPairs(realDivisionFile, parseWarnings);
        QVector<QPair<int, int>> realPairs;
        realPairs.reserve(realEntries.size());
        QHash<QString, int> realPairTimes;
        for (const RealDivisionEntry &entry : std::as_const(realEntries)) {
            const QString key = normalizedPairKey(entry.firstId, entry.secondId);
            if (exceptionKeys.contains(key))
                continue;
            realPairs.append(QPair<int, int>(entry.firstId, entry.secondId));
            realPairTimes.insert(key, entry.time);
        }

        if (realPairs.isEmpty()) {
            summary.warnings << QString("%1: no valid real division pairs found in %2")
                                .arg(info.fileName())
                                .arg(QFileInfo(realDivisionFile).fileName());
        }
        const QSet<QString> realKeys = pairKeys(realPairs);

        for (const auto &neighbor : neighbors) {
            const QString neighborKey = normalizedPairKey(neighbor.first, neighbor.second);
            if (exceptionKeys.contains(neighborKey))
                continue;

            PolygonItem *firstPoly = polygonsById.value(neighbor.first, nullptr);
            PolygonItem *secondPoly = polygonsById.value(neighbor.second, nullptr);
            if (!firstPoly || !secondPoly) {
                summary.warnings << QString("%1: neighbor pair (%2, %3) references missing polygons.")
                                    .arg(info.fileName())
                                    .arg(neighbor.first)
                                    .arg(neighbor.second);
                continue;
            }

            NeighborPairGeometryCalculator::Result geometry = NeighborPairGeometryCalculator::calculateForPair(firstPoly, secondPoly, geometrySettings);
            geometry.first = nullptr;
            geometry.second = nullptr;

            DivisionPairRow pairRow;
            pairRow.fileName = info.fileName();
            pairRow.firstId = neighbor.first;
            pairRow.secondId = neighbor.second;
            pairRow.geometry = geometry;
            pairRow.observedDivision = realKeys.contains(neighborKey);
            pairRow.estimatedDivision = estimatedKeys.contains(neighborKey);
            pairRow.divisionTime = realPairTimes.value(neighborKey, -1);
            summary.pairRows.append(pairRow);
        }

        FileDivisionResult fileResult;
        fileResult.fileName = info.fileName();
        fileResult.neighborPairCount = neighbors.size();
        fileResult.estimatedPairs = estimatedPairs;
        fileResult.realPairs = realPairs;
        fileResult.metrics = calculateMetrics(neighbors, estimatedPairs, realPairs);

        summary.files.append(fileResult);
        ++summary.filesWithResults;

        totalNeighborPairs += fileResult.metrics.neighborPairs;
        totalTruePositives += fileResult.metrics.truePositives;
        totalFalsePositives += fileResult.metrics.falsePositives;
        totalFalseNegatives += fileResult.metrics.falseNegatives;
        totalEstimatedPairs += std::max(0, fileResult.metrics.estimatedPairs);
        totalRealPairs += std::max(0, fileResult.metrics.realPairs);

        for (const QString &warn : parseWarnings)
            summary.warnings << QString("%1: %2").arg(info.fileName(), warn);
    }

    summary.totals = metricsFromCounts(totalNeighborPairs,
                                       totalTruePositives,
                                       totalFalsePositives,
                                       totalFalseNegatives,
                                       totalEstimatedPairs,
                                       totalRealPairs);

    return summary;
}

QVector<BatchDivisionEstimator::DesignatedGeometryConfig> BatchDivisionEstimator::loadDesignatedGeometryConfigCsv(const QString &filePath,
                                                                                                                  QStringList *warnings,
                                                                                                                  QStringList *errors)
{
    QVector<DesignatedGeometryConfig> configs;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errors)
            errors->append(QString("Unable to open %1: %2").arg(filePath, file.errorString()));
        return configs;
    }

    QTextStream in(&file);
    if (in.atEnd()) {
        if (warnings)
            warnings->append(QString("%1 is empty.").arg(QFileInfo(filePath).fileName()));
        return configs;
    }

    const QString headerLine = in.readLine();
    const QStringList headers = parseCsvLine(headerLine);
    QHash<QString, int> columnIndex;
    for (int i = 0; i < headers.size(); ++i) {
        columnIndex.insert(normalizeFeatureName(headers.at(i)), i);
    }

    const auto column = [&columnIndex](const QString &name) {
        return columnIndex.value(normalizeFeatureName(name), -1);
    };

    const int featureIdx = column("feature");
    const int cutoffIdx = column("cutoff_value");
    const int directionIdx = column("below_or_above");
    const int noteIdx = column("note");
    const int detailDirectionIdx = column("direction");

    int lineNumber = 1;
    while (!in.atEnd()) {
        const QString rawLine = in.readLine();
        ++lineNumber;
        if (rawLine.trimmed().isEmpty())
            continue;

        const QStringList fields = parseCsvLine(rawLine);
        if (fields.isEmpty())
            continue;

        auto fieldValue = [&fields](int idx) -> QString {
            if (idx < 0 || idx >= fields.size())
                return {};
            return fields.at(idx).trimmed();
        };

        DesignatedGeometryConfig config;
        config.featureName = fieldValue(featureIdx >= 0 ? featureIdx : 0);
        config.direction = fieldValue(detailDirectionIdx);
        config.note = fieldValue(noteIdx);

        const QString cutoffStr = fieldValue(cutoffIdx >= 0 ? cutoffIdx : -1);
        bool okCutoff = false;
        config.cutoffValue = cutoffStr.toDouble(&okCutoff);
        if (!okCutoff) {
            if (errors)
                errors->append(QString("Line %1: invalid cutoff value '%2'.").arg(lineNumber).arg(cutoffStr));
            continue;
        }

        const QString dirValue = fieldValue(directionIdx);
        if (!dirValue.isEmpty()) {
            const QString normalizedDir = dirValue.trimmed().toLower();
            if (normalizedDir.contains("below"))
                config.requireAbove = false;
            else if (normalizedDir.contains("above"))
                config.requireAbove = true;
            else if (warnings)
                warnings->append(QString("Line %1: unrecognized below_or_above value '%2', defaulting to above.").arg(lineNumber).arg(dirValue));
        }

        if (config.featureName.isEmpty()) {
            if (errors)
                errors->append(QString("Line %1: missing feature name.").arg(lineNumber));
            continue;
        }

        configs.append(config);
    }

    if (configs.isEmpty() && errors && errors->isEmpty() && warnings)
        warnings->append(QString("No valid configurations found in %1.").arg(QFileInfo(filePath).fileName()));

    return configs;
}

QVector<BatchDivisionEstimator::DesignatedGeometryResult> BatchDivisionEstimator::estimateDesignatedGeometry(
        const QString &directoryPath,
        const QVector<DesignatedGeometryConfig> &configs)
{
    QVector<DesignatedGeometryResult> results;
    results.reserve(configs.size());

    const QVector<DivisionEstimator::FeatureOption> options = DivisionEstimator::featureOptions();

    for (const DesignatedGeometryConfig &config : configs) {
        DesignatedGeometryResult result;
        result.config = config;

        const QString key = featureKeyFromName(config.featureName);
        if (key.isEmpty()) {
            result.errors << QString("Unknown feature '%1'.").arg(config.featureName);
            results.append(result);
            continue;
        }

        DivisionEstimator::FeatureOption option;
        for (const auto &opt : options) {
            if (opt.key == key) {
                option = opt;
                break;
            }
        }

        if (option.key.isEmpty()) {
            result.errors << QString("Feature '%1' is not available for estimation.").arg(config.featureName);
            results.append(result);
            continue;
        }

        DivisionEstimator::Criterion criterion;
        if (!DivisionEstimator::populateCriterionFromSelection(option, config.requireAbove, config.cutoffValue, criterion)) {
            result.errors << QString("Failed to configure criterion for feature '%1'.").arg(config.featureName);
            results.append(result);
            continue;
        }

        result.featureKey = option.key;
        result.featureLabel = option.label;

        const BatchResult batch = estimateDirectory(directoryPath, criterion);
        result.metrics = batch.totals;
        result.filesProcessed = batch.filesProcessed;
        result.filesWithResults = batch.filesWithResults;
        result.warnings = batch.warnings;
        result.errors.append(batch.errors);

        results.append(result);
    }

    return results;
}

bool BatchDivisionEstimator::exportDesignatedGeometryMetricsToCsv(const QString &filePath,
                                                                  const QVector<DesignatedGeometryResult> &results,
                                                                  QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open %1 for writing").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    QStringList headers = {
        "feature",
        "feature_key",
        "threshold",
        "comparison",
        "files_processed",
        "files_with_results",
        "estimated_pairs",
        "real_pairs",
        "true_positive",
        "false_positive",
        "false_negative",
        "true_negative",
        "precision",
        "recall",
        "f1",
        "specificity",
        "accuracy",
        "direction",
        "note",
        "warnings",
        "errors"
    };
    for (QString &header : headers)
        header = escapeForCsv(header);
    stream << headers.join(',') << '\n';

    for (const DesignatedGeometryResult &res : results) {
        QStringList row;
        row << res.featureLabel;
        row << res.featureKey;
        row << QString::number(res.config.cutoffValue, 'f', 6);
        row << (res.config.requireAbove ? ">= cutoff" : "<= cutoff");
        row << QString::number(res.filesProcessed);
        row << QString::number(res.filesWithResults);
        row << QString::number(std::max(0, res.metrics.estimatedPairs));
        row << QString::number(std::max(0, res.metrics.realPairs));
        row << QString::number(res.metrics.truePositives);
        row << QString::number(res.metrics.falsePositives);
        row << QString::number(res.metrics.falseNegatives);
        row << QString::number(res.metrics.trueNegatives);
        row << formatValue(res.metrics.precision);
        row << formatValue(res.metrics.recall);
        row << formatValue(res.metrics.f1);
        row << formatValue(res.metrics.specificity);
        row << formatValue(res.metrics.accuracy);
        row << res.config.direction;
        row << res.config.note;
        row << res.warnings.join(" | ");
        row << res.errors.join(" | ");

        for (QString &field : row)
            field = escapeForCsv(field);
        stream << row.join(',') << '\n';
    }

    return true;
}
