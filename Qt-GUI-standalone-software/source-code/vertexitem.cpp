#include "vertexitem.h"
#include "qlabel.h"
#include "qpushbutton.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSimpleTextItem>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPen>
#include <QBrush>
#include <QMessageBox>
#include <QRectF>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QComboBox>
#include <QColorDialog>


#include <algorithm>
#include <cmath>

static VertexItem::VertexStyle g_vertexStyle;


VertexItem::VertexItem(int id, const QPointF &pos, qreal radius) : m_id(id),m_radius(radius)
{
    //
    setRect(-m_radius, - m_radius, 2*m_radius, 2*m_radius);
    setPen(QPen(Qt::NoPen));
    setBrush(QBrush(QColor(0xaaffff)));

    setPos(pos);

    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
    setFlag(QGraphicsItem::ItemIsMovable, false);

    m_label = new QGraphicsSimpleTextItem(QString::number(m_id), this);
    applyCurrentStyle();
    updateLabelPos();

    setZValue(10);
}

void VertexItem::updateLabelPos(){
    if (!m_label) return;

    VertexStyle s = g_vertexStyle;

    // label left or right of circle
    if (s.indexOnleft) {
        // left
        m_label->setPos(-m_radius - 2.0 - m_label->boundingRect().width(),
                        -m_radius - 2.0);
    } else {
        // right (current behavior)
        m_label->setPos(m_radius + 2.0, -m_radius - 2.0);
    }
}

QVariant VertexItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if(change == QGraphicsItem::ItemSelectedHasChanged){
        const bool nowSelected = value.toBool();
        if(nowSelected){
            emit selected(this);
        }
    }

    if(change == QGraphicsItem::ItemPositionHasChanged){
        emit moved(this, pos());
    }

    return QGraphicsEllipseItem::itemChange(change, value);
}

void VertexItem::setMovableForScene(QGraphicsScene *scene, bool movable)
{
    if (!scene) return;

    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == VertexItem::Type) {
            it->setFlag(QGraphicsItem::ItemIsMovable, movable);
        }
    }
}

VertexItem* VertexItem::createWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent, int &nextVertexId)
{
//------ Check -------
    if(!scene || !view || scene->sceneRect().isNull() || scene->sceneRect().width()<=0 || scene->sceneRect().height()<=0)
    {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image before adding a vertex.");
        return nullptr;
    }

//------ Dialog -------
    QDialog dialog(parent);
    dialog.setWindowTitle("Add Vertex");

    QSpinBox *spinX = new QSpinBox(&dialog);
    QSpinBox *spinY = new QSpinBox(&dialog);

    spinX->setRange(-100000,100000);
    spinY->setRange(-100000,100000);

    QPointF centerScene = view->mapToScene(view->viewport()->rect().center());

    spinX->setValue(int(std::round(centerScene.x())));
    spinY->setValue(int(std::round(centerScene.y())));

    QFormLayout *form = new QFormLayout;
    form->addRow("X:",spinX);
    form->addRow("Y:",spinY);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

//Execute vertex adding

    if(dialog.exec() != QDialog::Accepted) return nullptr;

    const int x = spinX->value();
    const int y = spinY->value();

//Check position inside canvas
    if(!scene->sceneRect().contains(QPointF(x,y)))
    {
        QMessageBox::warning(parent, "Position Outside Canvas", QString("The vertex position (%1,%2) is outside the current canvas (%3-%4,%5-%6).").arg(x).arg(y).arg(scene->sceneRect().left()).arg(scene->sceneRect().right()).arg(scene->sceneRect().top()).arg(scene->sceneRect().bottom()));
        return nullptr;
    }

    QPointF pos(spinX->value(), spinY->value());

    auto *vertex = new VertexItem(nextVertexId++, pos);
    scene->addItem(vertex);

    return vertex;
}

VertexItem* VertexItem::findVertexById(QGraphicsScene* scene, int id)
{
    if(!scene) return nullptr;
    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == VertexItem::Type){
            auto *v = static_cast<VertexItem*>(it);
            if(v->id() == id) return v;
        }
    }

    return nullptr;
}

VertexItem* VertexItem::findVertexByPosition(QGraphicsScene *scene, const QPointF &p, double tol)
{
    if(!scene) return nullptr;

    VertexItem *best = nullptr;
    double bestDist2 = std::numeric_limits<double>::infinity();

    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == VertexItem::Type){
            auto *v = static_cast<VertexItem*>(it);
            QPointF vp = v->pos();
            double dx = vp.x() - p.x();
            double dy = vp.y() - p.y();
            double d2 = dx*dx + dy*dy;

            if(d2< bestDist2){
                bestDist2 = d2;
                best = v;
            }
        }
    }

    if(!best) return nullptr;
    if(tol<=0.0){
        //tol=0 means must match exactly
        return (bestDist2 == 0.0) ? best : nullptr;
    }

    return (bestDist2 <= tol*tol) ? best : nullptr;
}

