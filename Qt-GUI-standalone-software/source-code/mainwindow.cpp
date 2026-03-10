#include "mainwindow.h"
#include "debugmanager.h"
#include "ui_mainwindow.h"

#include "vertexitem.h"
#include "lineitem.h"
#include "polygonitem.h"
#include "neighborpair.h"
#include "imageanalysis.h"
#include "neighborgeometrycalculator.h"
#include "batchdivisionestimator.h"
#include "geometryio.h"

#include <QtMath>
#include <QSplitter>
#include <QShortcut>
#include <QKeySequence>
#include <QGraphicsItem>
#include <QMessageBox>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QHash>
#include <QSet>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QImageReader>
#include <QPainter>

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
bool anyGeometryCalculationEnabled(const NeighborPairGeometrySettings &settings)
{
    return settings.computeAreaRatio
            || settings.computeAreaStats
            || settings.computePerimeterRatio
            || settings.computePerimeterStats
            || settings.computeAspectRatio
            || settings.computeAspectRatioStats
            || settings.computeCircularityRatio
            || settings.computeCircularityStats
            || settings.computeSolidityRatio
            || settings.computeSolidityStats
            || settings.computeVertexCountRatio
            || settings.computeVertexCountStats
            || settings.computeCentroidDistance
            || settings.computeUnionAspectRatio
            || settings.computeUnionCircularity
            || settings.computeUnionConvexDeficiency
            || settings.computeNormalizedSharedEdgeLength
            || settings.computeSharedEdgeUnsharedVerticesDistance
            || settings.computeSharedEdgeUnsharedVerticesDistanceNormalized
            || settings.computeCentroidSharedEdgeDistance
            || settings.computeCentroidSharedEdgeDistanceNormalized
            || settings.computeJunctionAngleAverage
            || settings.computeJunctionAngleMax
            || settings.computeJunctionAngleMin
            || settings.computeJunctionAngleDifference
            || settings.computeJunctionAngleRatio
            || settings.computeSharedEdgeUnionCentroidDistance
            || settings.computeSharedEdgeUnionCentroidDistanceNormalized
            || settings.computeSharedEdgeUnionAxisAngle;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_canvasManager(this)
{
    ui->setupUi(this);


//Layout
    auto *layout = ui->centralwidget->layout();

    layout->removeWidget(ui->widget);
    layout->removeWidget(ui->graphicsView);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    splitter->addWidget(ui->widget);
    splitter->addWidget(ui->graphicsView);

    layout->addWidget(splitter);


//zoom and mouse
    auto view = ui->graphicsView;
    connect(view, &InteractiveGraphicsView::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(view, &InteractiveGraphicsView::mouseMovedOnScene, this, &MainWindow::onMousePositionChanged);


//add and delete items
    auto *scAddVertex = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_V), ui->graphicsView);
    connect(scAddVertex, &QShortcut::activated, this, [this](){addVertexAt(m_lastMouseScenePos);});

    auto *scDel = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), ui->graphicsView);
    connect(scDel, &QShortcut::activated, this, [this](){deleteSelectedItems();});

    auto *scAddLine = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(scAddLine, &QShortcut::activated, this, [this](){addLineFromSelectedVertices();});

    auto *scAddPolygon = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), this);
    connect(scAddPolygon, &QShortcut::activated, this, [this](){addPolygonFromSelectedVertices();});

//menuBar Open function
    connect(ui->actionCreate_Canvas, &QAction::triggered, this, &MainWindow::onCreateCanvas);
    connect(ui->actionOpen_Raw_Image, &QAction::triggered,this, &MainWindow::onOpenRawImage);
    connect(ui->actionOpen_Background, &QAction::triggered, this, &MainWindow::onOpenBackground);
    connect(ui->actionImport, &QAction::triggered, this, &MainWindow::onImport);
    connect(ui->actionImport_All_Data, &QAction::triggered, this, &MainWindow::onImportAllData);
    connect(ui->actionExport, &QAction::triggered, this, &MainWindow::onExport);
    connect(ui->actionExport_All_Data, &QAction::triggered, this, &MainWindow::onExportAllData);

//menuBar Edit function
    connect(ui->actionDelete_Image, &QAction::triggered, this, &MainWindow::onDeleteImage);
    connect(ui->actionEdit_Setting, &QAction::triggered, this, &MainWindow::onEditSetting);
    connect(ui->actionAdd_Vertex, &QAction::triggered, this, &MainWindow::onAddVertex);
    connect(ui->actionDelete_Vertex, &QAction::triggered, this, &MainWindow::onDeleteVertex);
    connect(ui->actionDelete_All_Vertices, &QAction::triggered, this, &MainWindow::onDeleteAllVertices);
    connect(ui->actionAdd_Line, &QAction::triggered, this, &MainWindow::onAddLine);
    connect(ui->actionDelete_Line, &QAction::triggered, this, &MainWindow::onDeleteLine);
    connect(ui->actionDelete_All_Lines, &QAction::triggered, this, &MainWindow::onDeleteAllLines);
    connect(ui->actionAdd_Polygon, &QAction::triggered, this, &MainWindow::onAddPolygon);
    connect(ui->actionDelete_Polygon, &QAction::triggered, this, &MainWindow::onDeletePolygon);
    connect(ui->actionDelete_All_Polygons, &QAction::triggered, this, &MainWindow::onDeleteAllPolygons);

//menuBar Detection function
    connect(ui->actionSkeletonization, &QAction::triggered, this, &MainWindow::onSkeletonization);
    connect(ui->actionVertex_Detection, &QAction::triggered, this, &MainWindow::onVertexDetection);
    connect(ui->actionLine_Detection, &QAction::triggered, this, &MainWindow::onLineDetection);
    connect(ui->actionPolygon_Detection, &QAction::triggered, this, &MainWindow::onPolygonDetection);
    connect(ui->actionDetect_Neighbor_Pairs, &QAction::triggered, this, &MainWindow::onDetectNeighborPairs);

//menuBar Geometry function
    connect(ui->actionGeometry_Calculation_Setting, &QAction::triggered, this, &MainWindow::onGeometryCalculationSetting);
    connect(ui->actionNeighbor_Pair_Geometry_Calculation, &QAction::triggered, this, &MainWindow::onNeighborPairGeometryCalculation);
    connect(ui->actionBatch_Neighbor_Pair_Geometry_Calculation, &QAction::triggered, this, &MainWindow::onBatchNeighborPairGeometryCalculation);
    connect(ui->actionBatch_Single_Cell_Geometry_Calculation, &QAction::triggered, this, &MainWindow::onBatchSingleCellGeometryCalculation);

//menuBar Estimate function
    connect(ui->actionEstimate_division_by_single_geometry, &QAction::triggered, this, &MainWindow::onEstimateDivisionBySingleGeometry);
    connect(ui->actionBatch_Estimate_Division_by_single_geometry, &QAction::triggered, this, &MainWindow::onBatchEstimateDivisionBySingleGeometry);
    connect(ui->actionBatch_Estimate_Division_by_designated_geometry, &QAction::triggered, this, &MainWindow::onBatchEstimateDivisionByDesignatedGeometry);
    connect(ui->actionCompare_with_real_division, &QAction::triggered, this, &MainWindow::onCompareWithRealDivision);
    connect(ui->actionPrecision_and_Recall_Curve_Over_Single_Geometry, &QAction::triggered, this, &MainWindow::onPrecisionAndRecallCurveOverSingleGeometry);

//menuBar Display function
    connect(ui->actionVertex_Display_Setting, &QAction::triggered, this, &MainWindow::onVertexDisplaySetting);
    connect(ui->actionLine_Display_Setting, &QAction::triggered, this, &MainWindow::onLineDisplaySetting);
    connect(ui->actionPolygon_Display_Setting, &QAction::triggered, this, &MainWindow::onPolygonDisplaySetting);
    connect(ui->actionNeighbor_Pair_Display_Setting, &QAction::triggered, this, &MainWindow::onNeighborPairDisplaySetting);
    connect(ui->actionDivision_Pair_Display_Setting, &QAction::triggered, this, &MainWindow::onDivisionPairDisplaySetting);

//menuBar Find function
    connect(ui->actionFind_Vertex, &QAction::triggered, this, &MainWindow::onFindVertex);
    connect(ui->actionFind_Line, &QAction::triggered, this, &MainWindow::onFindLine);
    connect(ui->actionFind_Polygon, &QAction::triggered, this, &MainWindow::onFindPolygon);

//menuBar Plot function

//menuBar Debug function
    connect(ui->actionGenerate_Random_Network, &QAction::triggered, this, &MainWindow::onGenerateRandomNetwork);
    connect(ui->actionComparing_Batch_Estimation, &QAction::triggered, this, &MainWindow::onCompareBatchEstimations);
    connect(ui->actionExport_Geometric_Feature_Names, &QAction::triggered, this, &MainWindow::onExportFeatureNames);


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onCreateCanvas()
{
    //Forget previous image data
    m_currentImage.release();
    m_currentCanvasHeight=0;
    m_currentCanvasWidth=0;
    bool hadBackground = hasBackgroundPixmap();
    detachBackgroundItem();

    clearNeighborLines();
    clearEstimatedDivisionArrows();
    clearRealDivisionArrows();
    resetEstimatedDivisionNumberLabel();
    resetDivisionComparisonLabels();
    resetStoredGeometryResults();
    m_realDivisionRecords.clear();
    resetVertexNumberLabel();
    resetLineNumberLabel();
    resetPolygonNumberLabel();

    auto info = m_canvasManager.createCanvasWithDialog(ui->graphicsView,this);
    if(!info.valid){
        if (hadBackground) {
            attachBackgroundToScene();
            fitViewToScene();
            double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
            updateCanvasSizeLabel(zoomPercent);
        }
        return;
    }

    m_currentCanvasWidth = info.width;
    m_currentCanvasHeight = info.height;

    attachBackgroundToScene();
    fitViewToScene();
    double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
    updateCanvasSizeLabel(zoomPercent);
}

void MainWindow::onOpenRawImage()
{
    openRawImageInternal(QString());
}

void MainWindow::onOpenBackground()
{
    QString filter = "Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp);;All Files (*.*)";
    QString filePath = QFileDialog::getOpenFileName(this, "Open Background Image", QString(), filter);

    if (filePath.isEmpty()) {
        return;
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this,
                             "Open Background Image",
                             QString("Failed to load background image from:\n%1").arg(filePath));
        return;
    }

    m_backgroundPixmap = QPixmap::fromImage(image);
    m_backgroundWidth = image.width();
    m_backgroundHeight = image.height();

    attachBackgroundToScene();
    fitViewToScene();
    double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
    updateCanvasSizeLabel(zoomPercent);
}

