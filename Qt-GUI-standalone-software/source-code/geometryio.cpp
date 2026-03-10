#include "geometryio.h"

#include "lineitem.h"
#include "neighborpair.h"
#include "polygonitem.h"
#include "qspinbox.h"
#include "vertexitem.h"

#include <QFile>
#include <QGraphicsScene>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRectF>
#include <QSizeF>
#include <QHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QSet>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QTextStream>

#include <limits>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
const char *kNeighborPairsPropertyName = "neighborPairs";

QString neighborPairKey(int a, int b)
{
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    return QString("%1-%2").arg(first).arg(second);
}

QVariant neighborPairsToVariant(const QVector<QPair<int, int>> &pairs)
{
    QVariantList list;
    for (const auto &pair : pairs) {
        QVariantList entry;
        entry << pair.first << pair.second;
        list.append(entry);
    }
    return list;
}

QVector<QPair<int, int>> neighborPairsFromVariant(const QVariant &variant)
{
    QVector<QPair<int, int>> pairs;
    if (!variant.isValid()) {
        return pairs;
    }

    const QVariantList list = variant.toList();
    QSet<QString> seenKeys;
    for (const QVariant &entryVar : list) {
        const QVariantList entry = entryVar.toList();
        if (entry.size() != 2)
            continue;

        bool okFirst = false;
        bool okSecond = false;
        const int first = entry.at(0).toInt(&okFirst);
        const int second = entry.at(1).toInt(&okSecond);
        if (!okFirst || !okSecond || first == second)
            continue;

        const QString key = neighborPairKey(first, second);
        if (seenKeys.contains(key))
            continue;
        seenKeys.insert(key);
        pairs.append(QPair<int, int>(first, second));
    }

    return pairs;
}

QVector<PolygonItem*> collectPolygons(QGraphicsScene *scene)
{
    QVector<PolygonItem*> polygons;
    if (!scene)
        return polygons;

    polygons.reserve(scene->items().size());
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type) {
            polygons.append(static_cast<PolygonItem*>(item));
        }
    }

    std::sort(polygons.begin(), polygons.end(), [](PolygonItem *a, PolygonItem *b){
        return a->id() < b->id();
    });

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

QVector<QPair<int, int>> computeNeighborPairsFromGeometry(const QVector<PolygonItem*> &polygons)
{
    QVector<QPair<int, int>> pairs;
    QSet<QString> seenKeys;

    for (int i = 0; i < polygons.size(); ++i) {
        for (int j = i + 1; j < polygons.size(); ++j) {
            NeighborPair pair(polygons[i], polygons[j]);
            if (!pair.isNeighbor())
                continue;

            const int firstId = polygons[i]->id();
            const int secondId = polygons[j]->id();
            const QString key = neighborPairKey(firstId, secondId);
            if (seenKeys.contains(key))
                continue;

            seenKeys.insert(key);
            pairs.append(QPair<int, int>(firstId, secondId));
        }
    }

    return pairs;
}

QVector<QPair<int, int>> filterNeighborPairsAgainstPolygons(const QVector<QPair<int, int>> &pairs,
                                                            const QHash<int, PolygonItem*> &polygonsById,
                                                            QStringList *warnings)
{
    QVector<QPair<int, int>> filtered;
    QSet<QString> seenKeys;
    for (const auto &pair : pairs) {
        const int firstId = pair.first;
        const int secondId = pair.second;
        if (firstId == secondId)
            continue;

        PolygonItem *first = polygonsById.value(firstId, nullptr);
        PolygonItem *second = polygonsById.value(secondId, nullptr);
        if (!first || !second) {
            if (warnings) {
                warnings->append(QString("Neighbor pair (%1, %2) references missing polygons.")
                                 .arg(firstId)
                                 .arg(secondId));
            }
            continue;
        }

        const QString key = neighborPairKey(firstId, secondId);
        if (seenKeys.contains(key))
            continue;

        seenKeys.insert(key);
        filtered.append(QPair<int, int>(firstId, secondId));
    }

    return filtered;
}

QVector<QPair<int, int>> parseNeighborPairsFromJson(const QJsonArray &array,
                                                    const QHash<int, PolygonItem*> &polygonsById,
                                                    QStringList &warnings)
{
    QVector<QPair<int, int>> pairs;
    QSet<QString> seenKeys;
    for (const auto &val : array) {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();
        if (!obj.contains("firstPolygonId") || !obj.contains("secondPolygonId")) {
            warnings.append("Neighbor pair entry is missing polygon ids.");
            continue;
        }

        const int firstId = obj.value("firstPolygonId").toInt();
        const int secondId = obj.value("secondPolygonId").toInt();
        if (firstId == secondId) {
            warnings.append(QString("Neighbor pair references the same polygon id %1 twice.").arg(firstId));
            continue;
        }

        if (!polygonsById.contains(firstId) || !polygonsById.contains(secondId)) {
            warnings.append(QString("Neighbor pair (%1, %2) references missing polygons.")
                            .arg(firstId)
                            .arg(secondId));
            continue;
        }

        const QString key = neighborPairKey(firstId, secondId);
        if (seenKeys.contains(key))
            continue;

        seenKeys.insert(key);
        pairs.append(QPair<int, int>(firstId, secondId));
    }

    return pairs;
}