bool VertexItem::deleteWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent){
// Basic check
    if(!view || !scene || scene->sceneRect().isNull() || scene->sceneRect().width() <=0 || scene->sceneRect().height() <=0 )
    {
        QMessageBox::warning(parent, "No Canvas or Image Loaded", "Please create a canvas or open an image before deleting a vertex.");
        return false;
    }

    //if no vertex exists
    bool hasVertex = false;
    for(QGraphicsItem *it : scene->items()){
        if(it && it->type() == VertexItem::Type){hasVertex = true; break;}
    }
    if(!hasVertex){
        QMessageBox::information(parent, "No Vertex", "There is no vertex to delete.");
        return false;
    }

    //---- Dialog UI----
    QDialog dialog(parent);
    dialog.setWindowTitle("Delete Vertex");

    auto *radioById = new QRadioButton("Delete by ID",&dialog);
    auto *radioByPos = new QRadioButton("Delete by Position (x,y)", &dialog);
    radioById->setChecked(true);

    auto *spinId = new QSpinBox(&dialog);
    spinId->setRange(0,1000000000);
    spinId->setValue(1);

    auto *spinX = new QSpinBox(&dialog);
    auto *spinY = new QSpinBox(&dialog);
    spinX->setRange(-100000,100000);
    spinY->setRange(-100000,100000);

    auto *spinTol = new QDoubleSpinBox(&dialog);
    spinTol->setRange(0.0,10000.0);
    spinTol->setDecimals(1);
    spinTol->setSingleStep(1.0);
    spinTol->setValue(5.0);

    auto updateEnabled = [&](){
        bool byId = radioById->isChecked();
        spinId->setEnabled(byId);

        spinX->setEnabled(!byId);
        spinY->setEnabled(!byId);
        spinTol->setEnabled(!byId);
    };

    QObject::connect(radioById, &QRadioButton::toggled, &dialog, updateEnabled);
    QObject::connect(radioByPos, &QRadioButton::toggled, &dialog, updateEnabled);
    updateEnabled();

//Layout
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(radioById);

    auto *formId = new QFormLayout;
    formId->addRow("ID:", spinId);
    layout->addLayout(formId);

    layout->addSpacing(8);
    layout->addWidget(radioByPos);

    auto *formPos = new QFormLayout;
    formPos->addRow("X:",spinX);
    formPos->addRow("Y:",spinY);
    formPos->addRow("Tolerence (px):", spinTol);
    layout->addLayout(formPos);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

//---- Run Dialog ----
    if(dialog.exec() != QDialog::Accepted) return false;

    VertexItem *target = nullptr;
    if(radioById->isChecked()){
        int id = spinId->value();
        target = VertexItem::findVertexById(scene,id);
        if(!target){
            QMessageBox::warning(parent, "Vertex Not Found", QString("No vertex with ID = %1 was found.").arg(id));
            return false;
        }
    } else{
        QPointF p(spinX->value(), spinY->value());
        double tol = spinTol->value();
        target = VertexItem::findVertexByPosition(scene, p, tol);
        if(!target){
            QMessageBox::warning(parent, "Vertex Not Found", QString("No vertex near (%1,%2) within tolerance %3 pixels").arg(p.x()).arg(p.y()).arg(tol));
            return false;
        }
    }

    //Delete it
    scene->removeItem(target);
    delete target;

    return true;
}


int VertexItem::deleteAllVertices(QGraphicsScene *scene, QWidget *parent)
{
    if(!scene){
        QMessageBox::warning(parent, "No Scene", "No canvas/image is loaded.");
        return 0;
    }

    QList<VertexItem*> vertices;
    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == VertexItem::Type){
            vertices.append(static_cast<VertexItem*>(it));
        }
    }

    if(vertices.isEmpty()){
        QMessageBox::information(parent, "No Vertex", "There are no vertices to delete.");
        return 0;
    }

    auto reply = QMessageBox::question(parent, "Delete All Vertices", QString("Delete all %1 vertices ?").arg(vertices.size()), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if(reply != QMessageBox::Yes) return 0;

    for(VertexItem *v: vertices){
        scene->removeItem(v);
        delete v;
    }

    return vertices.size();
}

int VertexItem::countVertices(QGraphicsScene *scene)
{
    if(!scene) return 0;

    int count = 0;
    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == VertexItem::Type) {
            ++count;
        }
    }

    return count;
}

VertexItem::VertexStyle VertexItem::currentStyle(){
    return g_vertexStyle;
}

