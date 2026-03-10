#ifndef PRECISIONRECALLSWEEPWORKER_H
#define PRECISIONRECALLSWEEPWORKER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "divisionestimator.h"
#include "batchdivisionestimator.h"

struct PrecisionRecallSweepResultRow {
    double threshold = 0.0;
    BatchDivisionEstimator::DivisionMetrics metrics;
};

struct PrecisionRecallSweepThresholdResult {
    double threshold = 0.0;
    QVector<BatchDivisionEstimator::DivisionPairRow> pairRows;
};

Q_DECLARE_METATYPE(PrecisionRecallSweepResultRow)
Q_DECLARE_METATYPE(PrecisionRecallSweepThresholdResult)

class PrecisionRecallSweepWorker : public QObject
{
    Q_OBJECT
public:
    struct Request {
        QString directory;
        DivisionEstimator::Criterion baseCriterion;
        double thresholdStart = 0.0;
        double thresholdEnd = 0.0;
        double thresholdStep = 1.0;
    };

    explicit PrecisionRecallSweepWorker(const Request &request, QObject *parent = nullptr);

public slots:
    void run();
    void requestCancel();

signals:
    void progress(int completed, int total, double threshold);
    void finished(bool success,
                  const QVector<PrecisionRecallSweepResultRow> &results,
                  const QVector<PrecisionRecallSweepThresholdResult> &pairsByThreshold,
                  const QStringList &warnings,
                  const QStringList &errors,
                  bool canceled);

private:
    Request m_request;
    bool m_cancelRequested = false;
};

#endif // PRECISIONRECALLSWEEPWORKER_H