NeighborPairGeometrySettings parseGeometrySettingsFromJson(const QJsonObject &obj, bool *parsed)
{
    NeighborPairGeometrySettings settings;
    bool anyField = false;
    const auto setBool = [&](const char *key, bool &target) {
        if (obj.contains(QLatin1String(key))) {
            target = obj.value(QLatin1String(key)).toBool(target);
            anyField = true;
        }
    };

    setBool("computeAreaRatio", settings.computeAreaRatio);
    setBool("computeAreaStats", settings.computeAreaStats);
    setBool("computePerimeterRatio", settings.computePerimeterRatio);
    setBool("computePerimeterStats", settings.computePerimeterStats);
    setBool("computeAspectRatio", settings.computeAspectRatio);
    setBool("computeAspectRatioStats", settings.computeAspectRatioStats);
    setBool("computeCircularityRatio", settings.computeCircularityRatio);
    setBool("computeCircularityStats", settings.computeCircularityStats);
    setBool("computeSolidityRatio", settings.computeSolidityRatio);
    setBool("computeSolidityStats", settings.computeSolidityStats);
    setBool("computeVertexCountRatio", settings.computeVertexCountRatio);
    setBool("computeVertexCountStats", settings.computeVertexCountStats);
    setBool("computeCentroidDistance", settings.computeCentroidDistance);
    setBool("computeUnionAspectRatio", settings.computeUnionAspectRatio);
    setBool("computeUnionCircularity", settings.computeUnionCircularity);
    setBool("computeUnionConvexDeficiency", settings.computeUnionConvexDeficiency);
    setBool("computeNormalizedSharedEdgeLength", settings.computeNormalizedSharedEdgeLength);
    setBool("computeSharedEdgeUnsharedVerticesDistance", settings.computeSharedEdgeUnsharedVerticesDistance);
    setBool("computeSharedEdgeUnsharedVerticesDistanceNormalized", settings.computeSharedEdgeUnsharedVerticesDistanceNormalized);
    setBool("computeCentroidSharedEdgeDistance", settings.computeCentroidSharedEdgeDistance);
    setBool("computeCentroidSharedEdgeDistanceNormalized", settings.computeCentroidSharedEdgeDistanceNormalized);
    setBool("computeJunctionAngleAverage", settings.computeJunctionAngleAverage);
    setBool("computeJunctionAngleMax", settings.computeJunctionAngleMax);
    setBool("computeJunctionAngleMin", settings.computeJunctionAngleMin);
    setBool("computeJunctionAngleDifference", settings.computeJunctionAngleDifference);
    setBool("computeJunctionAngleRatio", settings.computeJunctionAngleRatio);
    setBool("computeSharedEdgeUnionCentroidDistance", settings.computeSharedEdgeUnionCentroidDistance);
    setBool("computeSharedEdgeUnionCentroidDistanceNormalized", settings.computeSharedEdgeUnionCentroidDistanceNormalized);
    setBool("computeSharedEdgeUnionAxisAngle", settings.computeSharedEdgeUnionAxisAngle);

    if (parsed)
        *parsed = anyField;
    return settings;
}

NeighborPairGeometryCalculator::Result parseGeometryResultFromJson(const QJsonObject &obj, bool *parsed)
{
    NeighborPairGeometryCalculator::Result res;
    bool anyField = false;
    const auto setDouble = [&](const char *key, double &target) {
        const QJsonValue val = obj.value(QLatin1String(key));
        if (val.isDouble()) {
            target = val.toDouble();
            anyField = true;
        }
    };

    setDouble("areaRatio", res.areaRatio);
    setDouble("areaMean", res.areaMean);
    setDouble("areaMin", res.areaMin);
    setDouble("areaMax", res.areaMax);
    setDouble("areaDiff", res.areaDiff);
    setDouble("perimeterRatio", res.perimeterRatio);
    setDouble("perimeterMean", res.perimeterMean);
    setDouble("perimeterMin", res.perimeterMin);
    setDouble("perimeterMax", res.perimeterMax);
    setDouble("perimeterDiff", res.perimeterDiff);
    setDouble("aspectRatio", res.aspectRatio);
    setDouble("aspectMean", res.aspectMean);
    setDouble("aspectMin", res.aspectMin);
    setDouble("aspectMax", res.aspectMax);
    setDouble("aspectDiff", res.aspectDiff);
    setDouble("circularityRatio", res.circularityRatio);
    setDouble("circularityMean", res.circularityMean);
    setDouble("circularityMin", res.circularityMin);
    setDouble("circularityMax", res.circularityMax);
    setDouble("circularityDiff", res.circularityDiff);
    setDouble("solidityRatio", res.solidityRatio);
    setDouble("solidityMean", res.solidityMean);
    setDouble("solidityMin", res.solidityMin);
    setDouble("solidityMax", res.solidityMax);
    setDouble("solidityDiff", res.solidityDiff);
    setDouble("vertexCountRatio", res.vertexCountRatio);
    setDouble("vertexCountMean", res.vertexCountMean);
    setDouble("vertexCountMin", res.vertexCountMin);
    setDouble("vertexCountMax", res.vertexCountMax);
    setDouble("vertexCountDiff", res.vertexCountDiff);
    setDouble("centroidDistance", res.centroidDistance);
    setDouble("unionAspectRatio", res.unionAspectRatio);
    setDouble("unionCircularity", res.unionCircularity);
    setDouble("unionConvexDeficiency", res.unionConvexDeficiency);
    setDouble("normalizedSharedEdgeLength", res.normalizedSharedEdgeLength);
    setDouble("sharedEdgeLength", res.sharedEdgeLength);
    setDouble("sharedEdgeUnsharedVerticesDistance", res.sharedEdgeUnsharedVerticesDistance);
    setDouble("sharedEdgeUnsharedVerticesDistanceNormalized", res.sharedEdgeUnsharedVerticesDistanceNormalized);
    setDouble("centroidSharedEdgeDistance", res.centroidSharedEdgeDistance);
    setDouble("centroidSharedEdgeDistanceNormalized", res.centroidSharedEdgeDistanceNormalized);
    setDouble("centroidDistanceNormalized", res.centroidDistanceNormalized);
    setDouble("junctionAngleAverageDegrees", res.junctionAngleAverageDegrees);
    setDouble("junctionAngleMaxDegrees", res.junctionAngleMaxDegrees);
    setDouble("junctionAngleMinDegrees", res.junctionAngleMinDegrees);
    setDouble("junctionAngleDifferenceDegrees", res.junctionAngleDifferenceDegrees);
    setDouble("junctionAngleRatio", res.junctionAngleRatio);
    setDouble("sharedEdgeUnionCentroidDistance", res.sharedEdgeUnionCentroidDistance);
    setDouble("sharedEdgeUnionCentroidDistanceNormalized", res.sharedEdgeUnionCentroidDistanceNormalized);
    setDouble("sharedEdgeUnionAxisAngleDegrees", res.sharedEdgeUnionAxisAngleDegrees);

    if (parsed)
        *parsed = anyField;
    return res;
}

BatchDivisionEstimator::DivisionMetrics parsePerformanceMetricsFromJson(const QJsonObject &obj, bool *parsed)
{
    BatchDivisionEstimator::DivisionMetrics metrics;
    bool anyField = false;
    const auto setInt = [&](const char *key, int &target) {
        if (obj.contains(QLatin1String(key))) {
            target = obj.value(QLatin1String(key)).toInt(target);
            anyField = true;
        }
    };
    const auto setDouble = [&](const char *key, double &target) {
        const QJsonValue val = obj.value(QLatin1String(key));
        if (val.isDouble()) {
            target = val.toDouble();
            anyField = true;
        }
    };

    setInt("estimatedPairs", metrics.estimatedPairs);
    setInt("realPairs", metrics.realPairs);
    setInt("neighborPairs", metrics.neighborPairs);
    setInt("truePositive", metrics.truePositives);
    setInt("falsePositive", metrics.falsePositives);
    setInt("falseNegative", metrics.falseNegatives);
    setInt("trueNegative", metrics.trueNegatives);

    setDouble("precision", metrics.precision);
    setDouble("recall", metrics.recall);
    setDouble("f1", metrics.f1);
    setDouble("specificity", metrics.specificity);
    setDouble("accuracy", metrics.accuracy);

    if (parsed)
        *parsed = anyField;
    return metrics;
}

