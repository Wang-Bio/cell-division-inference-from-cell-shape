#ifndef POLYGONITEM_H
#define POLYGONITEM_H

#include <QObject>
#include <QGraphicsPolygonItem>
#include <QVector>
#include <QColor>

class QGraphicsScene;
class QGraphicsView;
class QWidget;
class QGraphicsSimpleTextItem;

class VertexItem;
class LineItem;

class PolygonItem : public QObject, public QGraphicsPolygonItem
{
    Q_OBJECT
public:
    enum { Type = QGraphicsItem::UserType + 1003 };



    explicit PolygonItem(const QVector<VertexItem*> &verticesCCW, QGraphicsScene *sceneForEdges = nullptr, bool autoCreateMissingEdges = true, int id = -1);

    int type() const override { return Type; }
    int id() const {return m_id; }

// --- geometry features ---
    double areaSigned() const;   // signed area (CCW positive)
    double area() const;         // absolute area
    double perimeter() const;
    QPointF centroid() const;    // polygon centroid (area-weighted)
    QVector<int> vertexIds() const;
    void updateGeometry();       // rebuild QPolygonF from vertex positions
    const QVector<VertexItem*>& vertices() const{return m_vertices; }

// --- global style ---
    struct PolygonStyle {
        QColor fillColor = QColor(128, 128, 128); // default grey
        QColor outlineColor = QColor(0, 0, 0, 0);   // default: no outline
        int outlineWidthPx = 1;
        bool showOutline = false;
        int opacityPercent = 25;

        bool useRandomColor = false;
        bool showIdText = true;
        int idTextPointSize = 8;
        bool visible = true;
    };

    static PolygonStyle currentStyle();
    void applyCurrentStyle();
    static void setCurrentStyle(const PolygonStyle &style);
    static void applyStyleToScene(QGraphicsScene *scene);
    static bool showDisplaySettingDialog(QGraphicsScene *scene, QWidget *parent);

// Basic: add, delete, count,find
    static PolygonItem* createWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent, bool autoCreateMissingEdges = true);
    static bool deleteWithDialog(QGraphicsScene *scene, QWidget *parent);
    static int deleteAllPolygons(QGraphicsScene *scene, QWidget *parent);
    static int countPolygons(QGraphicsScene *scene);
    static void setNextId(int nextId);

    static PolygonItem* findPolygonById(QGraphicsScene *scene, int id);
    static PolygonItem* findPolygonByVertexIds(QGraphicsScene *scene, const QVector<int> &ids);
    static PolygonItem* findWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent);

signals:
    void selected(PolygonItem *self);
    void geometryChanged(PolygonItem *self);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private slots:
    void onVertexMoved(VertexItem *v, const QPointF &pos);
    void onVertexDestroyed(QObject *obj);

private:
    int m_id = -1;
    static int s_nextId;

    QVector<VertexItem*> m_vertices;  // CCW order
    QVector<LineItem*>   m_edges;     // optional (between consecutive vertices)
    QGraphicsSimpleTextItem *m_label = nullptr;
    QColor m_fillColor;

    void connectVertexSignals();
    void rebuildEdges(QGraphicsScene *sceneForEdges, bool autoCreateMissingEdges);

    static bool ensureCCW(QVector<VertexItem*> &verts); // reverse if needed
    static QVector<int> parseIdList(const QString &text, bool *ok);

    // helpers
    static PolygonItem* buildPolygonFromVertexIds(QGraphicsScene *scene, const QVector<int> &ids, QWidget *parent, bool autoCreateMissingEdges);
};

#endif // POLYGONITEM_H
