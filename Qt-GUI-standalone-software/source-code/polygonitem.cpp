#include "polygonitem.h"
#include "qcheckbox.h"
#include "qlabel.h"
#include "qpushbutton.h"
#include "qradiobutton.h"
#include "qregularexpression.h"
#include "qspinbox.h"
#include "vertexitem.h"
#include "lineitem.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPen>
#include <QBrush>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QtMath>
#include <QRandomGenerator>
#include <QColorDialog>
#include <limits>
#include <algorithm>

// global style storage
static PolygonItem::PolygonStyle g_polyStyle;
int PolygonItem::s_nextId = 0;

PolygonItem::PolygonStyle PolygonItem::currentStyle() { return g_polyStyle; }
void PolygonItem::setCurrentStyle(const PolygonStyle &style) { g_polyStyle = style; }

void PolygonItem::applyStyleToScene(QGraphicsScene *scene)
{
    if (!scene) return;
    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == PolygonItem::Type) {
            static_cast<PolygonItem*>(it)->applyCurrentStyle();
        }
    }
}

PolygonItem::PolygonItem(const QVector<VertexItem*> &verticesCCW, QGraphicsScene *sceneForEdges, bool autoCreateMissingEdges, int id)
    : m_vertices(verticesCCW)
{
    if(id >= 0){
        m_id = id;
        s_nextId = std::max(s_nextId, id + 1);
    }else{
        m_id = s_nextId++;
    }
    setFlags(QGraphicsItem::ItemIsSelectable);
    setZValue(1); // polygons below lines and vertices

    // initialize fill color (may be random depending on global style)
    PolygonStyle s = g_polyStyle;
    if (s.useRandomColor) {
        QColor random = QColor::fromRgb(QRandomGenerator::global()->generate());
        random.setAlpha(s.fillColor.alpha());
        m_fillColor = random;
    } else {
        m_fillColor = s.fillColor;
    }

    m_label = new QGraphicsSimpleTextItem(QString::number(m_id), this);
    m_label->setZValue(20);

    applyCurrentStyle();

    // Ensure CCW (if user passed CW, we reverse)
    QVector<VertexItem*> tmp = m_vertices;
    if (!ensureCCW(tmp))
        m_vertices = tmp;

    connectVertexSignals();
    updateGeometry();

    // edges are optional
    rebuildEdges(sceneForEdges, autoCreateMissingEdges);
}

void PolygonItem::setNextId(int nextId)
{
    s_nextId = std::max(0, nextId);
}

void PolygonItem::applyCurrentStyle()
{
    PolygonStyle s = g_polyStyle;

    setVisible(s.visible);

    const int clampedOpacity = std::clamp(s.opacityPercent, 0, 100);
    const int alpha = qRound(clampedOpacity * 2.55);
    const auto applyOpacity = [alpha](const QColor &c) {
        QColor withAlpha = c;
        withAlpha.setAlpha(alpha);
        return withAlpha;
    };

    if (s.useRandomColor) {
        QColor random = QColor::fromRgb(QRandomGenerator::global()->generate());
        m_fillColor = applyOpacity(random);
    } else {
        m_fillColor = applyOpacity(s.fillColor);
    }

    setBrush(QBrush(m_fillColor));

    if (s.showOutline) {
        setPen(QPen(applyOpacity(s.outlineColor), s.outlineWidthPx));
    } else {
        setPen(Qt::NoPen);
    }

    if (!m_label) {
        m_label = new QGraphicsSimpleTextItem(QString::number(m_id), this);
        m_label->setZValue(12);
    }

    QFont f = m_label->font();
    f.setPointSize(s.idTextPointSize);
    m_label->setFont(f);
    m_label->setBrush(QBrush(Qt::white));
    m_label->setVisible(s.visible && s.showIdText);
    QPointF c = centroid();
    QRectF r = m_label->boundingRect();
    m_label->setPos(c.x() - r.width() / 2.0, c.y() - r.height() / 2.0);
}

QVector<int> PolygonItem::vertexIds() const
{
    QVector<int> ids;
    ids.reserve(m_vertices.size());
    for (auto *v : m_vertices) ids.push_back(v ? v->id() : -1);
    return ids;
}