DivisionEstimator::Criterion::MatchingMode matchingModeFromString(const QString &mode, bool *ok)
{
    if (mode == QLatin1String("LocalOptimal")) {
        if (ok) *ok = true;
        return DivisionEstimator::Criterion::MatchingMode::LocalOptimal;
    }
    if (mode == QLatin1String("GlobalMaximumWeight")) {
        if (ok) *ok = true;
        return DivisionEstimator::Criterion::MatchingMode::GlobalMaximumWeight;
    }
    if (mode == QLatin1String("Unconstrained")) {
        if (ok) *ok = true;
        return DivisionEstimator::Criterion::MatchingMode::Unconstrained;
    }
    if (ok) *ok = false;
    return DivisionEstimator::Criterion::MatchingMode::GlobalMaximumWeight;
}

DivisionEstimator::Criterion parseEstimationRuleFromJson(const QJsonObject &obj, bool *parsed)
{
    DivisionEstimator::Criterion criterion;
    bool modeParsed = false;
    criterion.featureKey = obj.value("featureKey").toString();
    criterion.featureLabel = obj.value("featureLabel").toString();
    criterion.requireAbove = obj.value("requireAbove").toBool(true);
    criterion.threshold = obj.value("threshold").toDouble(criterion.threshold);
    criterion.matchingMode = matchingModeFromString(obj.value("matchingMode").toString(), &modeParsed);

    bool settingsParsed = false;
    criterion.settings = parseGeometrySettingsFromJson(obj.value("geometrySettings").toObject(), &settingsParsed);

    if (parsed)
        *parsed = modeParsed || settingsParsed || obj.contains("featureKey") || obj.contains("threshold");
    return criterion;
}

QVector<BatchDivisionEstimator::GeometryEntry> parseGeometryEntriesFromJson(const QJsonArray &array,
                                                                            const QHash<int, PolygonItem*> &polygonsById,
                                                                            QStringList &warnings)
{
    QVector<BatchDivisionEstimator::GeometryEntry> entries;
    for (const auto &val : array) {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();
        const int firstId = obj.value("firstPolygonId").toInt(-1);
        const int secondId = obj.value("secondPolygonId").toInt(-1);
        if (firstId < 0 || secondId < 0) {
            warnings.append("Geometry entry is missing polygon ids.");
            continue;
        }

        if (!polygonsById.contains(firstId) || !polygonsById.contains(secondId)) {
            warnings.append(QString("Geometry entry (%1, %2) references missing polygons.")
                            .arg(firstId)
                            .arg(secondId));
            continue;
        }

        BatchDivisionEstimator::GeometryEntry entry;
        entry.fileName = obj.value("fileName").toString();
        entry.pairIndex = obj.value("pairIndex").toInt(entries.size() + 1);
        entry.firstId = firstId;
        entry.secondId = secondId;
        entry.observedDivision = obj.value("observedDivision").toBool(false);
        entry.divisionTime = obj.value("divisionTime").toInt(-1);
        if (!entry.observedDivision)
            entry.divisionTime = -1;

        bool geometryParsed = false;
        entry.geometry = parseGeometryResultFromJson(obj.value("geometry").toObject(), &geometryParsed);
        if (!geometryParsed) {
            warnings.append(QString("Geometry values for pair (%1, %2) could not be read.")
                            .arg(firstId)
                            .arg(secondId));
        }

        entries.append(entry);
    }
    return entries;
}

QVector<QPair<int, int>> parseDivisionPairsFromJson(const QJsonArray &array,
                                                    const QHash<int, PolygonItem*> &polygonsById,
                                                    QStringList &warnings,
                                                    const QString &label)
{
    QVector<QPair<int, int>> pairs;
    QSet<QString> seenKeys;
    for (const auto &val : array) {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();
        const int firstId = obj.value("firstPolygonId").toInt(-1);
        const int secondId = obj.value("secondPolygonId").toInt(-1);
        if (firstId < 0 || secondId < 0 || firstId == secondId) {
            warnings.append(QString("%1 entry is missing valid polygon ids.").arg(label));
            continue;
        }

        if (!polygonsById.contains(firstId) || !polygonsById.contains(secondId)) {
            warnings.append(QString("%1 (%2, %3) references missing polygons.")
                            .arg(label)
                            .arg(firstId)
                            .arg(secondId));
            continue;
        }

        const QString key = neighborPairKey(firstId, secondId);
        if (seenKeys.contains(key))
            continue;

        seenKeys.insert(key);
        pairs.append(QPair<int, int>(firstId, secondId));
    }
    return pairs;
}

QVector<DivisionPairRecord> parseDivisionPairRecordsFromJson(const QJsonArray &array,
                                                             const QHash<int, PolygonItem*> &polygonsById,
                                                             QStringList &warnings)
{
    QVector<DivisionPairRecord> records;
    QSet<QString> seenKeys;
    for (const auto &val : array) {
        if (!val.isObject())
            continue;

        const QJsonObject obj = val.toObject();
        const int firstId = obj.value("firstPolygonId").toInt(-1);
        const int secondId = obj.value("secondPolygonId").toInt(-1);
        if (firstId < 0 || secondId < 0 || firstId == secondId) {
            warnings.append("Real division entry is missing valid polygon ids.");
            continue;
        }

        if (!polygonsById.contains(firstId) || !polygonsById.contains(secondId)) {
            warnings.append(QString("Real division pair (%1, %2) references missing polygons.")
                            .arg(firstId)
                            .arg(secondId));
            continue;
        }

        const QString key = neighborPairKey(firstId, secondId);
        if (seenKeys.contains(key))
            continue;
        seenKeys.insert(key);

        DivisionPairRecord record;
        record.firstId = firstId;
        record.secondId = secondId;
        record.time = obj.value("time").toInt(-1);
        records.append(record);
    }
    return records;
}
} // namespace