void MainWindow::onImport()
{
    QString filter = "Supported Files (*.json *.png *.jpg *.jpeg *.tif *.tiff *.bmp);;JSON Files (*.json);;Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp);;All Files (*.*)";
    QString filePath = QFileDialog::getOpenFileName(this, "Import", QString(), filter);

    if(filePath.isEmpty()){
        return;
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if(suffix == "json"){
        importJsonFile(filePath);
    }else{
        openRawImageInternal(filePath);
    }
}

void MainWindow::onImportAllData()
{
    QString filter = "JSON Files (*.json);;All Files (*.*)";
    QString filePath = QFileDialog::getOpenFileName(this, "Import All Data", QString(), filter);

    if (filePath.isEmpty())
        return;

    if (QFileInfo(filePath).suffix().toLower() != "json") {
        QMessageBox::warning(this, "Import All Data", "Please select a JSON file exported with \"Export All Data\".");
        return;
    }

    importJsonFile(filePath);
}

void MainWindow::onExport()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "Export", "No canvas available to export. Please create or import a scene first.");
        return;
    }

    GeometryExportOptions options;
    options.exportDirectory = ui->label_input_directory_value->text();
    options.baseFileName = ui->label_input_file_name_value->text();
    if (!GeometryIO::showExportDialog(this,
                                      ui->label_input_directory_value->text(),
                                      ui->label_input_file_name_value->text(),
                                      &options)) {
        return;
    }

    QDir exportDir(options.exportDirectory.isEmpty() ? QDir::homePath() : options.exportDirectory);
    exportDir.mkpath(".");

    QString baseName = options.baseFileName;
    if (baseName.isEmpty() || baseName == "-")
        baseName = "export";
    baseName = QFileInfo(baseName).completeBaseName();

    QStringList successMessages;
    QStringList warningMessages;
    QString errorMessage;

    if (options.exportJson) {
        const QString jsonPath = exportDir.filePath(baseName + ".json");
        if (GeometryIO::exportToJson(jsonPath, scene, options, &errorMessage)) {
            successMessages << QString("Geometry JSON: %1").arg(jsonPath);
        } else {
            warningMessages << errorMessage;
        }
    }

    if (options.exportRealDivisionPairs) {
        if (m_realDivisionPairs.isEmpty()) {
            warningMessages << "No real division pairs available to export.";
        } else {
            const QString realPath = exportDir.filePath(baseName + "_real_division_pairs.csv");
            if (GeometryIO::exportDivisionPairs(realPath, baseName, m_realDivisionPairs, &errorMessage)) {
                successMessages << QString("Real division pairs: %1").arg(realPath);
            } else {
                warningMessages << errorMessage;
            }
        }
    }

    if (options.exportEstimatedDivisionPairs) {
        if (m_estimatedDivisionPairs.isEmpty()) {
            warningMessages << "No estimated division pairs available to export.";
        } else {
            const QString estPath = exportDir.filePath(baseName + "_estimated_division_pairs.csv");
            if (GeometryIO::exportDivisionPairs(estPath, baseName, m_estimatedDivisionPairs, &errorMessage)) {
                successMessages << QString("Estimated division pairs: %1").arg(estPath);
            } else {
                warningMessages << errorMessage;
            }
        }
    }

    if (options.exportNeighborGeometryCsv) {
        if (!m_hasGeometryCalculationResults || m_lastGeometryEntries.isEmpty()) {
            warningMessages << "No neighbor pair geometry calculations available to export.";
        } else {
            const QString geometryCsvPath = exportDir.filePath(baseName + "_neighbor_geometry.csv");
            const QVector<BatchDivisionEstimator::GeometryEntry> entries = GeometryIO::updateGeometryEntryFileNames(m_lastGeometryEntries, baseName);
            if (BatchDivisionEstimator::exportNeighborGeometryToCsv(geometryCsvPath, entries, m_lastGeometrySettings, &errorMessage)) {
                successMessages << QString("Neighbor pair geometry: %1").arg(geometryCsvPath);
            } else {
                warningMessages << errorMessage;
            }
        }
    }

    if (options.exportPerformanceMatrixCsv) {
        if (!m_hasPerformanceMetrics) {
            warningMessages << "No performance metrics available to export.";
        } else {
            const QString performanceCsvPath = exportDir.filePath(baseName + "_performance_matrix.csv");
            if (GeometryIO::exportPerformanceMetricsCsv(performanceCsvPath, m_lastPerformanceMetrics, &errorMessage)) {
                successMessages << QString("Performance matrix: %1").arg(performanceCsvPath);
            } else {
                warningMessages << errorMessage;
            }
        }
    }

    const auto exportImageIfRequested = [&](bool requested,
                                            const QString &fileNameSuffix,
                                            bool includePolygons,
                                            bool includeLines,
                                            bool includeVertices,
                                            bool includeRealArrows,
                                            bool includeEstimatedArrows,
                                            bool includeComparisonArrows,
                                            double scaleFactor) {
        if (!requested)
            return;
        const QString path = exportDir.filePath(baseName + fileNameSuffix + ".png");
        if (exportSceneImage(path,
                             includePolygons,
                             includeLines,
                             includeVertices,
                             includeRealArrows,
                             includeEstimatedArrows,
                             includeComparisonArrows,
                             scaleFactor)) {
            successMessages << QString("Image (%1): %2").arg(fileNameSuffix.mid(1)).arg(path);
        } else {
            warningMessages << QString("Failed to export image: %1").arg(path);
        }
    };

    if (options.exportRawImage && !hasBackgroundPixmap()) {
        warningMessages << "No background image available for raw image export.";
    } else {
        exportImageIfRequested(options.exportRawImage, "_raw", false, false, false, false, false, false, options.imageScaleFactor);
    }
    exportImageIfRequested(options.exportImageWithGeometry, "_geometry", true, true, true, false, false, false, options.imageScaleFactor);
    exportImageIfRequested(options.exportImageWithRealDivisions, "_real_divisions", true, true, true, true, false, false, options.imageScaleFactor);
    exportImageIfRequested(options.exportImageWithEstimatedDivisions, "_estimated_divisions", true, true, true, false, true, false, options.imageScaleFactor);
    exportImageIfRequested(options.exportImageWithComparedDivisions, "_compared_divisions", true, true, true, false, false, true, options.imageScaleFactor);

    QString message;
    if (!successMessages.isEmpty())
        message += "Exported:\n- " + successMessages.join("\n- ");
    if (!warningMessages.isEmpty()) {
        if (!message.isEmpty())
            message += "\n\n";
        message += "Warnings:\n- " + warningMessages.join("\n- ");
    }

    if (message.isEmpty())
        message = "Nothing was exported.";

    QMessageBox::information(this, "Export", message);
}

void MainWindow::onExportAllData()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "Export All Data", "No canvas available to export. Please create or import a scene first.");
        return;
    }

    QString defaultDir = ui->label_input_directory_value->text();
    if (defaultDir.isEmpty() || defaultDir == "-")
        defaultDir = QDir::homePath();

    QString baseName = ui->label_input_file_name_value->text();
    if (baseName.isEmpty() || baseName == "-")
        baseName = "export";
    baseName = QFileInfo(baseName).completeBaseName();

    const QString defaultPath = QDir(defaultDir).filePath(baseName + "_all_data.json");
    const QString savePath = QFileDialog::getSaveFileName(this,
                                                          "Export All Data",
                                                          defaultPath,
                                                          "JSON Files (*.json)");
    if (savePath.isEmpty())
        return;

    QFileInfo saveInfo(savePath);
    QDir saveDir = saveInfo.dir();
    if (!saveDir.exists())
        saveDir.mkpath(".");

    QVector<PolygonItem*> polygons;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type)
            polygons.append(static_cast<PolygonItem*>(item));
    }
    QHash<int, PolygonItem*> polygonById;
    for (PolygonItem *poly : polygons) {
        polygonById.insert(poly->id(), poly);
    }

    const auto pairKey = [](int a, int b) {
        const int first = std::min(a, b);
        const int second = std::max(a, b);
        return QString("%1-%2").arg(first).arg(second);
    };

    const QVector<QPair<int, int>> neighborPairs = GeometryIO::neighborPairsFromScene(scene);

    NeighborPairGeometrySettings geometrySettings = m_hasGeometryCalculationResults
            ? m_lastGeometrySettings
            : NeighborPairGeometryCalculator::currentSettings();
    const bool geometryEnabled = anyGeometryCalculationEnabled(geometrySettings);

    QHash<QString, BatchDivisionEstimator::GeometryEntry> storedGeometryByPair;
    for (const auto &entry : m_lastGeometryEntries) {
        storedGeometryByPair.insert(pairKey(entry.firstId, entry.secondId), entry);
    }

    QVector<BatchDivisionEstimator::GeometryEntry> geometryEntries;
    geometryEntries.reserve(neighborPairs.size());

    QStringList warningMessages;
    bool geometryDisabledWarningAdded = false;
    int pairIndex = 0;
    for (const auto &pair : neighborPairs) {
        BatchDivisionEstimator::GeometryEntry entry;
        const QString key = pairKey(pair.first, pair.second);
        if (storedGeometryByPair.contains(key)) {
            entry = storedGeometryByPair.value(key);
        } else if (geometryEnabled) {
            PolygonItem *first = polygonById.value(pair.first, nullptr);
            PolygonItem *second = polygonById.value(pair.second, nullptr);
            if (!first || !second) {
                warningMessages << QString("Neighbor pair (%1, %2) references missing polygons and was skipped during geometry export.")
                                   .arg(pair.first)
                                   .arg(pair.second);
                continue;
            }
            NeighborPairGeometryCalculator::Result result =
                    NeighborPairGeometryCalculator::calculateForPair(first, second, geometrySettings);
            result.first = nullptr;
            result.second = nullptr;
            entry.geometry = result;
        } else {
            if (!geometryDisabledWarningAdded) {
                warningMessages << "Geometry calculations are disabled; geometry values are not available for all neighbor pairs.";
                geometryDisabledWarningAdded = true;
            }
        }
        entry.fileName = baseName;
        entry.pairIndex = ++pairIndex;
        entry.firstId = pair.first;
        entry.secondId = pair.second;
        geometryEntries.append(entry);
    }

    if (neighborPairs.isEmpty()) {
        warningMessages << "No neighboring polygons were found; geometry and performance sections will be empty.";
    } else if (geometryEntries.isEmpty()) {
        warningMessages << "Neighbor pair geometry could not be generated.";
    }

    QHash<QString, int> realTimesByKey;
    for (const RealDivisionRecord &record : std::as_const(m_realDivisionRecords)) {
        const QString key = pairKey(record.firstId, record.secondId);
        if (!realTimesByKey.contains(key))
            realTimesByKey.insert(key, record.time);
    }

    QVector<DivisionPairRecord> realDivisionRecords;
    realDivisionRecords.reserve(m_realDivisionPairs.size());
    for (const auto &pair : std::as_const(m_realDivisionPairs)) {
        DivisionPairRecord record;
        record.firstId = pair.first;
        record.secondId = pair.second;
        record.time = realTimesByKey.value(pairKey(pair.first, pair.second), -1);
        realDivisionRecords.append(record);
    }

    BatchDivisionEstimator::DivisionMetrics performanceMetrics = m_lastPerformanceMetrics;
    bool hasPerformanceMetrics = m_hasPerformanceMetrics;
    if (!hasPerformanceMetrics && !neighborPairs.isEmpty()) {
        performanceMetrics = BatchDivisionEstimator::calculateMetrics(neighborPairs, m_estimatedDivisionPairs, m_realDivisionPairs);
        hasPerformanceMetrics = true;
    }

    ComprehensiveExportData exportData;
    exportData.neighborPairs = neighborPairs;
    exportData.geometryEntries = GeometryIO::updateGeometryEntryFileNames(geometryEntries, baseName);
    exportData.geometrySettings = geometrySettings;
    exportData.hasGeometryData = !geometryEntries.isEmpty();
    exportData.estimationCriterion = m_lastEstimationCriterion;
    exportData.hasEstimationCriterion = m_hasEstimationCriterion;
    exportData.estimatedDivisionPairs = m_estimatedDivisionPairs;
    exportData.realDivisionPairs = realDivisionRecords;
    exportData.performanceMetrics = performanceMetrics;
    exportData.hasPerformanceMetrics = hasPerformanceMetrics;

    QString errorMessage;
    QStringList exportWarnings = warningMessages;
    if (!GeometryIO::exportComprehensiveJson(savePath, scene, exportData, &errorMessage, &exportWarnings)) {
        QMessageBox::critical(this, "Export All Data", errorMessage.isEmpty() ? QString("Failed to export %1").arg(savePath) : errorMessage);
        return;
    }

    QString message = QString("Exported all data to:\n%1").arg(savePath);
    if (!exportWarnings.isEmpty())
        message += "\n\nWarnings:\n- " + exportWarnings.join("\n- ");

    QMessageBox::information(this, "Export All Data", message);
}

bool MainWindow::openRawImageInternal(const QString &filePath)
{
    //Forget previous image data
    m_currentImage.release();
    m_currentCanvasHeight=0;
    m_currentCanvasWidth=0;
    bool hadBackground = hasBackgroundPixmap();
    detachBackgroundItem();

    clearNeighborLines();
    clearEstimatedDivisionArrows();
    clearRealDivisionArrows();
    resetEstimatedDivisionNumberLabel();
    resetDivisionComparisonLabels();
    resetStoredGeometryResults();
    m_estimatedDivisionPairs.clear();
    m_realDivisionRecords.clear();
    resetVertexNumberLabel();
    resetLineNumberLabel();
    resetPolygonNumberLabel();
    resetSelectedItemLabels();

    auto info = m_canvasManager.openRawImage(ui->graphicsView, this, filePath);
    if(!info.valid) {
        if (hadBackground) {
            attachBackgroundToScene();
            fitViewToScene();
            double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
            updateCanvasSizeLabel(zoomPercent);
        }
        return false;
    }

    m_currentCanvasWidth = info.image.cols;
    m_currentCanvasHeight = info.image.rows;
    m_currentImage = info.image.clone();

    attachBackgroundToScene();
    fitViewToScene();
    double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
    updateCanvasSizeLabel(zoomPercent);

    ui->label_input_directory_value->setText(info.filePath);
    ui->label_input_file_name_value->setText(info.fileName);
    return true;
}