void PolygonItem::connectVertexSignals()
{
    for (auto *v : m_vertices) {
        if (!v) continue;
        connect(v, &VertexItem::moved, this, &PolygonItem::onVertexMoved);
        connect(v, &QObject::destroyed, this, &PolygonItem::onVertexDestroyed);
    }
}

void PolygonItem::updateGeometry()
{
    QPolygonF poly;
    poly.reserve(m_vertices.size());
    for (auto *v : m_vertices) {
        if (v) poly << v->pos();
    }
    setPolygon(poly);

    if (isSelected()) emit geometryChanged(this);

    if (m_label) {
        QPointF c = centroid();
        QRectF r = m_label->boundingRect();
        m_label->setPos(c.x() - r.width() / 2.0, c.y() - r.height() / 2.0);
    }
}

double PolygonItem::areaSigned() const
{
    const QPolygonF poly = polygon();
    const int n = poly.size();
    if (n < 3) return 0.0;

    double a = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF &p = poly[i];
        const QPointF &q = poly[(i + 1) % n];
        a += p.x() * q.y() - q.x() * p.y();
    }
    return 0.5 * a;
}

double PolygonItem::area() const
{
    return std::abs(areaSigned());
}

double PolygonItem::perimeter() const
{
    const QPolygonF poly = polygon();
    const int n = poly.size();
    if (n < 2) return 0.0;

    double per = 0.0;
    for (int i = 0; i < n; ++i) {
        per += QLineF(poly[i], poly[(i + 1) % n]).length();
    }
    return per;
}

QPointF PolygonItem::centroid() const
{
    const QPolygonF poly = polygon();
    const int n = poly.size();
    if (n < 3) return QPointF(0, 0);

    double A = areaSigned();
    if (std::abs(A) < 1e-12) {
        // fallback: average of points
        QPointF c(0, 0);
        for (auto &p : poly) c += p;
        return c / double(n);
    }

    double cx = 0.0, cy = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF &p = poly[i];
        const QPointF &q = poly[(i + 1) % n];
        double cross = p.x() * q.y() - q.x() * p.y();
        cx += (p.x() + q.x()) * cross;
        cy += (p.y() + q.y()) * cross;
    }
    cx /= (6.0 * A);
    cy /= (6.0 * A);
    return QPointF(cx, cy);
}

QVariant PolygonItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        if (value.toBool()) emit selected(this);
    }
    return QGraphicsPolygonItem::itemChange(change, value);
}

void PolygonItem::onVertexMoved(VertexItem *, const QPointF &)
{
    updateGeometry();
}

void PolygonItem::onVertexDestroyed(QObject *)
{
    // If any vertex disappears, polygon is no longer valid -> remove self.
    if (scene()) scene()->removeItem(this);
    delete this;
}

bool PolygonItem::ensureCCW(QVector<VertexItem*> &verts)
{
    // compute signed area from their positions
    if (verts.size() < 3) return false;
    double a = 0.0;
    for (int i = 0; i < verts.size(); ++i) {
        QPointF p = verts[i]->pos();
        QPointF q = verts[(i + 1) % verts.size()]->pos();
        a += p.x() * q.y() - q.x() * p.y();
    }
    // if clockwise, reverse to make CCW
    if (a < 0.0) {
        std::reverse(verts.begin(), verts.end());
    }
    return true;
}

void PolygonItem::rebuildEdges(QGraphicsScene *sceneForEdges, bool autoCreateMissingEdges)
{
    m_edges.clear();
    if (!sceneForEdges) return;
    if (m_vertices.size() < 2) return;

    for (int i = 0; i < m_vertices.size(); ++i) {
        VertexItem *a = m_vertices[i];
        VertexItem *b = m_vertices[(i + 1) % m_vertices.size()];
        if (!a || !b) continue;

        LineItem *edge = LineItem::findLineByVertexIds(sceneForEdges, a->id(), b->id());
        if (!edge && autoCreateMissingEdges) {
            edge = new LineItem(a, b);
            sceneForEdges->addItem(edge);
            edge->applyCurrentStyle(); // in case you have global line style

        }
        if (edge) m_edges.push_back(edge);
    }
}

