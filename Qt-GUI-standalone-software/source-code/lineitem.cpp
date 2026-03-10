#include "lineitem.h"
#include "qlabel.h"
#include "qpushbutton.h"
#include "vertexitem.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QtMath>
#include <QPen>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QCheckBox>

#include <algorithm>

int LineItem::s_nextId = 1;
static LineItem::LineStyle g_lineStyle;

LineItem::LineItem(VertexItem *v1, VertexItem *v2, int id):m_v1(v1),m_v2(v2) {
    //Basic Validation
    if(!m_v1||!m_v2||m_v1==m_v2) return;

    if(id > 0){
        m_id = id;
        s_nextId = std::max(s_nextId, id + 1);
    }else{
        m_id = s_nextId++;
    }

    setFlags(QGraphicsItem::ItemIsSelectable);
    applyCurrentStyle();   // use global style (color + width)


    updateGeometry();

    connect(m_v1, &VertexItem::moved, this, &LineItem::onVertexMoved);
    connect(m_v2, &VertexItem::moved, this, &LineItem::onVertexMoved);

    // If a vertex is deleted, delete this line automatically (avoid dangling pointers)
    connect(m_v1, &QObject::destroyed, this, &LineItem::onVertexDestroyed);
    connect(m_v2, &QObject::destroyed, this, &LineItem::onVertexDestroyed);

    setZValue(5); //keep under vertex
}

int LineItem::v1Id() const { return m_v1 ? m_v1->id() : -1; }
int LineItem::v2Id() const { return m_v2 ? m_v2->id() : -1; }

double LineItem::length() const
{
    return QLineF(line().p1(),line().p2()).length();
}

double LineItem::angle0to180() const
{
    QPointF a = line().p1();
    QPointF b = line().p2();
    double dx = b.x() - a.x();
    double dy = b.y() - a.y();

    double deg = qRadiansToDegrees(std::atan2(dy,dx));
    double ang = std::fmod(deg + 180.0, 180.0);
    if (ang < 0) ang += 180.0;
    return ang;
}

double LineItem::angle0to90() const
{
    // acute angle in [0,90]
    double a180 = angle0to180();
    return (a180 <= 90.0) ? a180 : (180.0 - a180);
}

QPointF LineItem::meanPos() const
{
    QPointF a = line().p1();
    QPointF b = line().p2();
    return QPointF(0.5 * (a.x()+b.x()), 0.5*(a.y()+b.y()));
}

void LineItem::updateGeometry()
{
    if (!m_v1 || !m_v2) return;
    setLine(QLineF(m_v1->pos(), m_v2->pos()));

    if(isSelected()){
        emit geometryChanged(this);
    }
}

void LineItem::onVertexMoved(VertexItem * /*v*/, const QPointF & /*pos*/)
{
    updateGeometry();
}

void LineItem::onVertexDestroyed(QObject * /*obj*/)
{
    // One endpoint died -> remove this line
    if (scene()) scene()->removeItem(this);
    delete this;
}

LineItem* LineItem::createWithDialog(QGraphicsScene *scene, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded",
                             "Please create a canvas or open an image first.");
        return nullptr;
    }

    // Dialog: enter 2 vertex IDs
    QDialog dialog(parent);
    dialog.setWindowTitle("Add Line (connect two vertices)");

    QSpinBox *spinId1 = new QSpinBox(&dialog);
    QSpinBox *spinId2 = new QSpinBox(&dialog);
    spinId1->setRange(0, 100000000);
    spinId2->setRange(0, 100000000);
    spinId1->setValue(1);
    spinId2->setValue(2);

    QFormLayout *form = new QFormLayout;
    form->addRow("Vertex ID 1:", spinId1);
    form->addRow("Vertex ID 2:", spinId2);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    const int id1 = spinId1->value();
    const int id2 = spinId2->value();

    if (id1 == id2) {
        QMessageBox::warning(parent, "Invalid IDs", "Please choose two different vertex IDs.");
        return nullptr;
    }

    VertexItem *v1 = VertexItem::findVertexById(scene, id1);
    VertexItem *v2 = VertexItem::findVertexById(scene, id2);

    if (!v1 || !v2) {
        QMessageBox::warning(parent, "Vertex Not Found",
                             QString("Cannot find vertex ID(s): %1%2")
                                 .arg(v1 ? "" : QString::number(id1) + " ")
                                 .arg(v2 ? "" : QString::number(id2)));
        return nullptr;
    }

    auto *line = new LineItem(v1, v2);
    scene->addItem(line);

    // auto-select the new line
    scene->clearSelection();
    line->setSelected(true);

    return line;
}