bool MainWindow::importJsonFile(const QString &filePath)
{
    GeometryImportResult result = GeometryIO::importFromJson(filePath, m_allowVertexManualMove);
    if(!result.success){
        QMessageBox::warning(this, "Import", result.errorMessage);
        return false;
    }

    detachBackgroundItem();

    clearNeighborLines();
    clearEstimatedDivisionArrows();
    clearRealDivisionArrows();
    resetEstimatedDivisionNumberLabel();
    resetDivisionComparisonLabels();
    resetStoredGeometryResults();
    m_estimatedDivisionPairs.clear();
    m_realDivisionRecords.clear();
    resetVertexNumberLabel();
    resetLineNumberLabel();
    resetPolygonNumberLabel();
    resetSelectedItemLabels();
    m_currentImage.release();

    m_canvasManager.deleteImageAndCanvas(ui->graphicsView, true);

    auto *scene = result.scene;
    if(!scene){
        QMessageBox::warning(this, "Import", "Import failed: scene was not created.");
        return false;
    }

    scene->setParent(ui->graphicsView);
    ui->graphicsView->setScene(scene);
    ui->graphicsView->fitInView(result.sceneRect, Qt::KeepAspectRatio);

    m_currentCanvasWidth = static_cast<int>(std::ceil(result.sceneRect.width()));
    m_currentCanvasHeight = static_cast<int>(std::ceil(result.sceneRect.height()));
    m_nextVertexId = result.nextVertexId;

    QHash<int, PolygonItem*> polygonById;
    for(QGraphicsItem *item : scene->items()){
        if(!item) continue;
        if(item->type() == VertexItem::Type){
            auto *vertex = static_cast<VertexItem*>(item);
            connect(vertex, &VertexItem::selected, this, &MainWindow::onVertexSelected);
            connect(vertex, &VertexItem::moved, this, &MainWindow::onVertexMoved);
        }else if(item->type() == LineItem::Type){
            connectLineSignals(static_cast<LineItem*>(item));
        }else if(item->type() == PolygonItem::Type){
            auto *polygon = static_cast<PolygonItem*>(item);
            connectPolygonSignals(polygon);
            polygonById.insert(polygon->id(), polygon);
        }
    }

    if (!result.neighborPairs.isEmpty()) {
        rebuildNeighborLinesFromPairs(result.neighborPairs);
    }

    if (result.hasGeometryData || !result.geometryEntries.isEmpty()) {
        m_lastGeometryEntries = result.geometryEntries;
        m_lastGeometrySettings = result.geometrySettings;
        m_hasGeometryCalculationResults = !m_lastGeometryEntries.isEmpty();
    }

    if (result.hasEstimationCriterion) {
        m_lastEstimationCriterion = result.estimationCriterion;
        m_hasEstimationCriterion = true;
    }

    const auto pairKey = [](int a, int b) {
        const int first = std::min(a, b);
        const int second = std::max(a, b);
        return QString("%1-%2").arg(first).arg(second);
    };

    const auto createArrowsForPairs = [&](const QVector<QPair<int, int>> &pairs,
                                          DivisionDisplay::ArrowCategory category,
                                          QVector<QGraphicsPathItem*> &container) {
        for (const auto &pair : pairs) {
            PolygonItem *firstPoly = polygonById.value(pair.first, nullptr);
            PolygonItem *secondPoly = polygonById.value(pair.second, nullptr);
            if (!firstPoly || !secondPoly)
                continue;

            auto *arrow = DivisionDisplay::createArrowItem(firstPoly->centroid(),
                                                           secondPoly->centroid(),
                                                           category);
            if (!arrow)
                continue;

            scene->addItem(arrow);
            container.append(arrow);
        }
    };

    if (!result.estimatedDivisionPairs.isEmpty()) {
        m_estimatedDivisionPairs = result.estimatedDivisionPairs;
        createArrowsForPairs(m_estimatedDivisionPairs, DivisionDisplay::ArrowCategory::Estimated, m_estimatedDivisionArrows);
        setEstimatedDivisionNumber(m_estimatedDivisionPairs.size());
    }

    if (!result.realDivisionRecords.isEmpty()) {
        QSet<QString> realKeys;
        for (const DivisionPairRecord &record : std::as_const(result.realDivisionRecords)) {
            const QString key = pairKey(record.firstId, record.secondId);
            if (realKeys.contains(key))
                continue;
            realKeys.insert(key);
            RealDivisionRecord realRecord;
            realRecord.firstId = record.firstId;
            realRecord.secondId = record.secondId;
            realRecord.time = record.time;
            m_realDivisionRecords.append(realRecord);
            m_realDivisionPairs.append(QPair<int, int>(record.firstId, record.secondId));
        }
        createArrowsForPairs(m_realDivisionPairs, DivisionDisplay::ArrowCategory::Real, m_realDivisionArrows);
        ui->label_real_division_number_value->setText(QString::number(m_realDivisionPairs.size()));
    }

    QVector<QPair<int, int>> neighborPairsForMetrics = result.neighborPairs;
    if (neighborPairsForMetrics.isEmpty())
        neighborPairsForMetrics = GeometryIO::neighborPairsFromScene(scene);

    QSet<QString> neighborKeys;
    for (const auto &pair : std::as_const(neighborPairsForMetrics))
        neighborKeys.insert(pairKey(pair.first, pair.second));

    QHash<QString, QPair<int, int>> estimatedPairByKey;
    QSet<QString> estimatedKeys;
    for (const auto &pair : std::as_const(m_estimatedDivisionPairs)) {
        const QString key = pairKey(pair.first, pair.second);
        estimatedKeys.insert(key);
        estimatedPairByKey.insert(key, pair);
    }

    QHash<QString, QPair<int, int>> realPairByKey;
    QSet<QString> realKeys;
    for (const auto &pair : std::as_const(m_realDivisionPairs)) {
        const QString key = pairKey(pair.first, pair.second);
        realKeys.insert(key);
        realPairByKey.insert(key, pair);
    }

    int truePositives = 0;
    int falsePositives = 0;
    int falseNegatives = 0;

    if (!neighborKeys.isEmpty() && (!estimatedKeys.isEmpty() || !realKeys.isEmpty())) {
        for (const QString &key : std::as_const(estimatedKeys)) {
            const QPair<int, int> pair = estimatedPairByKey.value(key);
            if (!neighborKeys.contains(key)) {
                result.warnings.append(QString("Estimated division pair (%1, %2) is not a neighbor in the current scene.")
                                       .arg(pair.first)
                                       .arg(pair.second));
                continue;
            }

            if (realKeys.contains(key)) {
                ++truePositives;
                m_truePositivePairs.append(pair);
                createArrowsForPairs({pair}, DivisionDisplay::ArrowCategory::TruePositive, m_truePositiveDivisionArrows);
            } else {
                ++falsePositives;
                m_falsePositivePairs.append(pair);
                createArrowsForPairs({pair}, DivisionDisplay::ArrowCategory::FalsePositive, m_falsePositiveDivisionArrows);
            }
        }

        for (const QString &key : std::as_const(realKeys)) {
            const QPair<int, int> pair = realPairByKey.value(key);
            if (!neighborKeys.contains(key)) {
                result.warnings.append(QString("Real division pair (%1, %2) is not a neighbor in the current scene.")
                                       .arg(pair.first)
                                       .arg(pair.second));
                continue;
            }

            if (estimatedKeys.contains(key))
                continue;

            ++falseNegatives;
            m_falseNegativePairs.append(pair);
            createArrowsForPairs({pair}, DivisionDisplay::ArrowCategory::FalseNegative, m_falseNegativeDivisionArrows);
        }
    }

    bool hasMetrics = false;
    BatchDivisionEstimator::DivisionMetrics metrics;
    if (result.hasPerformanceMetrics) {
        metrics = result.performanceMetrics;
        if (metrics.neighborPairs <= 0 && !neighborKeys.isEmpty())
            metrics.neighborPairs = neighborKeys.size();
        if (metrics.estimatedPairs < 0)
            metrics.estimatedPairs = estimatedKeys.size();
        if (metrics.realPairs < 0)
            metrics.realPairs = realKeys.size();
        hasMetrics = true;
    } else if (!neighborKeys.isEmpty() && (!estimatedKeys.isEmpty() || !realKeys.isEmpty())) {
        metrics = BatchDivisionEstimator::metricsFromCounts(neighborKeys.size(),
                                                            truePositives,
                                                            falsePositives,
                                                            falseNegatives,
                                                            estimatedKeys.size(),
                                                            realKeys.size());
        hasMetrics = true;
    }

    if (hasMetrics)
        applyDivisionMetricsToLabels(metrics);

    if(!result.warnings.isEmpty()){
        QMessageBox::warning(this, "Import", QString("Some items could not be imported:\n%1").arg(result.warnings.join("\n")));
    }

    updateVertexCountLabel();
    updateLineCountLabel();
    updatePolygonCountLabel();

    ui->label_input_directory_value->setText(filePath);
    ui->label_input_file_name_value->setText(QFileInfo(filePath).fileName());
    attachBackgroundToScene();
    fitViewToScene();
    double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
    updateCanvasSizeLabel(zoomPercent);

    return true;
}

void MainWindow::onZoomChanged(double zoomPercent)
{
    updateCanvasSizeLabel(zoomPercent);
}

void MainWindow::onMousePositionChanged(const QPointF &scenePos)
{
    m_lastMouseScenePos = scenePos;

    if(m_currentImage.empty()){
        ui->label_mouse_position_value->setText("-");
        ui->label_mouse_pixel_value->setText("-");
        return;
    }

    int x = static_cast<int>(std::floor(scenePos.x()));
    int y = static_cast<int>(std::floor(scenePos.y()));

    //check bounds
    if( x < 0 || y < 0 || x >= m_currentImage.cols || y >= m_currentImage.rows)
    {
        ui->label_mouse_position_value->setText("-");
        ui->label_mouse_pixel_value->setText("-");
        return;
    }

    QString text_Pos, text_Pixel;
    text_Pos = QString("(%1,%2)").arg(x).arg(y);
    int type = m_currentImage.type();
    int channels = m_currentImage.channels();

    if(channels ==1 )
    { //grayscaled image
        uchar v = m_currentImage.at<uchar>(y,x);
        text_Pixel = QString("gray=%3").arg(v);
    }
    else if(channels == 3)
    {//BGR
        cv::Vec3b bgr = m_currentImage.at<cv::Vec3b>(y,x);
        int B = bgr[0], G = bgr[1], R = bgr[2];
        text_Pixel = QString("RGB=(%1,%2,%3)").arg(R).arg(G).arg(B);
    }
    else if(channels == 4)
    {//BGRA
        cv::Vec4b bgra = m_currentImage.at<cv::Vec4b>(y,x);
        int B = bgra[0], G = bgra[1], R = bgra[2], A = bgra[3];
        text_Pixel = QString("RGBA=(%1,%2,%3,%4)").arg(R).arg(G).arg(B).arg(A);
    }
    else{
        text_Pixel = QString("(%3ch type=%4)").arg(channels).arg(type);
    }

    ui->label_mouse_position_value->setText(text_Pos);
    ui->label_mouse_pixel_value->setText(text_Pixel);
}

bool MainWindow::hasBackgroundPixmap() const
{
    return !m_backgroundPixmap.isNull();
}

void MainWindow::detachBackgroundItem()
{
    if (!m_backgroundItem)
        return;

    if (auto *scene = m_backgroundItem->scene()) {
        scene->removeItem(m_backgroundItem);
    }

    delete m_backgroundItem;
    m_backgroundItem = nullptr;
}

void MainWindow::attachBackgroundToScene()
{
    detachBackgroundItem();

    if (!hasBackgroundPixmap())
        return;

    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        scene = new QGraphicsScene(ui->graphicsView);
        ui->graphicsView->setScene(scene);
    }

    m_backgroundItem = scene->addPixmap(m_backgroundPixmap);
    m_backgroundItem->setZValue(-1000.0);
    m_backgroundItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_backgroundItem->setFlag(QGraphicsItem::ItemIsMovable, false);
    m_backgroundItem->setPos(0, 0);

    const QRectF bgRect = m_backgroundItem->boundingRect();
    const QRectF currentRect = scene->sceneRect();

    if (currentRect.isNull() || currentRect.width() <= 0 || currentRect.height() <= 0) {
        scene->setSceneRect(bgRect);
    } else {
        scene->setSceneRect(currentRect.united(bgRect));
    }
}

QVector<QGraphicsItem*> MainWindow::collectItemsByType(int type) const
{
    QVector<QGraphicsItem*> itemsOfType;
    auto *scene = ui->graphicsView->scene();
    if (!scene)
        return itemsOfType;

    const auto allItems = scene->items();
    for (QGraphicsItem *item : allItems) {
        if (item && item->type() == type)
            itemsOfType.append(item);
    }
    return itemsOfType;
}

void MainWindow::setVisibilityForItems(const QVector<QGraphicsItem*> &items, bool visible, QVector<QPair<QGraphicsItem*, bool>> &record) const
{
    for (QGraphicsItem *item : items) {
        if (!item)
            continue;
        record.append({item, item->isVisible()});
        item->setVisible(visible);
    }
}