GeometryImportResult GeometryIO::importFromJson(const QString &filePath, bool allowVertexManualMove)
{
    GeometryImportResult result;
    result.geometrySettings = NeighborPairGeometryCalculator::currentSettings();

    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly)){
        result.errorMessage = QString("Failed to open %1").arg(filePath);
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if(parseError.error != QJsonParseError::NoError || !doc.isObject()){
        result.errorMessage = QString("Invalid JSON: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonArray vertexArray = root.value("vertices").toArray();
    if(vertexArray.isEmpty()){
        result.errorMessage = "No vertices found in JSON file.";
        return result;
    }

    struct VertexData{int id; double x; double y;};
    QVector<VertexData> vertices;
    vertices.reserve(vertexArray.size());

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();

    for(const auto &val : vertexArray){
        if(!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        if(!obj.contains("id") || !obj.contains("x") || !obj.contains("y")) continue;

        const int id = obj.value("id").toInt();
        const double x = obj.value("x").toDouble();
        const double y = obj.value("y").toDouble();

        vertices.push_back({id,x,y});
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }

    if(vertices.isEmpty()){
        result.errorMessage = "No valid vertices could be read from the JSON file.";
        return result;
    }

    const double margin = 20.0;
    const double width = std::max(10.0, maxX - minX);
    const double height = std::max(10.0, maxY - minY);
    QRectF sceneRect(QPointF(minX, minY), QSizeF(width, height));
    sceneRect = sceneRect.adjusted(-margin, -margin, margin, margin);

    auto *scene = new QGraphicsScene();
    scene->setSceneRect(sceneRect);

    QHash<int, VertexItem*> vertexById;
    QHash<int, PolygonItem*> polygonById;
    int maxVertexId = 0;
    for(const auto &vData : vertices){
        auto *vertex = new VertexItem(vData.id, QPointF(vData.x, vData.y));
        scene->addItem(vertex);
        vertex->setFlag(QGraphicsItem::ItemIsMovable, allowVertexManualMove);

        vertexById.insert(vData.id, vertex);
        maxVertexId = std::max(maxVertexId, vData.id);
    }

    LineItem::setNextId(1);
    const QJsonArray lineArray = root.value("lines").toArray();
    for(const auto &val : lineArray){
        if(!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        if(!obj.contains("id") || !obj.contains("startVertexId") || !obj.contains("endVertexId")) continue;

        const int id = obj.value("id").toInt();
        const int startId = obj.value("startVertexId").toInt();
        const int endId = obj.value("endVertexId").toInt();

        VertexItem *v1 = vertexById.value(startId, nullptr);
        VertexItem *v2 = vertexById.value(endId, nullptr);

        if(!v1 || !v2){
            result.warnings << QString("Line %1 references missing vertices (%2, %3)").arg(id).arg(startId).arg(endId);
            continue;
        }

        auto *line = new LineItem(v1, v2, id);
        scene->addItem(line);
    }

    PolygonItem::setNextId(0);
    const QJsonArray polygonArray = root.value("polygons").toArray();
    for(const auto &val : polygonArray){
        if(!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        if(!obj.contains("id") || !obj.contains("vertexIds")) continue;

        const int id = obj.value("id").toInt();
        const QJsonArray vertexIds = obj.value("vertexIds").toArray();
        if(vertexIds.size() < 3){
            result.warnings << QString("Polygon %1 has fewer than 3 vertices").arg(id);
            continue;
        }

        QVector<VertexItem*> verts;
        verts.reserve(vertexIds.size());
        bool missingVertex = false;
        for(const auto &vIdVal : vertexIds){
            const int vId = vIdVal.toInt();
            VertexItem *v = vertexById.value(vId, nullptr);
            if(!v){
                result.warnings << QString("Polygon %1 references missing vertex %2").arg(id).arg(vId);
                missingVertex = true;
                break;
            }
            verts.push_back(v);
        }

        if(missingVertex) continue;

        auto *poly = new PolygonItem(verts, scene, false, id);
        scene->addItem(poly);
        polygonById.insert(id, poly);
    }

    const QJsonArray neighborPairArray = root.value("neighborPairs").toArray();
    if (!neighborPairArray.isEmpty()) {
        result.neighborPairs = parseNeighborPairsFromJson(neighborPairArray, polygonById, result.warnings);
        GeometryIO::setNeighborPairsOnScene(scene, result.neighborPairs);
    }

    const QJsonObject neighborGeometryObject = root.value("neighborPairGeometry").toObject();
    if (!neighborGeometryObject.isEmpty()) {
        bool geometrySettingsParsed = false;
        result.geometrySettings = parseGeometrySettingsFromJson(neighborGeometryObject.value("settings").toObject(), &geometrySettingsParsed);
        result.geometryEntries = parseGeometryEntriesFromJson(neighborGeometryObject.value("entries").toArray(), polygonById, result.warnings);
        result.hasGeometryData = geometrySettingsParsed || !result.geometryEntries.isEmpty();
    }

    if (result.neighborPairs.isEmpty() && !result.geometryEntries.isEmpty()) {
        QSet<QString> seenKeys;
        for (const auto &entry : std::as_const(result.geometryEntries)) {
            const QString key = neighborPairKey(entry.firstId, entry.secondId);
            if (seenKeys.contains(key))
                continue;
            seenKeys.insert(key);
            result.neighborPairs.append(QPair<int, int>(entry.firstId, entry.secondId));
        }
        if (!result.neighborPairs.isEmpty())
            GeometryIO::setNeighborPairsOnScene(scene, result.neighborPairs);
    }

    bool estimationParsed = false;
    const QJsonObject estimationObject = root.value("estimationRule").toObject();
    if (!estimationObject.isEmpty()) {
        result.estimationCriterion = parseEstimationRuleFromJson(estimationObject, &estimationParsed);
        result.hasEstimationCriterion = estimationParsed;
    }

    const QJsonArray estimatedDivisionsArray = root.value("estimatedDivisions").toArray();
    if (!estimatedDivisionsArray.isEmpty()) {
        result.estimatedDivisionPairs = parseDivisionPairsFromJson(estimatedDivisionsArray, polygonById, result.warnings, QStringLiteral("Estimated division pair"));
    }

    const QJsonArray realDivisionsArray = root.value("realDivisions").toArray();
    if (!realDivisionsArray.isEmpty()) {
        result.realDivisionRecords = parseDivisionPairRecordsFromJson(realDivisionsArray, polygonById, result.warnings);
    }

    bool metricsParsed = false;
    const QJsonObject metricsObject = root.value("performanceMetrics").toObject();
    if (!metricsObject.isEmpty()) {
        result.performanceMetrics = parsePerformanceMetricsFromJson(metricsObject, &metricsParsed);
        result.hasPerformanceMetrics = metricsParsed;
    }

    result.scene = scene;
    result.nextVertexId = maxVertexId + 1;
    result.sceneRect = sceneRect;
    result.success = true;

    return result;
}

void GeometryIO::setNeighborPairsOnScene(QGraphicsScene *scene, const QVector<QPair<int, int>> &pairs)
{
    if (!scene)
        return;

    scene->setProperty(kNeighborPairsPropertyName, neighborPairsToVariant(pairs));
}

QVector<QPair<int, int>> GeometryIO::neighborPairsFromScene(QGraphicsScene *scene)
{
    QVector<PolygonItem*> polygons = collectPolygons(scene);
    const QHash<int, PolygonItem*> polygonsById = polygonMap(polygons);

    const QVector<QPair<int, int>> fromProperty = filterNeighborPairsAgainstPolygons(
        neighborPairsFromVariant(scene ? scene->property(kNeighborPairsPropertyName) : QVariant()),
        polygonsById,
        nullptr);

    if (!fromProperty.isEmpty())
        return fromProperty;

    return computeNeighborPairsFromGeometry(polygons);
}

namespace {
QJsonArray serializeVertices(QGraphicsScene *scene)
{
    std::vector<VertexItem*> vertices;
    vertices.reserve(scene->items().size());
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == VertexItem::Type) {
            vertices.push_back(static_cast<VertexItem*>(item));
        }
    }

    std::sort(vertices.begin(), vertices.end(), [](VertexItem *a, VertexItem *b){
        return a->id() < b->id();
    });

    QJsonArray array;
    for (VertexItem *vertex : vertices) {
        const QPointF pos = vertex->pos();
        QJsonObject obj;
        obj.insert("id", vertex->id());
        obj.insert("x", pos.x());
        obj.insert("y", pos.y());
        array.append(obj);
    }

    return array;
}

QJsonArray serializeLines(QGraphicsScene *scene)
{
    std::vector<LineItem*> lines;
    lines.reserve(scene->items().size());
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == LineItem::Type) {
            lines.push_back(static_cast<LineItem*>(item));
        }
    }

    std::sort(lines.begin(), lines.end(), [](LineItem *a, LineItem *b){
        return a->id() < b->id();
    });

    QJsonArray array;
    for (LineItem *line : lines) {
        QJsonObject obj;
        obj.insert("id", line->id());
        obj.insert("startVertexId", line->v1Id());
        obj.insert("endVertexId", line->v2Id());
        array.append(obj);
    }

    return array;
}

QJsonArray serializePolygons(QGraphicsScene *scene)
{
    std::vector<PolygonItem*> polygons;
    polygons.reserve(scene->items().size());
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type) {
            polygons.push_back(static_cast<PolygonItem*>(item));
        }
    }

    std::sort(polygons.begin(), polygons.end(), [](PolygonItem *a, PolygonItem *b){
        return a->id() < b->id();
    });

    QJsonArray array;
    for (PolygonItem *polygon : polygons) {
        QJsonObject obj;
        obj.insert("id", polygon->id());
        QJsonArray vertexIdsArray;
        for (int vId : polygon->vertexIds()) {
            vertexIdsArray.append(vId);
        }
        obj.insert("vertexIds", vertexIdsArray);
        array.append(obj);
    }

    return array;
}

