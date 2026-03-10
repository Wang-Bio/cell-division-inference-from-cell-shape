#include "debugmanager.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "interactivegraphicsview.h"
#include "vertexitem.h"
#include "lineitem.h"
#include "polygonitem.h"
#include "divisionestimator.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPen>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <QVBoxLayout>

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (inQuotes) {
            if (c == '"') {
                const bool hasNext = (i + 1 < line.size());
                if (hasNext && line.at(i + 1) == '"') {
                    current.append('"');
                    ++i; // skip escaped quote
                } else {
                    inQuotes = false;
                }
            } else {
                current.append(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields << current;
                current.clear();
            } else {
                current.append(c);
            }
        }
    }

    fields << current;
    return fields;
}

QString escapeCsvField(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n'))
        escaped = QStringLiteral("\"%1\"").arg(escaped);
    return escaped;
}

QString formatValue(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral("N/A");
    return QString::number(value, 'f', 6);
}

QString normalizedPairKey(const QString &fileName, int a, int b, int observedDivision)
{
    const int first = std::min(a, b);
    const int second = std::max(a, b);
    return QString("%1|%2-%3|%4").arg(fileName).arg(first).arg(second).arg(observedDivision);
}

struct BatchComparisonRow {
    QString fileName;
    int firstId = -1;
    int secondId = -1;
    int observedDivision = -1;
    int estimatedDivision1 = -1;
    int estimatedDivision2 = -1;
    double featureValue1 = std::numeric_limits<double>::quiet_NaN();
    double featureValue2 = std::numeric_limits<double>::quiet_NaN();
};

struct BatchEstimationRow {
    QString fileName;
    int firstId = -1;
    int secondId = -1;
    int observedDivision = -1;
    int estimatedDivision = -1;
    double featureValue = std::numeric_limits<double>::quiet_NaN();
};

struct BatchCsvContent {
    QStringList headers;
    QHash<QString, int> headerIndex;
};

BatchCsvContent readBatchCsvHeader(const QString &filePath, QString *errorMessage)
{
    BatchCsvContent content;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Failed to open %1: %2").arg(QFileInfo(filePath).fileName(), file.errorString());
        return content;
    }

    QTextStream in(&file);
    if (in.atEnd()) {
        if (errorMessage)
            *errorMessage = QString("File %1 is empty.").arg(QFileInfo(filePath).fileName());
        return content;
    }

    const QString headerLine = in.readLine();
    content.headers = parseCsvLine(headerLine);

    for (int i = 0; i < content.headers.size(); ++i) {
        content.headerIndex.insert(content.headers.at(i).toLower(), i);
    }

    return content;
}