bool MainWindow::exportSceneImage(const QString &filePath,
                                  bool includePolygons,
                                  bool includeLines,
                                  bool includeVertices,
                                  bool includeRealArrows,
                                  bool includeEstimatedArrows,
                                  bool includeComparisonArrows,
                                  double scaleFactor)
{
    auto *scene = ui->graphicsView->scene();
    if (!scene)
        return false;

    const QRectF rect = scene->sceneRect();
    if (rect.isNull() || rect.width() <= 0 || rect.height() <= 0)
        return false;

    QVector<QPair<QGraphicsItem*, bool>> visibilityRecord;

    if (!includePolygons) {
        setVisibilityForItems(collectItemsByType(PolygonItem::Type), false, visibilityRecord);
    }

    if (!includeLines) {
        setVisibilityForItems(collectItemsByType(LineItem::Type), false, visibilityRecord);
        QVector<QGraphicsItem*> neighborLineItems;
        neighborLineItems.reserve(m_neighborLines.size());
        for (QGraphicsLineItem *line : m_neighborLines) {
            if (line)
                neighborLineItems.append(line);
        }
        setVisibilityForItems(neighborLineItems, false, visibilityRecord);
    }

    if (!includeVertices) {
        setVisibilityForItems(collectItemsByType(VertexItem::Type), false, visibilityRecord);
    }

    const auto hideArrowVector = [&](const QVector<QGraphicsPathItem*> &arrows, bool include) {
        if (include)
            return;
        QVector<QGraphicsItem*> items;
        items.reserve(arrows.size());
        for (QGraphicsPathItem *arrow : arrows) {
            if (arrow)
                items.append(arrow);
        }
        setVisibilityForItems(items, false, visibilityRecord);
    };

    hideArrowVector(m_realDivisionArrows, includeRealArrows);
    hideArrowVector(m_estimatedDivisionArrows, includeEstimatedArrows);
    if (!includeComparisonArrows) {
        hideArrowVector(m_truePositiveDivisionArrows, false);
        hideArrowVector(m_falsePositiveDivisionArrows, false);
        hideArrowVector(m_falseNegativeDivisionArrows, false);
    }

    const double clampedScale = std::clamp(scaleFactor, 1.0, 10.0);
    const int width = qCeil(rect.width() * clampedScale);
    const int height = qCeil(rect.height() * clampedScale);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.scale(clampedScale, clampedScale);
    scene->render(&painter, QRectF(QPointF(0, 0), QSizeF(rect.width(), rect.height())), rect);
    painter.end();

    const bool saved = image.save(filePath);

    for (auto it = visibilityRecord.rbegin(); it != visibilityRecord.rend(); ++it) {
        if (it->first)
            it->first->setVisible(it->second);
    }

    return saved;
}

void MainWindow::fitViewToScene()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene)
        return;

    ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void MainWindow::updateCanvasSizeLabel(double zoomPercent)
{
    int zoomInt = qRound(zoomPercent);

    QStringList parts;
    if (m_currentCanvasHeight > 0 && m_currentCanvasWidth > 0) {
        parts << QString("%1 x %2").arg(m_currentCanvasWidth).arg(m_currentCanvasHeight);
    }
    if (m_backgroundHeight > 0 && m_backgroundWidth > 0) {
        parts << QString("Background %1 x %2").arg(m_backgroundWidth).arg(m_backgroundHeight);
    }

    if(!parts.isEmpty()){
        ui->label_canvas_size_value->setText(QString("%1 (%2% zoom)").arg(parts.join(" | ")).arg(zoomInt));
    }
    else{
        ui->label_canvas_size_value->setText(QString("-"));
    }
}

void MainWindow::onDeleteImage(){
    clearNeighborLines();
    clearEstimatedDivisionArrows();
    clearRealDivisionArrows();
    detachBackgroundItem();
    m_canvasManager.deleteImageAndCanvas(ui->graphicsView, true);

    //Clear application state
    m_currentImage.release();
    m_currentCanvasWidth = 0;
    m_currentCanvasHeight = 0;
    m_realDivisionRecords.clear();

    //Update labels
    ui->label_input_directory_value->setText("-");
    ui->label_input_file_name_value->setText("-");
    ui->label_mouse_position_value->setText("-");
    ui->label_mouse_pixel_value->setText("-");
    resetVertexNumberLabel();
    resetLineNumberLabel();
    resetPolygonNumberLabel();
    resetEstimatedDivisionNumberLabel();
    resetDivisionComparisonLabels();
    resetStoredGeometryResults();

    if (hasBackgroundPixmap()) {
        attachBackgroundToScene();
        fitViewToScene();
    }

    double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
    updateCanvasSizeLabel(zoomPercent);

}

void MainWindow::onAddVertex()
{
    if (!m_allowManualVertexCreation) {
        QMessageBox::information(this, "Add Vertex Disabled", "Manual vertex creation is currently disabled in Edit Settings.");
        return;
    }

    auto *scene = ui->graphicsView->scene();
    if (!scene) return;

    VertexItem* v = VertexItem::createWithDialog(scene, ui->graphicsView, this, m_nextVertexId);
    if(!v) return;

    connect(v, &VertexItem::selected, this , &MainWindow::onVertexSelected);
    connect(v, &VertexItem::moved, this, &MainWindow::onVertexMoved);

    updateVertexCountLabel();
}

void MainWindow::onDeleteVertex()
{
    auto *scene = ui->graphicsView->scene();
    VertexItem::deleteWithDialog(scene, ui->graphicsView, this);
    resetStoredGeometryResults();
    updateVertexCountLabel();
    updateLineCountLabel();
}

void MainWindow::onDeleteAllVertices()
{
    VertexItem::deleteAllVertices(ui->graphicsView->scene(), this);
    resetStoredGeometryResults();
    updateVertexCountLabel();
    updateLineCountLabel();
}

void MainWindow::updateVertexCountLabel()
{
    int n = VertexItem::countVertices(ui->graphicsView->scene());
    setVertexNumber(n);
}

void MainWindow::onVertexSelected(VertexItem *v){
    if(!v) return;

    ui->label_selected_item_name->setText("Vertex");
    ui->label_selected_item_id_value->setText(QString::number(v->id()));
    ui->label_selected_item_pos_value->setText(QString("(%1,%2)").arg(v->pos().x(),0,'f',1).arg(v->pos().y(),0,'f',1));
}

void MainWindow::onVertexMoved(VertexItem *v, const QPointF &pos)
{
    Q_UNUSED(v);
    ui->label_selected_item_pos_value->setText(QString("%1,%2").arg(pos.x(),0,'f',1).arg(pos.y(),0,'f',1));
}

void MainWindow::addVertexAt(const QPointF &scenePos)
{
    if (!m_allowManualVertexCreation) {
        QMessageBox::information(this, "Add Vertex Disabled", "Manual vertex creation is currently disabled in Edit Settings.");
        return;
    }

    auto *scene = ui->graphicsView->scene();
    if (!scene || scene->sceneRect().isNull() ||
        scene->sceneRect().width() <= 0 || scene->sceneRect().height() <= 0)
    {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image before adding a vertex.");
        return;
    }

    // Use integer pixel center
    int x = static_cast<int>(std::floor(scenePos.x()));
    int y = static_cast<int>(std::floor(scenePos.y()));
    QPointF p(x, y);

    if (!scene->sceneRect().contains(p)) {
        // ignore if mouse is outside canvas
        return;
    }

    // Create vertex directly (no dialog)
    auto *v = new VertexItem(m_nextVertexId++, p);
    scene->addItem(v);

     v->setFlag(QGraphicsItem::ItemIsMovable, m_allowVertexManualMove);
    // Connect selection/move signals for label updates
    connect(v, &VertexItem::selected, this, &MainWindow::onVertexSelected);
    connect(v, &VertexItem::moved, this, &MainWindow::onVertexMoved);

    // Auto-select it
    scene->clearSelection();
    v->setSelected(true);

    updateVertexCountLabel();
    // Update selected labels immediately (optional; selection signal should also fire)
    onVertexSelected(v);
}

void MainWindow::deleteSelectedItems()
{
    clearNeighborLines();

    auto *scene = ui->graphicsView->scene();
    if (!scene) return;

    const auto selected = scene->selectedItems();
    if (selected.isEmpty())
        return;

    // 1) Collect selected vertices & lines & polygons (do NOT delete while iterating)
    QList<VertexItem*> verticesToDelete;
    QList<LineItem*> linesToDelete;
    QList<PolygonItem*> polygonsToDelete;

    for (QGraphicsItem *it : selected) {
        if (!it) continue;
        if (it->type() == PolygonItem::Type){
            polygonsToDelete.append(static_cast<PolygonItem*>(it));
        } else if (it->type() == LineItem::Type) {
            linesToDelete.append(static_cast<LineItem*>(it));
        } else if (it->type() == VertexItem::Type) {
            verticesToDelete.append(static_cast<VertexItem*>(it));
        }
    }

    // 2) Delete selected polygons
    for (PolygonItem *poly: polygonsToDelete){
        scene->removeItem(poly);
        delete poly;
    }

    // 3) Delete selected lines first
    for (LineItem *L : linesToDelete) {
        scene->removeItem(L);
        delete L;
    }

    // 4) Delete selected vertices
    for (VertexItem *V : verticesToDelete) {
        scene->removeItem(V);
        delete V;
    }



    // 5) Update UI
    updateVertexCountLabel();
    updateLineCountLabel();

    ui->label_selected_item_name->setText("-");
    ui->label_selected_item_id_value->setText("-");
    ui->label_selected_item_pos_value->setText("-");
}

void MainWindow::onVertexDisplaySetting(){
    VertexItem::showDisplaySettingDialog(ui->graphicsView->scene(),this);
}

void MainWindow::onFindVertex(){
    VertexItem::findWithDialog(ui->graphicsView->scene(),ui->graphicsView,this);
}

void MainWindow::onAddLine()
{
    if (!m_allowManualLineCreation) {
        QMessageBox::information(this, "Add Line Disabled", "Manual line creation is currently disabled in Edit Settings.");
        return;
    }

    LineItem* l = LineItem::createWithDialog(ui->graphicsView->scene(), this);
    connectLineSignals(l);
    updateLineCountLabel();
}

void MainWindow::onDeleteLine()
{
    LineItem::deleteWithDialog(ui->graphicsView->scene(), this);
    updateLineCountLabel();
}

void MainWindow::updateLineCountLabel()
{
    int n = LineItem::countLines(ui->graphicsView->scene());
    setLineNumber(n);
}

void MainWindow::updatePolygonCountLabel(){
    int n = PolygonItem::countPolygons(ui->graphicsView->scene());
    setPolygonNumber(n);
}

void MainWindow::addLineFromSelectedVertices()
{
    if (!m_allowManualLineCreation) {
        QMessageBox::information(this, "Add Line Disabled", "Manual line creation is currently disabled in Edit Settings.");
        return;
    }

    auto *scene = ui->graphicsView->scene();
    if (!scene) return;

    // Collect selected vertices
    QList<VertexItem*> selectedVertices;
    for (QGraphicsItem *it : scene->selectedItems()) {
        if (it && it->type() == VertexItem::Type) {
            selectedVertices.append(static_cast<VertexItem*>(it));
        }
    }

    if (selectedVertices.size() != 2) {
        QMessageBox::information(this, "Add Line", "Please select exactly TWO vertices, then press Ctrl+L.");
        return;
    }

    VertexItem *v1 = selectedVertices[0];
    VertexItem *v2 = selectedVertices[1];

    if (!v1 || !v2 || v1 == v2)
        return;

    // Optional: avoid duplicate line
    if (LineItem::findLineByVertexIds(scene, v1->id(), v2->id())) {
        QMessageBox::information(this, "Add Line", "A line between these two vertices already exists.");
        return;
    }

    // Create line
    auto *line = new LineItem(v1, v2);
    scene->addItem(line);

    // Auto-select line
    scene->clearSelection();
    line->setSelected(true);


    connectLineSignals(line);
    updateLineCountLabel();
}

void MainWindow::onDeleteAllLines()
{
    LineItem::deleteAllLines(ui->graphicsView->scene(),this);
    updateLineCountLabel();
}

void MainWindow::onLineSelected(LineItem *L)
{
    if (!L) return;

    ui->label_selected_item_name->setText("Line");
    ui->label_selected_item_id_value->setText(QString::number(L->id()));

    QPointF m = L->meanPos();
    ui->label_selected_item_pos_value->setText(
        QString("(%1, %2)").arg(m.x(), 0, 'f', 1).arg(m.y(), 0, 'f', 1)
    );
}

void MainWindow::onLineGeometryChanged(LineItem *L)
{
    if (!L) return;

    QPointF m = L->meanPos();
    ui->label_selected_item_pos_value->setText(
        QString("(%1, %2)").arg(m.x(), 0, 'f', 1).arg(m.y(), 0, 'f', 1)
        );
}

void MainWindow::onPolygonSelected(PolygonItem *poly)
{
    if (!poly) return;

    ui->label_selected_item_name->setText("Polygon");
    ui->label_selected_item_id_value->setText(QString::number(poly->id()));

    QPointF c = poly->centroid();
    ui->label_selected_item_pos_value->setText(
        QString("(%1, %2)").arg(c.x(), 0, 'f', 1).arg(c.y(), 0, 'f', 1)
        );
}

void MainWindow::onPolygonGeometryChanged(PolygonItem *poly)
{
    if (!poly) return;

    resetStoredGeometryResults();
    QPointF c = poly->centroid();
    ui->label_selected_item_pos_value->setText(QString("(%1, %2)").arg(c.x(), 0, 'f', 1).arg(c.y(), 0, 'f', 1));
    clearNeighborLines();
}