LineItem* LineItem::findLineByVertexIds(QGraphicsScene *scene, int id1, int id2)
{
    if (!scene) return nullptr;
    if (id1 == id2) return nullptr;

    const int a = std::min(id1, id2);
    const int b = std::max(id1, id2);

    const auto items = scene->items();
    for (QGraphicsItem *it : items) {
        if (it && it->type() == LineItem::Type) {
            auto *L = static_cast<LineItem*>(it);
            int x = std::min(L->v1Id(), L->v2Id());
            int y = std::max(L->v1Id(), L->v2Id());
            if (x == a && y == b)
                return L;
        }
    }
    return nullptr;
}

LineItem* LineItem::findLineById(QGraphicsScene *scene, int id)
{
    if (!scene) return nullptr;

    const auto items = scene->items();
    for (QGraphicsItem *it : items) {
        if (it && it->type() == LineItem::Type) {
            auto *L = static_cast<LineItem*>(it);
            if (L->id() == id)
                return L;
        }
    }
    return nullptr;
}

LineItem* LineItem::findWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent)
{
    if (!scene || scene->sceneRect().isNull() ||
        scene->sceneRect().width() <= 0 || scene->sceneRect().height() <= 0)
    {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image before finding a line.");
        return nullptr;
    }

    bool hasLine = false;
    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == LineItem::Type) { hasLine = true; break; }
    }
    if (!hasLine) {
        QMessageBox::information(parent, "No Lines", "There is no line to find.");
        return nullptr;
    }

    // ---- Dialog ----
    QDialog dialog(parent);
    dialog.setWindowTitle("Find Line");

    auto *radioByLineId = new QRadioButton("Find by Line ID", &dialog);
    auto *radioByVertexIds = new QRadioButton("Find by Vertex IDs", &dialog);
    radioByLineId->setChecked(true);

    auto *spinLineId = new QSpinBox(&dialog);
    spinLineId->setRange(0, 100000000);
    spinLineId->setValue(1);

    auto *spinV1 = new QSpinBox(&dialog);
    auto *spinV2 = new QSpinBox(&dialog);
    spinV1->setRange(0, 100000000);
    spinV2->setRange(0, 100000000);
    spinV1->setValue(1);
    spinV2->setValue(2);

    auto updateEnabled = [&]() {
        bool byLineId = radioByLineId->isChecked();
        spinLineId->setEnabled(byLineId);

        spinV1->setEnabled(!byLineId);
        spinV2->setEnabled(!byLineId);
    };
    QObject::connect(radioByLineId, &QRadioButton::toggled, &dialog, updateEnabled);
    QObject::connect(radioByVertexIds, &QRadioButton::toggled, &dialog, updateEnabled);
    updateEnabled();

    QFormLayout *form = new QFormLayout;
    form->addRow(radioByLineId);
    form->addRow("Line ID:", spinLineId);
    form->addRow(radioByVertexIds);
    form->addRow("Vertex ID 1:", spinV1);
    form->addRow("Vertex ID 2:", spinV2);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    // ---- Find ----
    LineItem *target = nullptr;

    if (radioByLineId->isChecked()) {
        int id = spinLineId->value();
        target = LineItem::findLineById(scene, id);
        if (!target) {
            QMessageBox::warning(parent, "Line Not Found", QString("No line with Line ID = %1 was found.").arg(id));
            return nullptr;
        }
    } else {
        int id1 = spinV1->value();
        int id2 = spinV2->value();
        if (id1 == id2) {
            QMessageBox::warning(parent, "Invalid IDs", "Please choose two different vertex IDs.");
            return nullptr;
        }

        target = LineItem::findLineByVertexIds(scene, id1, id2);
        if (!target) {
            QMessageBox::warning(parent, "Line Not Found", QString("No line connecting vertex %1 and %2 was found.").arg(id1).arg(id2));
            return nullptr;
        }
    }

    // Select it
    scene->clearSelection();
    target->setSelected(true);

    // Center view (optional)
    if (view) {
        view->centerOn(target->meanPos());
    }

    return target;
}