void VertexItem::setCurrentStyle(const VertexStyle &style)
{
    g_vertexStyle = style;
}

void VertexItem::applyStyleToScene(QGraphicsScene *scene)
{
    if(!scene) return;

    const auto items = scene->items();
    for(QGraphicsItem *it : items){
        if(it && it->type() == VertexItem::Type){
            static_cast<VertexItem*>(it)->applyCurrentStyle();
        }
    }
}

void VertexItem::applyCurrentStyle(){
    VertexStyle s = g_vertexStyle;

    setVisible(s.visible);

    const int clampedOpacity = std::clamp(s.opacityPercent, 0, 100);
    const int alpha = static_cast<int>(std::round(clampedOpacity * 2.55));
    auto applyOpacity = [alpha](const QColor &c) {
        QColor withAlpha = c;
        withAlpha.setAlpha(alpha);
        return withAlpha;
    };

    m_radius = s.radiusPx;
    setRect(-m_radius, -m_radius, 2*m_radius, 2*m_radius);

    setBrush(QBrush(applyOpacity(s.fillColor)));

    if(m_label){
        m_label->setVisible(s.visible && s.showIndex);
        updateLabelPos();
    }
}

bool VertexItem::showDisplaySettingDialog(QGraphicsScene *scene, QWidget *parent)
{
    if(!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded","Please create a canvas or open an image first");
        return false;
    }

    VertexStyle style = VertexItem::currentStyle();

    QDialog dialog(parent);
    dialog.setWindowTitle("Vertex Display Setting");

    //visibility
    QCheckBox *checkShowVertices = new QCheckBox("Show vertices", &dialog);
    checkShowVertices->setChecked(style.visible);

    //radius
    QSpinBox *spinRadius = new QSpinBox(&dialog);
    spinRadius->setRange(1,50);
    spinRadius->setValue(style.radiusPx);

    //opacity
    QSpinBox *spinOpacity = new QSpinBox(&dialog);
    spinOpacity->setRange(0, 100);
    spinOpacity->setValue(style.opacityPercent);

    //show index
    QCheckBox *checkShowIndex = new QCheckBox("Show index text", &dialog);
    checkShowIndex->setChecked(style.showIndex);

    //show index position
    // --- index position ---
    QComboBox *comboIndexPos = new QComboBox(&dialog);
    comboIndexPos->addItem("Right");
    comboIndexPos->addItem("Left");
    comboIndexPos->setCurrentIndex(style.indexOnleft ? 1 : 0);

    // --- color preview + choose button ---
    QLabel *colorPreview = new QLabel(&dialog);
    colorPreview->setFixedSize(40, 20);
    colorPreview->setAutoFillBackground(true);

    auto setPreviewColor = [](QLabel *lbl, const QColor &c){
        QPalette pal = lbl->palette();
        pal.setColor(QPalette::Window, c);
        lbl->setPalette(pal);
    };
    auto colorWithOpacity = [&](const QColor &c) {
        QColor withAlpha = c;
        const int alpha = static_cast<int>(std::round(std::clamp(spinOpacity->value(), 0, 100) * 2.55));
        withAlpha.setAlpha(alpha);
        return withAlpha;
    };
    setPreviewColor(colorPreview, colorWithOpacity(style.fillColor));

    QPushButton *btnColor = new QPushButton("Choose...", &dialog);
    QObject::connect(btnColor, &QPushButton::clicked, [&](){
        QColor c = QColorDialog::getColor(style.fillColor, &dialog, "Choose Vertex Color");
        if (c.isValid()) {
            style.fillColor = c;
            setPreviewColor(colorPreview, colorWithOpacity(c));
        }
    });
    QObject::connect(spinOpacity, &QSpinBox::valueChanged, &dialog, [&]() {
        setPreviewColor(colorPreview, colorWithOpacity(style.fillColor));
    });

    // --- layout ---
    QFormLayout *form = new QFormLayout;
    form->addRow(checkShowVertices);
    form->addRow("Vertex radius (px):", spinRadius);
    form->addRow("Opacity (%):", spinOpacity);

    QHBoxLayout *colorRow = new QHBoxLayout;
    colorRow->addWidget(colorPreview);
    colorRow->addWidget(btnColor);
    form->addRow("Vertex color:", colorRow);

    form->addRow(checkShowIndex);
    form->addRow("Index position:", comboIndexPos);

    // enable/disable index position
    auto refreshIndexUI = [&](){
        bool indicesEnabled = checkShowIndex->isChecked() && checkShowVertices->isChecked();
        comboIndexPos->setEnabled(indicesEnabled);
    };
    QObject::connect(checkShowIndex, &QCheckBox::toggled, &dialog, refreshIndexUI);
    QObject::connect(checkShowVertices, &QCheckBox::toggled, &dialog, refreshIndexUI);
    refreshIndexUI();

    auto refreshVisibilityUI = [&](){
        bool enabled = checkShowVertices->isChecked();
        spinRadius->setEnabled(enabled);
        checkShowIndex->setEnabled(enabled);
        comboIndexPos->setEnabled(enabled && checkShowIndex->isChecked());
        spinOpacity->setEnabled(enabled);
        colorPreview->setEnabled(enabled);
        btnColor->setEnabled(enabled);
    };
    QObject::connect(checkShowVertices, &QCheckBox::toggled, &dialog, refreshVisibilityUI);
    refreshVisibilityUI();

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);

    // --- execute ---
    if (dialog.exec() != QDialog::Accepted)
        return false;

    // --- save style from UI ---
    style.visible = checkShowVertices->isChecked();
    style.radiusPx = spinRadius->value();
    style.showIndex = checkShowIndex->isChecked();
    style.indexOnleft = (comboIndexPos->currentIndex() == 1);
    style.opacityPercent = spinOpacity->value();

    // --- apply ---
    VertexItem::setCurrentStyle(style);
    VertexItem::applyStyleToScene(scene);

    return true;

}

