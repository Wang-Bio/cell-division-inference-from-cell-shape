#ifndef CANVASMANAGER_H
#define CANVASMANAGER_H

#include <QColor>
#include <QObject>
#include <opencv2/core.hpp>
#include <QString>

class QGraphicsView;


struct CanvasInfo{
    int width;
    int height;
    QColor color;
    bool valid;
};

struct ImageInfo{
    cv::Mat image;
    QString filePath;
    QString fileName;
    bool valid;
};


class CanvasManager : public QObject
{
    Q_OBJECT
public:
    explicit CanvasManager(QObject *parent = nullptr);

    //show dialogs to get size/color and set up the canvas in the view
    CanvasInfo createCanvasWithDialog(QGraphicsView *view, QWidget *dialogParent = nullptr);
    ImageInfo openRawImage(QGraphicsView *view, QWidget *dialogParent = nullptr);
    ImageInfo openRawImage(QGraphicsView *view, QWidget *dialogParent, const QString &filePath);

    void deleteImageAndCanvas(QGraphicsView *view, bool resetViewTransform = true);
};

#endif // CANVASMANAGER_H