QJsonArray serializeNeighborPairs(QGraphicsScene *scene)
{
    const QVector<QPair<int, int>> pairs = GeometryIO::neighborPairsFromScene(scene);

    QJsonArray array;
    for (const auto &pair : pairs) {
        QJsonObject obj;
        obj.insert("firstPolygonId", pair.first);
        obj.insert("secondPolygonId", pair.second);
        array.append(obj);
    }

    return array;
}

QJsonValue numberOrNull(double value)
{
    return std::isfinite(value) ? QJsonValue(value) : QJsonValue();
}

QJsonValue intOrNull(int value)
{
    return value >= 0 ? QJsonValue(value) : QJsonValue();
}

QJsonObject serializeGeometrySettingsObject(const NeighborPairGeometrySettings &settings)
{
    QJsonObject obj;
    obj.insert("computeAreaRatio", settings.computeAreaRatio);
    obj.insert("computeAreaStats", settings.computeAreaStats);
    obj.insert("computePerimeterRatio", settings.computePerimeterRatio);
    obj.insert("computePerimeterStats", settings.computePerimeterStats);
    obj.insert("computeAspectRatio", settings.computeAspectRatio);
    obj.insert("computeAspectRatioStats", settings.computeAspectRatioStats);
    obj.insert("computeCircularityRatio", settings.computeCircularityRatio);
    obj.insert("computeCircularityStats", settings.computeCircularityStats);
    obj.insert("computeSolidityRatio", settings.computeSolidityRatio);
    obj.insert("computeSolidityStats", settings.computeSolidityStats);
    obj.insert("computeVertexCountRatio", settings.computeVertexCountRatio);
    obj.insert("computeVertexCountStats", settings.computeVertexCountStats);
    obj.insert("computeCentroidDistance", settings.computeCentroidDistance);
    obj.insert("computeUnionAspectRatio", settings.computeUnionAspectRatio);
    obj.insert("computeUnionCircularity", settings.computeUnionCircularity);
    obj.insert("computeUnionConvexDeficiency", settings.computeUnionConvexDeficiency);
    obj.insert("computeNormalizedSharedEdgeLength", settings.computeNormalizedSharedEdgeLength);
    obj.insert("computeSharedEdgeUnsharedVerticesDistance", settings.computeSharedEdgeUnsharedVerticesDistance);
    obj.insert("computeSharedEdgeUnsharedVerticesDistanceNormalized", settings.computeSharedEdgeUnsharedVerticesDistanceNormalized);
    obj.insert("computeCentroidSharedEdgeDistance", settings.computeCentroidSharedEdgeDistance);
    obj.insert("computeCentroidSharedEdgeDistanceNormalized", settings.computeCentroidSharedEdgeDistanceNormalized);
    obj.insert("computeJunctionAngleAverage", settings.computeJunctionAngleAverage);
    obj.insert("computeJunctionAngleMax", settings.computeJunctionAngleMax);
    obj.insert("computeJunctionAngleMin", settings.computeJunctionAngleMin);
    obj.insert("computeJunctionAngleDifference", settings.computeJunctionAngleDifference);
    obj.insert("computeJunctionAngleRatio", settings.computeJunctionAngleRatio);
    obj.insert("computeSharedEdgeUnionCentroidDistance", settings.computeSharedEdgeUnionCentroidDistance);
    obj.insert("computeSharedEdgeUnionCentroidDistanceNormalized", settings.computeSharedEdgeUnionCentroidDistanceNormalized);
    obj.insert("computeSharedEdgeUnionAxisAngle", settings.computeSharedEdgeUnionAxisAngle);
    return obj;
}

