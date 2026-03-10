#include "divisionestimator.h"

#include "batchdivisionestimator.h"
#include "precisionrecallsweepworker.h"
#include "neighborpair.h"
#include "polygonitem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QTextStream>
#include <QDebug>
#include <QEventLoop>
#include <QThread>
#include <QVBoxLayout>
#include <QDir>
#include <QHash>
#include <QtMath>
#include <cmath>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <array>
#include <utility>

namespace {
constexpr double kArrowHeadLength = 5.0;
constexpr double kArrowHeadWidth = 3.0;
constexpr int kArrowCategoryCount = 5;

int categoryIndex(DivisionDisplay::ArrowCategory category)
{
    switch (category) {
    case DivisionDisplay::ArrowCategory::Estimated: return 0;
    case DivisionDisplay::ArrowCategory::Real: return 1;
    case DivisionDisplay::ArrowCategory::TruePositive: return 2;
    case DivisionDisplay::ArrowCategory::FalsePositive: return 3;
    case DivisionDisplay::ArrowCategory::FalseNegative: return 4;
    }
    return 0;
}

std::array<DivisionArrowStyle, kArrowCategoryCount> g_arrowStyles = {
    DivisionArrowStyle{QColor("#FF8C00"), 2, DivisionArrowStyle::ArrowType::DoubleHeaded, 5.0, 3.0},
    DivisionArrowStyle{QColor("#FF9999"), 2, DivisionArrowStyle::ArrowType::DoubleHeaded, 5.0, 3.0},
    DivisionArrowStyle{QColor("#AA1E1E"), 2, DivisionArrowStyle::ArrowType::DoubleHeaded, 5.0, 3.0},
    DivisionArrowStyle{QColor("#50AA78"), 2, DivisionArrowStyle::ArrowType::DoubleHeaded, 5.0, 3.0},
    DivisionArrowStyle{QColor("#3C78C8"), 2, DivisionArrowStyle::ArrowType::DoubleHeaded, 5.0, 3.0},
};

std::array<bool, kArrowCategoryCount> g_arrowVisibility = {true, true, true, true, true};
}

DivisionArrowStyle& DivisionDisplay::mutableStyle(ArrowCategory category)
{
    const int index = std::clamp(categoryIndex(category), 0, kArrowCategoryCount - 1);
    return g_arrowStyles[static_cast<qsizetype>(index)];
}

DivisionArrowStyle DivisionDisplay::currentStyle(ArrowCategory category)
{
    return mutableStyle(category);
}

void DivisionDisplay::setCurrentStyle(ArrowCategory category, const DivisionArrowStyle &style)
{
    mutableStyle(category) = style;
}

QPainterPath::Element DivisionDisplay::defaultElement(const QPainterPath &path, int index)
{
    if (index >= 0 && index < path.elementCount())
        return path.elementAt(index);
    return {0.0, 0.0, QPainterPath::MoveToElement};
}

QPainterPath DivisionDisplay::createArrowPath(const QPointF &start, const QPointF &end, const DivisionArrowStyle &style)
{
    QPainterPath path;
    path.moveTo(start);
    path.lineTo(end);

    if (style.arrowType == DivisionArrowStyle::ArrowType::LineOnly)
        return path;

    const QPointF direction = end - start;
    const double length = std::hypot(direction.x(), direction.y());
    if (length <= 0.0)
        return path;

    const QPointF unitDir = direction / length;
    const QPointF unitPerp(-unitDir.y(), unitDir.x());

    const double headLength = (style.arrowHeadLengthPx > 0.0) ? style.arrowHeadLengthPx : kArrowHeadLength;
    const double headWidth = (style.arrowHeadWidthPx > 0.0) ? style.arrowHeadWidthPx : kArrowHeadWidth;

    const auto buildHead = [&](const QPointF &tip, const QPointF &dir) {
        const QPointF backPoint = tip - dir * headLength;
        const QPointF left = backPoint + unitPerp * headWidth;
        const QPointF right = backPoint - unitPerp * headWidth;
        return QVector<QPointF>{left, right};
    };

    const auto addHead = [&](const QPointF &tip, const QPointF &dir) {
        const auto head = buildHead(tip, dir);
        path.moveTo(tip);
        path.lineTo(head[0]);
        path.moveTo(tip);
        path.lineTo(head[1]);
    };

    if (style.arrowType == DivisionArrowStyle::ArrowType::DoubleHeaded) {
        addHead(start, -unitDir);
        addHead(end, unitDir);
    } else if (style.arrowType == DivisionArrowStyle::ArrowType::SingleHeaded) {
        addHead(end, unitDir);
    }

    return path;
}

