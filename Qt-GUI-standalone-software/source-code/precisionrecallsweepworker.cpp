#include "precisionrecallsweepworker.h"

#include <QtMath>

PrecisionRecallSweepWorker::PrecisionRecallSweepWorker(const Request &request, QObject *parent)
    : QObject(parent)
    , m_request(request)
{
    qRegisterMetaType<PrecisionRecallSweepResultRow>("PrecisionRecallSweepResultRow");
    qRegisterMetaType<QVector<PrecisionRecallSweepResultRow>>("QVector<PrecisionRecallSweepResultRow>");
    qRegisterMetaType<PrecisionRecallSweepThresholdResult>("PrecisionRecallSweepThresholdResult");
    qRegisterMetaType<QVector<PrecisionRecallSweepThresholdResult>>("QVector<PrecisionRecallSweepThresholdResult>");
}

void PrecisionRecallSweepWorker::requestCancel()
{
    m_cancelRequested = true;
}

void PrecisionRecallSweepWorker::run()
{
    QVector<PrecisionRecallSweepResultRow> results;
    QVector<PrecisionRecallSweepThresholdResult> pairsByThreshold;
    QStringList warnings;
    QStringList errors;
    bool anyProcessed = false;

    const double start = m_request.thresholdStart;
    const double end = m_request.thresholdEnd;
    const double step = m_request.thresholdStep;

    const int totalSteps = (step > 0.0)
            ? static_cast<int>(std::floor((end - start) / step + 1.0 + 1e-9))
            : 0;

    if (totalSteps <= 0) {
        emit finished(false, results, pairsByThreshold, warnings, errors, false);
        return;
    }

    const auto appendMessages = [](QStringList &target, const QStringList &source, double threshold) {
        const QString thresholdStr = QString::number(threshold, 'f', 6);
        for (const QString &msg : source)
            target << QString("Threshold %1: %2").arg(thresholdStr, msg);
    };

    int completed = 0;
    for (double threshold = start; threshold <= end + 1e-9; threshold += step) {
        if (m_cancelRequested) {
            emit finished(false, results, pairsByThreshold, warnings, errors, true);
            return;
        }

        DivisionEstimator::Criterion runCriterion = m_request.baseCriterion;
        runCriterion.threshold = threshold;

        const BatchDivisionEstimator::BatchResult summary = BatchDivisionEstimator::estimateDirectory(m_request.directory, runCriterion);
        results.append({threshold, summary.totals});
        pairsByThreshold.append({threshold, summary.pairRows});
        anyProcessed = anyProcessed || summary.filesProcessed > 0 || summary.filesWithResults > 0;
        appendMessages(errors, summary.errors, threshold);
        appendMessages(warnings, summary.warnings, threshold);

        ++completed;
        emit progress(completed, totalSteps, threshold);
    }

    if (!anyProcessed) {
        errors << QString("No geometry files were processed in %1.").arg(m_request.directory);
    }

    emit finished(anyProcessed, results, pairsByThreshold, warnings, errors, false);
}