QVariant LineItem::itemChange(GraphicsItemChange change, const QVariant &value){
    if(change == QGraphicsItem::ItemSelectedHasChanged){
        if(value.toBool()){
            emit selected(this);
        }
    }
    return QGraphicsLineItem::itemChange(change,value);
}

bool LineItem::deleteWithDialog(QGraphicsScene *scene, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded",
                             "Please create a canvas or open an image first.");
        return false;
    }

    // 1) If user already selected a line, delete all selected lines directly
    QList<LineItem*> selectedLines;
    for (QGraphicsItem *it : scene->selectedItems()) {
        if (it && it->type() == LineItem::Type)
            selectedLines.append(static_cast<LineItem*>(it));
    }

    if (!selectedLines.isEmpty()) {
        auto reply = QMessageBox::question(parent,
                                           "Delete Line",
                                           QString("Delete %1 selected line(s)?").arg(selectedLines.size()),
                                           QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return false;

        for (LineItem *L : selectedLines) {
            scene->removeItem(L);
            delete L;
        }
        return true;
    }

    // 2) Otherwise ask for vertex IDs
    QDialog dialog(parent);
    dialog.setWindowTitle("Delete Line (by vertex IDs)");

    QSpinBox *spinId1 = new QSpinBox(&dialog);
    QSpinBox *spinId2 = new QSpinBox(&dialog);
    spinId1->setRange(0, 100000000);
    spinId2->setRange(0, 100000000);
    spinId1->setValue(1);
    spinId2->setValue(2);

    QFormLayout *form = new QFormLayout;
    form->addRow("Vertex ID 1:", spinId1);
    form->addRow("Vertex ID 2:", spinId2);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    const int id1 = spinId1->value();
    const int id2 = spinId2->value();

    if (id1 == id2) {
        QMessageBox::warning(parent, "Invalid IDs", "Please choose two different vertex IDs.");
        return false;
    }

    LineItem *target = LineItem::findLineByVertexIds(scene, id1, id2);
    if (!target) {
        QMessageBox::warning(parent, "Line Not Found",
                             QString("No line connecting vertex %1 and %2 was found.")
                                 .arg(id1).arg(id2));
        return false;
    }

    scene->removeItem(target);
    delete target;
    return true;
}

int LineItem::countLines(QGraphicsScene *scene)
{
    if(!scene) return 0;

    int count = 0;
    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == LineItem::Type){
            ++count;
        }
    }

    return count;
}