void MainWindow::onFindLine(){
    LineItem::findWithDialog(ui->graphicsView->scene(),ui->graphicsView,this);
}

void MainWindow::onLineDisplaySetting()
{
    LineItem::showDisplaySettingDialog(ui->graphicsView->scene(), this);
}

void MainWindow::connectLineSignals(LineItem *line)
{
    if (!line) return;

    connect(line, &LineItem::selected, this, &MainWindow::onLineSelected, Qt::UniqueConnection);
    connect(line, &LineItem::geometryChanged, this, &MainWindow::onLineGeometryChanged, Qt::UniqueConnection);
}

void MainWindow::connectAllLinesInScene()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) return;

    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == LineItem::Type) {
            connectLineSignals(static_cast<LineItem*>(it));
        }
    }
}

void MainWindow::connectPolygonSignals(PolygonItem *polygon)
{
    if (!polygon) return;

    connect(polygon, &PolygonItem::selected, this, &MainWindow::onPolygonSelected, Qt::UniqueConnection);
    connect(polygon, &PolygonItem::geometryChanged, this, &MainWindow::onPolygonGeometryChanged, Qt::UniqueConnection);
}

void MainWindow::connectAllPolygonsInScene()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) return;

    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == PolygonItem::Type) {
            connectPolygonSignals(static_cast<PolygonItem*>(it));
        }
    }
}

void MainWindow::onAddPolygon(){
    if (!m_allowManualPolygonCreation) {
        QMessageBox::information(this, "Add Polygon Disabled", "Manual polygon creation is currently disabled in Edit Settings.");
        return;
    }

    resetStoredGeometryResults();
    PolygonItem *poly = PolygonItem::createWithDialog(ui->graphicsView->scene(), ui->graphicsView, this, true);
    connectPolygonSignals(poly);
    connectAllLinesInScene();
    updatePolygonCountLabel();
    updateLineCountLabel();
}


void MainWindow::onDeletePolygon(){
    if(PolygonItem::deleteWithDialog(ui->graphicsView->scene(), this)){
        resetStoredGeometryResults();
        updatePolygonCountLabel();
    }
}

void MainWindow::onDeleteAllPolygons(){
    int deleted = PolygonItem::deleteAllPolygons(ui->graphicsView->scene(), this);
    if(deleted > 0){
        resetStoredGeometryResults();
        updatePolygonCountLabel();
    }
}

void MainWindow::onFindPolygon(){
    PolygonItem *poly = PolygonItem::findWithDialog(ui->graphicsView->scene(), ui->graphicsView, this);
    connectPolygonSignals(poly);
}

void MainWindow::onPolygonDisplaySetting(){
    PolygonItem::showDisplaySettingDialog(ui->graphicsView->scene(), this);
}

void MainWindow::addPolygonFromSelectedVertices()
{
    if (!m_allowManualPolygonCreation) {
        QMessageBox::information(this, "Add Polygon Disabled", "Manual polygon creation is currently disabled in Edit Settings.");
        return;
    }

    auto *scene = ui->graphicsView->scene();
    if (!scene) return;

    // Collect selected vertices
    QList<VertexItem*> selectedVertices;
    for (QGraphicsItem *it : scene->selectedItems()) {
        if (it && it->type() == VertexItem::Type) {
            selectedVertices.append(static_cast<VertexItem*>(it));
        }
    }

    if (selectedVertices.size() < 3) {
        QMessageBox::information(this, "Add Polygon", "Please select at least THREE vertices, then press Ctrl+P.");
        return;
    }

    // Ensure uniqueness
    QSet<int> uniqueIds;
    for (auto *v : selectedVertices) {
        if (!v) continue;
        uniqueIds.insert(v->id());
    }

    if (uniqueIds.size() < selectedVertices.size()) {
        QMessageBox::information(this, "Add Polygon", "Please select unique vertices (no duplicates).");
        return;
    }

    // Order vertices counter-clockwise around centroid
    QPointF centroid(0.0, 0.0);
    for (auto *v : selectedVertices) centroid += v->pos();
    centroid /= selectedVertices.size();

    std::sort(selectedVertices.begin(), selectedVertices.end(), [centroid](VertexItem *a, VertexItem *b){
        const QPointF pa = a->pos();
        const QPointF pb = b->pos();
        double angleA = std::atan2(pa.y() - centroid.y(), pa.x() - centroid.x());
        double angleB = std::atan2(pb.y() - centroid.y(), pb.x() - centroid.x());
        return angleA < angleB;
    });

    QVector<int> ids;
    ids.reserve(selectedVertices.size());
    for (auto *v : selectedVertices) ids.push_back(v->id());

    if (PolygonItem::findPolygonByVertexIds(scene, ids)) {
        QMessageBox::information(this, "Add Polygon", "A polygon with these vertices already exists.");
        return;
    }

    resetStoredGeometryResults();
    auto *poly = new PolygonItem(selectedVertices, scene, true);
    scene->addItem(poly);

    scene->clearSelection();
    poly->setSelected(true);

    connectPolygonSignals(poly);
    connectAllLinesInScene();
    updatePolygonCountLabel();
    updateLineCountLabel();
}

void MainWindow::onGenerateRandomNetwork(){
    DebugManager::generate(this);
}

void MainWindow::onCompareBatchEstimations()
{
    DebugManager::compareBatchEstimations(this);
}

void MainWindow::onExportFeatureNames()
{
    DebugManager::exportFeatureNames(this);
}

void MainWindow::clearNeighborLines()
{
    QGraphicsScene *scene = ui->graphicsView->scene();
    for (QGraphicsLineItem *line : m_neighborLines) {
        if (scene && line)
            scene->removeItem(line);
        delete line;
    }
    m_neighborLines.clear();

    if (scene) {
        GeometryIO::setNeighborPairsOnScene(scene, {});
    }

    ui->label_neighbor_polygon_number_value->setText(QString::number(0));
}

void MainWindow::rebuildNeighborLinesFromPairs(const QVector<QPair<int, int>> &pairs)
{
    auto *scene = ui->graphicsView->scene();
    clearNeighborLines();

    if (!scene)
        return;

    QHash<int, PolygonItem*> polygonsById;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type) {
            auto *poly = static_cast<PolygonItem*>(item);
            polygonsById.insert(poly->id(), poly);
        }
    }

    const NeighborLineStyle style = NeighborPairDisplay::currentStyle();
    QVector<QPair<int, int>> validPairs;
    int neighborCount = 0;

    for (const auto &pair : pairs) {
        PolygonItem *first = polygonsById.value(pair.first, nullptr);
        PolygonItem *second = polygonsById.value(pair.second, nullptr);
        if (!first || !second)
            continue;

        auto *line = new QGraphicsLineItem(QLineF(first->centroid(), second->centroid()));
        line->setPen(QPen(style.color, style.widthPx, Qt::DashLine));
        line->setZValue(1);
        line->setVisible(NeighborPairDisplay::displayEnabled());
        scene->addItem(line);
        m_neighborLines.append(line);
        validPairs.append(pair);
        ++neighborCount;
    }

    GeometryIO::setNeighborPairsOnScene(scene, validPairs);
    ui->label_neighbor_polygon_number_value->setText(QString::number(neighborCount));
}

void MainWindow::onDetectNeighborPairs()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return;
    }

    clearNeighborLines();

    QVector<PolygonItem*> polygons;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type)
            polygons.append(static_cast<PolygonItem*>(item));
    }

    if (polygons.size() < 2) {
        QMessageBox::information(this, "Detect Neighbor Pairs", "Please ensure at least two polygons are available.");
        return;
    }

    const NeighborLineStyle style = NeighborPairDisplay::currentStyle();
    int neighborCount = 0;
    QVector<QPair<int, int>> neighborPairs;

    for (int i = 0; i < polygons.size(); ++i) {
        for (int j = i + 1; j < polygons.size(); ++j) {
            NeighborPair pair(polygons[i], polygons[j]);
            if (!pair.isNeighbor())
                continue;

            QPointF c1 = polygons[i]->centroid();
            QPointF c2 = polygons[j]->centroid();

            auto *line = new QGraphicsLineItem(QLineF(c1, c2));
            line->setPen(QPen(style.color, style.widthPx, Qt::DashLine));
            line->setZValue(1);
            line->setVisible(NeighborPairDisplay::displayEnabled());
            scene->addItem(line);
            m_neighborLines.append(line);

            ++neighborCount;
            neighborPairs.append(QPair<int, int>(polygons[i]->id(), polygons[j]->id()));
        }
    }

    GeometryIO::setNeighborPairsOnScene(scene, neighborPairs);
    ui->label_neighbor_polygon_number_value->setText(QString::number(neighborCount));

    if (neighborCount == 0) {
        QMessageBox::information(this, "Detect Neighbor Pairs", "No neighboring polygons were found.");
        clearNeighborLines();
    }
}

void MainWindow::onNeighborPairDisplaySetting()
{
    NeighborPairDisplay::showDisplaySettingDialog(ui->graphicsView->scene(), m_neighborLines, this);
}

void MainWindow::onDivisionPairDisplaySetting()
{
    DivisionDisplay::showDisplaySettingDialog(ui->graphicsView->scene(),
                                              m_estimatedDivisionArrows,
                                              m_realDivisionArrows,
                                              m_truePositiveDivisionArrows,
                                              m_falsePositiveDivisionArrows,
                                              m_falseNegativeDivisionArrows,
                                              this);
}

void MainWindow::onSkeletonization()
{
    if (m_currentImage.empty()) {
        QMessageBox::warning(this, "No Image Loaded", "Please open a raw image before running skeletonization.");
        return;
    }

    cv::Mat segmented = ImageAnalysis::segmentWithGuoHall(m_currentImage);
    if (segmented.empty()) {
        QMessageBox::warning(this, "Segmentation Failed", "The segmentation result is empty.");
        return;
    }

    m_currentImage = segmented;

    QImage qimg(segmented.data, segmented.cols, segmented.rows, static_cast<int>(segmented.step), QImage::Format_Grayscale8);
    QImage copy = qimg.copy();

    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        scene = new QGraphicsScene(ui->graphicsView);
        ui->graphicsView->setScene(scene);
    }

    detachBackgroundItem();
    scene->clear();
    scene->setSceneRect(0, 0, copy.width(), copy.height());
    scene->addPixmap(QPixmap::fromImage(copy));

    m_currentCanvasWidth = segmented.cols;
    m_currentCanvasHeight = segmented.rows;
    attachBackgroundToScene();
    fitViewToScene();
    double zoomPercent = static_cast<InteractiveGraphicsView*>(ui->graphicsView)->currentZoomPercent();
    updateCanvasSizeLabel(zoomPercent);
}

void MainWindow::resetSelectedItemLabels()
{
    ui->label_selected_item_name->setText("-");
    ui->label_selected_item_id_value->setText("-");
    ui->label_selected_item_pos_value->setText("-");
}

void MainWindow::onVertexDetection()
{
    if (m_currentImage.empty()) {
        QMessageBox::warning(this, "No Skeleton Image", "Please run skeletonization before detecting vertices.");
        return;
    }

    auto *scene = ui->graphicsView->scene();
    if (!scene || scene->sceneRect().isNull()) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image before detecting vertices.");
        return;
    }

    if (m_currentImage.channels() != 1) {
        QMessageBox::warning(this, "Unsupported Image", "Vertex detection requires a skeletonized grayscale image.");
        return;
    }

    const int rows = m_currentImage.rows;
    const int cols = m_currentImage.cols;
    if (rows < 3 || cols < 3) {
        QMessageBox::information(this, "Vertex Detection", "Image is too small to detect vertices.");
        return;
    }

    const std::vector<cv::Point2d> detectedVertices = ImageAnalysis::detectVertices(m_currentImage);

    int addedCount = 0;
    for (const auto &point : detectedVertices) {
        QPointF position(point.x, point.y);
        if (VertexItem::findVertexByPosition(scene, position, 0.0))
            continue;

        auto *vertex = new VertexItem(m_nextVertexId++, position);
        scene->addItem(vertex);
        vertex->setFlag(QGraphicsItem::ItemIsMovable, m_allowVertexManualMove);
        connect(vertex, &VertexItem::selected, this, &MainWindow::onVertexSelected);
        connect(vertex, &VertexItem::moved, this, &MainWindow::onVertexMoved);
        ++addedCount;
    }

    updateVertexCountLabel();

    //QMessageBox::information(this, "Vertex Detection", QString("Detected %1 vertices with more than three 8-neighbors.").arg(addedCount));
}

