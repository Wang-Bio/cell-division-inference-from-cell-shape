#include "neighborpair.h"

#include "polygonitem.h"
#include "vertexitem.h"

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QCheckBox>

#include <algorithm>


static NeighborLineStyle g_neighborLineStyle;
static bool g_neighborLinesVisible = true;

NeighborPair::NeighborPair(PolygonItem *first, PolygonItem *second)
    : m_first(first)
    , m_second(second)
{
    if (!m_first || !m_second || m_first == m_second)
        return;

    computeRelationships();
}

bool NeighborPair::isNeighbor() const
{
    return m_sharedVertices.size() >= 2;
}

void NeighborPair::computeRelationships()
{
    const QVector<VertexItem*> vertsA = m_first->vertices();
    const QVector<VertexItem*> vertsB = m_second->vertices();

    QHash<int, VertexItem*> mapA;
    for (VertexItem *v : vertsA) {
        if (v)
            mapA.insert(v->id(), v);
    }

    QHash<int, VertexItem*> mapB;
    for (VertexItem *v : vertsB) {
        if (v)
            mapB.insert(v->id(), v);
    }

    // Shared vertices (intersection of ids)
    for (auto it = mapA.constBegin(); it != mapA.constEnd(); ++it) {
        if (mapB.contains(it.key()))
            m_sharedVertices.push_back(it.value());
    }

    if (!isNeighbor())
        return;

    // Edge lists
    const QVector<EdgeInfo> edgesA = buildEdges(vertsA);
    const QVector<EdgeInfo> edgesB = buildEdges(vertsB);

    QSet<QString> edgeKeysB;
    for (const EdgeInfo &e : edgesB)
        edgeKeysB.insert(edgeKey(e));

    // Shared edges: present in both polygons and comprised of shared vertices
    for (const EdgeInfo &edge : edgesA) {
        if (!edge.v1 || !edge.v2)
            continue;

        const bool v1Shared = mapB.contains(edge.v1->id());
        const bool v2Shared = mapB.contains(edge.v2->id());
        if (v1Shared && v2Shared && edgeKeysB.contains(edgeKey(edge)))
            m_sharedEdges.push_back(edge);
    }

    // Connecting edges/vertices: edges with exactly one shared vertex
    auto classifyEdges = [&](const QVector<EdgeInfo> &edges, const QHash<int, VertexItem*> &sharedLookup, QVector<EdgeInfo> &connectingEdges, QVector<VertexItem*> &connectingVertices, UnsharedVertices &unsharedInfo)
    {
        for (const EdgeInfo &edge : edges) {
            const bool v1Shared = edge.v1 && sharedLookup.contains(edge.v1->id());
            const bool v2Shared = edge.v2 && sharedLookup.contains(edge.v2->id());

            if (v1Shared ^ v2Shared) {
                connectingEdges.push_back(edge);
                if (edge.v1 && !v1Shared) addIfMissing(connectingVertices, edge.v1);
                if (edge.v2 && !v2Shared) addIfMissing(connectingVertices, edge.v2);
            }
        }

        for (VertexItem *v : connectingVertices)
            addIfMissing(unsharedInfo.adjacentToShared, v);
    };

    classifyEdges(edgesA, mapB, m_connectingEdgesFirst, m_connectingVerticesFirst, m_unsharedFirst);
    classifyEdges(edgesB, mapA, m_connectingEdgesSecond, m_connectingVerticesSecond, m_unsharedSecond);

    auto gatherUnshared = [&](const QHash<int, VertexItem*> &ownMap,
                              const QHash<int, VertexItem*> &otherMap,
                              const QVector<VertexItem*> &adjacent,
                              UnsharedVertices &unsharedInfo) {
        for (auto it = ownMap.constBegin(); it != ownMap.constEnd(); ++it) {
            VertexItem *v = it.value();
            if (otherMap.contains(it.key()))
                continue;

            if (std::find(adjacent.begin(), adjacent.end(), v) != adjacent.end())
                continue; // already recorded as adjacent to shared

            addIfMissing(unsharedInfo.others, v);
        }
    };

    gatherUnshared(mapA, mapB, m_unsharedFirst.adjacentToShared, m_unsharedFirst);
    gatherUnshared(mapB, mapA, m_unsharedSecond.adjacentToShared, m_unsharedSecond);
}

