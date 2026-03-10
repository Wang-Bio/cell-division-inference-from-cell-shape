#ifndef BATCHDIVISIONESTIMATOR_H
#define BATCHDIVISIONESTIMATOR_H

#include <QDir>
#include <QFileInfo>
#include <QPair>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QSet>

#include "divisionestimator.h"
#include "neighborgeometrycalculator.h"

class BatchDivisionEstimator
{
public:
    struct GeometryEntry {
        QString fileName;
        int pairIndex = 0;
        int firstId = -1;
        int secondId = -1;
        bool observedDivision = false;
        int divisionTime = -1;
        NeighborPairGeometryCalculator::Result geometry;
    };

    struct GeometrySummary {
        QVector<GeometryEntry> entries;
        QStringList errors;
        QStringList warnings;
        int filesProcessed = 0;
        int filesWithResults = 0;
    };

    struct SingleCellGeometryEntry {
        QString fileName;
        int polygonId = -1;
        bool inDividingPair = false;
        double area = -1.0;
        double perimeter = -1.0;
        double aspectRatio = -1.0;
        double circularity = -1.0;
        double solidity = -1.0;
        int vertexCount = 0;
    };

    struct SingleCellGeometrySummary {
        QVector<SingleCellGeometryEntry> entries;
        QStringList errors;
        QStringList warnings;
        int filesProcessed = 0;
        int filesWithResults = 0;
    };

    struct DivisionMetrics {
        int estimatedPairs = -1;
        int realPairs = -1;
        int truePositives = 0;
        int falsePositives = 0;
        int falseNegatives = 0;
        int trueNegatives = 0;
        int neighborPairs = 0;

        double precision = -1.0;
        double recall = -1.0;
        double f1 = -1.0;
        double specificity = -1.0;
        double accuracy = -1.0;
    };

    struct FileDivisionResult {
        QString fileName;
        int neighborPairCount = 0;
        QVector<QPair<int, int>> estimatedPairs;
        QVector<QPair<int, int>> realPairs;
        DivisionMetrics metrics;
    };

    struct DivisionPairRow {
        QString fileName;
        int firstId = -1;
        int secondId = -1;
        NeighborPairGeometryCalculator::Result geometry;
        int divisionTime = -1;
        bool observedDivision = false;
        bool estimatedDivision = false;
    };

    struct DesignatedGeometryConfig {
        QString featureName;
        double cutoffValue = 0.0;
        bool requireAbove = true;
        QString direction;
        QString note;
    };

    struct DesignatedGeometryResult {
        DesignatedGeometryConfig config;
        QString featureKey;
        QString featureLabel;
        DivisionMetrics metrics;
        int filesProcessed = 0;
        int filesWithResults = 0;
        QStringList warnings;
        QStringList errors;
    };

    struct BatchResult {
        QVector<FileDivisionResult> files;
        QVector<DivisionPairRow> pairRows;
        QStringList errors;
        QStringList warnings;
        int filesProcessed = 0;
        int filesWithResults = 0;
        DivisionMetrics totals;
    };

    static GeometrySummary processNeighborGeometryDirectory(const QString &directoryPath, const NeighborPairGeometrySettings &settings);
    static bool exportNeighborGeometryToCsv(const QString &filePath,
                                            const QVector<GeometryEntry> &entries,
                                            const NeighborPairGeometrySettings &settings,
                                            QString *errorMessage = nullptr);
    static SingleCellGeometrySummary processSingleCellGeometryDirectory(const QString &directoryPath);
    static bool exportSingleCellGeometryToCsv(const QString &filePath,
                                              const QVector<SingleCellGeometryEntry> &entries,
                                              QString *errorMessage = nullptr);

    static QHash<QString, QSet<QString>> loadExceptionPairs(const QDir &directory, QStringList *warnings = nullptr);
    static QSet<QString> exceptionPairsForFile(const QFileInfo &fileInfo, const QHash<QString, QSet<QString>> &exceptionMap);

    static BatchResult estimateDirectory(const QString &directoryPath, const DivisionEstimator::Criterion &criterion);
    static bool exportDivisionEstimatesToCsv(const QString &filePath,
                                             const QVector<DivisionPairRow> &rows,
                                             QString *errorMessage = nullptr);
    static QVector<DesignatedGeometryConfig> loadDesignatedGeometryConfigCsv(const QString &filePath,
                                                                             QStringList *warnings = nullptr,
                                                                             QStringList *errors = nullptr);
    static QVector<DesignatedGeometryResult> estimateDesignatedGeometry(const QString &directoryPath,
                                                                        const QVector<DesignatedGeometryConfig> &configs);
    static bool exportDesignatedGeometryMetricsToCsv(const QString &filePath,
                                                     const QVector<DesignatedGeometryResult> &results,
                                                     QString *errorMessage = nullptr);
    static DivisionMetrics metricsFromCounts(int neighborCount,
                                             int truePositives,
                                             int falsePositives,
                                             int falseNegatives,
                                             int estimatedCount = -1,
                                             int realCount = -1);
    static DivisionMetrics calculateMetrics(const QVector<QPair<int, int>> &neighborPairs,
                                            const QVector<QPair<int, int>> &estimatedPairs,
                                            const QVector<QPair<int, int>> &realPairs);
};

#endif // BATCHDIVISIONESTIMATOR_H