void MainWindow::onLineDetection()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return;
    }

    if (m_currentImage.empty()) {
        QMessageBox::warning(this, "No Image Loaded", "Please open a raw image and run skeletonization first.");
        return;
    }

    if (m_currentImage.channels() != 1) {
        QMessageBox::warning(this, "Skeleton Required", "Please run skeletonization so the current image is a single-channel skeleton.");
        return;
    }

    QList<VertexItem*> vertexItems;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == VertexItem::Type)
            vertexItems.append(static_cast<VertexItem*>(item));
    }

    if (vertexItems.size() < 2) {
        QMessageBox::information(this, "Line Detection", "Please ensure at least two vertices exist before detecting lines.");
        return;
    }

    std::sort(vertexItems.begin(), vertexItems.end(), [](VertexItem *a, VertexItem *b) {
        return a && b ? a->id() < b->id() : a < b;
    });

    std::vector<cv::Point> vertices;
    vertices.reserve(static_cast<std::size_t>(vertexItems.size()));
    for (VertexItem *v : vertexItems) {
        if (!v)
            continue;
        QPointF p = v->pos();
        vertices.emplace_back(cvRound(p.x()), cvRound(p.y()));
    }

    const std::vector<ImageAnalysis::LineConnection> connections =
        ImageAnalysis::detectLines(m_currentImage, vertices, 1);

    int addedCount = 0;
    for (const auto &connection : connections) {
        const int startIdx = connection.startVertexIndex;
        const int endIdx = connection.endVertexIndex;

        if (startIdx < 0 || endIdx < 0 ||
            startIdx >= vertexItems.size() || endIdx >= vertexItems.size())
            continue;

        VertexItem *v1 = vertexItems[startIdx];
        VertexItem *v2 = vertexItems[endIdx];

        if (!v1 || !v2 || v1 == v2)
            continue;

        if (LineItem::findLineByVertexIds(scene, v1->id(), v2->id()))
            continue;

        auto *line = new LineItem(v1, v2);
        scene->addItem(line);
        connectLineSignals(line);
        ++addedCount;
    }

    updateLineCountLabel();

    //QMessageBox::information(this, "Line Detection", QString("Detected %1 connections and added %2 new lines.").arg(connections.size()).arg(addedCount));
}

void MainWindow::onPolygonDetection()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene || scene->items().isEmpty()) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return;
    }

    QList<VertexItem*> vertexItems;
    QList<LineItem*> lineItems;

    for (QGraphicsItem *item : scene->items()) {
        if (!item)
            continue;
        if (item->type() == VertexItem::Type)
            vertexItems.append(static_cast<VertexItem*>(item));
        else if (item->type() == LineItem::Type)
            lineItems.append(static_cast<LineItem*>(item));
    }

    if (vertexItems.size() < 3 || lineItems.size() < 3) {
        QMessageBox::information(this, "Polygon Detection", "Please ensure at least three vertices and three lines exist.");
        return;
    }

    resetStoredGeometryResults();

    std::vector<ImageAnalysis::VertexGeometry> vertices;
    vertices.reserve(vertexItems.size());
    for (VertexItem *v : vertexItems) {
        if (!v)
            continue;
        vertices.push_back({v->id(), cv::Point2d(v->pos().x(), v->pos().y())});
    }

    std::vector<ImageAnalysis::GraphEdge> edges;
    edges.reserve(lineItems.size());
    for (LineItem *line : lineItems) {
        if (!line)
            continue;
        const int id1 = line->v1Id();
        const int id2 = line->v2Id();
        if (id1 < 0 || id2 < 0)
            continue;
        edges.push_back({id1, id2});
    }

    const std::vector<std::vector<int>> detectedPolygons = ImageAnalysis::detectPolygonsCCW(vertices, edges);

    auto toQVector = [](const std::vector<int> &ids) {
        QVector<int> qvec;
        qvec.reserve(static_cast<int>(ids.size()));
        for (int id : ids)
            qvec.push_back(id);
        return qvec;
    };

    auto canonicalKeyForPolygon = [](const QVector<int> &ids) {
        std::vector<int> vec;
        vec.reserve(ids.size());
        for (int id : ids)
            vec.push_back(id);
        return QString::fromStdString(ImageAnalysis::polygonKey(vec));
    };

    QSet<QString> existingKeys;
    QList<PolygonItem*> polygonsInScene;
    for (QGraphicsItem *item : scene->items()) {
        if (!item || item->type() != PolygonItem::Type)
            continue;

        auto *poly = static_cast<PolygonItem*>(item);
        polygonsInScene.append(poly);
        const QString key = canonicalKeyForPolygon(poly->vertexIds());
        existingKeys.insert(key);
    }

    int addedCount = 0;
    QSet<QString> newKeys;

    QHash<int, VertexItem*> vertexById;
    for (VertexItem *v : vertexItems) {
        if (v)
            vertexById.insert(v->id(), v);
    }

    for (const auto &poly : detectedPolygons) {
        const QVector<int> polyIds = toQVector(poly);
        const QString key = canonicalKeyForPolygon(polyIds);
        if (key.isEmpty())
            continue;

        if (existingKeys.contains(key) || newKeys.contains(key))
            continue;

        newKeys.insert(key);

        if (PolygonItem::findPolygonByVertexIds(scene, polyIds))
            continue;

        QVector<VertexItem*> polyVertices;
        polyVertices.reserve(polyIds.size());
        bool allVerticesFound = true;
        for (int vid : polyIds) {
            VertexItem *vertex = vertexById.value(vid, nullptr);
            if (!vertex) {
                allVerticesFound = false;
                break;
            }
            polyVertices.append(vertex);
        }

        if (!allVerticesFound || polyVertices.size() < 3)
            continue;

        PolygonItem *polygonItem = new PolygonItem(polyVertices, scene, false);
        scene->addItem(polygonItem);

        connectPolygonSignals(polygonItem);
        ++addedCount;
    }

    QHash<QString, PolygonItem*> seenPolygons;
    QList<PolygonItem*> duplicates;
    for (PolygonItem *poly : polygonsInScene) {
        if (!poly)
            continue;
        const QString key = canonicalKeyForPolygon(poly->vertexIds());
        if (seenPolygons.contains(key))
            duplicates.append(poly);
        else
            seenPolygons.insert(key, poly);
    }

    for (PolygonItem *dup : duplicates) {
        if (dup && dup->scene())
            dup->scene()->removeItem(dup);
        delete dup;
    }

    updatePolygonCountLabel();

    //QMessageBox::information(this, "Polygon Detection",QString("Detected %1 polygons, added %2 new polygons, removed %3 duplicates.").arg(detectedPolygons.size()).arg(addedCount).arg(duplicates.size()));
}

void MainWindow::applyVertexMovableSetting()
{
    VertexItem::setMovableForScene(ui->graphicsView->scene(), m_allowVertexManualMove);
}

void MainWindow::onEditSetting()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Edit Settings");

    auto *moveVertexCheck = new QCheckBox("Allow moving vertices manually", &dialog);
    moveVertexCheck->setChecked(m_allowVertexManualMove);

    auto *createVertexCheck = new QCheckBox("Allow manual vertex creation", &dialog);
    createVertexCheck->setChecked(m_allowManualVertexCreation);

    auto *createLineCheck = new QCheckBox("Allow manual line creation", &dialog);
    createLineCheck->setChecked(m_allowManualLineCreation);

    auto *createPolygonCheck = new QCheckBox("Allow manual polygon creation", &dialog);
    createPolygonCheck->setChecked(m_allowManualPolygonCreation);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(moveVertexCheck);
    layout->addWidget(createVertexCheck);
    layout->addWidget(createLineCheck);
    layout->addWidget(createPolygonCheck);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_allowVertexManualMove = moveVertexCheck->isChecked();
    m_allowManualVertexCreation = createVertexCheck->isChecked();
    m_allowManualLineCreation = createLineCheck->isChecked();
    m_allowManualPolygonCreation = createPolygonCheck->isChecked();

    applyVertexMovableSetting();
}


void MainWindow::onGeometryCalculationSetting()
{
    NeighborPairGeometryCalculator::showSettingsDialog(this);
}

void MainWindow::onNeighborPairGeometryCalculation()
{
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return;
    }

    QVector<PolygonItem*> polygons;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type)
            polygons.append(static_cast<PolygonItem*>(item));
    }

    if (polygons.size() < 2) {
        QMessageBox::information(this, "Neighbor Pair Geometry", "Please ensure at least two polygons are available.");
        return;
    }

    const NeighborPairGeometrySettings settings = NeighborPairGeometryCalculator::currentSettings();
    const bool anyEnabled = anyGeometryCalculationEnabled(settings);

    if (!anyEnabled) {
        QMessageBox::information(this, "Neighbor Pair Geometry", "All geometry calculations are currently disabled. Enable them via Geometry Calculation Setting.");
        return;
    }

    resetStoredGeometryResults();
    int pairIndexForExport = 0;
    QString geometryFileName = ui->label_input_file_name_value->text();
    if (geometryFileName.isEmpty() || geometryFileName == "-")
        geometryFileName = QStringLiteral("current_scene");
    else
        geometryFileName = QFileInfo(geometryFileName).completeBaseName();

    QVector<NeighborPairGeometryCalculator::Result> results;
    int neighborCount = 0;

    for (int i = 0; i < polygons.size(); ++i) {
        for (int j = i + 1; j < polygons.size(); ++j) {
            NeighborPair pair(polygons[i], polygons[j]);
            if (!pair.isNeighbor())
                continue;

            ++neighborCount;
            const NeighborPairGeometryCalculator::Result result =
                    NeighborPairGeometryCalculator::calculateForPair(polygons[i], polygons[j], settings);
            results.append(result);

            NeighborPairGeometryCalculator::Result storedResult = result;
            storedResult.first = nullptr;
            storedResult.second = nullptr;

            BatchDivisionEstimator::GeometryEntry entry;
            entry.fileName = geometryFileName;
            entry.pairIndex = ++pairIndexForExport;
            entry.firstId = polygons[i]->id();
            entry.secondId = polygons[j]->id();
            entry.geometry = storedResult;
            m_lastGeometryEntries.append(entry);
        }
    }

    if (neighborCount == 0) {
        QMessageBox::information(this, "Neighbor Pair Geometry", "No neighboring polygons were found.");
        return;
    }

    if (!m_lastGeometryEntries.isEmpty()) {
        m_lastGeometrySettings = settings;
        m_hasGeometryCalculationResults = true;
    }

    NeighborPairGeometryCalculator::showResultsDialog(results, settings, this);
}

void MainWindow::onBatchNeighborPairGeometryCalculation()
{
    const NeighborPairGeometrySettings settings = NeighborPairGeometryCalculator::currentSettings();
    if (!anyGeometryCalculationEnabled(settings)) {
        QMessageBox::information(this, "Batch Neighbor Pair Geometry", "All geometry calculations are currently disabled. Enable them via Geometry Calculation Setting.");
        return;
    }

    const QString directory = QFileDialog::getExistingDirectory(this, "Select Directory with Geometry JSON Files");
    if (directory.isEmpty())
        return;

    const BatchDivisionEstimator::GeometrySummary summary = BatchDivisionEstimator::processNeighborGeometryDirectory(directory, settings);

    if (summary.entries.isEmpty()) {
        QString message = QString("Processed %1 file(s) but no neighbor pair geometries were produced.").arg(summary.filesProcessed);
        if (!summary.errors.isEmpty()) {
            message += "\n\nErrors:\n- " + summary.errors.join("\n- ");
        }
        if (!summary.warnings.isEmpty()) {
            message += "\n\nWarnings:\n- " + summary.warnings.join("\n- ");
        }
        QMessageBox::information(this, "Batch Neighbor Pair Geometry", message);
        return;
    }

    const QString defaultPath = QDir(directory).filePath("batch_neighbor_pair_geometry.csv");
    const QString savePath = QFileDialog::getSaveFileName(this,
                                                          "Export Neighbor Pair Geometry",
                                                          defaultPath,
                                                          "CSV Files (*.csv)");
    if (savePath.isEmpty())
        return;

    QString errorMessage;
    if (!BatchDivisionEstimator::exportNeighborGeometryToCsv(savePath, summary.entries, settings, &errorMessage)) {
        QMessageBox::critical(this, "Batch Neighbor Pair Geometry", errorMessage.isEmpty() ? QString("Failed to export CSV to %1").arg(savePath) : errorMessage);
        return;
    }

    QString message = QString("Processed %1 file(s) and exported %2 neighbor pairs to:\n%3")
            .arg(summary.filesProcessed)
            .arg(summary.entries.size())
            .arg(savePath);

    if (!summary.errors.isEmpty())
        message += "\n\nErrors:\n- " + summary.errors.join("\n- ");
    if (!summary.warnings.isEmpty())
        message += "\n\nWarnings:\n- " + summary.warnings.join("\n- ");

    QMessageBox::information(this, "Batch Neighbor Pair Geometry", message);
}

