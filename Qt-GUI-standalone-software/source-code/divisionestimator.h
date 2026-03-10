#ifndef DIVISIONESTIMATOR_H
#define DIVISIONESTIMATOR_H

#include <QColor>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QPair>

#include "neighborgeometrycalculator.h"

class PrecisionRecallSweepWorker;
class PolygonItem;
class QWidget;
class QGraphicsScene;
class QGraphicsPathItem;

struct DivisionArrowStyle {
    enum class ArrowType {
        DoubleHeaded,
        SingleHeaded,
        LineOnly,
    };

    QColor color = Qt::red;
    int widthPx = 2;
    ArrowType arrowType = ArrowType::DoubleHeaded;
    double arrowHeadLengthPx = 5.0;
    double arrowHeadWidthPx = 3.0;
};

class DivisionDisplay
{
public:
    enum class ArrowCategory {
        Estimated,
        Real,
        TruePositive,
        FalsePositive,
        FalseNegative,
    };

    static DivisionArrowStyle currentStyle(ArrowCategory category);
    static void setCurrentStyle(ArrowCategory category, const DivisionArrowStyle &style);
    static void applyStyleToArrows(const QVector<QGraphicsPathItem*> &arrows, ArrowCategory category);
    static bool displayEnabled(ArrowCategory category);
    static void setDisplayEnabled(ArrowCategory category, bool enabled);
    static void applyVisibilityToArrows(const QVector<QGraphicsPathItem*> &arrows, bool visible);
    static bool showDisplaySettingDialog(QGraphicsScene *scene,
                                         const QVector<QGraphicsPathItem*> &estimatedArrows,
                                         const QVector<QGraphicsPathItem*> &realArrows,
                                         const QVector<QGraphicsPathItem*> &truePositiveArrows,
                                         const QVector<QGraphicsPathItem*> &falsePositiveArrows,
                                         const QVector<QGraphicsPathItem*> &falseNegativeArrows,
                                         QWidget *parent);
    static QGraphicsPathItem* createArrowItem(const QPointF &start,
                                              const QPointF &end,
                                              ArrowCategory category = ArrowCategory::Estimated);

private:
    static DivisionArrowStyle& mutableStyle(ArrowCategory category);
    static QPainterPath createArrowPath(const QPointF &start, const QPointF &end, const DivisionArrowStyle &style);
    static QPainterPath::Element defaultElement(const QPainterPath &path, int index);
};

struct NeighborPairGeometrySettings;

class DivisionEstimator
{
public:
    struct FeatureOption {
        QString key;
        QString label;
        QString description;
    };

    struct Criterion {
        QString featureKey;
        QString featureLabel;
        bool requireAbove = true;
        double threshold = 145.0;
        enum class MatchingMode {
            LocalOptimal,
            GlobalMaximumWeight,
            Unconstrained,
        } matchingMode = MatchingMode::GlobalMaximumWeight;
        NeighborPairGeometrySettings settings;
    };

    struct EstimationResult {
        QVector<QGraphicsPathItem*> arrows;
        int examinedPairs = 0;
        int matchedPairs = 0;
        QVector<QPair<int, int>> matchedPairIds;
    };

    static QVector<FeatureOption> featureOptions();
    static bool showSingleFeatureDialog(QWidget *parent, Criterion &criterion);
    static bool populateCriterionFromSelection(const FeatureOption &option,
                                               bool requireAbove,
                                               double threshold,
                                               Criterion &criterion);
    static void ensureSettingForFeature(const QString &key, NeighborPairGeometrySettings &settings);
    static bool showPrecisionRecallSweepDialog(QWidget *parent,
                                               const QString &defaultDirectoryHint = QString());
    static EstimationResult estimatePairs(const QVector<PolygonItem*> &polygons, const Criterion &criterion, QGraphicsScene *scene);

private:
    static double featureValue(const QString &key, const NeighborPairGeometryCalculator::Result &result);
    static double matchScore(const Criterion &criterion, double featureValue);
    struct CandidateMatch {
        int first = -1;
        int second = -1;
        double score = 0.0;
        double featureValue = 0.0;
    };
    static QVector<CandidateMatch> greedyMatching(int polygonCount, const QVector<CandidateMatch> &candidates);
    static QVector<CandidateMatch> maximumWeightMatching(int polygonCount, const QVector<CandidateMatch> &candidates);
    static QVector<CandidateMatch> selectMatches(int polygonCount, const QVector<CandidateMatch> &candidates, Criterion::MatchingMode mode);
};

#endif // DIVISIONESTIMATOR_H
