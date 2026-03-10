#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPair>

#include "canvasmanager.h"
#include "vertexitem.h"
#include "lineitem.h"
#include "polygonitem.h"
#include "divisionestimator.h"
#include "batchdivisionestimator.h"

#include <opencv2/core.hpp>
#include <QPixmap>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DebugManager;
class QGraphicsLineItem;
class QGraphicsPathItem;
class QGraphicsPixmapItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    struct RealDivisionRecord {
        int firstId = -1;
        int secondId = -1;
        int time = -1;
    };

    Ui::MainWindow *ui;

//Canvas related
    CanvasManager m_canvasManager;
    int m_currentCanvasWidth = 0;
    int m_currentCanvasHeight = 0;
    cv::Mat m_currentImage;
    QVector<QGraphicsLineItem*> m_neighborLines;
    QVector<QGraphicsPathItem*> m_estimatedDivisionArrows;
    QVector<QPair<int, int>> m_estimatedDivisionPairs;
    QVector<QGraphicsPathItem*> m_realDivisionArrows;
    QVector<QPair<int, int>> m_realDivisionPairs;
    QVector<QGraphicsPathItem*> m_truePositiveDivisionArrows;
    QVector<QGraphicsPathItem*> m_falsePositiveDivisionArrows;
    QVector<QGraphicsPathItem*> m_falseNegativeDivisionArrows;
    QVector<QPair<int, int>> m_truePositivePairs;
    QVector<QPair<int, int>> m_falsePositivePairs;
    QVector<QPair<int, int>> m_falseNegativePairs;
    QVector<RealDivisionRecord> m_realDivisionRecords;
    QPixmap m_backgroundPixmap;
    QGraphicsPixmapItem *m_backgroundItem = nullptr;
    int m_backgroundWidth = 0;
    int m_backgroundHeight = 0;
    QVector<BatchDivisionEstimator::GeometryEntry> m_lastGeometryEntries;
    NeighborPairGeometrySettings m_lastGeometrySettings;
    BatchDivisionEstimator::DivisionMetrics m_lastPerformanceMetrics;
    bool m_hasGeometryCalculationResults = false;
    bool m_hasPerformanceMetrics = false;
    DivisionEstimator::Criterion m_lastEstimationCriterion;
    bool m_hasEstimationCriterion = false;

    void updateCanvasSizeLabel(double zoomPercent);
    void attachBackgroundToScene();
    void detachBackgroundItem();
    void fitViewToScene();
    bool hasBackgroundPixmap() const;
    QPointF m_lastMouseScenePos = QPointF(0,0);

    void addVertexAt(const QPointF &scenePos);
    void deleteSelectedItems();
    void addLineFromSelectedVertices();
    void addPolygonFromSelectedVertices();
    void clearNeighborLines();
    void clearEstimatedDivisionArrows();
    void clearRealDivisionArrows();
    void clearDivisionComparisonArrows();
    void clearDivisionArrowVector(QVector<QGraphicsPathItem*> &arrows);
    void rebuildNeighborLinesFromPairs(const QVector<QPair<int, int>> &pairs);
    void resetEstimatedDivisionNumberLabel();
    void setEstimatedDivisionNumber(int count);
    void resetVertexNumberLabel();
    void setVertexNumber(int count);
    void resetLineNumberLabel();
    void setLineNumber(int count);
    void resetPolygonNumberLabel();
    void setPolygonNumber(int count);
    void resetDivisionComparisonLabels();
    void resetStoredGeometryResults();
    void resetPerformanceMetrics();
    void resetEstimationRule();
    void applyDivisionMetricsToLabels(const BatchDivisionEstimator::DivisionMetrics &metrics);

    bool openRawImageInternal(const QString &filePath = QString());
    bool importJsonFile(const QString &filePath);
    void resetSelectedItemLabels();

    void connectLineSignals(LineItem *line);
    void connectAllLinesInScene();
    void connectPolygonSignals(PolygonItem* polygon);
    void connectAllPolygonsInScene();
    void applyVertexMovableSetting();
    bool exportSceneImage(const QString &filePath,
                          bool includePolygons,
                          bool includeLines,
                          bool includeVertices,
                          bool includeRealArrows,
                          bool includeEstimatedArrows,
                          bool includeComparisonArrows,
                          double scaleFactor);
    QVector<QGraphicsItem*> collectItemsByType(int type) const;
    void setVisibilityForItems(const QVector<QGraphicsItem*> &items, bool visible, QVector<QPair<QGraphicsItem*, bool>> &record) const;

//Network related
    int m_nextVertexId = 1;
    bool m_allowVertexManualMove = false;
    bool m_allowManualVertexCreation = true;
    bool m_allowManualLineCreation = true;
    bool m_allowManualPolygonCreation = true;

private slots:

//graphicsView & label functions
    void onZoomChanged(double zoomPercent);
    void onMousePositionChanged(const QPointF &scenePos);

    void updateVertexCountLabel();
    void updateLineCountLabel();
    void updatePolygonCountLabel();
    void onVertexSelected(VertexItem *v);
    void onVertexMoved(VertexItem *v, const QPointF &pos);
    void onLineSelected(LineItem *L);
    void onLineGeometryChanged(LineItem *L);
    void onPolygonSelected(PolygonItem* poly);
    void onPolygonGeometryChanged(PolygonItem *poly);

//menuBar Open functions
    void onCreateCanvas();
    void onOpenRawImage();
    void onOpenBackground();
    void onImport();
    void onImportAllData();
    void onExport();
    void onExportAllData();

//menuBar Edit functions
    void onDeleteImage();
    void onEditSetting();
    void onAddVertex();
    void onDeleteVertex();
    void onDeleteAllVertices();
    void onAddLine();
    void onDeleteLine();
    void onDeleteAllLines();
    void onAddPolygon();
    void onDeletePolygon();
    void onDeleteAllPolygons();

//menuBar Detection functions
    void onSkeletonization();
    void onVertexDetection();
    void onLineDetection();
    void onPolygonDetection();
    void onDetectNeighborPairs();

//menuBar Geometry functions
    void onGeometryCalculationSetting();
    void onNeighborPairGeometryCalculation();
    void onBatchNeighborPairGeometryCalculation();
    void onBatchSingleCellGeometryCalculation();

//menuBar Estimation functions
    void onEstimateDivisionBySingleGeometry();
    void onBatchEstimateDivisionBySingleGeometry();
    void onBatchEstimateDivisionByDesignatedGeometry();
    void onCompareWithRealDivision();

//menuBar Plot functions
    void onPrecisionAndRecallCurveOverSingleGeometry();

//menuBar Display functions
    void onVertexDisplaySetting();
    void onLineDisplaySetting();
    void onPolygonDisplaySetting();
    void onNeighborPairDisplaySetting();
    void onDivisionPairDisplaySetting();

//menuBar Find functions
    void onFindVertex();
    void onFindLine();
    void onFindPolygon();

//menuBar Debug functions
    void onGenerateRandomNetwork();
    void onCompareBatchEstimations();
    void onExportFeatureNames();

    friend class DebugManager;

};
#endif // MAINWINDOW_H