QVector<int> PolygonItem::parseIdList(const QString &text, bool *ok)
{
    QVector<int> ids;
    *ok = false;

    QString t = text;
    t.replace(",", " ");
    t.replace(";", " ");
    const QStringList parts = t.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    if (parts.size() < 3) return ids;

    ids.reserve(parts.size());
    for (const QString &p : parts) {
        bool oneOk = false;
        int id = p.toInt(&oneOk);
        if (!oneOk) return {};
        ids.push_back(id);
    }

    // optional: ensure unique
    QSet<int> s;
    for (int id : ids) {
        if (s.contains(id)) return {};
        s.insert(id);
    }

    *ok = true;
    return ids;
}

PolygonItem* PolygonItem::buildPolygonFromVertexIds(QGraphicsScene *scene, const QVector<int> &ids, QWidget *parent, bool autoCreateMissingEdges)
{
    QVector<VertexItem*> verts;
    verts.reserve(ids.size());

    for (int id : ids) {
        VertexItem *v = VertexItem::findVertexById(scene, id);
        if (!v) {
            QMessageBox::warning(parent, "Vertex Not Found",
                                 QString("Cannot find vertex with ID = %1").arg(id));
            return nullptr;
        }
        verts.push_back(v);
    }

    ensureCCW(verts);

    auto *poly = new PolygonItem(verts, scene, autoCreateMissingEdges);
    scene->addItem(poly);

    scene->clearSelection();
    poly->setSelected(true);

    return poly;
}

PolygonItem* PolygonItem::createWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent, bool autoCreateMissingEdges)
{
    if (!scene || scene->sceneRect().isNull() ||
        scene->sceneRect().width() <= 0 || scene->sceneRect().height() <= 0)
    {
        QMessageBox::warning(parent, "No Canvas Loaded",
                             "Please create a canvas or open an image before creating a polygon.");
        return nullptr;
    }

    // dialog: list of vertex IDs in order
    QDialog dialog(parent);
    dialog.setWindowTitle("Add Polygon (vertex IDs in CCW order)");

    QLineEdit *editIds = new QLineEdit(&dialog);
    editIds->setPlaceholderText("Example: 1, 2, 3, 4");

    // nice default: if user selected vertices, prefill IDs in selection order is unknown,
    // so we leave blank. (Can add later.)

    QFormLayout *form = new QFormLayout;
    form->addRow("Vertex IDs:", editIds);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    bool ok = false;
    QVector<int> ids = parseIdList(editIds->text(), &ok);
    if (!ok) {
        QMessageBox::warning(parent, "Invalid Input",
                             "Please enter at least 3 UNIQUE integer IDs, separated by commas or spaces.");
        return nullptr;
    }

    auto *poly = buildPolygonFromVertexIds(scene, ids, parent, autoCreateMissingEdges);

    // optional: center view
    if (poly && view) view->centerOn(poly->centroid());

    return poly;
}

PolygonItem* PolygonItem::findPolygonByVertexIds(QGraphicsScene *scene, const QVector<int> &ids)
{
    if (!scene || ids.isEmpty()) return nullptr;

    QSet<int> target(ids.begin(), ids.end());
    const int targetSize = ids.size();

    for (QGraphicsItem *it : scene->items()) {
        if (!it || it->type() != PolygonItem::Type) continue;

        auto *poly = static_cast<PolygonItem*>(it);
        const QVector<int> polyIds = poly->vertexIds();

        if (polyIds.size() != targetSize)
            continue;

        QSet<int> polySet(polyIds.begin(), polyIds.end());
        if (polySet == target)
            return poly;
    }

    return nullptr;
}

PolygonItem* PolygonItem::findPolygonById(QGraphicsScene *scene, int id)
{
    if (!scene) return nullptr;

    for (QGraphicsItem *it : scene->items()) {
        if (!it || it->type() != PolygonItem::Type) continue;

        auto *poly = static_cast<PolygonItem*>(it);
        if (poly->id() == id)
            return poly;
    }

    return nullptr;
}