QJsonObject serializeGeometryResult(const NeighborPairGeometryCalculator::Result &res)
{
    QJsonObject obj;
    obj.insert("areaRatio", numberOrNull(res.areaRatio));
    obj.insert("areaMean", numberOrNull(res.areaMean));
    obj.insert("areaMin", numberOrNull(res.areaMin));
    obj.insert("areaMax", numberOrNull(res.areaMax));
    obj.insert("areaDiff", numberOrNull(res.areaDiff));
    obj.insert("perimeterRatio", numberOrNull(res.perimeterRatio));
    obj.insert("perimeterMean", numberOrNull(res.perimeterMean));
    obj.insert("perimeterMin", numberOrNull(res.perimeterMin));
    obj.insert("perimeterMax", numberOrNull(res.perimeterMax));
    obj.insert("perimeterDiff", numberOrNull(res.perimeterDiff));
    obj.insert("aspectRatio", numberOrNull(res.aspectRatio));
    obj.insert("aspectMean", numberOrNull(res.aspectMean));
    obj.insert("aspectMin", numberOrNull(res.aspectMin));
    obj.insert("aspectMax", numberOrNull(res.aspectMax));
    obj.insert("aspectDiff", numberOrNull(res.aspectDiff));
    obj.insert("circularityRatio", numberOrNull(res.circularityRatio));
    obj.insert("circularityMean", numberOrNull(res.circularityMean));
    obj.insert("circularityMin", numberOrNull(res.circularityMin));
    obj.insert("circularityMax", numberOrNull(res.circularityMax));
    obj.insert("circularityDiff", numberOrNull(res.circularityDiff));
    obj.insert("solidityRatio", numberOrNull(res.solidityRatio));
    obj.insert("solidityMean", numberOrNull(res.solidityMean));
    obj.insert("solidityMin", numberOrNull(res.solidityMin));
    obj.insert("solidityMax", numberOrNull(res.solidityMax));
    obj.insert("solidityDiff", numberOrNull(res.solidityDiff));
    obj.insert("vertexCountRatio", numberOrNull(res.vertexCountRatio));
    obj.insert("vertexCountMean", numberOrNull(res.vertexCountMean));
    obj.insert("vertexCountMin", numberOrNull(res.vertexCountMin));
    obj.insert("vertexCountMax", numberOrNull(res.vertexCountMax));
    obj.insert("vertexCountDiff", numberOrNull(res.vertexCountDiff));
    obj.insert("centroidDistance", numberOrNull(res.centroidDistance));
    obj.insert("unionAspectRatio", numberOrNull(res.unionAspectRatio));
    obj.insert("unionCircularity", numberOrNull(res.unionCircularity));
    obj.insert("unionConvexDeficiency", numberOrNull(res.unionConvexDeficiency));
    obj.insert("normalizedSharedEdgeLength", numberOrNull(res.normalizedSharedEdgeLength));
    obj.insert("sharedEdgeLength", numberOrNull(res.sharedEdgeLength));
    obj.insert("sharedEdgeUnsharedVerticesDistance", numberOrNull(res.sharedEdgeUnsharedVerticesDistance));
    obj.insert("sharedEdgeUnsharedVerticesDistanceNormalized", numberOrNull(res.sharedEdgeUnsharedVerticesDistanceNormalized));
    obj.insert("centroidSharedEdgeDistance", numberOrNull(res.centroidSharedEdgeDistance));
    obj.insert("centroidSharedEdgeDistanceNormalized", numberOrNull(res.centroidSharedEdgeDistanceNormalized));
    obj.insert("centroidDistanceNormalized", numberOrNull(res.centroidDistanceNormalized));
    obj.insert("junctionAngleAverageDegrees", numberOrNull(res.junctionAngleAverageDegrees));
    obj.insert("junctionAngleMaxDegrees", numberOrNull(res.junctionAngleMaxDegrees));
    obj.insert("junctionAngleMinDegrees", numberOrNull(res.junctionAngleMinDegrees));
    obj.insert("junctionAngleDifferenceDegrees", numberOrNull(res.junctionAngleDifferenceDegrees));
    obj.insert("junctionAngleRatio", numberOrNull(res.junctionAngleRatio));
    obj.insert("sharedEdgeUnionCentroidDistance", numberOrNull(res.sharedEdgeUnionCentroidDistance));
    obj.insert("sharedEdgeUnionCentroidDistanceNormalized", numberOrNull(res.sharedEdgeUnionCentroidDistanceNormalized));
    obj.insert("sharedEdgeUnionAxisAngleDegrees", numberOrNull(res.sharedEdgeUnionAxisAngleDegrees));
    return obj;
}

QJsonObject serializeGeometryEntry(const BatchDivisionEstimator::GeometryEntry &entry)
{
    QJsonObject obj;
    obj.insert("fileName", entry.fileName);
    obj.insert("pairIndex", entry.pairIndex);
    obj.insert("firstPolygonId", entry.firstId);
    obj.insert("secondPolygonId", entry.secondId);
    obj.insert("observedDivision", entry.observedDivision);
    obj.insert("divisionTime", entry.observedDivision ? entry.divisionTime : -1);
    obj.insert("geometry", serializeGeometryResult(entry.geometry));
    return obj;
}

QJsonArray serializeDivisionPairs(const QVector<QPair<int, int>> &pairs)
{
    QJsonArray array;
    for (const auto &pair : pairs) {
        QJsonObject obj;
        obj.insert("firstPolygonId", pair.first);
        obj.insert("secondPolygonId", pair.second);
        array.append(obj);
    }
    return array;
}

QJsonArray serializeDivisionPairRecords(const QVector<DivisionPairRecord> &pairs)
{
    QJsonArray array;
    for (const auto &pair : pairs) {
        QJsonObject obj;
        obj.insert("firstPolygonId", pair.firstId);
        obj.insert("secondPolygonId", pair.secondId);
        if (pair.time >= 0)
            obj.insert("time", pair.time);
        array.append(obj);
    }
    return array;
}

QString matchingModeToString(DivisionEstimator::Criterion::MatchingMode mode)
{
    switch (mode) {
    case DivisionEstimator::Criterion::MatchingMode::LocalOptimal:
        return QStringLiteral("LocalOptimal");
    case DivisionEstimator::Criterion::MatchingMode::GlobalMaximumWeight:
        return QStringLiteral("GlobalMaximumWeight");
    case DivisionEstimator::Criterion::MatchingMode::Unconstrained:
        return QStringLiteral("Unconstrained");
    }
    return QStringLiteral("Unknown");
}

QJsonObject serializeEstimationRule(const DivisionEstimator::Criterion &criterion)
{
    QJsonObject obj;
    obj.insert("featureKey", criterion.featureKey);
    obj.insert("featureLabel", criterion.featureLabel);
    obj.insert("requireAbove", criterion.requireAbove);
    obj.insert("threshold", criterion.threshold);
    obj.insert("matchingMode", matchingModeToString(criterion.matchingMode));
    obj.insert("geometrySettings", serializeGeometrySettingsObject(criterion.settings));
    return obj;
}

