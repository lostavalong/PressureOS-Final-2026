#include "PressureSignalProcessor.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QtMath>

#include <algorithm>
#include <numeric>

namespace {
constexpr double pi = 3.14159265358979323846;

bool require(bool condition, const QString &message)
{
    if (condition)
        return true;
    QTextStream(stderr) << "FAILED: " << message << Qt::endl;
    return false;
}

struct Metrics {
    double mean = 0.0;
    double peakToPeak = 0.0;
    double slope = 0.0;
};

Metrics metrics(const QVector<double> &values, const QVector<double> &times)
{
    Metrics result;
    if (values.isEmpty())
        return result;
    result.mean = std::accumulate(values.cbegin(), values.cend(), 0.0) / values.size();
    const auto bounds = std::minmax_element(values.cbegin(), values.cend());
    result.peakToPeak = *bounds.second - *bounds.first;
    double meanTime = std::accumulate(times.cbegin(), times.cend(), 0.0) / times.size();
    double covariance = 0.0;
    double variance = 0.0;
    for (int index = 0; index < values.size(); ++index) {
        const double dt = times.at(index) - meanTime;
        covariance += dt * (values.at(index) - result.mean);
        variance += dt * dt;
    }
    result.slope = variance > 0.0 ? covariance / variance : 0.0;
    return result;
}

bool syntheticRegression()
{
    PressureSignalProcessor filter;
    const auto mode = PressureSignalProcessor::FilterMode::PeriodicPrecision;
    constexpr qint64 intervalMs = 60;
    QVector<double> settled;
    QVector<double> settledTimes;
    qint64 timestamp = 0;
    for (int index = 0; index < 750; ++index, timestamp += intervalMs) {
        const double seconds = timestamp / 1000.0;
        const double raw = 20.0 + 0.22 * qSin(2.0 * pi * seconds / 6.2);
        const double output = filter.process(raw, timestamp, mode);
        if (seconds > 30.0) {
            settled.push_back(output);
            settledTimes.push_back(seconds);
        }
    }
    const Metrics first = metrics(settled, settledTimes);
    if (!require(filter.precisionReady(), QStringLiteral("initial precision readiness"))
        || !require(qAbs(first.mean - 20.0) < 0.03, QStringLiteral("initial centre bias"))
        || !require(first.peakToPeak < 0.06, QStringLiteral("initial periodic suppression")))
        return false;

    double output = 20.0;
    for (int index = 0; index < 40; ++index, timestamp += intervalMs) {
        const double fraction = (index + 1) / 40.0;
        const double raw = 20.0 + 80.0 * fraction;
        output = filter.process(raw, timestamp, mode);
        if (!require(output <= raw + 0.35 && output >= raw - 6.0,
                     QStringLiteral("rising transition remains measurement-bound")))
            return false;
    }
    for (int index = 0; index < 5; ++index, timestamp += intervalMs)
        output = filter.process(100.0, timestamp, mode);
    if (!require(output > 99.5, QStringLiteral("rising transition follows within median latency")))
        return false;

    settled.clear();
    settledTimes.clear();
    for (int index = 0; index < 700; ++index, timestamp += intervalMs) {
        const double seconds = timestamp / 1000.0;
        const double raw = 100.0 + 0.22 * qSin(2.0 * pi * seconds / 6.2);
        output = filter.process(raw, timestamp, mode);
        if (index > 350) {
            settled.push_back(output);
            settledTimes.push_back(seconds);
        }
    }
    const Metrics high = metrics(settled, settledTimes);
    if (!require(filter.precisionReady(), QStringLiteral("precision reacquisition after rise"))
        || !require(qAbs(high.mean - 100.0) < 0.03, QStringLiteral("high plateau centre"))
        || !require(high.peakToPeak < 0.06, QStringLiteral("high plateau suppression"))
        || !require(qAbs(high.slope) < 0.001, QStringLiteral("no post-rise false drift")))
        return false;

    for (int index = 0; index < 40; ++index, timestamp += intervalMs) {
        const double fraction = (index + 1) / 40.0;
        const double raw = 100.0 - 80.0 * fraction;
        output = filter.process(raw, timestamp, mode);
        if (!require(output >= raw - 0.35 && output <= raw + 6.0,
                     QStringLiteral("falling transition remains measurement-bound")))
            return false;
    }
    settled.clear();
    settledTimes.clear();
    for (int index = 0; index < 700; ++index, timestamp += intervalMs) {
        const double seconds = timestamp / 1000.0;
        const double raw = 20.0 + 0.22 * qSin(2.0 * pi * seconds / 6.2);
        output = filter.process(raw, timestamp, mode);
        if (index > 350) {
            settled.push_back(output);
            settledTimes.push_back(seconds);
        }
    }
    const Metrics low = metrics(settled, settledTimes);
    return require(filter.precisionReady(), QStringLiteral("precision reacquisition after fall"))
        && require(qAbs(low.mean - 20.0) < 0.03, QStringLiteral("low plateau centre"))
        && require(low.peakToPeak < 0.06, QStringLiteral("low plateau suppression"))
        && require(qAbs(low.slope) < 0.001, QStringLiteral("no post-fall false drift"));
}

bool strongPeriodicLockRegression()
{
    PressureSignalProcessor filter;
    const auto mode = PressureSignalProcessor::FilterMode::PeriodicLockStrong;
    constexpr qint64 intervalMs = 75;
    constexpr double periodSeconds = 5.0;
    qint64 timestamp = 0;
    QVector<double> settled;
    QVector<double> settledTimes;
    double output = 0.0;
    for (int index = 0; index < 720; ++index, timestamp += intervalMs) {
        const double seconds = timestamp / 1000.0;
        const double raw = 20.0 + 0.95 * qSin(2.0 * pi * seconds / periodSeconds)
            + 0.015 * qSin(seconds * 2.71);
        output = filter.process(raw, timestamp, mode);
        if (seconds > 34.0) {
            settled.push_back(output);
            settledTimes.push_back(seconds);
        }
    }
    const Metrics low = metrics(settled, settledTimes);
    if (!require(filter.strongPeriodicReady(), QStringLiteral("strong periodic lock readiness"))
        || !require(qAbs(filter.precisionDetectedPeriodSeconds() - periodSeconds) < 0.35,
                    QStringLiteral("strong periodic period detection"))
        || !require(qAbs(low.mean - 20.0) < 0.035,
                    QStringLiteral("strong periodic centre remains unbiased"))
        || !require(low.peakToPeak < 0.08,
                    QStringLiteral("strong periodic waveform suppression"))) {
        return false;
    }

    // A genuine pressure transition must release the periodic lock and return
    // to the median path instead of holding the old plateau.
    for (int index = 0; index < 20; ++index, timestamp += intervalMs) {
        const double raw = 20.0 + 80.0 * (index + 1) / 20.0;
        output = filter.process(raw, timestamp, mode);
    }
    for (int index = 0; index < 5; ++index, timestamp += intervalMs)
        output = filter.process(100.0, timestamp, mode);
    if (!require(output > 99.5, QStringLiteral("strong periodic lock releases on step"))
        || !require(!filter.strongPeriodicReady(),
                    QStringLiteral("strong periodic state cleared after step"))) {
        return false;
    }

    settled.clear();
    settledTimes.clear();
    for (int index = 0; index < 720; ++index, timestamp += intervalMs) {
        const double seconds = timestamp / 1000.0;
        const double raw = 100.0 + 0.95 * qSin(2.0 * pi * seconds / periodSeconds)
            + 0.015 * qSin(seconds * 2.71);
        output = filter.process(raw, timestamp, mode);
        if (index > 460) {
            settled.push_back(output);
            settledTimes.push_back(seconds);
        }
    }
    const Metrics high = metrics(settled, settledTimes);
    return require(filter.strongPeriodicReady(),
                   QStringLiteral("strong periodic reacquisition after step"))
        && require(qAbs(high.mean - 100.0) < 0.035,
                   QStringLiteral("strong periodic new plateau centre"))
        && require(high.peakToPeak < 0.08,
                   QStringLiteral("strong periodic new plateau suppression"));
}

bool replayRealTrace(const QString &fileName)
{
    QFile file(QStringLiteral(PRESSUREOS_SOURCE_DIR "/artifacts/") + fileName);
    if (!require(file.open(QIODevice::ReadOnly), QStringLiteral("open real trace %1").arg(fileName)))
        return false;
    const QString text = QString::fromLatin1(file.readAll());
    const QRegularExpression frame(QStringLiteral(
        "@PS1,M,(\\d+),(\\d+),(\\d+),(\\d+),[0-9A-Fa-f]{8}\\*[0-9A-Fa-f]{4}"));
    QSet<quint64> seenSequences;
    QVector<qint64> timestamps;
    QVector<double> rawValues;
    auto match = frame.globalMatch(text);
    while (match.hasNext()) {
        const QRegularExpressionMatch current = match.next();
        const quint64 sequence = current.captured(1).toULongLong();
        if (seenSequences.contains(sequence))
            continue;
        seenSequences.insert(sequence);
        timestamps.push_back(current.captured(2).toLongLong());
        rawValues.push_back(PressureSignalProcessor::calibratedPressure(
            current.captured(3).toUInt()));
    }
    if (!require(rawValues.size() > 1500, QStringLiteral("real trace frame count")))
        return false;

    PressureSignalProcessor filter;
    QVector<double> outputValues;
    outputValues.reserve(rawValues.size());
    for (int index = 0; index < rawValues.size(); ++index) {
        outputValues.push_back(filter.process(
            rawValues.at(index), timestamps.at(index),
            PressureSignalProcessor::FilterMode::PeriodicPrecision));
    }

    const qint64 tailStart = timestamps.last() - 20000;
    QVector<double> tailOutput;
    QVector<double> tailRaw;
    QVector<double> tailTimes;
    for (int index = 0; index < outputValues.size(); ++index) {
        if (timestamps.at(index) < tailStart)
            continue;
        tailOutput.push_back(outputValues.at(index));
        tailRaw.push_back(rawValues.at(index));
        tailTimes.push_back((timestamps.at(index) - tailStart) / 1000.0);
    }
    const Metrics filtered = metrics(tailOutput, tailTimes);
    const Metrics raw = metrics(tailRaw, tailTimes);
    QTextStream(stdout) << fileName << ": frames=" << rawValues.size()
                        << " tail_mean=" << filtered.mean
                        << " tail_p2p=" << filtered.peakToPeak
                        << " tail_slope=" << filtered.slope << Qt::endl;
    return require(filter.precisionReady(), QStringLiteral("real trace precision readiness"))
        && require(qAbs(filtered.mean - raw.mean) < 0.06,
                   QStringLiteral("real trace centre remains unbiased"))
        && require(filtered.peakToPeak < 0.12,
                   QStringLiteral("real trace periodic suppression"))
        && require(qAbs(filtered.slope) < 0.005,
                   QStringLiteral("real trace has no false drift"));
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (!syntheticRegression())
        return 1;
    if (!strongPeriodicLockRegression())
        return 3;
    if (!replayRealTrace(QStringLiteral("pressureos_dynamic_20260826.trace"))
        || !replayRealTrace(QStringLiteral("pressureos_dynamic_down_20260826.trace")))
        return 2;
    QTextStream(stdout) << "pressure filter regression passed" << Qt::endl;
    return 0;
}