VertexItem* VertexItem::findWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent){
    // Must have canvas/image
    if (!scene || scene->sceneRect().isNull() ||
        scene->sceneRect().width() <= 0 || scene->sceneRect().height() <= 0)
    {
        QMessageBox::warning(parent, "No Canvas Loaded",
                             "Please create a canvas or open an image before finding a vertex.");
        return nullptr;
    }

    // Must have at least one vertex
    bool hasVertex = false;
    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == VertexItem::Type) { hasVertex = true; break; }
    }
    if (!hasVertex) {
        QMessageBox::information(parent, "No Vertex", "There is no vertex to find.");
        return nullptr;
    }

    // ---- Dialog ----
    QDialog dialog(parent);
    dialog.setWindowTitle("Find Vertex");

    auto *radioById  = new QRadioButton("Find by ID", &dialog);
    auto *radioByPos = new QRadioButton("Find by Position (x, y)", &dialog);
    radioById->setChecked(true);

    auto *spinId = new QSpinBox(&dialog);
    spinId->setRange(0, 100000000);
    spinId->setValue(1);

    auto *spinX = new QSpinBox(&dialog);
    auto *spinY = new QSpinBox(&dialog);
    spinX->setRange(-100000, 100000);
    spinY->setRange(-100000, 100000);

    // Default position: view center
    if (view) {
        QPointF centerScene = view->mapToScene(view->viewport()->rect().center());
        spinX->setValue(int(std::round(centerScene.x())));
        spinY->setValue(int(std::round(centerScene.y())));
    }

    auto *spinTol = new QDoubleSpinBox(&dialog);
    spinTol->setRange(0.0, 10000.0);
    spinTol->setDecimals(1);
    spinTol->setSingleStep(1.0);
    spinTol->setValue(8.0); // default tolerance (pixels)

    // Enable/disable depending on mode
    auto updateEnabled = [&]() {
        bool byId = radioById->isChecked();
        spinId->setEnabled(byId);

        spinX->setEnabled(!byId);
        spinY->setEnabled(!byId);
        spinTol->setEnabled(!byId);
    };
    QObject::connect(radioById,  &QRadioButton::toggled, &dialog, updateEnabled);
    QObject::connect(radioByPos, &QRadioButton::toggled, &dialog, updateEnabled);
    updateEnabled();

    // Layout
    auto *form = new QFormLayout;
    form->addRow(radioById);
    form->addRow("ID:", spinId);
    form->addRow(radioByPos);
    form->addRow("X:", spinX);
    form->addRow("Y:", spinY);
    form->addRow("Tolerance (px):", spinTol);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    // ---- Find ----
    VertexItem *target = nullptr;

    if (radioById->isChecked()) {
        const int id = spinId->value();
        target = VertexItem::findVertexById(scene, id);
        if (!target) {
            QMessageBox::warning(parent, "Vertex Not Found",
                                 QString("No vertex with ID = %1 was found.").arg(id));
            return nullptr;
        }
    } else {
        const QPointF p(spinX->value(), spinY->value());
        const double tol = spinTol->value();
        target = VertexItem::findVertexByPosition(scene, p, tol);
        if (!target) {
            QMessageBox::warning(parent, "Vertex Not Found",
                                 QString("No vertex near (%1, %2) within tolerance %3 pixels.")
                                     .arg(p.x()).arg(p.y()).arg(tol));
            return nullptr;
        }
    }

    // ---- Select it ----
    scene->clearSelection();
    target->setSelected(true);

    // Optional: center view on it (nice UX)
    if (view) {
        view->centerOn(target);
    }

    return target;
}