void MainWindow::onBatchSingleCellGeometryCalculation()
{
    const QString directory = QFileDialog::getExistingDirectory(this, "Select Directory with Geometry JSON Files");
    if (directory.isEmpty())
        return;

    const BatchDivisionEstimator::SingleCellGeometrySummary summary =
            BatchDivisionEstimator::processSingleCellGeometryDirectory(directory);

    if (summary.entries.isEmpty()) {
        QString message = QString("Processed %1 file(s) but no single-cell geometries were produced.").arg(summary.filesProcessed);
        if (!summary.errors.isEmpty()) {
            message += "\n\nErrors:\n- " + summary.errors.join("\n- ");
        }
        if (!summary.warnings.isEmpty()) {
            message += "\n\nWarnings:\n- " + summary.warnings.join("\n- ");
        }
        QMessageBox::information(this, "Batch Single Cell Geometry", message);
        return;
    }

    const QString defaultPath = QDir(directory).filePath("batch_single_cell_geometry.csv");
    const QString savePath = QFileDialog::getSaveFileName(this,
                                                          "Export Single Cell Geometry",
                                                          defaultPath,
                                                          "CSV Files (*.csv)");
    if (savePath.isEmpty())
        return;

    QString errorMessage;
    if (!BatchDivisionEstimator::exportSingleCellGeometryToCsv(savePath, summary.entries, &errorMessage)) {
        QMessageBox::critical(this, "Batch Single Cell Geometry", errorMessage.isEmpty() ? QString("Failed to export CSV to %1").arg(savePath) : errorMessage);
        return;
    }

    QString message = QString("Processed %1 file(s) and exported %2 cells to:\n%3")
            .arg(summary.filesProcessed)
            .arg(summary.entries.size())
            .arg(savePath);

    if (!summary.errors.isEmpty())
        message += "\n\nErrors:\n- " + summary.errors.join("\n- ");
    if (!summary.warnings.isEmpty())
        message += "\n\nWarnings:\n- " + summary.warnings.join("\n- ");

    QMessageBox::information(this, "Batch Single Cell Geometry", message);
}

void MainWindow::clearDivisionArrowVector(QVector<QGraphicsPathItem*> &arrows)
{
    QGraphicsScene *scene = ui->graphicsView->scene();
    for (QGraphicsPathItem *arrow : arrows) {
        if (scene && arrow)
            scene->removeItem(arrow);
        delete arrow;
    }
    arrows.clear();
}

void MainWindow::clearDivisionComparisonArrows()
{
    clearDivisionArrowVector(m_truePositiveDivisionArrows);
    clearDivisionArrowVector(m_falsePositiveDivisionArrows);
    clearDivisionArrowVector(m_falseNegativeDivisionArrows);
    m_truePositivePairs.clear();
    m_falsePositivePairs.clear();
    m_falseNegativePairs.clear();
}

void MainWindow::clearEstimatedDivisionArrows()
{
    clearDivisionArrowVector(m_estimatedDivisionArrows);
    m_estimatedDivisionPairs.clear();
    resetEstimationRule();
    clearDivisionComparisonArrows();
}

void MainWindow::clearRealDivisionArrows()
{
    clearDivisionArrowVector(m_realDivisionArrows);
    m_realDivisionPairs.clear();
    clearDivisionComparisonArrows();
}

void MainWindow::resetEstimatedDivisionNumberLabel()
{
    ui->label_estimated_division_number_value->setText("-");
}

void MainWindow::setEstimatedDivisionNumber(int count)
{
    ui->label_estimated_division_number_value->setText(QString::number(count));
}

void MainWindow::resetVertexNumberLabel()
{
    setVertexNumber(0);
}

void MainWindow::setVertexNumber(int count)
{
    ui->label_vertex_number_value->setText(QString::number(count));
}

void MainWindow::resetLineNumberLabel()
{
    setLineNumber(0);
}

void MainWindow::setLineNumber(int count)
{
    ui->label_line_number_value->setText(QString::number(count));
}

void MainWindow::resetPolygonNumberLabel()
{
    setPolygonNumber(0);
}

void MainWindow::resetDivisionComparisonLabels()
{
    ui->label_real_division_number_value->setText("-");
    ui->label_true_positive_number_value->setText("-");
    ui->label_false_positive_number_value->setText("-");
    ui->label_false_negative_number_value->setText("-");
    ui->label_true_negative_number_value->setText("-");
    ui->label_precision_value->setText("-");
    ui->label_recall_value->setText("-");
    ui->label_F1_score_value->setText("-");
    ui->label_specificity_value->setText("-");
    ui->label_accuracy_value->setText("-");
    resetPerformanceMetrics();
}

void MainWindow::resetStoredGeometryResults()
{
    m_lastGeometryEntries.clear();
    m_lastGeometrySettings = NeighborPairGeometrySettings();
    m_hasGeometryCalculationResults = false;
}

void MainWindow::resetPerformanceMetrics()
{
    m_lastPerformanceMetrics = BatchDivisionEstimator::DivisionMetrics();
    m_hasPerformanceMetrics = false;
}

void MainWindow::resetEstimationRule()
{
    m_lastEstimationCriterion = DivisionEstimator::Criterion();
    m_hasEstimationCriterion = false;
}

void MainWindow::applyDivisionMetricsToLabels(const BatchDivisionEstimator::DivisionMetrics &metrics)
{
    m_lastPerformanceMetrics = metrics;
    m_hasPerformanceMetrics = true;

    const auto formatMetric = [](double value) {
        if (value < 0.0 || std::isnan(value))
            return QString("-");
        return QString::number(value, 'f', 3);
    };

    const auto formatCount = [](int value) {
        return (value < 0) ? QStringLiteral("-") : QString::number(value);
    };

    ui->label_real_division_number_value->setText(formatCount(metrics.realPairs));
    ui->label_estimated_division_number_value->setText(formatCount(metrics.estimatedPairs));
    ui->label_true_positive_number_value->setText(QString::number(metrics.truePositives));
    ui->label_false_positive_number_value->setText(QString::number(metrics.falsePositives));
    ui->label_false_negative_number_value->setText(QString::number(metrics.falseNegatives));
    ui->label_true_negative_number_value->setText(QString::number(metrics.trueNegatives));
    ui->label_precision_value->setText(formatMetric(metrics.precision));
    ui->label_recall_value->setText(formatMetric(metrics.recall));
    ui->label_F1_score_value->setText(formatMetric(metrics.f1));
    ui->label_specificity_value->setText(formatMetric(metrics.specificity));
    ui->label_accuracy_value->setText(formatMetric(metrics.accuracy));
}

void MainWindow::setPolygonNumber(int count)
{
    ui->label_polygon_number_value->setText(QString::number(count));
}

void MainWindow::onEstimateDivisionBySingleGeometry(){
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return;
    }

    QVector<PolygonItem*> polygons;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type)
            polygons.append(static_cast<PolygonItem*>(item));
    }

    if (polygons.size() < 2) {
        QMessageBox::information(this, "Estimate Division", "Please ensure at least two polygons are available.");
        return;
    }

    DivisionEstimator::Criterion criterion;
    if (!DivisionEstimator::showSingleFeatureDialog(this, criterion))
        return;

    clearEstimatedDivisionArrows();
    resetEstimatedDivisionNumberLabel();
    resetDivisionComparisonLabels();
    m_estimatedDivisionPairs.clear();

    m_lastEstimationCriterion = criterion;
    m_hasEstimationCriterion = true;

    const DivisionEstimator::EstimationResult estimation = DivisionEstimator::estimatePairs(polygons, criterion, scene);

    if (estimation.examinedPairs == 0) {
        setEstimatedDivisionNumber(0);
        QMessageBox::information(this, "Estimate Division", "No neighboring polygons were found.");
        return;
    }

    setEstimatedDivisionNumber(estimation.matchedPairs);

    if (estimation.matchedPairs == 0) {
        QMessageBox::information(this, "Estimate Division", "No neighbor pairs matched the selected geometry threshold.");
        return;
    }

    m_estimatedDivisionArrows = estimation.arrows;
    m_estimatedDivisionPairs = estimation.matchedPairIds;

    QMessageBox::information(this,
                             "Estimate Division",
                             QString("%1 of %2 neighbor pairs matched %3 %4 %5.")
                                 .arg(estimation.matchedPairs)
                                 .arg(estimation.examinedPairs)
                                 .arg(criterion.featureLabel)
                                  .arg(criterion.requireAbove ? ">=" : "<=")
                                  .arg(criterion.threshold));
}

void MainWindow::onBatchEstimateDivisionBySingleGeometry()
{
    QString defaultDir = ui->label_input_directory_value->text();
    if (defaultDir.isEmpty() || defaultDir == "-")
        defaultDir = QDir::homePath();

    const QString directory = QFileDialog::getExistingDirectory(this,
                                                                "Select Directory with Geometry JSON Files",
                                                                defaultDir);
    if (directory.isEmpty())
        return;

    DivisionEstimator::Criterion criterion;
    if (!DivisionEstimator::showSingleFeatureDialog(this, criterion))
        return;

    const BatchDivisionEstimator::BatchResult summary = BatchDivisionEstimator::estimateDirectory(directory, criterion);

    if (summary.filesProcessed == 0 && summary.files.isEmpty()) {
        resetDivisionComparisonLabels();
        QMessageBox::information(this, "Batch Estimate Division", "No geometry files were processed.");
        return;
    }

    applyDivisionMetricsToLabels(summary.totals);

    const auto formatMetric = [](double value) {
        if (value < 0.0 || std::isnan(value))
            return QStringLiteral("-");
        return QString::number(value, 'f', 3);
    };

    QString message = QString("Processed %1 file(s).").arg(summary.filesProcessed);
    message += QString("\nFiles with results: %1").arg(summary.filesWithResults);
    message += QString("\nEstimated pairs: %1").arg(summary.totals.estimatedPairs < 0 ? 0 : summary.totals.estimatedPairs);
    message += QString("\nReal pairs: %1").arg(summary.totals.realPairs < 0 ? 0 : summary.totals.realPairs);
    message += QString("\nTrue Positive: %1").arg(summary.totals.truePositives);
    message += QString("\nFalse Positive: %1").arg(summary.totals.falsePositives);
    message += QString("\nTrue Negative: %1").arg(summary.totals.trueNegatives);
    message += QString("\nFalse Negative: %1").arg(summary.totals.falseNegatives);
    message += QString("\nPrecision: %1").arg(formatMetric(summary.totals.precision));
    message += QString("\nRecall: %1").arg(formatMetric(summary.totals.recall));
    message += QString("\nF1: %1").arg(formatMetric(summary.totals.f1));
    message += QString("\nSpecificity: %1").arg(formatMetric(summary.totals.specificity));
    message += QString("\nAccuracy: %1").arg(formatMetric(summary.totals.accuracy));

    if (!summary.errors.isEmpty())
        message += "\n\nErrors:\n- " + summary.errors.join("\n- ");
    if (!summary.warnings.isEmpty())
        message += "\n\nWarnings:\n- " + summary.warnings.join("\n- ");

    QString exportMessage;
    if (!summary.pairRows.isEmpty()) {
        const QString defaultPath = QDir(directory).filePath("batch_single_geometry_estimation.csv");
        const QString savePath = QFileDialog::getSaveFileName(this,
                                                              "Export Batch Division Estimation",
                                                              defaultPath,
                                                              "CSV Files (*.csv)");
        if (!savePath.isEmpty()) {
            QString errorMessage;
            if (!BatchDivisionEstimator::exportDivisionEstimatesToCsv(savePath, summary.pairRows, &errorMessage)) {
                QMessageBox::critical(this, "Batch Estimate Division", errorMessage.isEmpty() ? QString("Failed to export CSV to %1").arg(savePath) : errorMessage);
                return;
            }
            exportMessage = QString("\n\nExported %1 neighbor pairs to:\n%2").arg(summary.pairRows.size()).arg(savePath);
        }
    }

    QMessageBox::information(this, "Batch Estimate Division", message + exportMessage);
}