int LineItem::deleteAllLines(QGraphicsScene *scene, QWidget *parent)
{
    if(!scene){
        QMessageBox::warning(parent, "No Canvas Loaded","Please create a canvas or open an image first.");
        return 0;
    }

    QList<LineItem*> lines;
    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == LineItem::Type){
            lines.append(static_cast<LineItem*>(it));
        }
    }

    auto reply = QMessageBox::question(parent, "Delete All Lines",QString("Delete all %1 lines?").arg(lines.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if(reply != QMessageBox::Yes) return 0;

    for(LineItem *L: lines){
        scene->removeItem(L);
        delete L;
    }

    return lines.size();
}

void LineItem::setNextId(int nextId)
{
    s_nextId = std::max(1, nextId);
}

LineItem::LineStyle LineItem::currentStyle()
{
    return g_lineStyle;
}

void LineItem::setCurrentStyle(const LineStyle &style)
{
    g_lineStyle = style;
}

void LineItem::applyCurrentStyle()
{
    LineStyle s = g_lineStyle;
    setVisible(s.visible);

    const int clampedOpacity = std::clamp(s.opacityPercent, 0, 100);
    const int alpha = qRound(clampedOpacity * 2.55);
    QColor penColor = s.color;
    penColor.setAlpha(alpha);

    setPen(QPen(penColor, s.widthPx));
}

void LineItem::applyStyleToScene(QGraphicsScene *scene)
{
    if (!scene) return;

    const auto items = scene->items();
    for (QGraphicsItem *it : items) {
        if (it && it->type() == LineItem::Type) {
            static_cast<LineItem*>(it)->applyCurrentStyle();
        }
    }
}

bool LineItem::showDisplaySettingDialog(QGraphicsScene *scene, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return false;
    }

    LineStyle style = LineItem::currentStyle();

    QDialog dialog(parent);
    dialog.setWindowTitle("Line Display Setting");

    // visibility
    QCheckBox *checkShowLines = new QCheckBox("Show lines", &dialog);
    checkShowLines->setChecked(style.visible);

    // width
    QSpinBox *spinWidth = new QSpinBox(&dialog);
    spinWidth->setRange(1, 20);
    spinWidth->setValue(style.widthPx);

    // opacity
    QSpinBox *spinOpacity = new QSpinBox(&dialog);
    spinOpacity->setRange(0, 100);
    spinOpacity->setValue(style.opacityPercent);

    // color preview + button
    QLabel *colorPreview = new QLabel(&dialog);
    colorPreview->setFixedSize(40, 20);
    colorPreview->setAutoFillBackground(true);

    auto setPreviewColor = [](QLabel *lbl, const QColor &c) {
        QPalette pal = lbl->palette();
        pal.setColor(QPalette::Window, c);
        lbl->setPalette(pal);
    };
    auto colorWithOpacity = [&](const QColor &c) {
        QColor withAlpha = c;
        const int alpha = qRound(std::clamp(spinOpacity->value(), 0, 100) * 2.55);
        withAlpha.setAlpha(alpha);
        return withAlpha;
    };
    setPreviewColor(colorPreview, colorWithOpacity(style.color));

    QPushButton *btnColor = new QPushButton("Choose...", &dialog);
    QObject::connect(btnColor, &QPushButton::clicked, [&]() {
        QColor c = QColorDialog::getColor(style.color, &dialog, "Choose Line Color");
        if (c.isValid()) {
            style.color = c;
            setPreviewColor(colorPreview, colorWithOpacity(c));
        }
    });

    // layout
    QFormLayout *form = new QFormLayout;
    form->addRow(checkShowLines);
    form->addRow("Line width (px):", spinWidth);
    form->addRow("Opacity (%):", spinOpacity);

    QHBoxLayout *colorRow = new QHBoxLayout;
    colorRow->addWidget(colorPreview);
    colorRow->addWidget(btnColor);
    form->addRow("Line color:", colorRow);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);

    auto refreshVisibilityUi = [&]() {
        bool enabled = checkShowLines->isChecked();
        spinWidth->setEnabled(enabled);
        spinOpacity->setEnabled(enabled);
        colorPreview->setEnabled(enabled);
        btnColor->setEnabled(enabled);
    };
    QObject::connect(checkShowLines, &QCheckBox::toggled, &dialog, refreshVisibilityUi);
    QObject::connect(spinOpacity, &QSpinBox::valueChanged, &dialog, [&]() {
        setPreviewColor(colorPreview, colorWithOpacity(style.color));
    });
    refreshVisibilityUi();

    if (dialog.exec() != QDialog::Accepted)
        return false;

    // save from UI
    style.visible = checkShowLines->isChecked();
    style.widthPx = spinWidth->value();
    style.opacityPercent = spinOpacity->value();

    // apply globally + update scene lines
    LineItem::setCurrentStyle(style);
    LineItem::applyStyleToScene(scene);

    return true;
}
