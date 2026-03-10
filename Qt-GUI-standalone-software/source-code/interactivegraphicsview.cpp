#include "interactivegraphicsview.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>

InteractiveGraphicsView::InteractiveGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::ScrollHandDrag);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);

    setMouseTracking(true);
}

double InteractiveGraphicsView::currentZoomPercent() const
{
    double scaleFactor = transform().m11();
    return scaleFactor * 100.0;
}

void InteractiveGraphicsView::wheelEvent(QWheelEvent *event)
{
    double factor = (event->angleDelta().y()>0) ? zoomInFactor : zoomOutFactor;

    double currentScale = transform().m11();
    if(factor>1.0 && currentScale>50.0) return;
    if(factor<1.0 && currentScale<0.02) return;

    scale(factor, factor);

    emit zoomChanged(currentZoomPercent());

    event->accept();
}

void InteractiveGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    QPointF scenePos = mapToScene(event->pos());
    emit mouseMovedOnScene(scenePos);

    QGraphicsView::mouseMoveEvent(event);
}

void InteractiveGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier)){
        setDragMode(QGraphicsView::RubberBandDrag);
        setRubberBandSelectionMode(Qt::IntersectsItemShape);
        QGraphicsView::mousePressEvent(event);
        return;
    }

    setDragMode(QGraphicsView::ScrollHandDrag);
    QGraphicsView::mousePressEvent(event);
}

void InteractiveGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);

    if(dragMode()==QGraphicsView::RubberBandDrag){
        setDragMode(QGraphicsView::ScrollHandDrag);
    }
}