bool PolygonItem::deleteWithDialog(QGraphicsScene *scene, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return false;
    }

    const int polyCount = PolygonItem::countPolygons(scene);
    if (polyCount == 0) {
        QMessageBox::information(parent, "No Polygon", "There is no polygon to delete.");
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Delete Polygon");

    auto *radioByVertices = new QRadioButton("Delete by vertex IDs", &dialog);
    auto *radioByPolygonId = new QRadioButton("Delete by polygon ID", &dialog);
    radioByVertices->setChecked(true);

    QLineEdit *editIds = new QLineEdit(&dialog);
    editIds->setPlaceholderText("Example: 1, 2, 3");

    QSpinBox *spinPolyId = new QSpinBox(&dialog);
    spinPolyId->setRange(1, std::numeric_limits<int>::max());
    spinPolyId->setValue(1);

    auto updateEnabled = [&]() {
        const bool byVertices = radioByVertices->isChecked();
        editIds->setEnabled(byVertices);
        spinPolyId->setEnabled(!byVertices);
    };

    QObject::connect(radioByVertices, &QRadioButton::toggled, &dialog, updateEnabled);
    QObject::connect(radioByPolygonId, &QRadioButton::toggled, &dialog, updateEnabled);
    updateEnabled();

    QFormLayout *formVertices = new QFormLayout;
    formVertices->addRow("Vertex IDs:", editIds);

    QFormLayout *formPolyId = new QFormLayout;
    formPolyId->addRow("Polygon ID:", spinPolyId);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(radioByVertices);
    layout->addLayout(formVertices);
    layout->addSpacing(8);
    layout->addWidget(radioByPolygonId);
    layout->addLayout(formPolyId);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    PolygonItem *target = nullptr;

    if (radioByVertices->isChecked()) {
        bool ok = false;
        QVector<int> ids = parseIdList(editIds->text(), &ok);
        if (!ok) {
            QMessageBox::warning(parent, "Invalid Input",
                                 "Please enter at least 3 UNIQUE integer IDs separated by commas or spaces.");
            return false;
        }

        target = PolygonItem::findPolygonByVertexIds(scene, ids);
        if (!target) {
            QMessageBox::warning(parent, "Polygon Not Found", "No polygon matches the provided vertex IDs.");
            return false;
        }
    } else {
        const int polyId = spinPolyId->value();
        target = PolygonItem::findPolygonById(scene, polyId);
        if (!target) {
            QMessageBox::warning(parent, "Polygon Not Found", QString("No polygon with ID %1 was found.").arg(polyId));
            return false;
        }
    }

    scene->removeItem(target);
    delete target;

    return true;
}

int PolygonItem::deleteAllPolygons(QGraphicsScene *scene, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return 0;
    }

    QList<PolygonItem*> polys;
    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == PolygonItem::Type) {
            polys.append(static_cast<PolygonItem*>(it));
        }
    }

    if (polys.isEmpty()) {
        QMessageBox::information(parent, "No Polygon", "There are no polygons to delete.");
        return 0;
    }

    auto reply = QMessageBox::question(parent, "Delete All Polygons",
                                       QString("Delete all %1 polygons?").arg(polys.size()),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return 0;

    for (PolygonItem *poly : polys) {
        scene->removeItem(poly);
        delete poly;
    }

    return polys.size();
}

int PolygonItem::countPolygons(QGraphicsScene *scene)
{
    if (!scene) return 0;

    int count = 0;
    for (QGraphicsItem *it : scene->items()) {
        if (it && it->type() == PolygonItem::Type) {
            ++count;
        }
    }
    return count;
}