void DivisionDisplay::applyStyleToArrows(const QVector<QGraphicsPathItem*> &arrows, ArrowCategory category)
{
    const DivisionArrowStyle style = currentStyle(category);
    for (QGraphicsPathItem *arrow : arrows) {
        if (!arrow)
            continue;

        QPointF start = arrow->data(0).toPointF();
        QPointF end = arrow->data(1).toPointF();
        if (!arrow->data(0).isValid() || !arrow->data(1).isValid()) {
            const QPainterPath path = arrow->path();
            const auto e0 = defaultElement(path, 0);
            const auto e1 = defaultElement(path, 1);
            start = QPointF(e0.x, e0.y);
            end = QPointF(e1.x, e1.y);
        }

        arrow->setPath(createArrowPath(start, end, style));
        QPen pen(style.color, style.widthPx, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        arrow->setPen(pen);
    }
}

bool DivisionDisplay::displayEnabled(ArrowCategory category)
{
    const int index = std::clamp(categoryIndex(category), 0, kArrowCategoryCount - 1);
    return g_arrowVisibility[static_cast<qsizetype>(index)];
}

void DivisionDisplay::setDisplayEnabled(ArrowCategory category, bool enabled)
{
    const int index = std::clamp(categoryIndex(category), 0, kArrowCategoryCount - 1);
    g_arrowVisibility[static_cast<qsizetype>(index)] = enabled;
}

void DivisionDisplay::applyVisibilityToArrows(const QVector<QGraphicsPathItem*> &arrows, bool visible)
{
    for (QGraphicsPathItem *arrow : arrows) {
        if (!arrow)
            continue;
        arrow->setVisible(visible);
    }
}

bool DivisionDisplay::showDisplaySettingDialog(QGraphicsScene *scene,
                                               const QVector<QGraphicsPathItem*> &estimatedArrows,
                                               const QVector<QGraphicsPathItem*> &realArrows,
                                               const QVector<QGraphicsPathItem*> &truePositiveArrows,
                                               const QVector<QGraphicsPathItem*> &falsePositiveArrows,
                                               const QVector<QGraphicsPathItem*> &falseNegativeArrows,
                                               QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Division Pair Display Setting");

    const auto setPreviewColor = [](QLabel *lbl, const QColor &c) {
        QPalette pal = lbl->palette();
        pal.setColor(QPalette::Window, c);
        lbl->setPalette(pal);
    };

    struct StyleWidgets {
        QGroupBox *box = nullptr;
        QCheckBox *displayToggle = nullptr;
        QSpinBox *widthSpin = nullptr;
        QComboBox *arrowTypeCombo = nullptr;
        QDoubleSpinBox *headLengthSpin = nullptr;
        QDoubleSpinBox *headWidthSpin = nullptr;
        QLabel *colorPreview = nullptr;
        QPushButton *colorButton = nullptr;
        QColor color;
    };

    const auto createStyleGroup = [&](StyleWidgets &widgets,
                                      const QString &title,
                                      const DivisionArrowStyle &initialStyle,
                                      bool initialVisible) {
        widgets.box = new QGroupBox(title, &dialog);
        widgets.displayToggle = new QCheckBox("Display arrows", widgets.box);
        widgets.displayToggle->setChecked(initialVisible);

        widgets.widthSpin = new QSpinBox(widgets.box);
        widgets.widthSpin->setRange(1, 20);
        widgets.widthSpin->setValue(initialStyle.widthPx);

        widgets.arrowTypeCombo = new QComboBox(widgets.box);
        widgets.arrowTypeCombo->addItem("Double-headed arrow", static_cast<int>(DivisionArrowStyle::ArrowType::DoubleHeaded));
        widgets.arrowTypeCombo->addItem("Single-headed arrow", static_cast<int>(DivisionArrowStyle::ArrowType::SingleHeaded));
        widgets.arrowTypeCombo->addItem("Line only", static_cast<int>(DivisionArrowStyle::ArrowType::LineOnly));
        widgets.arrowTypeCombo->setCurrentIndex(widgets.arrowTypeCombo->findData(static_cast<int>(initialStyle.arrowType)));

        widgets.headLengthSpin = new QDoubleSpinBox(widgets.box);
        widgets.headLengthSpin->setRange(1.0, 200.0);
        widgets.headLengthSpin->setDecimals(1);
        widgets.headLengthSpin->setValue(initialStyle.arrowHeadLengthPx);

        widgets.headWidthSpin = new QDoubleSpinBox(widgets.box);
        widgets.headWidthSpin->setRange(1.0, 100.0);
        widgets.headWidthSpin->setDecimals(1);
        widgets.headWidthSpin->setValue(initialStyle.arrowHeadWidthPx);

        widgets.colorPreview = new QLabel(widgets.box);
        widgets.colorPreview->setFixedSize(40, 20);
        widgets.colorPreview->setAutoFillBackground(true);
        widgets.color = initialStyle.color;
        setPreviewColor(widgets.colorPreview, widgets.color);

        widgets.colorButton = new QPushButton("Choose...", widgets.box);
        QObject::connect(widgets.colorButton, &QPushButton::clicked, &dialog, [&dialog, &widgets]() {
            QColor c = QColorDialog::getColor(widgets.color, &dialog, "Choose Arrow Color");
            if (c.isValid()) {
                widgets.color = c;
                QPalette pal = widgets.colorPreview->palette();
                pal.setColor(QPalette::Window, c);
                widgets.colorPreview->setPalette(pal);
            }
        });

        auto *form = new QFormLayout;
        form->addRow("Arrow width (px):", widgets.widthSpin);
        form->addRow("Arrow type:", widgets.arrowTypeCombo);
        form->addRow("Arrow head length (px):", widgets.headLengthSpin);
        form->addRow("Arrow head width (px):", widgets.headWidthSpin);

        auto *colorRow = new QHBoxLayout;
        colorRow->addWidget(widgets.colorPreview);
        colorRow->addWidget(widgets.colorButton);
        form->addRow("Arrow color:", colorRow);

        auto *boxLayout = new QVBoxLayout(widgets.box);
        boxLayout->addWidget(widgets.displayToggle);
        boxLayout->addLayout(form);

        const auto refreshHeadControls = [&widgets]() {
            const auto type = static_cast<DivisionArrowStyle::ArrowType>(widgets.arrowTypeCombo->currentData().toInt());
            const bool enableHeads = type != DivisionArrowStyle::ArrowType::LineOnly;
            widgets.headLengthSpin->setEnabled(enableHeads);
            widgets.headWidthSpin->setEnabled(enableHeads);
        };
        QObject::connect(widgets.arrowTypeCombo, &QComboBox::currentIndexChanged, &dialog, refreshHeadControls);
        refreshHeadControls();
    };

    struct CategoryWidgets {
        ArrowCategory category;
        QString title;
        StyleWidgets widgets;
        DivisionArrowStyle style;
        bool visible = true;
        const QVector<QGraphicsPathItem*> *arrows = nullptr;
    };

    QVector<CategoryWidgets> categories = {
        {ArrowCategory::Estimated, "Estimated division arrows", {}, currentStyle(ArrowCategory::Estimated), displayEnabled(ArrowCategory::Estimated), &estimatedArrows},
        {ArrowCategory::Real, "Real division arrows", {}, currentStyle(ArrowCategory::Real), displayEnabled(ArrowCategory::Real), &realArrows},
        {ArrowCategory::TruePositive, "True positive division arrows", {}, currentStyle(ArrowCategory::TruePositive), displayEnabled(ArrowCategory::TruePositive), &truePositiveArrows},
        {ArrowCategory::FalsePositive, "False positive division arrows", {}, currentStyle(ArrowCategory::FalsePositive), displayEnabled(ArrowCategory::FalsePositive), &falsePositiveArrows},
        {ArrowCategory::FalseNegative, "False negative division arrows", {}, currentStyle(ArrowCategory::FalseNegative), displayEnabled(ArrowCategory::FalseNegative), &falseNegativeArrows},
    };

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *mainLayout = new QVBoxLayout(&dialog);
    for (auto &category : categories) {
        createStyleGroup(category.widgets, category.title, category.style, category.visible);
        mainLayout->addWidget(category.widgets.box);
    }
    mainLayout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    for (auto &category : categories) {
        category.style.widthPx = category.widgets.widthSpin->value();
        category.style.arrowType = static_cast<DivisionArrowStyle::ArrowType>(category.widgets.arrowTypeCombo->currentData().toInt());
        category.style.arrowHeadLengthPx = category.widgets.headLengthSpin->value();
        category.style.arrowHeadWidthPx = category.widgets.headWidthSpin->value();
        category.style.color = category.widgets.color;
        category.visible = category.widgets.displayToggle->isChecked();

        setCurrentStyle(category.category, category.style);
        setDisplayEnabled(category.category, category.visible);

        if (category.arrows) {
            applyStyleToArrows(*category.arrows, category.category);
            applyVisibilityToArrows(*category.arrows, category.visible);
        }
    }
    return true;
}

QGraphicsPathItem* DivisionDisplay::createArrowItem(const QPointF &start, const QPointF &end, ArrowCategory category)
{
    if (start == end)
        return nullptr;

    const DivisionArrowStyle style = currentStyle(category);
    QPainterPath path = createArrowPath(start, end, style);
    if (path.isEmpty())
        return nullptr;

    auto *item = new QGraphicsPathItem(path);
    QPen pen(style.color, style.widthPx, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    item->setPen(pen);
    item->setZValue(8);
    item->setData(0, start);
    item->setData(1, end);
    item->setData(2, static_cast<int>(category));
    item->setVisible(displayEnabled(category));
    return item;
}

QVector<DivisionEstimator::FeatureOption> DivisionEstimator::featureOptions()
{
    QVector<FeatureOption> options;
    options.append({"areaRatio", "Area Ratio (smaller/bigger)", "Ratio of smaller to larger polygon area"});
    options.append({"areaMean", "Area Mean", "Average area of the two polygons"});
    options.append({"areaMin", "Area Min", "Smaller polygon area"});
    options.append({"areaMax", "Area Max", "Larger polygon area"});
    options.append({"areaDiff", "Area Difference", "Difference between polygon areas"});

    options.append({"perimeterRatio", "Perimeter Ratio (smaller/bigger)", "Ratio of smaller to larger perimeter"});
    options.append({"perimeterMean", "Perimeter Mean", "Average polygon perimeter"});
    options.append({"perimeterMin", "Perimeter Min", "Smaller polygon perimeter"});
    options.append({"perimeterMax", "Perimeter Max", "Larger polygon perimeter"});
    options.append({"perimeterDiff", "Perimeter Difference", "Difference between polygon perimeters"});

    options.append({"aspectRatio", "Aspect Ratio (smaller/bigger)", "Ratio of smaller to larger aspect ratio"});
    options.append({"aspectMean", "Aspect Ratio Mean", "Average polygon aspect ratio"});
    options.append({"aspectMin", "Aspect Ratio Min", "Smaller polygon aspect ratio"});
    options.append({"aspectMax", "Aspect Ratio Max", "Larger polygon aspect ratio"});
    options.append({"aspectDiff", "Aspect Ratio Difference", "Difference between polygon aspect ratios"});

    options.append({"circularityRatio", "Circularity Ratio (smaller/bigger)", "Ratio of smaller to larger circularity"});
    options.append({"circularityMean", "Circularity Mean", "Average polygon circularity"});
    options.append({"circularityMin", "Circularity Min", "Smaller polygon circularity"});
    options.append({"circularityMax", "Circularity Max", "Larger polygon circularity"});
    options.append({"circularityDiff", "Circularity Difference", "Difference between polygon circularities"});

    options.append({"solidityRatio", "Solidity Ratio (smaller/bigger)", "Ratio of smaller to larger solidity"});
    options.append({"solidityMean", "Solidity Mean", "Average polygon solidity"});
    options.append({"solidityMin", "Solidity Min", "Smaller polygon solidity"});
    options.append({"solidityMax", "Solidity Max", "Larger polygon solidity"});
    options.append({"solidityDiff", "Solidity Difference", "Difference between polygon solidities"});

    options.append({"vertexCountRatio", "Vertex Count Ratio (smaller/bigger)", "Ratio of smaller to larger vertex count"});
    options.append({"vertexCountMean", "Vertex Count Mean", "Average vertex count"});
    options.append({"vertexCountMin", "Vertex Count Min", "Smaller vertex count"});
    options.append({"vertexCountMax", "Vertex Count Max", "Larger vertex count"});
    options.append({"vertexCountDiff", "Vertex Count Difference", "Difference between vertex counts"});

    options.append({"centroidDistance", "Centroid Distance", "Distance between polygon centroids"});
    options.append({"unionAspectRatio", "Union Aspect Ratio", "Aspect ratio of the polygon union"});
    options.append({"unionCircularity", "Union Circularity", "Circularity of the polygon union"});
    options.append({"unionConvexDeficiency", "Union Convex Deficiency", "Convex deficiency of the union"});
    options.append({"normalizedSharedEdgeLength", "Normalized Shared Edge Length", "Shared edge length normalized by perimeters"});
    options.append({"sharedEdgeUnsharedVerticesDistance", "Shared Edge to Unshared Vertices Distance", "Average distance from shared edge to unshared vertices"});
    options.append({"sharedEdgeUnsharedVerticesDistanceNormalized", "Normalized Shared Edge to Unshared Vertices Distance", "Normalized distance from shared edge to unshared vertices"});
    options.append({"centroidSharedEdgeDistance", "Centroid to Shared Edge Distance", "Average centroid to shared edge distance"});
    options.append({"centroidSharedEdgeDistanceNormalized", "Normalized Centroid to Shared Edge Distance", "Normalized centroid to shared edge distance"});
    options.append({"junctionAngleAverageDegrees", "Junction Angle Average (deg)", "Average junction angle in degrees"});
    options.append({"junctionAngleMaxDegrees", "Junction Angle Max (deg)", "Maximum junction angle in degrees"});
    options.append({"junctionAngleMinDegrees", "Junction Angle Min (deg)", "Minimum junction angle in degrees"});
    options.append({"junctionAngleDifferenceDegrees", "Junction Angle Difference (deg)", "Difference between largest and smallest junction angles"});
    options.append({"junctionAngleRatio", "Junction Angle Ratio", "Ratio of smaller to larger junction angle"});
    options.append({"sharedEdgeUnionCentroidDistance", "Shared Edge to Union Centroid Distance", "Distance between shared edge midpoint and union centroid"});
    options.append({"sharedEdgeUnionCentroidDistanceNormalized", "Normalized Shared Edge to Union Centroid Distance", "Normalized distance between shared edge midpoint and union centroid"});
    options.append({"sharedEdgeUnionAxisAngleDegrees", "Shared Edge to Union Axis Angle (deg)", "Angle between shared edge and union principal axis"});

    return options;
}

bool DivisionEstimator::showSingleFeatureDialog(QWidget *parent, Criterion &criterion)
{
    QDialog dialog(parent);
    dialog.setWindowTitle("Estimate Division by Single Geometry");

    const QVector<FeatureOption> options = featureOptions();

    auto *featureCombo = new QComboBox(&dialog);
    const QString defaultFeatureKey = "junctionAngleAverageDegrees";
    for (const FeatureOption &opt : options) {
        featureCombo->addItem(opt.label, opt.key);
    }
    const int defaultFeatureIndex = featureCombo->findData(defaultFeatureKey);
    if (defaultFeatureIndex >= 0)
        featureCombo->setCurrentIndex(defaultFeatureIndex);

    auto *directionCombo = new QComboBox(&dialog);
    directionCombo->addItem("Above or equal to threshold", true);
    directionCombo->addItem("Below or equal to threshold", false);

    auto *thresholdSpin = new QDoubleSpinBox(&dialog);
    thresholdSpin->setDecimals(4);
    thresholdSpin->setRange(-1e9, 1e9);
    thresholdSpin->setValue(criterion.threshold);

    auto *matchingModeCombo = new QComboBox(&dialog);
    matchingModeCombo->addItem("Local best matching (greedy)",
                               static_cast<int>(Criterion::MatchingMode::LocalOptimal));
    matchingModeCombo->addItem("Global maximum weight matching",
                               static_cast<int>(Criterion::MatchingMode::GlobalMaximumWeight));
    matchingModeCombo->addItem("No constraint (allow multiple matches)",
                               static_cast<int>(Criterion::MatchingMode::Unconstrained));
    const int defaultMatchingIndex = matchingModeCombo->findData(static_cast<int>(criterion.matchingMode));
    if (defaultMatchingIndex >= 0)
        matchingModeCombo->setCurrentIndex(defaultMatchingIndex);

    auto *descriptionLabel = new QLabel(&dialog);
    descriptionLabel->setWordWrap(true);
    const auto updateDescription = [featureCombo, descriptionLabel, options]() {
        const int idx = featureCombo->currentIndex();
        if (idx < 0 || idx >= options.size())
            return;
        const QVariant keyData = featureCombo->itemData(idx);
        for (const auto &opt : options) {
            if (opt.key == keyData.toString()) {
                descriptionLabel->setText(opt.description);
                break;
            }
        }
    };
    updateDescription();
    QObject::connect(featureCombo,
                     static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     &dialog,
                     updateDescription);

    auto *form = new QFormLayout;
    form->addRow("Geometry feature", featureCombo);
    form->addRow("Comparison", directionCombo);
    form->addRow("Threshold", thresholdSpin);
    form->addRow("Matching", matchingModeCombo);
    form->addRow("Description", descriptionLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    const int selectedIndex = featureCombo->currentIndex();
    if (selectedIndex < 0)
        return false;

    const QString key = featureCombo->itemData(selectedIndex).toString();
    FeatureOption selectedOpt;
    for (const auto &opt : options) {
        if (opt.key == key) {
            selectedOpt = opt;
            break;
        }
    }

    const bool requireAbove = directionCombo->currentData().toBool();
    const double threshold = thresholdSpin->value();
    criterion.matchingMode = static_cast<Criterion::MatchingMode>(
        matchingModeCombo->currentData().toInt());
    return populateCriterionFromSelection(selectedOpt, requireAbove, threshold, criterion);
}

bool DivisionEstimator::showPrecisionRecallSweepDialog(QWidget *parent, const QString &defaultDirectoryHint)
{
    Criterion criterion;
    const QString dialogTitle = "Batch Estimate Division by Ranging Single Geometry";

    QString defaultDir = defaultDirectoryHint;
    if (defaultDir.isEmpty() || defaultDir == "-")
        defaultDir = QDir::homePath();

    QDialog dialog(parent);
    dialog.setWindowTitle(dialogTitle);

    auto *directoryEdit = new QLineEdit(defaultDir, &dialog);
    auto *browseButton = new QPushButton("Browse...", &dialog);
    auto *dirContainer = new QWidget(&dialog);
    auto *dirLayout = new QHBoxLayout(dirContainer);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->addWidget(directoryEdit);
    dirLayout->addWidget(browseButton);
    QObject::connect(browseButton, &QPushButton::clicked, &dialog, [parent, directoryEdit]() {
        QString startDir = directoryEdit->text().trimmed();
        if (startDir.isEmpty() || startDir == "-")
            startDir = QDir::homePath();
        const QString chosenDir = QFileDialog::getExistingDirectory(parent, "Select Directory with Geometry JSON Files", startDir);
        if (!chosenDir.isEmpty())
            directoryEdit->setText(chosenDir);
    });

    const QVector<DivisionEstimator::FeatureOption> options = DivisionEstimator::featureOptions();
    auto *featureCombo = new QComboBox(&dialog);
    const QString defaultFeatureKey = "junctionAngleAverageDegrees";
    for (const auto &opt : options) {
        featureCombo->addItem(opt.label, opt.key);
        const int idx = featureCombo->count() - 1;
        featureCombo->setItemData(idx, opt.description, Qt::ToolTipRole);
    }
    const int defaultFeatureIndex = featureCombo->findData(defaultFeatureKey);
    if (defaultFeatureIndex >= 0)
        featureCombo->setCurrentIndex(defaultFeatureIndex);

    auto *directionCombo = new QComboBox(&dialog);
    directionCombo->addItem("Above or equal to threshold", true);
    directionCombo->addItem("Below or equal to threshold", false);

    auto *thresholdStartSpin = new QDoubleSpinBox(&dialog);
    thresholdStartSpin->setDecimals(4);
    thresholdStartSpin->setRange(-1e9, 1e9);
    thresholdStartSpin->setValue(0.0);

    auto *thresholdEndSpin = new QDoubleSpinBox(&dialog);
    thresholdEndSpin->setDecimals(4);
    thresholdEndSpin->setRange(-1e9, 1e9);
    thresholdEndSpin->setValue(criterion.threshold);

    auto *thresholdStepSpin = new QDoubleSpinBox(&dialog);
    thresholdStepSpin->setDecimals(4);
    thresholdStepSpin->setRange(0.0001, 1e9);
    thresholdStepSpin->setValue(5.0);

    auto *matchingModeCombo = new QComboBox(&dialog);
    matchingModeCombo->addItem("Local best matching (greedy)",
                               static_cast<int>(DivisionEstimator::Criterion::MatchingMode::LocalOptimal));
    matchingModeCombo->addItem("Global maximum weight matching",
                               static_cast<int>(DivisionEstimator::Criterion::MatchingMode::GlobalMaximumWeight));
    matchingModeCombo->addItem("No constraint (allow multiple matches)",
                               static_cast<int>(DivisionEstimator::Criterion::MatchingMode::Unconstrained));
    const int defaultMatchingIndex = matchingModeCombo->findData(static_cast<int>(criterion.matchingMode));
    if (defaultMatchingIndex >= 0)
        matchingModeCombo->setCurrentIndex(defaultMatchingIndex);

    auto *descriptionLabel = new QLabel(&dialog);
    descriptionLabel->setWordWrap(true);
    const auto updateDescription = [featureCombo, descriptionLabel, options]() {
        const int idx = featureCombo->currentIndex();
        if (idx < 0 || idx >= options.size())
            return;
        const QVariant keyData = featureCombo->itemData(idx);
        for (const auto &opt : options) {
            if (opt.key == keyData.toString()) {
                descriptionLabel->setText(opt.description);
                break;
            }
        }
    };
    updateDescription();
    QObject::connect(featureCombo,
                     static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     &dialog,
                     updateDescription);

    auto *form = new QFormLayout;
    form->addRow("Input directory", dirContainer);
    form->addRow("Geometry feature", featureCombo);
    form->addRow("Comparison", directionCombo);
    form->addRow("Threshold start", thresholdStartSpin);
    form->addRow("Threshold end", thresholdEndSpin);
    form->addRow("Threshold step", thresholdStepSpin);
    form->addRow("Matching", matchingModeCombo);
    form->addRow("Description", descriptionLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString directory = directoryEdit->text().trimmed();
    if (directory.isEmpty()) {
        QMessageBox::warning(parent, dialogTitle, "Please choose an input directory.");
        return false;
    }

    QDir dir(directory);
    if (!dir.exists()) {
        QMessageBox::warning(parent, dialogTitle, QString("Directory does not exist: %1").arg(directory));
        return false;
    }

    const int featureIndex = featureCombo->currentIndex();
    if (featureIndex < 0 || featureIndex >= options.size()) {
        QMessageBox::warning(parent, dialogTitle, "Please select a geometry feature.");
        return false;
    }

    const double thresholdStart = thresholdStartSpin->value();
    const double thresholdEnd = thresholdEndSpin->value();
    const double thresholdStep = thresholdStepSpin->value();

    if (thresholdStep <= 0.0) {
        QMessageBox::warning(parent, dialogTitle, "Threshold step must be greater than zero.");
        return false;
    }

    if (thresholdStart > thresholdEnd) {
        QMessageBox::warning(parent, dialogTitle, "Threshold start must be less than or equal to threshold end.");
        return false;
    }

    const auto selectedOption = options.at(featureIndex);
    const bool requireAbove = directionCombo->currentData().toBool();
    const auto matchingMode = static_cast<DivisionEstimator::Criterion::MatchingMode>(
        matchingModeCombo->currentData().toInt());

    if (!DivisionEstimator::populateCriterionFromSelection(selectedOption, requireAbove, thresholdStart, criterion)) {
        QMessageBox::warning(parent, dialogTitle, "Failed to prepare the selected feature.");
        return false;
    }
    criterion.matchingMode = matchingMode;

    const int totalSteps = static_cast<int>(std::floor((thresholdEnd - thresholdStart) / thresholdStep + 1.0 + 1e-9));

    PrecisionRecallSweepWorker::Request request;
    request.directory = directory;
    request.baseCriterion = criterion;
    request.thresholdStart = thresholdStart;
    request.thresholdEnd = thresholdEnd;
    request.thresholdStep = thresholdStep;

    QProgressDialog progress("Calculating batch estimations over the threshold range...", "Cancel", 0, totalSteps, parent);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    QVector<PrecisionRecallSweepResultRow> results;
    QVector<PrecisionRecallSweepThresholdResult> pairsByThreshold;
    QStringList warnings;
    QStringList errors;
    bool success = false;
    bool canceled = false;

    auto *thread = new QThread(parent);
    auto *worker = new PrecisionRecallSweepWorker(request);
    worker->moveToThread(thread);

    QEventLoop loop;
    QObject::connect(thread, &QThread::started, worker, &PrecisionRecallSweepWorker::run);
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(worker, &PrecisionRecallSweepWorker::progress, parent, [&](int completed, int total, double threshold) {
        progress.setMaximum(total);
        progress.setValue(completed);
        progress.setLabelText(QString("Processing threshold %1").arg(QString::number(threshold, 'f', 6)));
    });
    QObject::connect(&progress, &QProgressDialog::canceled, worker, &PrecisionRecallSweepWorker::requestCancel);
    QObject::connect(worker, &PrecisionRecallSweepWorker::finished, parent,
                     [&](bool workerSuccess,
                         const QVector<PrecisionRecallSweepResultRow> &workerResults,
                         const QVector<PrecisionRecallSweepThresholdResult> &workerPairsByThreshold,
                         const QStringList &workerWarnings,
                         const QStringList &workerErrors,
                         bool workerCanceled) {
        success = workerSuccess;
        results = workerResults;
        pairsByThreshold = workerPairsByThreshold;
        warnings = workerWarnings;
        errors = workerErrors;
        canceled = workerCanceled;
        loop.quit();
    });
    QObject::connect(worker, &PrecisionRecallSweepWorker::finished, thread, &QThread::quit);

    thread->start();
    loop.exec();
    progress.reset();
    thread->wait();
    thread->deleteLater();

    if (canceled) {
        QMessageBox::information(parent, dialogTitle, "Estimation canceled.");
        return false;
    }

    if (!success || results.isEmpty() || pairsByThreshold.isEmpty()) {
        const QString message = errors.isEmpty()
                ? QString("No geometry files were processed in %1.").arg(directory)
                : errors.join("\n");
        QMessageBox::warning(parent, dialogTitle, message);
        return false;
    }

    QVector<QString> thresholdLabels;
    thresholdLabels.reserve(results.size());
    for (const auto &row : std::as_const(results))
        thresholdLabels.append(QString::number(row.threshold, 'f', 6));

    struct PairAggregate {
        QString fileName;
        int firstId = -1;
        int secondId = -1;
        double featureValue = std::numeric_limits<double>::quiet_NaN();
        bool observedDivision = false;
        QHash<QString, bool> estimatedByThreshold;
    };

    const auto pairKey = [](const BatchDivisionEstimator::DivisionPairRow &row) {
        const int first = std::min(row.firstId, row.secondId);
        const int second = std::max(row.firstId, row.secondId);
        return QString("%1|%2|%3").arg(row.fileName).arg(first).arg(second);
    };

    QHash<QString, PairAggregate> aggregateMap;
    for (const auto &thresholdResult : std::as_const(pairsByThreshold)) {
        const QString thresholdLabel = QString::number(thresholdResult.threshold, 'f', 6);
        for (const auto &pairRow : thresholdResult.pairRows) {
            const QString key = pairKey(pairRow);
            auto it = aggregateMap.find(key);
            if (it == aggregateMap.end()) {
                PairAggregate agg;
                agg.fileName = pairRow.fileName;
                agg.firstId = pairRow.firstId;
                agg.secondId = pairRow.secondId;
                agg.observedDivision = pairRow.observedDivision;
                agg.featureValue = DivisionEstimator::featureValue(criterion.featureKey, pairRow.geometry);
                it = aggregateMap.insert(key, agg);
            } else {
                if (!std::isfinite(it->featureValue)) {
                    it->featureValue = DivisionEstimator::featureValue(criterion.featureKey, pairRow.geometry);
                }
                it->observedDivision = it->observedDivision || pairRow.observedDivision;
            }
            it->estimatedByThreshold.insert(thresholdLabel, pairRow.estimatedDivision);
        }
    }

    if (aggregateMap.isEmpty()) {
        QMessageBox::warning(parent, dialogTitle, "No neighbor pairs were available to export.");
        return false;
    }

    QVector<PairAggregate> aggregateRows = aggregateMap.values().toVector();
    std::sort(aggregateRows.begin(), aggregateRows.end(), [](const PairAggregate &a, const PairAggregate &b) {
        if (a.fileName != b.fileName)
            return a.fileName < b.fileName;
        if (a.firstId != b.firstId)
            return a.firstId < b.firstId;
        return a.secondId < b.secondId;
    });

    const auto featureValueToString = [](double value) {
        return std::isfinite(value) ? QString::number(value, 'f', 6) : QStringLiteral("-");
    };
    const auto matchingModeLabel = [](DivisionEstimator::Criterion::MatchingMode mode) {
        switch (mode) {
        case DivisionEstimator::Criterion::MatchingMode::LocalOptimal:
            return QStringLiteral("Local best matching (greedy)");
        case DivisionEstimator::Criterion::MatchingMode::GlobalMaximumWeight:
            return QStringLiteral("Global maximum weight matching");
        case DivisionEstimator::Criterion::MatchingMode::Unconstrained:
        default:
            return QStringLiteral("No constraint (allow multiple matches)");
        }
    };
    const auto escapeCsv = [](const QString &value) {
        QString escaped = value;
        escaped.replace('"', "\"\"");
        if (escaped.contains(',') || escaped.contains('"'))
            escaped = QString("\"%1\"").arg(escaped);
        return escaped;
    };

    const QString defaultCsv = dir.filePath("batch_single_geometry_threshold_sweep.csv");
    const QString defaultPerformanceCsv = dir.filePath("batch_single_geometry_threshold_performance.csv");
    const QString savePath = QFileDialog::getSaveFileName(parent,
                                                          "Export Batch Estimation Threshold Sweep",
                                                          defaultCsv,
                                                          "CSV Files (*.csv)");
    if (savePath.isEmpty())
        return false;

    const QString performanceSavePath = QFileDialog::getSaveFileName(parent,
                                                                     "Export Performance Matrix for Threshold Sweep",
                                                                     defaultPerformanceCsv,
                                                                     "CSV Files (*.csv)");
    if (performanceSavePath.isEmpty()) {
        QMessageBox::warning(parent, dialogTitle, "Please choose a file to export the performance matrix.");
        return false;
    }

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(parent, dialogTitle, QString("Unable to open %1 for writing.").arg(savePath));
        return false;
    }

    QFile performanceFile(performanceSavePath);
    if (!performanceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(parent, dialogTitle, QString("Unable to open %1 for writing.").arg(performanceSavePath));
        return false;
    }

    QTextStream out(&file);
    QTextStream performanceOut(&performanceFile);
    const auto writeRow = [&escapeCsv](QTextStream &stream, const QStringList &fields) {
        QStringList escaped = fields;
        for (QString &field : escaped)
            field = escapeCsv(field);
        stream << escaped.join(',') << '\n';
    };
    const auto metricToString = [](double value) {
        return std::isfinite(value) ? QString::number(value, 'f', 6) : QStringLiteral("-");
    };
    const auto countToString = [](int value) {
        return value < 0 ? QStringLiteral("-") : QString::number(value);
    };

    writeRow(out, {"setting", "value"});
    writeRow(out, {"feature_label", criterion.featureLabel});
    writeRow(out, {"feature_key", criterion.featureKey});
    writeRow(out, {"comparison", criterion.requireAbove ? "above_or_equal" : "below_or_equal"});
    writeRow(out, {"threshold_start", QString::number(thresholdStart, 'f', 6)});
    writeRow(out, {"threshold_end", QString::number(thresholdEnd, 'f', 6)});
    writeRow(out, {"threshold_step", QString::number(thresholdStep, 'f', 6)});
    writeRow(out, {"matching_mode", matchingModeLabel(matchingMode)});
    out << '\n';

    QStringList headers = {
        QStringLiteral("file_name"),
        QStringLiteral("cell_id_1"),
        QStringLiteral("cell_id_2"),
        QString("%1_value").arg(criterion.featureKey),
        QStringLiteral("observed_division")
    };
    for (const QString &label : std::as_const(thresholdLabels))
        headers << QString("estimated_division_%1").arg(label);

    writeRow(out, headers);

    for (const auto &row : std::as_const(aggregateRows)) {
        QStringList fields;
        fields << row.fileName
               << QString::number(row.firstId)
               << QString::number(row.secondId)
               << featureValueToString(row.featureValue)
               << (row.observedDivision ? "1" : "0");
        for (const QString &label : std::as_const(thresholdLabels))
            fields << (row.estimatedByThreshold.value(label, false) ? "1" : "0");
        writeRow(out, fields);
    }

    writeRow(performanceOut, {
        QStringLiteral("threshold"),
        QStringLiteral("real_division_pairs"),
        QStringLiteral("estimated_division_pairs"),
        QStringLiteral("true_positive"),
        QStringLiteral("false_positive"),
        QStringLiteral("false_negative"),
        QStringLiteral("true_negative"),
        QStringLiteral("neighbor_pairs"),
        QStringLiteral("precision"),
        QStringLiteral("recall"),
        QStringLiteral("f1"),
        QStringLiteral("specificity"),
        QStringLiteral("accuracy")
    });

    for (const auto &row : std::as_const(results)) {
        const auto &m = row.metrics;
        writeRow(performanceOut, {
            QString::number(row.threshold, 'f', 6),
            countToString(m.realPairs),
            countToString(m.estimatedPairs),
            QString::number(m.truePositives),
            QString::number(m.falsePositives),
            QString::number(m.falseNegatives),
            QString::number(m.trueNegatives),
            QString::number(m.neighborPairs),
            metricToString(m.precision),
            metricToString(m.recall),
            metricToString(m.f1),
            metricToString(m.specificity),
            metricToString(m.accuracy)
        });
    }

    QString message = QString("Evaluated %1 threshold values for %2 neighbor pairs and exported the sweep to:\n- %3\n- %4")
                          .arg(results.size())
                          .arg(aggregateRows.size())
                          .arg(savePath)
                          .arg(performanceSavePath);

    if (!warnings.isEmpty())
        message += "\n\nWarnings:\n- " + warnings.join("\n- ");
    if (!errors.isEmpty())
        message += "\n\nErrors:\n- " + errors.join("\n- ");

    QMessageBox::information(parent, dialogTitle, message);
    return true;
}

bool DivisionEstimator::populateCriterionFromSelection(const FeatureOption &option, bool requireAbove, double threshold,
                                                       Criterion &criterion)
{
    if (option.key.isEmpty())
        return false;

    criterion.featureKey = option.key;
    criterion.featureLabel = option.label;
    criterion.requireAbove = requireAbove;
    criterion.threshold = threshold;
    criterion.settings = NeighborPairGeometryCalculator::currentSettings();
    ensureSettingForFeature(option.key, criterion.settings);
    return true;
}

double DivisionEstimator::featureValue(const QString &key, const NeighborPairGeometryCalculator::Result &result)
{
    if (key == "areaRatio") return result.areaRatio;
    if (key == "areaMean") return result.areaMean;
    if (key == "areaMin") return result.areaMin;
    if (key == "areaMax") return result.areaMax;
    if (key == "areaDiff") return result.areaDiff;

    if (key == "perimeterRatio") return result.perimeterRatio;
    if (key == "perimeterMean") return result.perimeterMean;
    if (key == "perimeterMin") return result.perimeterMin;
    if (key == "perimeterMax") return result.perimeterMax;
    if (key == "perimeterDiff") return result.perimeterDiff;

    if (key == "aspectRatio") return result.aspectRatio;
    if (key == "aspectMean") return result.aspectMean;
    if (key == "aspectMin") return result.aspectMin;
    if (key == "aspectMax") return result.aspectMax;
    if (key == "aspectDiff") return result.aspectDiff;

    if (key == "circularityRatio") return result.circularityRatio;
    if (key == "circularityMean") return result.circularityMean;
    if (key == "circularityMin") return result.circularityMin;
    if (key == "circularityMax") return result.circularityMax;
    if (key == "circularityDiff") return result.circularityDiff;

    if (key == "solidityRatio") return result.solidityRatio;
    if (key == "solidityMean") return result.solidityMean;
    if (key == "solidityMin") return result.solidityMin;
    if (key == "solidityMax") return result.solidityMax;
    if (key == "solidityDiff") return result.solidityDiff;

    if (key == "vertexCountRatio") return result.vertexCountRatio;
    if (key == "vertexCountMean") return result.vertexCountMean;
    if (key == "vertexCountMin") return result.vertexCountMin;
    if (key == "vertexCountMax") return result.vertexCountMax;
    if (key == "vertexCountDiff") return result.vertexCountDiff;

    if (key == "centroidDistance") return result.centroidDistance;
    if (key == "unionAspectRatio") return result.unionAspectRatio;
    if (key == "unionCircularity") return result.unionCircularity;
    if (key == "unionConvexDeficiency") return result.unionConvexDeficiency;
    if (key == "normalizedSharedEdgeLength") return result.normalizedSharedEdgeLength;
    if (key == "sharedEdgeUnsharedVerticesDistance") return result.sharedEdgeUnsharedVerticesDistance;
    if (key == "sharedEdgeUnsharedVerticesDistanceNormalized") return result.sharedEdgeUnsharedVerticesDistanceNormalized;
    if (key == "centroidSharedEdgeDistance") return result.centroidSharedEdgeDistance;
    if (key == "centroidSharedEdgeDistanceNormalized") return result.centroidSharedEdgeDistanceNormalized;
    if (key == "junctionAngleAverageDegrees") return result.junctionAngleAverageDegrees;
    if (key == "junctionAngleMaxDegrees") return result.junctionAngleMaxDegrees;
    if (key == "junctionAngleMinDegrees") return result.junctionAngleMinDegrees;
    if (key == "junctionAngleDifferenceDegrees") return result.junctionAngleDifferenceDegrees;
    if (key == "junctionAngleRatio") return result.junctionAngleRatio;
    if (key == "sharedEdgeUnionCentroidDistance") return result.sharedEdgeUnionCentroidDistance;
    if (key == "sharedEdgeUnionCentroidDistanceNormalized") return result.sharedEdgeUnionCentroidDistanceNormalized;
    if (key == "sharedEdgeUnionAxisAngleDegrees") return result.sharedEdgeUnionAxisAngleDegrees;

    return std::numeric_limits<double>::quiet_NaN();
}

void DivisionEstimator::ensureSettingForFeature(const QString &key, NeighborPairGeometrySettings &settings)
{
    if (key.startsWith("area")) {
        if (key == "areaRatio") settings.computeAreaRatio = true;
        else settings.computeAreaStats = true;
        return;
    }

    if (key.startsWith("perimeter")) {
        if (key == "perimeterRatio") settings.computePerimeterRatio = true;
        else settings.computePerimeterStats = true;
        return;
    }

    if (key.startsWith("aspect")) {
        if (key == "aspectRatio") settings.computeAspectRatio = true;
        else settings.computeAspectRatioStats = true;
        return;
    }

    if (key.startsWith("circularity")) {
        if (key == "circularityRatio") settings.computeCircularityRatio = true;
        else settings.computeCircularityStats = true;
        return;
    }

    if (key.startsWith("solidity")) {
        if (key == "solidityRatio") settings.computeSolidityRatio = true;
        else settings.computeSolidityStats = true;
        return;
    }

    if (key.startsWith("vertexCount")) {
        if (key == "vertexCountRatio") settings.computeVertexCountRatio = true;
        else settings.computeVertexCountStats = true;
        return;
    }

    if (key == "centroidDistance") { settings.computeCentroidDistance = true; return; }
    if (key == "unionAspectRatio") { settings.computeUnionAspectRatio = true; return; }
    if (key == "unionCircularity") { settings.computeUnionCircularity = true; return; }
    if (key == "unionConvexDeficiency") { settings.computeUnionConvexDeficiency = true; return; }
    if (key == "normalizedSharedEdgeLength") { settings.computeNormalizedSharedEdgeLength = true; return; }
    if (key == "sharedEdgeUnsharedVerticesDistance") { settings.computeSharedEdgeUnsharedVerticesDistance = true; return; }
    if (key == "sharedEdgeUnsharedVerticesDistanceNormalized") { settings.computeSharedEdgeUnsharedVerticesDistanceNormalized = true; return; }
    if (key == "centroidSharedEdgeDistance") { settings.computeCentroidSharedEdgeDistance = true; return; }
    if (key == "centroidSharedEdgeDistanceNormalized") { settings.computeCentroidSharedEdgeDistanceNormalized = true; return; }

    if (key.startsWith("junctionAngle")) {
        settings.computeJunctionAngleAverage = true;
        settings.computeJunctionAngleMax = true;
        settings.computeJunctionAngleMin = true;
        settings.computeJunctionAngleDifference = true;
        settings.computeJunctionAngleRatio = true;
        return;
    }

    if (key == "sharedEdgeUnionCentroidDistance") { settings.computeSharedEdgeUnionCentroidDistance = true; return; }
    if (key == "sharedEdgeUnionCentroidDistanceNormalized") { settings.computeSharedEdgeUnionCentroidDistanceNormalized = true; return; }
    if (key == "sharedEdgeUnionAxisAngleDegrees") { settings.computeSharedEdgeUnionAxisAngle = true; return; }
}

DivisionEstimator::EstimationResult DivisionEstimator::estimatePairs(const QVector<PolygonItem*> &polygons,
                                                                     const Criterion &criterion,
                                                                     QGraphicsScene *scene)
{
    EstimationResult result;
    if (!scene)
        return result;

    QVector<CandidateMatch> candidates;
    for (int i = 0; i < polygons.size(); ++i) {
        for (int j = i + 1; j < polygons.size(); ++j) {
            NeighborPair pair(polygons[i], polygons[j]);
            if (!pair.isNeighbor())
                continue;

            ++result.examinedPairs;
            const auto geometry = NeighborPairGeometryCalculator::calculateForPair(polygons[i], polygons[j], criterion.settings);
            const double value = featureValue(criterion.featureKey, geometry);
            if (!std::isfinite(value))
                continue;

            const bool matches = criterion.requireAbove ? value >= criterion.threshold
                                                        : value <= criterion.threshold;
            if (!matches)
                continue;

            const double score = matchScore(criterion, value);
            candidates.append({i, j, score, value});
        }
    }

    const QVector<CandidateMatch> selected = selectMatches(polygons.size(), candidates, criterion.matchingMode);
    result.matchedPairs = selected.size();

    for (const auto &match : selected) {
        PolygonItem *firstPoly = polygons.value(match.first, nullptr);
        PolygonItem *secondPoly = polygons.value(match.second, nullptr);
        if (!firstPoly || !secondPoly)
            continue;

        result.matchedPairIds.append(QPair<int, int>(firstPoly->id(), secondPoly->id()));

        auto *arrow = DivisionDisplay::createArrowItem(firstPoly->centroid(),
                                                       secondPoly->centroid(),
                                                       DivisionDisplay::ArrowCategory::Estimated);
        if (arrow) {
            scene->addItem(arrow);
            result.arrows.append(arrow);
        }
    }

    return result;
}

double DivisionEstimator::matchScore(const Criterion &criterion, double featureValue)
{
    return criterion.requireAbove ? featureValue - criterion.threshold : criterion.threshold - featureValue;
}

QVector<DivisionEstimator::CandidateMatch> DivisionEstimator::greedyMatching(int polygonCount,
                                                                            const QVector<CandidateMatch> &candidates)
{
    QVector<CandidateMatch> sorted = candidates;
    std::sort(sorted.begin(), sorted.end(), [](const CandidateMatch &a, const CandidateMatch &b) {
        return a.score > b.score;
    });

    QVector<bool> used(static_cast<qsizetype>(polygonCount), false);
    QVector<CandidateMatch> chosen;

    for (const auto &cand : sorted) {
        if (cand.first < 0 || cand.second < 0)
            continue;
        if (cand.first >= polygonCount || cand.second >= polygonCount)
            continue;
        if (used[cand.first] || used[cand.second])
            continue;
        used[cand.first] = true;
        used[cand.second] = true;
        chosen.append(cand);
    }

    return chosen;
}

QVector<DivisionEstimator::CandidateMatch> DivisionEstimator::maximumWeightMatching(int polygonCount,
                                                                                   const QVector<CandidateMatch> &candidates)
{
    if (polygonCount <= 1 || candidates.isEmpty())
        return {};

    if (polygonCount >= static_cast<int>(sizeof(quint64) * 8)) {
        qWarning() << "Global maximum weight matching supports up to"
                   << (static_cast<int>(sizeof(quint64) * 8) - 1)
                   << "polygons. Falling back to greedy matching.";
        return greedyMatching(polygonCount, candidates);
    }

    QVector<QVector<CandidateMatch>> adjacency(polygonCount);
    for (const auto &cand : candidates) {
        if (cand.first < 0 || cand.second < 0)
            continue;
        if (cand.first >= polygonCount || cand.second >= polygonCount)
            continue;
        adjacency[cand.first].append(cand);
        adjacency[cand.second].append({cand.second, cand.first, cand.score, cand.featureValue});
    }

    struct Solution {
        double weight = 0.0;
        QVector<CandidateMatch> matches;
    };

    std::unordered_map<quint64, Solution> cache;
    const auto solve = [&](auto &&self, quint64 usedMask) -> Solution {
        auto it = cache.find(usedMask);
        if (it != cache.end())
            return it->second;

        int firstFree = -1;
        for (int idx = 0; idx < polygonCount; ++idx) {
            if ((usedMask & (quint64(1) << idx)) == 0) {
                firstFree = idx;
                break;
            }
        }

        if (firstFree == -1)
            return {};

        const quint64 baseMask = usedMask | (quint64(1) << firstFree);
        Solution best = self(self, baseMask);

        for (const auto &edge : adjacency[firstFree]) {
            const int other = edge.second;
            if (other == firstFree)
                continue;
            if (baseMask & (quint64(1) << other))
                continue;

            const quint64 newMask = baseMask | (quint64(1) << other);
            Solution candidate = self(self, newMask);
            const double totalWeight = candidate.weight + edge.score;
            if (totalWeight > best.weight) {
                candidate.weight = totalWeight;
                candidate.matches.append(edge);
                best = candidate;
            }
        }

        cache.insert({usedMask, best});
        return best;
    };

    Solution finalSolution = solve(solve, 0);
    return finalSolution.matches;
}

QVector<DivisionEstimator::CandidateMatch> DivisionEstimator::selectMatches(int polygonCount,
                                                                           const QVector<CandidateMatch> &candidates,
                                                                           Criterion::MatchingMode mode)
{
    if (mode == Criterion::MatchingMode::Unconstrained) {
        QVector<CandidateMatch> all = candidates;
        std::sort(all.begin(), all.end(), [](const CandidateMatch &a, const CandidateMatch &b) {
            return a.score > b.score;
        });
        return all;
    }
    if (mode == Criterion::MatchingMode::GlobalMaximumWeight)
        return maximumWeightMatching(polygonCount, candidates);
    return greedyMatching(polygonCount, candidates);
}
