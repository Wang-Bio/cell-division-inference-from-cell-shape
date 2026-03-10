#ifndef INTERACTIVEGRAPHICSVIEW_H
#define INTERACTIVEGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QPointF>

class InteractiveGraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit InteractiveGraphicsView(QWidget *parent = nullptr);

    double currentZoomPercent() const;

signals:
    void zoomChanged(double zoomPercent);
    void mouseMovedOnScene(const QPointF &scenePos);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    const double zoomInFactor = 1.15;
    const double zoomOutFactor = 1.0/zoomInFactor;
};

#endif // INTERACTIVEGRAPHICSVIEW_H