QVector<NeighborPair::EdgeInfo> NeighborPair::buildEdges(const QVector<VertexItem*> &vertices)
{
    QVector<EdgeInfo> edges;
    if (vertices.size() < 2)
        return edges;

    edges.reserve(vertices.size());
    for (int i = 0; i < vertices.size(); ++i) {
        VertexItem *v1 = vertices[i];
        VertexItem *v2 = vertices[(i + 1) % vertices.size()];
        edges.push_back({v1, v2});
    }

    return edges;
}

QString NeighborPair::edgeKey(const EdgeInfo &edge)
{
    const int id1 = edge.v1 ? edge.v1->id() : -1;
    const int id2 = edge.v2 ? edge.v2->id() : -1;
    const int a = std::min(id1, id2);
    const int b = std::max(id1, id2);
    return QString("%1-%2").arg(a).arg(b);
}

void NeighborPair::addIfMissing(QVector<VertexItem*> &vec, VertexItem *v)
{
    if (!v)
        return;

    if (std::find(vec.begin(), vec.end(), v) == vec.end())
        vec.push_back(v);
}

NeighborLineStyle NeighborPairDisplay::currentStyle()
{
    return g_neighborLineStyle;
}

void NeighborPairDisplay::setCurrentStyle(const NeighborLineStyle &style)
{
    g_neighborLineStyle = style;
}

void NeighborPairDisplay::applyStyleToLines(const QVector<QGraphicsLineItem*> &lines)
{
    const NeighborLineStyle style = g_neighborLineStyle;
    for (QGraphicsLineItem *line : lines) {
        if (!line) continue;
        QPen pen(style.color, style.widthPx, Qt::DashLine);
        line->setPen(pen);
    }
}

bool NeighborPairDisplay::displayEnabled()
{
    return g_neighborLinesVisible;
}

void NeighborPairDisplay::setDisplayEnabled(bool enabled)
{
    g_neighborLinesVisible = enabled;
}

void NeighborPairDisplay::applyVisibilityToLines(const QVector<QGraphicsLineItem*> &lines, bool visible)
{
    for(QGraphicsLineItem *line : lines){
        if(!line) continue;
        line->setVisible(visible);
    }
}

bool NeighborPairDisplay::showDisplaySettingDialog(QGraphicsScene *scene, const QVector<QGraphicsLineItem*> &lines, QWidget *parent)
{
    if (!scene) {
        QMessageBox::warning(parent, "No Canvas Loaded", "Please create a canvas or open an image first.");
        return false;
    }

    NeighborLineStyle style = NeighborPairDisplay::currentStyle();
    bool linesVisible = NeighborPairDisplay::displayEnabled();

    QDialog dialog(parent);
    dialog.setWindowTitle("Neighbor Display Setting");

    QCheckBox *displayToggle = new QCheckBox("Display neighbor lines", &dialog);
    displayToggle->setChecked(linesVisible);

    QSpinBox *spinWidth = new QSpinBox(&dialog);
    spinWidth->setRange(1, 20);
    spinWidth->setValue(style.widthPx);

    QLabel *colorPreview = new QLabel(&dialog);
    colorPreview->setFixedSize(40, 20);
    colorPreview->setAutoFillBackground(true);

    auto setPreviewColor = [](QLabel *lbl, const QColor &c) {
        QPalette pal = lbl->palette();
        pal.setColor(QPalette::Window, c);
        lbl->setPalette(pal);
    };
    setPreviewColor(colorPreview, style.color);

    QPushButton *btnColor = new QPushButton("Choose...", &dialog);
    QObject::connect(btnColor, &QPushButton::clicked, [&]() {
        QColor c = QColorDialog::getColor(style.color, &dialog, "Choose Neighbor Line Color");
        if (c.isValid()) {
            style.color = c;
            setPreviewColor(colorPreview, c);
        }
    });

    QFormLayout *form = new QFormLayout;
    form->addRow("Line width (px):", spinWidth);

    QHBoxLayout *colorRow = new QHBoxLayout;
    colorRow->addWidget(colorPreview);
    colorRow->addWidget(btnColor);
    form->addRow("Line color:", colorRow);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->addWidget(displayToggle);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    style.widthPx = spinWidth->value();
    linesVisible = displayToggle->isChecked();

    NeighborPairDisplay::setCurrentStyle(style);
    NeighborPairDisplay::applyStyleToLines(lines);
    NeighborPairDisplay::setDisplayEnabled(linesVisible);
    NeighborPairDisplay::applyVisibilityToLines(lines, linesVisible);
    return true;
}
