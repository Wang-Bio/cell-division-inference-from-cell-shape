#ifndef VERTEXITEM_H
#define VERTEXITEM_H

#include <QObject>
#include <QGraphicsEllipseItem>
#include <QPointF>
#include <QColor>

class QGraphicsView;
class QGraphicsScene;

class QGraphicsSimpleTextItem;

class VertexItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT


public:
    VertexItem(int id, const QPointF &pos, qreal radius = 2.0);

    int id() const {return m_id;}
    int type() const override { return Type; }

    enum { Type = QGraphicsItem::UserType + 1001 };

//Vertex Style
    struct VertexStyle{
        QColor fillColor = QColor(0xaaffff);
        int radiusPx = 2;
        bool showIndex = false;
        bool indexOnleft = false;
        int opacityPercent = 100;
        bool visible = true;
    };
    static VertexStyle currentStyle();
    static void setCurrentStyle(const VertexStyle &style);
    static void applyStyleToScene(QGraphicsScene *scene);
    void applyCurrentStyle();
    static bool showDisplaySettingDialog(QGraphicsScene *scene, QWidget *parent);

//Basics: create, delete, find, count, find
    static VertexItem* createWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent, int &nextVertexId);
    static bool deleteWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent);
    static int deleteAllVertices(QGraphicsScene *scene, QWidget *parent);

    static VertexItem* findVertexById(QGraphicsScene *scene, int id);
    static VertexItem* findVertexByPosition(QGraphicsScene *scene, const QPointF &p, double tol);
    static VertexItem* findWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent);

    static int countVertices(QGraphicsScene *scene);

    static void setMovableForScene(QGraphicsScene *scene, bool movable);

signals:
    void selected(VertexItem *self);
    void moved(VertexItem *self, const QPointF &pos);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    int m_id;
    qreal m_radius;
    QGraphicsSimpleTextItem *m_label = nullptr;

    void updateLabelPos();
};

#endif // VERTEXITEM_H
