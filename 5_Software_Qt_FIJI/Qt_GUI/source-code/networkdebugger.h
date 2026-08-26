#ifndef NETWORKDEBUGGER_H
#define NETWORKDEBUGGER_H

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QGraphicsScene;
class QGraphicsView;
class QWidget;

struct NetworkDebugIssue {
    enum Severity { Error, Warning, Info } severity = Error;
    QString code;
    QVector<int> vertexIds, lineIds, polygonIds;
    QRectF location;
    QString description, correction;
};

struct NetworkDebugTolerances {
    static constexpr double coordinate = 1e-6;
    static constexpr double strictDistancePx = 1.0;
    static constexpr double shortLinePx = 2.0;
    static constexpr double boundaryDistancePx = 2.0;
    static constexpr double minimumAreaPx2 = 1.0;
    static constexpr double relativeOverlapArea = 1e-4;
    static constexpr double supportedFraction = 0.80;
    static constexpr double maximumUnsupportedGapPx = 2.0;
};

class NetworkDebugger {
public:
    static QVector<NetworkDebugIssue> inspect(QGraphicsScene *scene);
    static void show(QWidget *parent, QGraphicsScene *scene, QGraphicsView *view);
    static void clearHighlights(QGraphicsScene *scene);
};

#endif