QJsonObject serializePerformanceMetrics(const BatchDivisionEstimator::DivisionMetrics &metrics)
{
    QJsonObject obj;
    obj.insert("estimatedPairs", intOrNull(metrics.estimatedPairs));
    obj.insert("realPairs", intOrNull(metrics.realPairs));
    obj.insert("neighborPairs", intOrNull(metrics.neighborPairs));
    obj.insert("truePositive", metrics.truePositives);
    obj.insert("falsePositive", metrics.falsePositives);
    obj.insert("falseNegative", metrics.falseNegatives);
    obj.insert("trueNegative", metrics.trueNegatives);
    obj.insert("precision", numberOrNull(metrics.precision));
    obj.insert("recall", numberOrNull(metrics.recall));
    obj.insert("f1", numberOrNull(metrics.f1));
    obj.insert("specificity", numberOrNull(metrics.specificity));
    obj.insert("accuracy", numberOrNull(metrics.accuracy));
    return obj;
}
} // namespace

bool GeometryIO::exportToJson(const QString &filePath, QGraphicsScene *scene, const GeometryExportOptions &options, QString *errorMessage)
{
    if (!options.exportJson)
        return true;

    if (!scene) {
        if (errorMessage) {
            *errorMessage = "No scene available to export.";
        }
        return false;
    }

    QJsonObject root;
    if (options.exportVertices) {
        root.insert("vertices", serializeVertices(scene));
    }
    if (options.exportLines) {
        root.insert("lines", serializeLines(scene));
    }
    if (options.exportPolygons) {
        root.insert("polygons", serializePolygons(scene));
    }
    if (options.exportNeighborPairs) {
        const QJsonArray neighborPairs = serializeNeighborPairs(scene);
        if (!neighborPairs.isEmpty()) {
            root.insert("neighborPairs", neighborPairs);
        }
    }

    const QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open %1 for writing.").arg(filePath);
        }
        return false;
    }

    const auto bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (errorMessage) {
            *errorMessage = QString("Failed to write data to %1").arg(filePath);
        }
        return false;
    }

    return true;
}

bool GeometryIO::exportDivisionPairs(const QString &filePath,
                                     const QString &fileName,
                                     const QVector<QPair<int, int>> &pairs,
                                     QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Failed to open %1 for writing.").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    stream << "file_name,first_cell_id,second_cell_id,divided\n";
    for (const auto &pair : pairs) {
        stream << fileName << ','
               << pair.first << ','
               << pair.second << ','
               << 1 << '\n';
    }

    return true;
}

QVector<BatchDivisionEstimator::GeometryEntry> GeometryIO::updateGeometryEntryFileNames(const QVector<BatchDivisionEstimator::GeometryEntry> &entries,
                                                                                       const QString &fileName)
{
    QVector<BatchDivisionEstimator::GeometryEntry> updated = entries;
    if (fileName.isEmpty())
        return updated;

    for (auto &entry : updated)
        entry.fileName = fileName;
    return updated;
}

bool GeometryIO::exportPerformanceMetricsCsv(const QString &filePath,
                                             const BatchDivisionEstimator::DivisionMetrics &metrics,
                                             QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Unable to open %1 for writing").arg(filePath);
        return false;
    }

    const auto formatMetric = [](double value) {
        if (value < 0.0 || std::isnan(value))
            return QStringLiteral("N/A");
        return QString::number(value, 'f', 6);
    };

    QTextStream stream(&file);
    const QStringList headers = {
        "estimated_pairs",
        "real_pairs",
        "neighbor_pairs",
        "true_positive",
        "false_positive",
        "false_negative",
        "true_negative",
        "precision",
        "recall",
        "f1",
        "specificity",
        "accuracy"
    };
    stream << headers.join(',') << '\n';

    QStringList row;
    row << QString::number(metrics.estimatedPairs);
    row << QString::number(metrics.realPairs);
    row << QString::number(metrics.neighborPairs);
    row << QString::number(metrics.truePositives);
    row << QString::number(metrics.falsePositives);
    row << QString::number(metrics.falseNegatives);
    row << QString::number(metrics.trueNegatives);
    row << formatMetric(metrics.precision);
    row << formatMetric(metrics.recall);
    row << formatMetric(metrics.f1);
    row << formatMetric(metrics.specificity);
    row << formatMetric(metrics.accuracy);

    stream << row.join(',') << '\n';
    return true;
}

