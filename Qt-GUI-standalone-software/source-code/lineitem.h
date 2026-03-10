#ifndef LINEITEM_H
#define LINEITEM_H

#include <QObject>
#include <QGraphicsLineItem>
#include <QGraphicsView>

class VertexItem;
class QGraphicsScene;
class QWidget;

class LineItem : public QObject, public QGraphicsLineItem
{
    Q_OBJECT
public:
    enum{Type = QGraphicsItem::UserType + 1002};

    LineItem(VertexItem *v1, VertexItem *v2, int id = -1);
    int type() const override{return Type;}

    int id() const {return m_id;}
    int v1Id() const;
    int v2Id() const;

    double length() const;
    double angle0to180() const;
    double angle0to90() const;
    QPointF meanPos() const;

//Basic function: add, delete, find, count
    void updateGeometry();
    static LineItem* createWithDialog(QGraphicsScene *scene, QWidget *parent);
    static bool deleteWithDialog(QGraphicsScene *scene, QWidget *parent);
    static int deleteAllLines(QGraphicsScene *scene, QWidget *parent);

    static LineItem* findLineByVertexIds(QGraphicsScene *scene, int id1, int id2);
    static LineItem* findLineById(QGraphicsScene *scene, int id);
    static LineItem* findWithDialog(QGraphicsScene *scene, QGraphicsView *view, QWidget *parent);
    static int countLines(QGraphicsScene *scene);

    static void setNextId(int nextId);

//Style and Display
    struct LineStyle{QColor color = Qt::gray; int widthPx = 2; int opacityPercent = 100; bool visible = true;};

    static LineStyle currentStyle();
    static void setCurrentStyle(const LineStyle &style);
    static void applyStyleToScene(QGraphicsScene *scene);
    void applyCurrentStyle();
    static bool showDisplaySettingDialog(QGraphicsScene *scene, QWidget *parent);

signals:
    void selected(LineItem *self);
    void geometryChanged(LineItem *self);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private slots:
    void onVertexMoved(VertexItem *v, const QPointF &pos);
    void onVertexDestroyed(QObject *obj);

private:
    int m_id = -1;
    static int s_nextId;

    VertexItem *m_v1 = nullptr;
    VertexItem *m_v2 = nullptr;
};

#endif // LINEITEM_H
