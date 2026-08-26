#ifndef NEIGHBORPAIR_H
#define NEIGHBORPAIR_H

#include "qgraphicsitem.h"
#include <QColor>
#include <QHash>
#include <QString>
#include <QVector>

class PolygonItem;
class VertexItem;
class GraphicsLineItem;
class QGraphicsScene;
class QWidget;

struct NeighborLineStyle{QColor color =QColor(255,85,0); int widthPx = 1;};

// Represents a relationship between two polygons that share two or more vertices.
class NeighborPair
{
public:
    struct EdgeInfo {
        VertexItem *v1 = nullptr;
        VertexItem *v2 = nullptr;
    };

    struct UnsharedVertices {
        QVector<VertexItem*> adjacentToShared; // directly connected to a shared vertex
        QVector<VertexItem*> others;           // not connected to any shared vertex
    };

    NeighborPair(PolygonItem *first, PolygonItem *second);

    bool isNeighbor() const;

    PolygonItem* first() const { return m_first; }
    PolygonItem* second() const { return m_second; }

    const QVector<VertexItem*>& sharedVertices() const { return m_sharedVertices; }
    const QVector<EdgeInfo>& sharedEdges() const { return m_sharedEdges; }

    const QVector<VertexItem*>& connectingVerticesFirst() const { return m_connectingVerticesFirst; }
    const QVector<VertexItem*>& connectingVerticesSecond() const { return m_connectingVerticesSecond; }

    const QVector<EdgeInfo>& connectingEdgesFirst() const { return m_connectingEdgesFirst; }
    const QVector<EdgeInfo>& connectingEdgesSecond() const { return m_connectingEdgesSecond; }

    const UnsharedVertices& unsharedFirst() const { return m_unsharedFirst; }
    const UnsharedVertices& unsharedSecond() const { return m_unsharedSecond; }

private:
    PolygonItem *m_first = nullptr;
    PolygonItem *m_second = nullptr;

    QVector<VertexItem*> m_sharedVertices;
    QVector<EdgeInfo> m_sharedEdges;

    QVector<VertexItem*> m_connectingVerticesFirst;
    QVector<VertexItem*> m_connectingVerticesSecond;

    QVector<EdgeInfo> m_connectingEdgesFirst;
    QVector<EdgeInfo> m_connectingEdgesSecond;

    UnsharedVertices m_unsharedFirst;
    UnsharedVertices m_unsharedSecond;

    void computeRelationships();
    static QVector<EdgeInfo> buildEdges(const QVector<VertexItem*> &vertices);
    static QString edgeKey(const EdgeInfo &edge);
    static void addIfMissing(QVector<VertexItem*> &vec, VertexItem *v);
};

class NeighborPairDisplay{
public:
    static NeighborLineStyle currentStyle();
    static void setCurrentStyle(const NeighborLineStyle &style);
    static void applyStyleToLines(const QVector<QGraphicsLineItem*> &lines);
    static bool displayEnabled();
    static void setDisplayEnabled(bool enabled);
    static void applyVisibilityToLines(const QVector<QGraphicsLineItem*> &lines, bool visible);
    static bool showDisplaySettingDialog(QGraphicsScene *scene, const QVector<QGraphicsLineItem*> &lines, QWidget *parent);
};

#endif // NEIGHBORPAIR_H