void MainWindow::onBatchEstimateDivisionByDesignatedGeometry()
{
    QString defaultDir = ui->label_input_directory_value->text();
    if (defaultDir.isEmpty() || defaultDir == "-")
        defaultDir = QDir::homePath();

    const QString directory = QFileDialog::getExistingDirectory(this,
                                                                "Select Directory with Geometry JSON Files",
                                                                defaultDir);
    if (directory.isEmpty())
        return;

    const QString classifierPath = QFileDialog::getOpenFileName(this,
                                                                "Select CSV with Designated Geometry Criteria",
                                                                directory,
                                                                "CSV Files (*.csv)");
    if (classifierPath.isEmpty())
        return;

    QStringList parseWarnings;
    QStringList parseErrors;
    const QVector<BatchDivisionEstimator::DesignatedGeometryConfig> configs =
            BatchDivisionEstimator::loadDesignatedGeometryConfigCsv(classifierPath, &parseWarnings, &parseErrors);

    if (!parseErrors.isEmpty()) {
        QMessageBox::critical(this,
                              "Batch Estimate Division (Designated)",
                              QString("Unable to load classifier CSV:\n- %1").arg(parseErrors.join("\n- ")));
        return;
    }

    if (configs.isEmpty()) {
        QString message = "No valid classifier rows were found.";
        if (!parseWarnings.isEmpty())
            message += "\n\nWarnings:\n- " + parseWarnings.join("\n- ");
        QMessageBox::information(this, "Batch Estimate Division (Designated)", message);
        return;
    }

    const QVector<BatchDivisionEstimator::DesignatedGeometryResult> results =
            BatchDivisionEstimator::estimateDesignatedGeometry(directory, configs);

    if (results.isEmpty()) {
        QMessageBox::information(this, "Batch Estimate Division (Designated)", "No estimations were performed.");
        return;
    }

    applyDivisionMetricsToLabels(results.constLast().metrics);

    const auto formatMetric = [](double value) {
        if (value < 0.0 || std::isnan(value))
            return QStringLiteral("-");
        return QString::number(value, 'f', 3);
    };

    QString message = QString("Processed %1 configuration(s) from %2.\nDirectory: %3")
            .arg(results.size())
            .arg(QFileInfo(classifierPath).fileName())
            .arg(directory);

    QStringList allWarnings = parseWarnings;
    QStringList allErrors;

    for (const auto &res : results) {
        message += QString("\n\nFeature: %1 (%2)").arg(res.featureLabel.isEmpty() ? res.config.featureName : res.featureLabel,
                                                      res.featureKey.isEmpty() ? "-" : res.featureKey);
        message += QString("\nThreshold: %1 (%2)")
                .arg(QString::number(res.config.cutoffValue, 'f', 4))
                .arg(res.config.requireAbove ? ">= cutoff" : "<= cutoff");
        message += QString("\nFiles: %1 processed, %2 with results")
                .arg(res.filesProcessed)
                .arg(res.filesWithResults);
        message += QString("\nEstimated pairs: %1").arg(res.metrics.estimatedPairs < 0 ? 0 : res.metrics.estimatedPairs);
        message += QString("\nReal pairs: %1").arg(res.metrics.realPairs < 0 ? 0 : res.metrics.realPairs);
        message += QString("\nTP: %1  FP: %2  FN: %3  TN: %4")
                .arg(res.metrics.truePositives)
                .arg(res.metrics.falsePositives)
                .arg(res.metrics.falseNegatives)
                .arg(res.metrics.trueNegatives);
        message += QString("\nPrecision: %1  Recall: %2  F1: %3  Specificity: %4  Accuracy: %5")
                .arg(formatMetric(res.metrics.precision))
                .arg(formatMetric(res.metrics.recall))
                .arg(formatMetric(res.metrics.f1))
                .arg(formatMetric(res.metrics.specificity))
                .arg(formatMetric(res.metrics.accuracy));

        if (!res.config.direction.isEmpty())
            message += QString("\nDirection: %1").arg(res.config.direction);
        if (!res.config.note.isEmpty())
            message += QString("\nNote: %1").arg(res.config.note);

        if (!res.warnings.isEmpty()) {
            allWarnings.append(res.warnings);
        }
        if (!res.errors.isEmpty()) {
            allErrors.append(res.errors);
        }
    }

    QString exportMessage;
    const QString defaultSavePath = QDir(directory).filePath("designated_geometry_performance.csv");
    const QString savePath = QFileDialog::getSaveFileName(this,
                                                          "Export Designated Geometry Performance",
                                                          defaultSavePath,
                                                          "CSV Files (*.csv)");
    if (!savePath.isEmpty()) {
        QString errorMessage;
        if (!BatchDivisionEstimator::exportDesignatedGeometryMetricsToCsv(savePath, results, &errorMessage)) {
            QMessageBox::critical(this,
                                  "Batch Estimate Division (Designated)",
                                  errorMessage.isEmpty() ? QString("Failed to export CSV to %1").arg(savePath) : errorMessage);
            return;
        }
        exportMessage = QString("\n\nExported performance matrix to:\n%1").arg(savePath);
    }

    if (!allErrors.isEmpty())
        message += "\n\nErrors:\n- " + allErrors.join("\n- ");
    if (!allWarnings.isEmpty())
        message += "\n\nWarnings:\n- " + allWarnings.join("\n- ");

    QMessageBox::information(this, "Batch Estimate Division (Designated)", message + exportMessage);
}

void MainWindow::onPrecisionAndRecallCurveOverSingleGeometry()
{
    QString defaultDir = ui->label_input_directory_value->text();
    if (defaultDir.isEmpty() || defaultDir == "-")
        defaultDir = QDir::homePath();

    DivisionEstimator::showPrecisionRecallSweepDialog(this, defaultDir);
}

void MainWindow::onCompareWithRealDivision(){
    auto *scene = ui->graphicsView->scene();
    if (!scene) {
        QMessageBox::warning(this, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return;
    }

    if (m_estimatedDivisionPairs.isEmpty()) {
        QMessageBox::information(this,
                                 "Compare With Real Division",
                                 "No estimated division pairs available. Please run an estimation first.");
        return;
    }

    QString defaultDir = ui->label_input_directory_value->text();
    if (defaultDir.isEmpty() || defaultDir == "-")
        defaultDir = QDir::homePath();

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          "Select Real Division File",
                                                          defaultDir,
                                                          "Text Files (*.txt *.csv);;All Files (*.*)");
    if (filePath.isEmpty())
        return;

    clearRealDivisionArrows();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Compare With Real Division", QString("Failed to open file: %1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    QVector<RealDivisionRecord> parsedRecords;
    QStringList parseWarnings;
    int lineNumber = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        ++lineNumber;

        if (line.isEmpty())
            continue;

        if (lineNumber == 1 && !line.at(0).isDigit()) {
            // Treat non-numeric first line as header
            continue;
        }

        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            parseWarnings.append(QString("Line %1: Expected at least two integers.").arg(lineNumber));
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

        if (!okFirst || !okSecond || !okTime) {
            parseWarnings.append(QString("Line %1: Unable to parse integers.").arg(lineNumber));
            continue;
        }

        parsedRecords.append({firstId, secondId, time});
    }

    file.close();

    if (parsedRecords.isEmpty()) {
        QMessageBox::information(this, "Compare With Real Division", "No valid real division pairs were found in the file.");
        return;
    }

    m_realDivisionRecords = parsedRecords;

    QVector<PolygonItem*> polygons;
    for (QGraphicsItem *item : scene->items()) {
        if (item && item->type() == PolygonItem::Type)
            polygons.append(static_cast<PolygonItem*>(item));
    }
    QHash<int, PolygonItem*> polygonById;
    for (PolygonItem *poly : polygons) {
        polygonById.insert(poly->id(), poly);
    }

    if (polygons.size() < 2) {
        QMessageBox::information(this, "Compare With Real Division", "Please ensure at least two polygons are available.");
        return;
    }

    const auto pairKey = [](int a, int b) {
        const int first = std::min(a, b);
        const int second = std::max(a, b);
        return QString("%1-%2").arg(first).arg(second);
    };

    QSet<QString> neighborKeys;
    for (int i = 0; i < polygons.size(); ++i) {
        for (int j = i + 1; j < polygons.size(); ++j) {
            NeighborPair pair(polygons[i], polygons[j]);
            if (!pair.isNeighbor())
                continue;

            neighborKeys.insert(pairKey(polygons[i]->id(), polygons[j]->id()));
        }
    }

    if (neighborKeys.isEmpty()) {
        QMessageBox::information(this, "Compare With Real Division", "No neighboring polygons were found.");
        return;
    }

    QSet<QString> realKeys;
    QHash<QString, QPair<int, int>> realPairByKey;
    for (const RealDivisionRecord &record : m_realDivisionRecords) {
        const QString key = pairKey(record.firstId, record.secondId);
        if (!neighborKeys.contains(key)) {
            parseWarnings.append(QString("Pair (%1, %2) is not a neighbor in the current scene and was ignored.")
                                     .arg(record.firstId)
                                     .arg(record.secondId));
            continue;
        }
        if (realKeys.contains(key))
            continue;
        realKeys.insert(key);
        m_realDivisionPairs.append(QPair<int, int>(record.firstId, record.secondId));
        realPairByKey.insert(key, QPair<int, int>(record.firstId, record.secondId));
    }

    if (realKeys.isEmpty()) {
        QMessageBox::information(this, "Compare With Real Division", "No real division pairs matched the current scene's neighbors.");
        return;
    }

    for (const auto &pair : std::as_const(m_realDivisionPairs)) {
        PolygonItem *firstPoly = polygonById.value(pair.first, nullptr);
        PolygonItem *secondPoly = polygonById.value(pair.second, nullptr);
        if (!firstPoly || !secondPoly)
            continue;

        auto *arrow = DivisionDisplay::createArrowItem(firstPoly->centroid(),
                                                       secondPoly->centroid(),
                                                       DivisionDisplay::ArrowCategory::Real);
        if (!arrow)
            continue;

        scene->addItem(arrow);
        m_realDivisionArrows.append(arrow);
    }

    QSet<QString> estimatedKeys;
    QHash<QString, QPair<int, int>> estimatedPairByKey;
    for (const auto &pair : m_estimatedDivisionPairs) {
        const QString key = pairKey(pair.first, pair.second);
        if (!neighborKeys.contains(key)) {
            parseWarnings.append(QString("Estimated pair (%1, %2) is not a neighbor in the current scene and was ignored.")
                                     .arg(pair.first)
                                     .arg(pair.second));
            continue;
        }
        estimatedKeys.insert(key);
        estimatedPairByKey.insert(key, pair);
    }

    ui->label_estimated_division_number_value->setText(QString::number(estimatedKeys.size()));

    if (estimatedKeys.isEmpty()) {
        QMessageBox::information(this, "Compare With Real Division", "No estimated division pairs matched the current scene's neighbors.");
        return;
    }

    int truePositives = 0;
    int falsePositives = 0;
    int falseNegatives = 0;

    const auto addCategoryArrow = [&](const QPair<int, int> &pair,
                                      DivisionDisplay::ArrowCategory category,
                                      QVector<QGraphicsPathItem*> &container) {
        PolygonItem *firstPoly = polygonById.value(pair.first, nullptr);
        PolygonItem *secondPoly = polygonById.value(pair.second, nullptr);
        if (!firstPoly || !secondPoly)
            return;

        auto *arrow = DivisionDisplay::createArrowItem(firstPoly->centroid(),
                                                       secondPoly->centroid(),
                                                       category);
        if (!arrow)
            return;

        scene->addItem(arrow);
        container.append(arrow);
    };

    for (const QString &key : std::as_const(estimatedKeys)) {
        if (realKeys.contains(key)) {
            ++truePositives;
            const QPair<int, int> pair = estimatedPairByKey.value(key, realPairByKey.value(key));
            m_truePositivePairs.append(pair);
            addCategoryArrow(pair, DivisionDisplay::ArrowCategory::TruePositive, m_truePositiveDivisionArrows);
        } else {
            ++falsePositives;
            const QPair<int, int> pair = estimatedPairByKey.value(key);
            m_falsePositivePairs.append(pair);
            addCategoryArrow(pair, DivisionDisplay::ArrowCategory::FalsePositive, m_falsePositiveDivisionArrows);
        }
    }

    for (const QString &key : std::as_const(realKeys)) {
        if (estimatedKeys.contains(key))
            continue;

        ++falseNegatives;
        const QPair<int, int> pair = realPairByKey.value(key);
        m_falseNegativePairs.append(pair);
        addCategoryArrow(pair, DivisionDisplay::ArrowCategory::FalseNegative, m_falseNegativeDivisionArrows);
    }

    const BatchDivisionEstimator::DivisionMetrics metrics =
            BatchDivisionEstimator::metricsFromCounts(neighborKeys.size(),
                                                      truePositives,
                                                      falsePositives,
                                                      falseNegatives,
                                                      estimatedKeys.size(),
                                                      realKeys.size());

    applyDivisionMetricsToLabels(metrics);

    const auto formatMetric = [](double value) {
        if (value < 0.0 || std::isnan(value))
            return QString("-");
        return QString::number(value, 'f', 3);
    };

    QStringList summaryLines;
    summaryLines << QString("True Positive: %1").arg(metrics.truePositives)
                 << QString("False Positive: %1").arg(metrics.falsePositives)
                 << QString("True Negative: %1").arg(metrics.trueNegatives)
                 << QString("False Negative: %1").arg(metrics.falseNegatives)
                 << QString("Precision: %1").arg(formatMetric(metrics.precision))
                 << QString("Recall: %1").arg(formatMetric(metrics.recall))
                 << QString("F1: %1").arg(formatMetric(metrics.f1))
                 << QString("Specificity: %1").arg(formatMetric(metrics.specificity))
                 << QString("Accuracy: %1").arg(formatMetric(metrics.accuracy));

    if (!parseWarnings.isEmpty()) {
        summaryLines << "" << "Warnings:";
    for (const QString &warn : parseWarnings)
        summaryLines << QString("- %1").arg(warn);
    }

    QMessageBox::information(this, "Compare With Real Division", summaryLines.join('\n'));
}
