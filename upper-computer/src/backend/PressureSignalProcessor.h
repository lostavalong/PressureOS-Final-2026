#pragma once

#include <QQueue>
#include <QString>
#include <QVector>
#include <QtGlobal>

class PressureSignalProcessor
{
public:
    enum class FilterMode {
        Raw,
        Median5,
        MovingAverage10,
        IirBalanced,
        IirStrong,
        PeriodicLockStrong,
        PeriodicPrecision
    };

    static constexpr const char *calibrationVersion() { return "CAL-Q2-UP-20260824-R1"; }

    static FilterMode modeFromName(const QString &name);
    static double calibratedPressure(quint32 rawCode);

    void reset();
    double process(double calibratedKPa, qint64 timestampMs, FilterMode mode);
    void primePeriodicHistory(const QVector<double> &values,
                              const QVector<qint64> &timestamps,
                              double detectedPeriodSeconds = 0.0);

    bool precisionReady() const { return m_precisionReady || m_strongPeriodicReady; }
    double precisionWindowProgress() const { return m_precisionProgress; }
    double precisionDetectedPeriodSeconds() const
    {
        return m_strongDetectedPeriodSeconds > 0.0
            ? m_strongDetectedPeriodSeconds : m_detectedPeriodSeconds;
    }
    bool strongPeriodicReady() const { return m_strongPeriodicReady; }

private:
    struct TimedSample {
        qint64 timestampMs = 0;
        double value = 0.0;
    };

    void appendRecent(double value);
    double recentMedian(int count) const;
    double recentMean(int count) const;
    void appendFastHistory(double value, qint64 timestampMs);
    double robustMotionRateKPaPerSecond() const;
    void appendPrecision(double value, qint64 timestampMs);
    double precisionMean(qint64 startTimestampMs, qint64 endTimestampMs) const;
    void updatePrecisionModel(qint64 timestampMs, bool force = false);
    void resetPrecisionForMotion(double value, qint64 timestampMs);
    double precisionEstimate() const;
    void appendStrongObservation(double value, qint64 timestampMs);
    void resetStrongPeriodic(double value, qint64 timestampMs);
    void updateStrongPeriodicModel(qint64 timestampMs, bool force = false);
    double strongPeriodicEstimate(double fastValue, qint64 timestampMs) const;

    QQueue<double> m_recent;
    QQueue<TimedSample> m_fastHistory;
    QQueue<TimedSample> m_precision;
    QQueue<TimedSample> m_strongObservation;
    double m_iirBalanced = 0.0;
    double m_iirStrong = 0.0;
    double m_precisionProgress = 0.0;
    double m_detectedPeriodSeconds = 0.0;
    double m_periodicRangeKPa = 0.0;
    double m_precisionBaselineKPa = 0.0;
    qint64 m_precisionModelTimestampMs = -1;
    qint64 m_lastPeriodEvaluationMs = -1;
    qint64 m_lastStrongEvaluationMs = -1;
    qint64 m_strongLockTimestampMs = -1;
    qint64 m_lastTimestampMs = -1;
    bool m_initialized = false;
    bool m_precisionReady = false;
    bool m_periodicCandidate = false;
    bool m_strongPeriodicReady = false;
    int m_strongConfirmations = 0;
    int m_strongMisses = 0;
    double m_strongDetectedPeriodSeconds = 0.0;
    double m_strongBaselineKPa = 0.0;
    double m_strongAmplitudeKPa = 0.0;
};