bool GeometryIO::showExportDialog(QWidget *parent, const QString &directoryHint, const QString &fileNameHint, GeometryExportOptions *options)
{
    if (!options) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Export");

    auto *jsonCheck = new QCheckBox("Geometry JSON", &dialog);
    jsonCheck->setChecked(options->exportJson);
    auto *vertexCheck = new QCheckBox("Vertex information", &dialog);
    vertexCheck->setChecked(options->exportVertices);
    auto *lineCheck = new QCheckBox("Line information", &dialog);
    lineCheck->setChecked(options->exportLines);
    auto *polygonCheck = new QCheckBox("Polygon information", &dialog);
    polygonCheck->setChecked(options->exportPolygons);
    auto *neighborPairCheck = new QCheckBox("Neighbor pair information", &dialog);
    neighborPairCheck->setChecked(options->exportNeighborPairs);

    auto *realDivisionCheck = new QCheckBox("Real division pairs", &dialog);
    realDivisionCheck->setChecked(options->exportRealDivisionPairs);
    auto *estimatedDivisionCheck = new QCheckBox("Estimated division pairs", &dialog);
    estimatedDivisionCheck->setChecked(options->exportEstimatedDivisionPairs);

    auto *neighborGeometryCsvCheck = new QCheckBox("Neighbor pair geometry (CSV)", &dialog);
    neighborGeometryCsvCheck->setChecked(options->exportNeighborGeometryCsv);

    auto *performanceMatrixCheck = new QCheckBox("Performance matrix (CSV)", &dialog);
    performanceMatrixCheck->setChecked(options->exportPerformanceMatrixCsv);

    auto *rawImageCheck = new QCheckBox("Raw image (background only)", &dialog);
    rawImageCheck->setChecked(options->exportRawImage);
    auto *geometryImageCheck = new QCheckBox("Image with polygons/lines/vertices", &dialog);
    geometryImageCheck->setChecked(options->exportImageWithGeometry);
    auto *realImageCheck = new QCheckBox("Image with real division pairs", &dialog);
    realImageCheck->setChecked(options->exportImageWithRealDivisions);
    auto *estimatedImageCheck = new QCheckBox("Image with estimated division pairs", &dialog);
    estimatedImageCheck->setChecked(options->exportImageWithEstimatedDivisions);
    auto *comparedImageCheck = new QCheckBox("Image with compared division pairs", &dialog);
    comparedImageCheck->setChecked(options->exportImageWithComparedDivisions);

    auto *qualityLabel = new QLabel("Image quality scale (1 = current size, higher = sharper):", &dialog);
    auto *qualitySpin = new QDoubleSpinBox(&dialog);
    qualitySpin->setRange(1.0, 10.0);
    qualitySpin->setSingleStep(0.5);
    qualitySpin->setValue(options->imageScaleFactor);
    qualitySpin->setDecimals(1);

    auto *directoryEdit = new QLineEdit(&dialog);
    QString initialDir = directoryHint;
    if (initialDir.isEmpty() || initialDir == "-") {
        initialDir = QDir::homePath();
    } else {
        QFileInfo dirInfo(initialDir);
        if (dirInfo.isFile())
            initialDir = dirInfo.absolutePath();
    }
    directoryEdit->setText(initialDir);

    auto *browseButton = new QPushButton("Browse...", &dialog);
    QObject::connect(browseButton, &QPushButton::clicked, &dialog, [directoryEdit, &dialog](){
        const QString dir = QFileDialog::getExistingDirectory(&dialog, "Select Export Directory", directoryEdit->text());
        if (!dir.isEmpty())
            directoryEdit->setText(dir);
    });

    auto *baseNameEdit = new QLineEdit(&dialog);
    QString suggestedName = fileNameHint;
    if (!suggestedName.isEmpty()) {
        suggestedName = QFileInfo(suggestedName).completeBaseName();
    }
    if (suggestedName.isEmpty())
        suggestedName = "export";
    baseNameEdit->setText(suggestedName);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Data", &dialog));
    layout->addWidget(jsonCheck);
    layout->addWidget(vertexCheck);
    layout->addWidget(lineCheck);
    layout->addWidget(polygonCheck);
    layout->addWidget(neighborPairCheck);
    layout->addWidget(realDivisionCheck);
    layout->addWidget(estimatedDivisionCheck);
    layout->addWidget(neighborGeometryCsvCheck);
    layout->addWidget(performanceMatrixCheck);

    layout->addWidget(new QLabel("Images", &dialog));
    layout->addWidget(rawImageCheck);
    layout->addWidget(geometryImageCheck);
    layout->addWidget(realImageCheck);
    layout->addWidget(estimatedImageCheck);
    layout->addWidget(comparedImageCheck);
    layout->addWidget(qualityLabel);
    layout->addWidget(qualitySpin);

    layout->addWidget(new QLabel("Destination", &dialog));
    auto *dirLayout = new QHBoxLayout;
    dirLayout->addWidget(directoryEdit);
    dirLayout->addWidget(browseButton);
    layout->addLayout(dirLayout);

    auto *baseNameLayout = new QHBoxLayout;
    baseNameLayout->addWidget(new QLabel("Base file name:", &dialog));
    baseNameLayout->addWidget(baseNameEdit);
    layout->addLayout(baseNameLayout);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QString selectedDir = directoryEdit->text().trimmed();
    if (selectedDir.isEmpty())
        selectedDir = QDir::homePath();

    QDir dir(selectedDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    options->exportJson = jsonCheck->isChecked();
    options->exportVertices = vertexCheck->isChecked();
    options->exportLines = lineCheck->isChecked();
    options->exportPolygons = polygonCheck->isChecked();
    options->exportNeighborPairs = neighborPairCheck->isChecked();
    options->exportRealDivisionPairs = realDivisionCheck->isChecked();
    options->exportEstimatedDivisionPairs = estimatedDivisionCheck->isChecked();
    options->exportNeighborGeometryCsv = neighborGeometryCsvCheck->isChecked();
    options->exportPerformanceMatrixCsv = performanceMatrixCheck->isChecked();
    options->exportRawImage = rawImageCheck->isChecked();
    options->exportImageWithGeometry = geometryImageCheck->isChecked();
    options->exportImageWithRealDivisions = realImageCheck->isChecked();
    options->exportImageWithEstimatedDivisions = estimatedImageCheck->isChecked();
    options->exportImageWithComparedDivisions = comparedImageCheck->isChecked();
    options->imageScaleFactor = qualitySpin->value();
    options->exportDirectory = dir.absolutePath();
    options->baseFileName = baseNameEdit->text().trimmed().isEmpty() ? "export" : baseNameEdit->text().trimmed();
    return true;
}

bool GeometryIO::exportComprehensiveJson(const QString &filePath,
                                         QGraphicsScene *scene,
                                         const ComprehensiveExportData &data,
                                         QString *errorMessage,
                                         QStringList *warnings)
{
    if (!scene) {
        if (errorMessage)
            *errorMessage = "No scene available to export.";
        return false;
    }

    QJsonObject root;
    root.insert("vertices", serializeVertices(scene));
    root.insert("lines", serializeLines(scene));
    root.insert("polygons", serializePolygons(scene));

    QJsonArray neighborPairArray = data.neighborPairs.isEmpty()
            ? serializeNeighborPairs(scene)
            : serializeDivisionPairs(data.neighborPairs);
    if (!neighborPairArray.isEmpty())
        root.insert("neighborPairs", neighborPairArray);
    else if (warnings)
        warnings->append("No neighbor pairs were detected in the scene.");

    QJsonObject geometryObject;
    geometryObject.insert("settings", serializeGeometrySettingsObject(data.geometrySettings));
    QJsonArray geometryEntriesArray;
    for (const auto &entry : data.geometryEntries) {
        geometryEntriesArray.append(serializeGeometryEntry(entry));
    }
    geometryObject.insert("entries", geometryEntriesArray);
    root.insert("neighborPairGeometry", geometryObject);
    if (!data.hasGeometryData && geometryEntriesArray.isEmpty() && warnings)
        warnings->append("Neighbor pair geometry results are missing.");

    if (data.hasEstimationCriterion)
        root.insert("estimationRule", serializeEstimationRule(data.estimationCriterion));
    else if (warnings)
        warnings->append("No estimation rule was available to export.");

    if (!data.estimatedDivisionPairs.isEmpty())
        root.insert("estimatedDivisions", serializeDivisionPairs(data.estimatedDivisionPairs));
    else if (warnings)
        warnings->append("No estimated division pairs were available to export.");

    if (!data.realDivisionPairs.isEmpty())
        root.insert("realDivisions", serializeDivisionPairRecords(data.realDivisionPairs));
    else if (warnings)
        warnings->append("No real division pairs were available to export.");

    if (data.hasPerformanceMetrics)
        root.insert("performanceMetrics", serializePerformanceMetrics(data.performanceMetrics));
    else if (warnings)
        warnings->append("Performance metrics were not available to export.");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QString("Failed to open %1 for writing.").arg(filePath);
        return false;
    }

    const QJsonDocument doc(root);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (errorMessage)
            *errorMessage = QString("Failed to write data to %1").arg(filePath);
        return false;
    }

    return true;
}
