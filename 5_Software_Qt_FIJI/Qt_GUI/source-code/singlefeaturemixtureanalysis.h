#ifndef SINGLEFEATUREMIXTUREANALYSIS_H
#define SINGLEFEATUREMIXTUREANALYSIS_H

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

struct SingleFeatureMixtureOptions {
    QString csvPath;
    QString feature = QStringLiteral("junctionAngleAverageDegrees");
    QString outputDirectory;
    QString observedColumn = QStringLiteral("observed_division");
    QString exceptionColumn = QStringLiteral("exception_label");
    quint32 seed = 0;
};

class QWidget;

class SingleFeatureMixtureWorker : public QObject
{
    Q_OBJECT
public:
    explicit SingleFeatureMixtureWorker(const SingleFeatureMixtureOptions &options,
                                        QObject *parent = nullptr);
public slots:
    void run();
    void cancel();
signals:
    void progress(int value, int maximum, const QString &message);
    void finished(bool success, const QString &message);
private:
    SingleFeatureMixtureOptions m_options;
    std::atomic_bool m_cancelled{false};
};

class SingleFeatureMixtureAnalysis
{
public:
    static bool getOptions(QWidget *parent, SingleFeatureMixtureOptions *options);
};

#endif
