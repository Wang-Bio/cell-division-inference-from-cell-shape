#ifndef OVERLAPINDEXANALYSIS_H
#define OVERLAPINDEXANALYSIS_H

#include <QObject>
#include <QString>
#include <atomic>

struct OverlapIndexOptions {
    QString csvPath;
    QString outputDirectory;
};

class QWidget;

class OverlapIndexWorker : public QObject
{
    Q_OBJECT
public:
    explicit OverlapIndexWorker(const OverlapIndexOptions &options, QObject *parent = nullptr);
public slots:
    void run();
    void cancel();
signals:
    void progress(int value, int maximum, const QString &message);
    void finished(bool success, const QString &message);
private:
    OverlapIndexOptions m_options;
    std::atomic_bool m_cancelled{false};
};

class OverlapIndexAnalysis
{
public:
    static bool getOptions(QWidget *parent, OverlapIndexOptions *options);
};

#endif