PolygonItem* PolygonItem::findWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent)
{
    if (!scene || scene->sceneRect().isNull() ||
        scene->sceneRect().width() <= 0 || scene->sceneRect().height() <= 0)
    {
        QMessageBox::warning(parent, "No Canvas Loaded",
                             "Please create a canvas or open an image before finding a polygon.");
        return nullptr;
    }

    const int polyCount = PolygonItem::countPolygons(scene);
    if (polyCount == 0) {
        QMessageBox::information(parent, "No Polygon", "There is no polygon to find.");
        return nullptr;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Find Polygon");

    auto *radioByVertices = new QRadioButton("Find by vertex IDs", &dialog);
    auto *radioByPolygonId = new QRadioButton("Find by polygon ID", &dialog);
    radioByVertices->setChecked(true);

    QLineEdit *editIds = new QLineEdit(&dialog);
    editIds->setPlaceholderText("Example: 1, 2, 3");

    QSpinBox *spinPolyId = new QSpinBox(&dialog);
    spinPolyId->setRange(1, std::numeric_limits<int>::max());
    spinPolyId->setValue(1);

    auto updateEnabled = [&]() {
        const bool byVertices = radioByVertices->isChecked();
        editIds->setEnabled(byVertices);
        spinPolyId->setEnabled(!byVertices);
    };

    QObject::connect(radioByVertices, &QRadioButton::toggled, &dialog, updateEnabled);
    QObject::connect(radioByPolygonId, &QRadioButton::toggled, &dialog, updateEnabled);
    updateEnabled();

    QFormLayout *formVertices = new QFormLayout;
    formVertices->addRow("Vertex IDs:", editIds);

    QFormLayout *formPolyId = new QFormLayout;
    formPolyId->addRow("Polygon ID:", spinPolyId);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(radioByVertices);
    layout->addLayout(formVertices);
    layout->addSpacing(8);
    layout->addWidget(radioByPolygonId);
    layout->addLayout(formPolyId);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    PolygonItem *target = nullptr;

    if (radioByVertices->isChecked()) {
        bool ok = false;
        QVector<int> ids = parseIdList(editIds->text(), &ok);
        if (!ok) {
            QMessageBox::warning(parent, "Invalid Input",
                                 "Please enter at least 3 UNIQUE integer IDs separated by commas or spaces.");
            return nullptr;
        }

        target = PolygonItem::findPolygonByVertexIds(scene, ids);
        if (!target) {
            QMessageBox::warning(parent, "Polygon Not Found",
                                 "No polygon matches the provided vertex IDs.");
            return nullptr;
        }
    } else {
        const int polyId = spinPolyId->value();
        target = PolygonItem::findPolygonById(scene, polyId);
        if (!target) {
            QMessageBox::warning(parent, "Polygon Not Found",
                                 QString("No polygon with ID %1 was found.").arg(polyId));
            return nullptr;
        }
    }

    scene->clearSelection();
    target->setSelected(true);

    if (view) {
        view->centerOn(target->centroid());
    }

    return target;
}


bool PolygonItem::showDisplaySettingDialog(QGraphicsScene *scene, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return false;
    }

    PolygonStyle style = PolygonItem::currentStyle();

    QDialog dialog(parent);
    dialog.setWindowTitle("Polygon Display Setting");

    QCheckBox *checkShowPolygons = new QCheckBox("Show polygons", &dialog);
    checkShowPolygons->setChecked(style.visible);

    // --- Fill color options ---
    auto *radioUniform = new QRadioButton("Use uniform color", &dialog);
    auto *radioRandom  = new QRadioButton("Use random color per polygon", &dialog);
    radioUniform->setChecked(!style.useRandomColor);
    radioRandom->setChecked(style.useRandomColor);

    QLabel *colorPreview = new QLabel(&dialog);
    colorPreview->setFixedSize(40, 20);
    colorPreview->setAutoFillBackground(true);

    QSpinBox *spinOpacity = new QSpinBox(&dialog);
    spinOpacity->setRange(0, 100);
    spinOpacity->setValue(style.opacityPercent);

    auto setPreviewColor = [](QLabel *lbl, const QColor &c){
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
    setPreviewColor(colorPreview, colorWithOpacity(style.fillColor));

    QPushButton *btnChooseColor = new QPushButton("Choose...", &dialog);
    QObject::connect(btnChooseColor, &QPushButton::clicked, [&]() {
        QColor c = QColorDialog::getColor(style.fillColor, &dialog, "Choose Polygon Color");
        if (c.isValid()) {
            style.fillColor = c;
            setPreviewColor(colorPreview, colorWithOpacity(c));
        }
    });

    auto refreshFillUi = [&]() {
        bool uniform = radioUniform->isChecked() && checkShowPolygons->isChecked();
        colorPreview->setEnabled(uniform);
        btnChooseColor->setEnabled(uniform);
    };
    QObject::connect(radioUniform, &QRadioButton::toggled, &dialog, refreshFillUi);
    QObject::connect(radioRandom, &QRadioButton::toggled, &dialog, refreshFillUi);
    QObject::connect(spinOpacity, &QSpinBox::valueChanged, &dialog, [&]() {
        setPreviewColor(colorPreview, colorWithOpacity(style.fillColor));
    });
    refreshFillUi();

    // --- Outline options ---
    QCheckBox *checkOutline = new QCheckBox("Show outline", &dialog);
    checkOutline->setChecked(style.showOutline);

    QSpinBox *spinOutlineWidth = new QSpinBox(&dialog);
    spinOutlineWidth->setRange(1, 20);
    spinOutlineWidth->setValue(style.outlineWidthPx);

    QLabel *outlinePreview = new QLabel(&dialog);
    outlinePreview->setFixedSize(40, 20);
    outlinePreview->setAutoFillBackground(true);
    setPreviewColor(outlinePreview, colorWithOpacity(style.outlineColor));
    QObject::connect(spinOpacity, &QSpinBox::valueChanged, &dialog, [&]() {
        setPreviewColor(outlinePreview, colorWithOpacity(style.outlineColor));
    });

    QPushButton *btnOutlineColor = new QPushButton("Choose...", &dialog);
    QObject::connect(btnOutlineColor, &QPushButton::clicked, [&]() {
        QColor c = QColorDialog::getColor(style.outlineColor, &dialog, "Choose Outline Color");
        if (c.isValid()) {
            style.outlineColor = c;
            setPreviewColor(outlinePreview, colorWithOpacity(c));
        }
    });

    auto refreshOutlineUi = [&]() {
        bool enabled = checkOutline->isChecked() && checkShowPolygons->isChecked();
        spinOutlineWidth->setEnabled(enabled);
        outlinePreview->setEnabled(enabled);
        btnOutlineColor->setEnabled(enabled);
    };
    QObject::connect(checkOutline, &QCheckBox::toggled, &dialog, refreshOutlineUi);
    refreshOutlineUi();

    // --- Cell ID text options ---
    QCheckBox *checkShowId = new QCheckBox("Show Cell ID", &dialog);
    checkShowId->setChecked(style.showIdText);

    QSpinBox *spinIdSize = new QSpinBox(&dialog);
    spinIdSize->setRange(6, 72);
    spinIdSize->setValue(style.idTextPointSize);

    auto refreshIdUi = [&]() {
        spinIdSize->setEnabled(checkShowId->isChecked() && checkShowPolygons->isChecked());
    };
    QObject::connect(checkShowId, &QCheckBox::toggled, &dialog, refreshIdUi);
    refreshIdUi();

    auto refreshVisibilityUi = [&]() {
        bool show = checkShowPolygons->isChecked();
        radioUniform->setEnabled(show);
        radioRandom->setEnabled(show);
        checkOutline->setEnabled(show);
        checkShowId->setEnabled(show);
        spinOpacity->setEnabled(show);
        colorPreview->setEnabled(show && radioUniform->isChecked());
        btnChooseColor->setEnabled(show && radioUniform->isChecked());
        refreshOutlineUi();
        refreshIdUi();
    };
    QObject::connect(checkShowPolygons, &QCheckBox::toggled, &dialog, refreshVisibilityUi);
    refreshVisibilityUi();

    // --- Layout ---
    QFormLayout *form = new QFormLayout;
    form->addRow(checkShowPolygons);
    form->addRow(radioUniform);
    QHBoxLayout *fillRow = new QHBoxLayout;
    fillRow->addWidget(colorPreview);
    fillRow->addWidget(btnChooseColor);
    form->addRow("Uniform color:", fillRow);
    form->addRow(radioRandom);
    form->addRow("Fill opacity (%):", spinOpacity);

    form->addRow(checkOutline);
    QHBoxLayout *outlineRow = new QHBoxLayout;
    outlineRow->addWidget(outlinePreview);
    outlineRow->addWidget(btnOutlineColor);
    form->addRow("Outline color:", outlineRow);
    form->addRow("Outline width (px):", spinOutlineWidth);

    form->addRow(checkShowId);
    form->addRow("Cell ID text size:", spinIdSize);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    // Save
    style.visible = checkShowPolygons->isChecked();
    style.useRandomColor = radioRandom->isChecked();
    style.showOutline = checkOutline->isChecked();
    style.outlineWidthPx = spinOutlineWidth->value();
    style.opacityPercent = spinOpacity->value();
    style.showIdText = checkShowId->isChecked();
    style.idTextPointSize = spinIdSize->value();

    PolygonItem::setCurrentStyle(style);
    PolygonItem::applyStyleToScene(scene);
    return true;
}