bool loadBatchEstimationRows(const QString &filePath,
                             const QString &featureName,
                             QHash<QString, BatchEstimationRow> &rowsByKey,
                             QStringList &warnings,
                             QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Failed to open %1: %2").arg(QFileInfo(filePath).fileName(), file.errorString());
        return false;
    }

    QTextStream in(&file);
    if (in.atEnd()) {
        if (errorMessage)
            *errorMessage = QString("%1 is empty.").arg(QFileInfo(filePath).fileName());
        return false;
    }

    const QStringList headers = parseCsvLine(in.readLine());
    QHash<QString, int> headerIndex;
    for (int i = 0; i < headers.size(); ++i) {
        headerIndex.insert(headers.at(i).toLower(), i);
    }

    auto indexFor = [&](const QString &name) {
        return headerIndex.value(name.toLower(), -1);
    };

    const int fileIdx = indexFor("filename");
    const int c1Idx = indexFor("cell_1_id");
    const int c2Idx = indexFor("cell_2_id");
    const int observedIdx = indexFor("observed_division");
    const int estimatedIdx = indexFor("estimated_division");
    const int featureIdx = indexFor(featureName);

    if (fileIdx < 0 || c1Idx < 0 || c2Idx < 0 || observedIdx < 0 || estimatedIdx < 0) {
        if (errorMessage)
            *errorMessage = QString("%1 is missing required columns.").arg(QFileInfo(filePath).fileName());
        return false;
    }
    if (featureIdx < 0) {
        if (errorMessage)
            *errorMessage = QString("%1 does not contain the selected feature \"%2\".").arg(QFileInfo(filePath).fileName(), featureName);
        return false;
    }

    const int requiredColumns = std::max({fileIdx, c1Idx, c2Idx, observedIdx, estimatedIdx, featureIdx});

    int lineNumber = 1; // header already read
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++lineNumber;
        if (line.trimmed().isEmpty())
            continue;

        const QStringList fields = parseCsvLine(line);
        if (fields.size() <= requiredColumns) {
            warnings << QString("%1 line %2: Expected at least %3 columns, found %4.")
                        .arg(QFileInfo(filePath).fileName())
                        .arg(lineNumber)
                        .arg(requiredColumns + 1)
                        .arg(fields.size());
            continue;
        }

        bool okId1 = false;
        bool okId2 = false;
        bool okObserved = false;
        bool okEstimated = false;
        const int id1 = fields.at(c1Idx).toInt(&okId1);
        const int id2 = fields.at(c2Idx).toInt(&okId2);
        const int observed = fields.at(observedIdx).toInt(&okObserved);
        const int estimated = fields.at(estimatedIdx).toInt(&okEstimated);

        if (!okId1 || !okId2 || !okObserved || !okEstimated) {
            warnings << QString("%1 line %2: Failed to parse ids or division flags.")
                        .arg(QFileInfo(filePath).fileName())
                        .arg(lineNumber);
            continue;
        }

        bool okFeature = false;
        const double featureValue = fields.at(featureIdx).toDouble(&okFeature);
        if (!okFeature) {
            warnings << QString("%1 line %2: Unable to parse feature value for \"%3\".")
                        .arg(QFileInfo(filePath).fileName())
                        .arg(lineNumber)
                        .arg(featureName);
        }

        const QString fileName = fields.at(fileIdx);
        const QString key = normalizedPairKey(fileName, id1, id2, observed);
        if (rowsByKey.contains(key)) {
            warnings << QString("%1 line %2: Duplicate entry for %3. Keeping the first occurrence.")
                        .arg(QFileInfo(filePath).fileName())
                        .arg(lineNumber)
                        .arg(key);
            continue;
        }

        BatchEstimationRow row;
        row.fileName = fileName;
        row.firstId = std::min(id1, id2);
        row.secondId = std::max(id1, id2);
        row.observedDivision = observed;
        row.estimatedDivision = estimated;
        row.featureValue = featureValue;

        rowsByKey.insert(key, row);
    }

    return true;
}

bool exportBatchComparison(const QString &filePath,
                           const QString &featureName,
                           const QVector<BatchComparisonRow> &rows,
                           QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString("Failed to write %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << "filename,cell_1_id,cell_2_id,observed_division,estimated_division_difference," << featureName << "_difference\n";
    for (const BatchComparisonRow &row : rows) {
        const int estDiff = row.estimatedDivision1 - row.estimatedDivision2;
        const double featureDiff = row.featureValue1 - row.featureValue2;

        QStringList line = {
            escapeCsvField(row.fileName),
            QString::number(row.firstId),
            QString::number(row.secondId),
            QString::number(row.observedDivision),
            QString::number(estDiff),
            formatValue(featureDiff)
        };
        out << line.join(',') << "\n";
    }

    return true;
}
} // namespace

