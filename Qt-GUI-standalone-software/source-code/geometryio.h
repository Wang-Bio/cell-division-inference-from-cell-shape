#ifndef GEOMETRYIO_H
#define GEOMETRYIO_H

#include <QString>
#include <QStringList>
#include <QRectF>
#include <QPair>
#include <QVector>

#include "batchdivisionestimator.h"

class QGraphicsScene;
class QWidget;

struct GeometryExportOptions
{
    bool exportJson = true;
    bool exportVertices = true;
    bool exportLines = true;
    bool exportPolygons = true;
    bool exportNeighborPairs = true;

    bool exportRealDivisionPairs = false;
    bool exportEstimatedDivisionPairs = false;
    bool exportNeighborGeometryCsv = false;
    bool exportPerformanceMatrixCsv = false;

    bool exportRawImage = false;
    bool exportImageWithGeometry = false;
    bool exportImageWithRealDivisions = false;
    bool exportImageWithEstimatedDivisions = false;
    bool exportImageWithComparedDivisions = false;

    double imageScaleFactor = 3.0;

    QString exportDirectory;
    QString baseFileName;
};

struct DivisionPairRecord
{
    int firstId = -1;
    int secondId = -1;
    int time = -1;
};

struct GeometryImportResult
{
    bool success = false;
    QString errorMessage;
    QStringList warnings;
    QGraphicsScene *scene = nullptr;
    int nextVertexId = 1;
    QRectF sceneRect;
    QVector<QPair<int, int>> neighborPairs;
    QVector<BatchDivisionEstimator::GeometryEntry> geometryEntries;
    NeighborPairGeometrySettings geometrySettings;
    bool hasGeometryData = false;
    DivisionEstimator::Criterion estimationCriterion;
    bool hasEstimationCriterion = false;
    QVector<QPair<int, int>> estimatedDivisionPairs;
    QVector<DivisionPairRecord> realDivisionRecords;
    BatchDivisionEstimator::DivisionMetrics performanceMetrics;
    bool hasPerformanceMetrics = false;
};

struct ComprehensiveExportData
{
    QVector<QPair<int, int>> neighborPairs;
    QVector<BatchDivisionEstimator::GeometryEntry> geometryEntries;
    NeighborPairGeometrySettings geometrySettings;
    bool hasGeometryData = false;

    DivisionEstimator::Criterion estimationCriterion;
    bool hasEstimationCriterion = false;

    QVector<QPair<int, int>> estimatedDivisionPairs;
    QVector<DivisionPairRecord> realDivisionPairs;

    BatchDivisionEstimator::DivisionMetrics performanceMetrics;
    bool hasPerformanceMetrics = false;
};

class GeometryIO
{
public:
    static GeometryImportResult importFromJson(const QString &filePath, bool allowVertexManualMove);
    static bool exportToJson(const QString &filePath, QGraphicsScene *scene, const GeometryExportOptions &options, QString *errorMessage = nullptr);
    static bool exportDivisionPairs(const QString &filePath,
                                    const QString &fileName,
                                    const QVector<QPair<int, int>> &pairs,
                                    QString *errorMessage = nullptr);
    static bool showExportDialog(QWidget *parent, const QString &directoryHint, const QString &fileNameHint, GeometryExportOptions *options);
    static void setNeighborPairsOnScene(QGraphicsScene *scene, const QVector<QPair<int, int>> &pairs);
    static QVector<QPair<int, int>> neighborPairsFromScene(QGraphicsScene *scene);
    static QVector<BatchDivisionEstimator::GeometryEntry> updateGeometryEntryFileNames(const QVector<BatchDivisionEstimator::GeometryEntry> &entries,
                                                                                        const QString &fileName);
    static bool exportPerformanceMetricsCsv(const QString &filePath,
                                            const BatchDivisionEstimator::DivisionMetrics &metrics,
                                            QString *errorMessage = nullptr);
    static bool exportComprehensiveJson(const QString &filePath,
                                        QGraphicsScene *scene,
                                        const ComprehensiveExportData &data,
                                        QString *errorMessage = nullptr,
                                        QStringList *warnings = nullptr);
};

#endif // GEOMETRYIO_H