void DebugManager::generate(MainWindow *mainWindow)
{
    if (!mainWindow)
        return;

    QGraphicsScene *scene = mainWindow->ui->graphicsView->scene();

    QDialog dialog(mainWindow);
    dialog.setWindowTitle("Generate Random Network");

    QFormLayout *form = new QFormLayout(&dialog);

    QSpinBox *spinPolyCount = new QSpinBox(&dialog);
    spinPolyCount->setRange(2, 200);
    spinPolyCount->setValue(10);
    form->addRow("Polygon count:", spinPolyCount);

    const bool needCanvas = (!scene || scene->sceneRect().isNull() ||
                             scene->sceneRect().width() <= 0 || scene->sceneRect().height() <= 0);

    QSpinBox *spinWidth = nullptr;
    QSpinBox *spinHeight = nullptr;

    if (needCanvas) {
        spinWidth = new QSpinBox(&dialog);
        spinHeight = new QSpinBox(&dialog);

        spinWidth->setRange(100, 20000);
        spinHeight->setRange(100, 20000);
        spinWidth->setValue(800);
        spinHeight->setValue(800);

        form->addRow("Canvas width:", spinWidth);
        form->addRow("Canvas height:", spinHeight);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const int polygonCount = spinPolyCount->value();

    if (polygonCount < 2)
        return;

    if (needCanvas && spinWidth && spinHeight) {
        const int w = spinWidth->value();
        const int h = spinHeight->value();

        if (scene) {
            mainWindow->ui->graphicsView->setScene(nullptr);
            delete scene;
        }

        scene = new QGraphicsScene(mainWindow->ui->graphicsView);
        scene->setSceneRect(0, 0, w, h);
        scene->setBackgroundBrush(Qt::black);
        scene->addRect(0, 0, w, h, QPen(Qt::black));

        mainWindow->ui->graphicsView->setScene(scene);
        mainWindow->ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

        mainWindow->m_currentCanvasWidth = w;
        mainWindow->m_currentCanvasHeight = h;
        mainWindow->m_currentImage.release();
        mainWindow->updateCanvasSizeLabel(static_cast<InteractiveGraphicsView*>(mainWindow->ui->graphicsView)->currentZoomPercent());
    }

    if (!scene)
        return;

    scene->clear();
    scene->addRect(scene->sceneRect(), QPen(Qt::black));
    mainWindow->m_nextVertexId = 1;

    const QRectF bounds = scene->sceneRect();
    QRandomGenerator *rng = QRandomGenerator::global();

    QVector<QPointF> sites;
    sites.reserve(polygonCount);
    for (int i = 0; i < polygonCount; ++i) {
        const double x = bounds.left() + rng->bounded(bounds.width());
        const double y = bounds.top() + rng->bounded(bounds.height());
        sites.push_back(QPointF(x, y));
    }

    auto clipWithHalfPlane = [](const QPolygonF &poly, const QPointF &pointOnLine, const QPointF &normal) {
        QPolygonF result;
        if (poly.isEmpty()) return result;

        auto isInside = [&](const QPointF &p) {
            return QPointF::dotProduct(p - pointOnLine, normal) >= -1e-6;
        };

        for (int i = 0; i < poly.size(); ++i) {
            QPointF current = poly[i];
            QPointF next = poly[(i + 1) % poly.size()];

            const bool currentInside = isInside(current);
            const bool nextInside = isInside(next);

            if (currentInside && nextInside) {
                result << next;
            } else if (currentInside && !nextInside) {
                QPointF dir = next - current;
                const double denom = QPointF::dotProduct(dir, normal);
                if (std::abs(denom) > 1e-12) {
                    const double t = QPointF::dotProduct(pointOnLine - current, normal) / denom;
                    if (t >= 0.0 && t <= 1.0) {
                        result << (current + t * dir);
                    }
                }
            } else if (!currentInside && nextInside) {
                QPointF dir = next - current;
                const double denom = QPointF::dotProduct(dir, normal);
                if (std::abs(denom) > 1e-12) {
                    const double t = QPointF::dotProduct(pointOnLine - current, normal) / denom;
                    if (t >= 0.0 && t <= 1.0) {
                        result << (current + t * dir);
                    }
                }
                result << next;
            }
        }

        return result;
    };

    QVector<QPolygonF> cells;
    cells.reserve(sites.size());

    for (int i = 0; i < sites.size(); ++i) {
        QPolygonF cell;
        cell << bounds.topLeft() << bounds.topRight() << bounds.bottomRight() << bounds.bottomLeft();

        for (int j = 0; j < sites.size(); ++j) {
            if (i == j) continue;

            QPointF mid = 0.5 * (sites[i] + sites[j]);
            QPointF normal = sites[i] - sites[j]; // points toward site i
            cell = clipWithHalfPlane(cell, mid, normal);

            if (cell.size() < 3)
                break;
        }

        cells.push_back(cell);
    }

    auto snapPoint = [](const QPointF &p) {
        return QPointF(std::round(p.x() * 1000.0) / 1000.0,
                       std::round(p.y() * 1000.0) / 1000.0);
    };

    QHash<QString, VertexItem*> vertexLookup;
    auto vertexKey = [](const QPointF &p) {
        return QString("%1_%2").arg(p.x(), 0, 'f', 3).arg(p.y(), 0, 'f', 3);
    };

    auto getOrCreateVertex = [&](const QPointF &p) {
        QPointF snapped = snapPoint(p);
        QString key = vertexKey(snapped);
        if (vertexLookup.contains(key)) {
            return vertexLookup.value(key);
        }

        auto *v = new VertexItem(mainWindow->m_nextVertexId++, snapped);
        vertexLookup.insert(key, v);
        return v;
    };

    QVector<PolygonItem*> polygons;
    polygons.reserve(cells.size());

    for (const QPolygonF &cell : std::as_const(cells)) {
        QVector<VertexItem*> verts;
        verts.reserve(cell.size());

        for (const QPointF &p : cell) {
            verts.append(getOrCreateVertex(p));
        }

        auto *poly = new PolygonItem(verts, scene, true);
        polygons.append(poly);
    }

    for (VertexItem *v : vertexLookup) {
        scene->addItem(v);
    }

    for (PolygonItem *poly : polygons) {
        scene->addItem(poly);
    }

    mainWindow->connectAllLinesInScene();
    mainWindow->connectAllPolygonsInScene();
    mainWindow->updateVertexCountLabel();
    mainWindow->updateLineCountLabel();
    mainWindow->updatePolygonCountLabel();

    mainWindow->ui->graphicsView->scene()->clearSelection();
}

void DebugManager::compareBatchEstimations(MainWindow *mainWindow)
{
    if (!mainWindow)
        return;

    QDialog dialog(mainWindow);
    dialog.setWindowTitle("Comparing Batch Estimation");

    auto *form = new QFormLayout(&dialog);

    auto createFileSelector = [&](const QString &labelText) {
        auto *container = new QWidget(&dialog);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);

        auto *edit = new QLineEdit(container);
        auto *button = new QPushButton("Browse", container);

        layout->addWidget(edit, 1);
        layout->addWidget(button);
        form->addRow(labelText, container);

        return QPair<QLineEdit*, QPushButton*>(edit, button);
    };

    auto file1Widgets = createFileSelector("Batch CSV #1:");
    auto file2Widgets = createFileSelector("Batch CSV #2:");

    auto *featureCombo = new QComboBox(&dialog);
    featureCombo->setEditable(false);
    featureCombo->setPlaceholderText("Select a feature");
    form->addRow("Geometric Feature:", featureCombo);

    auto *featureStatus = new QLabel(&dialog);
    featureStatus->setWordWrap(true);
    form->addRow("", featureStatus);

    auto updateFeatureOptions = [&]() {
        featureCombo->clear();
        featureStatus->clear();

        const QString path1 = file1Widgets.first->text().trimmed();
        const QString path2 = file2Widgets.first->text().trimmed();
        if (path1.isEmpty() || path2.isEmpty())
            return;

        QString error1;
        QString error2;
        BatchCsvContent header1 = readBatchCsvHeader(path1, &error1);
        BatchCsvContent header2 = readBatchCsvHeader(path2, &error2);

        if (!error1.isEmpty()) {
            featureStatus->setText(error1);
            return;
        }
        if (!error2.isEmpty()) {
            featureStatus->setText(error2);
            return;
        }

        const QSet<QString> reserved = {
            "filename",
            "cell_1_id",
            "cell_2_id",
            "division_timing",
            "observed_division",
            "estimated_division"
        };

        QStringList featureCandidates1;
        for (const QString &header : header1.headers) {
            if (!reserved.contains(header.toLower()))
                featureCandidates1 << header;
        }

        QSet<QString> features2Lower;
        for (const QString &header : header2.headers) {
            if (!reserved.contains(header.toLower()))
                features2Lower.insert(header.toLower());
        }

        QStringList intersection;
        for (const QString &feature : featureCandidates1) {
            if (features2Lower.contains(feature.toLower()))
                intersection << feature;
        }

        if (intersection.isEmpty()) {
            featureStatus->setText("No common geometric features found between the selected CSV files.");
            return;
        }

        intersection.sort(Qt::CaseInsensitive);
        featureCombo->addItems(intersection);
    };

    auto browseForFile = [&](QLineEdit *targetEdit) {
        const QString path = QFileDialog::getOpenFileName(&dialog,
                                                          "Select Batch Estimation CSV",
                                                          mainWindow->ui->label_input_directory_value->text(),
                                                          "CSV Files (*.csv);;All Files (*.*)");
        if (!path.isEmpty()) {
            targetEdit->setText(path);
            updateFeatureOptions();
        }
    };

    QObject::connect(file1Widgets.first, &QLineEdit::textChanged, &dialog, updateFeatureOptions);
    QObject::connect(file2Widgets.first, &QLineEdit::textChanged, &dialog, updateFeatureOptions);
    QObject::connect(file1Widgets.second, &QPushButton::clicked, &dialog, [=]() { browseForFile(file1Widgets.first); });
    QObject::connect(file2Widgets.second, &QPushButton::clicked, &dialog, [=]() { browseForFile(file2Widgets.first); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Compare && Export");
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString filePath1 = file1Widgets.first->text().trimmed();
    const QString filePath2 = file2Widgets.first->text().trimmed();
    const QString featureName = featureCombo->currentText().trimmed();

    if (filePath1.isEmpty() || filePath2.isEmpty()) {
        QMessageBox::warning(mainWindow, "Comparing Batch Estimation", "Please select both CSV files.");
        return;
    }
    if (featureName.isEmpty()) {
        QMessageBox::warning(mainWindow, "Comparing Batch Estimation", "Please select a geometric feature.");
        return;
    }

    QStringList warnings;
    QHash<QString, BatchEstimationRow> rowsFile1;
    QHash<QString, BatchEstimationRow> rowsFile2;
    QString errorMessage;

    if (!loadBatchEstimationRows(filePath1, featureName, rowsFile1, warnings, &errorMessage)) {
        QMessageBox::critical(mainWindow, "Comparing Batch Estimation", errorMessage.isEmpty() ? "Failed to read the first CSV file." : errorMessage);
        return;
    }
    if (!loadBatchEstimationRows(filePath2, featureName, rowsFile2, warnings, &errorMessage)) {
        QMessageBox::critical(mainWindow, "Comparing Batch Estimation", errorMessage.isEmpty() ? "Failed to read the second CSV file." : errorMessage);
        return;
    }

    QVector<BatchComparisonRow> differences;
    for (auto it = rowsFile1.constBegin(); it != rowsFile1.constEnd(); ++it) {
        const QString &key = it.key();
        if (!rowsFile2.contains(key))
            continue;

        const BatchEstimationRow &row1 = it.value();
        const BatchEstimationRow &row2 = rowsFile2.value(key);
        if (row1.estimatedDivision == row2.estimatedDivision)
            continue;

        BatchComparisonRow diffRow;
        diffRow.fileName = row1.fileName;
        diffRow.firstId = row1.firstId;
        diffRow.secondId = row1.secondId;
        diffRow.observedDivision = row1.observedDivision;
        diffRow.estimatedDivision1 = row1.estimatedDivision;
        diffRow.estimatedDivision2 = row2.estimatedDivision;
        diffRow.featureValue1 = row1.featureValue;
        diffRow.featureValue2 = row2.featureValue;

        differences.append(diffRow);
    }

    if (differences.isEmpty()) {
        QMessageBox::information(mainWindow, "Comparing Batch Estimation", "No rows with differing estimated divisions were found.");
        return;
    }

    QString defaultDir = QFileInfo(filePath1).absolutePath();
    if (defaultDir.isEmpty())
        defaultDir = mainWindow->ui->label_input_directory_value->text();

    const QString savePath = QFileDialog::getSaveFileName(mainWindow,
                                                          "Save Comparison CSV",
                                                          QDir(defaultDir).filePath("batch_estimation_comparison.csv"),
                                                          "CSV Files (*.csv)");
    if (savePath.isEmpty())
        return;

    if (!exportBatchComparison(savePath, featureName, differences, &errorMessage)) {
        QMessageBox::critical(mainWindow, "Comparing Batch Estimation", errorMessage.isEmpty() ? "Failed to export comparison CSV." : errorMessage);
        return;
    }

    QString message = QString("Exported %1 differing rows to:\n%2").arg(differences.size()).arg(savePath);
    if (!warnings.isEmpty()) {
        message += "\n\nWarnings:\n- " + warnings.join("\n- ");
    }

    QMessageBox::information(mainWindow, "Comparing Batch Estimation", message);
}

void DebugManager::exportFeatureNames(MainWindow *mainWindow)
{
    if (!mainWindow)
        return;

    const QVector<DivisionEstimator::FeatureOption> options = DivisionEstimator::featureOptions();
    if (options.isEmpty()) {
        QMessageBox::information(mainWindow,
                                 "Export Geometric Features",
                                 "No geometric features are available to export.");
        return;
    }

    QString baseDir = mainWindow->ui->label_input_directory_value->text();
    if (baseDir.isEmpty())
        baseDir = QDir::homePath();
    const QString defaultPath = QDir(baseDir).filePath("geometric_features.csv");

    const QString savePath = QFileDialog::getSaveFileName(mainWindow,
                                                          "Export Geometric Features",
                                                          defaultPath,
                                                          "CSV Files (*.csv)");
    if (savePath.isEmpty())
        return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(mainWindow,
                              "Export Geometric Features",
                              QString("Failed to write %1: %2").arg(savePath, file.errorString()));
        return;
    }

    QTextStream out(&file);
    out << "feature_key,feature_label,description\n";
    for (const auto &opt : options) {
        QStringList row = {
            escapeCsvField(opt.key),
            escapeCsvField(opt.label),
            escapeCsvField(opt.description)
        };
        out << row.join(',') << "\n";
    }

    QMessageBox::information(mainWindow,
                             "Export Geometric Features",
                             QString("Exported %1 geometric features to:\n%2").arg(options.size()).arg(savePath));
}
