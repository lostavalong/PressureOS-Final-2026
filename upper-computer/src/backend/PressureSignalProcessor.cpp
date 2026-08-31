#include "PressureSignalProcessor.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace {
constexpr double kCalibrationOffsetKPa = -102.3969598250119;
constexpr double kPi = 3.14159265358979323846;
constexpr double kCalibrationLinearKPaPerCode = 0.0002085601271357902;
constexpr double kCalibrationQuadraticKPaPerCode2 = 2.895347824842157e-13;
constexpr qint64 kPrecisionHistoryMs = 190000;
constexpr qint64 kPeriodEvaluationIntervalMs = 1000;
constexpr qint64 kAperiodicWindowMs = 8000;
constexpr qint64 kFastMotionWindowMs = 700;
constexpr double kMinimumPeriodSeconds = 4.0;
constexpr double kMaximumPeriodSeconds = 90.0;
constexpr double kMinimumPeriodicRangeKPa = 0.10;
constexpr double kDynamicRateKPaPerSecond = 1.20;
constexpr double kMinimumLevelChangeKPa = 0.60;
constexpr double kSmoothingHalfWindowSeconds = 0.35;
constexpr int kRequiredCompleteCycles = 2;
constexpr int kRecentCapacity = 15;

// Dedicated strong periodic-lock mode. Its observation history is independent
// from the ordinary motion detector, so the steep flank of a large sine wave
// cannot repeatedly erase the very evidence needed to recognise that wave.
constexpr qint64 kStrongHistoryMs = 100000;
constexpr qint64 kStrongEvaluationIntervalMs = 500;
constexpr qint64 kStrongMinimumHistoryMs = 12000;
constexpr double kStrongMinimumPeriodSeconds = 3.5;
constexpr double kStrongMaximumPeriodSeconds = 30.0;
constexpr double kStrongFrequencyStepHz = 0.0025;
constexpr double kStrongMinimumAmplitudeKPa = 0.06;
constexpr double kStrongMaximumAmplitudeKPa = 8.0;
constexpr double kStrongMinimumImprovement = 0.55;
constexpr double kStrongPrelockTransitionRateKPaPerSecond = 8.0;
constexpr int kStrongRequiredCycles = 3;
constexpr int kStrongRequiredConfirmations = 3;
constexpr int kStrongMaximumMisses = 10;
constexpr qint64 kStrongBlendMs = 1200;

double medianOfSorted(const std::vector<double> &values)
{
    if (values.empty())
        return 0.0;
    const std::size_t middle = values.size() / 2;
    if (values.size() % 2 == 0)
        return (values.at(middle - 1) + values.at(middle)) * 0.5;
    return values.at(middle);
}

double quantileOfSorted(const std::vector<double> &values, double quantile)
{
    if (values.empty())
        return 0.0;
    const double position = qBound(0.0, quantile, 1.0)
        * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper)
        return values.at(lower);
    const double fraction = position - static_cast<double>(lower);
    return values.at(lower) * (1.0 - fraction) + values.at(upper) * fraction;
}

bool solveThreeByThree(double augmented[3][4], double result[3])
{
    for (int column = 0; column < 3; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 3; ++row) {
            if (qAbs(augmented[row][column]) > qAbs(augmented[pivot][column]))
                pivot = row;
        }
        if (qAbs(augmented[pivot][column]) < 1e-10)
            return false;
        if (pivot != column) {
            for (int entry = column; entry < 4; ++entry)
                std::swap(augmented[pivot][entry], augmented[column][entry]);
        }
        const double divisor = augmented[column][column];
        for (int entry = column; entry < 4; ++entry)
            augmented[column][entry] /= divisor;
        for (int row = 0; row < 3; ++row) {
            if (row == column)
                continue;
            const double factor = augmented[row][column];
            for (int entry = column; entry < 4; ++entry)
                augmented[row][entry] -= factor * augmented[column][entry];
        }
    }
    for (int row = 0; row < 3; ++row)
        result[row] = augmented[row][3];
    return true;
}
}

PressureSignalProcessor::FilterMode PressureSignalProcessor::modeFromName(const QString &name)
{
    if (name.startsWith(QStringLiteral("无滤波")))
        return FilterMode::Raw;
    if (name.startsWith(QStringLiteral("中值")))
        return FilterMode::Median5;
    if (name.startsWith(QStringLiteral("滑动")))
        return FilterMode::MovingAverage10;
    if (name.startsWith(QStringLiteral("稳态周期锁定")))
        return FilterMode::PeriodicLockStrong;
    if (name.contains(QStringLiteral("强抑制")))
        return FilterMode::IirStrong;
    if (name.startsWith(QStringLiteral("IIR")))
        return FilterMode::IirBalanced;
    return FilterMode::PeriodicPrecision;
}

double PressureSignalProcessor::calibratedPressure(quint32 rawCode)
{
    const double value = static_cast<double>(rawCode);
    // Horner form limits avoidable rounding and keeps the deployed expression
    // identical to the 2026-08-24 traceable calibration report.
    return (kCalibrationQuadraticKPaPerCode2 * value
            + kCalibrationLinearKPaPerCode) * value
        + kCalibrationOffsetKPa;
}

void PressureSignalProcessor::reset()
{
    m_recent.clear();
    m_fastHistory.clear();
    m_precision.clear();
    m_strongObservation.clear();
    m_iirBalanced = 0.0;
    m_iirStrong = 0.0;
    m_precisionProgress = 0.0;
    m_detectedPeriodSeconds = 0.0;
    m_periodicRangeKPa = 0.0;
    m_precisionBaselineKPa = 0.0;
    m_precisionModelTimestampMs = -1;
    m_lastPeriodEvaluationMs = -1;
    m_lastStrongEvaluationMs = -1;
    m_strongLockTimestampMs = -1;
    m_lastTimestampMs = -1;
    m_initialized = false;
    m_precisionReady = false;
    m_periodicCandidate = false;
    m_strongPeriodicReady = false;
    m_strongConfirmations = 0;
    m_strongMisses = 0;
    m_strongDetectedPeriodSeconds = 0.0;
    m_strongBaselineKPa = 0.0;
    m_strongAmplitudeKPa = 0.0;
}

double PressureSignalProcessor::process(double calibratedKPa, qint64 timestampMs,
                                        FilterMode mode)
{
    if (!qIsFinite(calibratedKPa))
        return calibratedKPa;

    if (m_lastTimestampMs >= 0 && timestampMs <= m_lastTimestampMs)
        reset();

    const double intervalSeconds = m_lastTimestampMs >= 0
        ? qBound(0.001, (timestampMs - m_lastTimestampMs) / 1000.0, 1.0)
        : 0.08;
    m_lastTimestampMs = timestampMs;

    appendRecent(calibratedKPa);
    if (!m_initialized) {
        m_iirBalanced = calibratedKPa;
        m_iirStrong = calibratedKPa;
        m_initialized = true;
    } else {
        // Time-constant based IIR coefficients keep the filter behaviour stable
        // when the real serial sample rate varies around 10-13 Hz.
        const double balancedAlpha = 1.0 - qExp(-intervalSeconds / 0.55);
        const double strongAlpha = 1.0 - qExp(-intervalSeconds / 1.75);
        m_iirBalanced += balancedAlpha * (calibratedKPa - m_iirBalanced);
        m_iirStrong += strongAlpha * (calibratedKPa - m_iirStrong);
    }

    const double fast = recentMedian(5);
    appendFastHistory(fast, timestampMs);
    const double motionRate = robustMotionRateKPaPerSecond();

    if (mode == FilterMode::PeriodicLockStrong) {
        // Before lock, a large regular wave remains in the observation buffer.
        // Once locked, only a change outside the learned amplitude envelope or
        // a much faster transition releases it, keeping pressure steps prompt.
        const double expectedSineRate = m_strongDetectedPeriodSeconds > 0.0
            ? m_strongAmplitudeKPa * 2.0 * kPi / m_strongDetectedPeriodSeconds
            : 0.0;
        const double unlockRate = qMax(3.0, expectedSineRate * 2.4);
        const double unlockGap = qMax(1.0, m_strongAmplitudeKPa * 2.6 + 0.20);
        const bool confirmedLevelChange = m_strongPeriodicReady
            && (motionRate > unlockRate
                || qAbs(fast - m_strongBaselineKPa) > unlockGap);
        const bool obviousPrelockTransition = !m_strongPeriodicReady
            && motionRate > kStrongPrelockTransitionRateKPaPerSecond;
        if (confirmedLevelChange || obviousPrelockTransition) {
            resetStrongPeriodic(calibratedKPa, timestampMs);
        } else {
            appendStrongObservation(calibratedKPa, timestampMs);
            updateStrongPeriodicModel(timestampMs);
        }
        return m_strongPeriodicReady
            ? strongPeriodicEstimate(fast, timestampMs) : fast;
    }

    const double levelChangeThreshold = qMax(kMinimumLevelChangeKPa,
                                             m_periodicRangeKPa * 1.50);
    const bool pressureMoving = motionRate > kDynamicRateKPaPerSecond
        || (m_precisionReady
            && qAbs(fast - m_precisionBaselineKPa) > levelChangeThreshold);

    if (pressureMoving) {
        // A pressure change invalidates the old plateau completely.  Keeping or
        // translating that history was the source of the former post-step
        // false drift.  The detected period is retained only as frequency
        // evidence; every pressure sample used for the new centre starts here.
        resetPrecisionForMotion(calibratedKPa, timestampMs);
        m_iirBalanced = fast;
        m_iirStrong = fast;
    } else {
        appendPrecision(calibratedKPa, timestampMs);
        updatePrecisionModel(timestampMs);
    }
    const double precise = pressureMoving ? fast : precisionEstimate();

    switch (mode) {
    case FilterMode::Raw:
        return calibratedKPa;
    case FilterMode::Median5:
        return fast;
    case FilterMode::MovingAverage10:
        return recentMean(10);
    case FilterMode::IirBalanced:
        return m_iirBalanced;
    case FilterMode::IirStrong:
        return m_iirStrong;
    case FilterMode::PeriodicLockStrong:
        return fast;
    case FilterMode::PeriodicPrecision:
        // Dynamic and reacquisition states remain responsive.  Precision mode
        // is entered only after the new plateau itself has supplied enough
        // evidence; it never extrapolates a centre into the future.
        return m_precisionReady ? precise : fast;
    }
    return calibratedKPa;
}

void PressureSignalProcessor::appendRecent(double value)
{
    m_recent.enqueue(value);
    while (m_recent.size() > kRecentCapacity)
        m_recent.dequeue();
}

double PressureSignalProcessor::recentMedian(int count) const
{
    const int used = qMin(count, m_recent.size());
    if (used <= 0)
        return 0.0;
    std::vector<double> values;
    values.reserve(used);
    for (int index = m_recent.size() - used; index < m_recent.size(); ++index)
        values.push_back(m_recent.at(index));
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    if (values.size() % 2 != 0)
        return *middle;
    const double upper = *middle;
    const double lower = *std::max_element(values.begin(), middle);
    return (lower + upper) / 2.0;
}

double PressureSignalProcessor::recentMean(int count) const
{
    const int used = qMin(count, m_recent.size());
    if (used <= 0)
        return 0.0;
    double total = 0.0;
    for (int index = m_recent.size() - used; index < m_recent.size(); ++index)
        total += m_recent.at(index);
    return total / used;
}

void PressureSignalProcessor::appendStrongObservation(double value,
                                                       qint64 timestampMs)
{
    m_strongObservation.enqueue({timestampMs, value});
    const qint64 cutoff = timestampMs - kStrongHistoryMs;
    while (m_strongObservation.size() >= 2
           && m_strongObservation.at(1).timestampMs <= cutoff) {
        m_strongObservation.dequeue();
    }
}

void PressureSignalProcessor::resetStrongPeriodic(double value,
                                                   qint64 timestampMs)
{
    m_strongObservation.clear();
    m_strongObservation.enqueue({timestampMs, value});
    m_strongPeriodicReady = false;
    m_strongConfirmations = 0;
    m_strongMisses = 0;
    m_strongDetectedPeriodSeconds = 0.0;
    m_strongBaselineKPa = value;
    m_strongAmplitudeKPa = 0.0;
    m_lastStrongEvaluationMs = timestampMs;
    m_strongLockTimestampMs = -1;
}

void PressureSignalProcessor::updateStrongPeriodicModel(qint64 timestampMs,
                                                         bool force)
{
    if (m_strongObservation.size() < 60)
        return;
    if (!force && m_lastStrongEvaluationMs >= 0
        && timestampMs - m_lastStrongEvaluationMs < kStrongEvaluationIntervalMs) {
        return;
    }
    m_lastStrongEvaluationMs = timestampMs;

    const qint64 historySpanMs = timestampMs
        - m_strongObservation.front().timestampMs;
    if (historySpanMs < kStrongMinimumHistoryMs)
        return;

    // Cap the fit at roughly 700 evenly spaced observations. This keeps the
    // computation deterministic on Raspberry Pi while retaining the full span.
    const int stride = qMax(1, m_strongObservation.size() / 700);
    QVector<double> times;
    QVector<double> values;
    times.reserve(m_strongObservation.size() / stride + 1);
    values.reserve(m_strongObservation.size() / stride + 1);
    for (int index = 0; index < m_strongObservation.size(); index += stride) {
        const TimedSample &sample = m_strongObservation.at(index);
        times.push_back((sample.timestampMs - timestampMs) / 1000.0);
        values.push_back(sample.value);
    }
    if ((m_strongObservation.size() - 1) % stride != 0) {
        const TimedSample &sample = m_strongObservation.back();
        times.push_back((sample.timestampMs - timestampMs) / 1000.0);
        values.push_back(sample.value);
    }
    if (values.size() < 40)
        return;

    const double mean = std::accumulate(values.cbegin(), values.cend(), 0.0)
        / static_cast<double>(values.size());
    double constantError = 0.0;
    double meanTime = 0.0;
    for (double time : times)
        meanTime += time;
    meanTime /= static_cast<double>(times.size());
    double timeVariance = 0.0;
    double timeCovariance = 0.0;
    for (int index = 0; index < values.size(); ++index) {
        const double centeredValue = values.at(index) - mean;
        const double centeredTime = times.at(index) - meanTime;
        constantError += centeredValue * centeredValue;
        timeVariance += centeredTime * centeredTime;
        timeCovariance += centeredTime * centeredValue;
    }
    if (constantError < 1e-8 || timeVariance < 1e-8)
        return;
    const double linearSlope = timeCovariance / timeVariance;

    double bestError = std::numeric_limits<double>::infinity();
    double bestBaseline = mean;
    double bestFrequency = 0.0;
    double bestAmplitude = 0.0;
    const double spanSeconds = historySpanMs / 1000.0;
    const double minimumFrequency = 1.0 / kStrongMaximumPeriodSeconds;
    const double maximumFrequency = 1.0 / kStrongMinimumPeriodSeconds;
    for (double frequency = minimumFrequency;
         frequency <= maximumFrequency + 1e-9;
         frequency += kStrongFrequencyStepHz) {
        if (spanSeconds * frequency < kStrongRequiredCycles)
            continue;

        double normal[3][4] {};
        for (int index = 0; index < values.size(); ++index) {
            const double angle = 2.0 * kPi * frequency * times.at(index);
            const double basis[3] {1.0, qSin(angle), qCos(angle)};
            for (int row = 0; row < 3; ++row) {
                normal[row][3] += basis[row] * values.at(index);
                for (int column = 0; column < 3; ++column)
                    normal[row][column] += basis[row] * basis[column];
            }
        }
        const double ridge = values.size() * 0.0005;
        normal[1][1] += ridge;
        normal[2][2] += ridge;
        double coefficients[3] {};
        if (!solveThreeByThree(normal, coefficients))
            continue;
        const double amplitude = qSqrt(coefficients[1] * coefficients[1]
                                       + coefficients[2] * coefficients[2]);
        if (amplitude < kStrongMinimumAmplitudeKPa
            || amplitude > kStrongMaximumAmplitudeKPa) {
            continue;
        }
        double error = 0.0;
        for (int index = 0; index < values.size(); ++index) {
            const double angle = 2.0 * kPi * frequency * times.at(index);
            const double fitted = coefficients[0]
                + coefficients[1] * qSin(angle)
                + coefficients[2] * qCos(angle);
            const double residual = values.at(index) - fitted;
            error += residual * residual;
        }
        if (error < bestError) {
            bestError = error;
            bestBaseline = coefficients[0];
            bestFrequency = frequency;
            bestAmplitude = amplitude;
        }
    }

    const double improvement = bestFrequency > 0.0
        ? 1.0 - bestError / constantError : 0.0;
    const double candidatePeriod = bestFrequency > 0.0
        ? 1.0 / bestFrequency : 0.0;
    const double maximumTrend = qMax(0.035,
        bestAmplitude / qMax(candidatePeriod, 1.0) * 0.16);
    const double centreGuard = qMax(0.35, bestAmplitude * 0.40);
    const bool valid = bestFrequency > 0.0
        && improvement >= kStrongMinimumImprovement
        && qAbs(linearSlope) <= maximumTrend
        && qAbs(bestBaseline - mean) <= centreGuard;

    if (!valid) {
        m_strongConfirmations = 0;
        if (m_strongPeriodicReady && ++m_strongMisses >= kStrongMaximumMisses) {
            m_strongPeriodicReady = false;
            m_strongDetectedPeriodSeconds = 0.0;
            m_strongAmplitudeKPa = 0.0;
            m_strongLockTimestampMs = -1;
        }
        return;
    }

    const bool periodConsistent = m_strongDetectedPeriodSeconds <= 0.0
        || qAbs(candidatePeriod - m_strongDetectedPeriodSeconds)
            <= qMax(0.45, m_strongDetectedPeriodSeconds * 0.18);
    const bool centreConsistent = !m_strongPeriodicReady
        || qAbs(bestBaseline - m_strongBaselineKPa)
            <= qMax(0.30, bestAmplitude * 0.45);
    if (!periodConsistent || !centreConsistent) {
        m_strongConfirmations = 1;
        m_strongDetectedPeriodSeconds = candidatePeriod;
        m_strongBaselineKPa = bestBaseline;
        m_strongAmplitudeKPa = bestAmplitude;
        return;
    }

    ++m_strongConfirmations;
    m_strongMisses = 0;
    m_strongDetectedPeriodSeconds = m_strongDetectedPeriodSeconds > 0.0
        ? m_strongDetectedPeriodSeconds * 0.72 + candidatePeriod * 0.28
        : candidatePeriod;
    m_strongAmplitudeKPa = m_strongAmplitudeKPa > 0.0
        ? m_strongAmplitudeKPa * 0.72 + bestAmplitude * 0.28
        : bestAmplitude;
    m_strongBaselineKPa = m_strongPeriodicReady
        ? m_strongBaselineKPa * 0.82 + bestBaseline * 0.18
        : bestBaseline;
    if (!m_strongPeriodicReady
        && m_strongConfirmations >= kStrongRequiredConfirmations) {
        m_strongPeriodicReady = true;
        m_strongLockTimestampMs = timestampMs;
    }
}

double PressureSignalProcessor::strongPeriodicEstimate(double fastValue,
                                                        qint64 timestampMs) const
{
    if (!m_strongPeriodicReady || m_strongLockTimestampMs < 0)
        return fastValue;
    const double blend = qBound(0.0,
        (timestampMs - m_strongLockTimestampMs) / static_cast<double>(kStrongBlendMs), 1.0);
    return fastValue * (1.0 - blend) + m_strongBaselineKPa * blend;
}

void PressureSignalProcessor::appendFastHistory(double value, qint64 timestampMs)
{
    m_fastHistory.enqueue({timestampMs, value});
    const qint64 cutoff = timestampMs - kFastMotionWindowMs;
    while (m_fastHistory.size() >= 2
           && m_fastHistory.at(1).timestampMs <= cutoff) {
        m_fastHistory.dequeue();
    }
}

double PressureSignalProcessor::robustMotionRateKPaPerSecond() const
{
    if (m_fastHistory.size() < 2)
        return 0.0;
    const TimedSample &first = m_fastHistory.front();
    const TimedSample &last = m_fastHistory.back();
    const double spanSeconds = (last.timestampMs - first.timestampMs) / 1000.0;
    if (spanSeconds < 0.35)
        return 0.0;
    return qAbs(last.value - first.value) / spanSeconds;
}

void PressureSignalProcessor::appendPrecision(double value, qint64 timestampMs)
{
    m_precision.enqueue({timestampMs, value});
    const qint64 cutoff = timestampMs - kPrecisionHistoryMs;
    // Retain one sample before the cutoff so the integration can interpolate
    // the exact window boundary despite irregular serial arrival intervals.
    while (m_precision.size() >= 2 && m_precision.at(1).timestampMs <= cutoff)
        m_precision.dequeue();
}

double PressureSignalProcessor::precisionMean(qint64 startTimestampMs,
                                              qint64 endTimestampMs) const
{
    if (m_precision.isEmpty())
        return 0.0;
    if (m_precision.size() == 1)
        return m_precision.front().value;

    const qint64 start = qMax(startTimestampMs, m_precision.front().timestampMs);
    const qint64 end = qMin(endTimestampMs, m_precision.back().timestampMs);
    if (end <= start)
        return m_precision.back().value;

    int index = 0;
    while (index < m_precision.size()
           && m_precision.at(index).timestampMs < start) {
        ++index;
    }

    double previousValue = m_precision.front().value;
    if (index == 0) {
        previousValue = m_precision.front().value;
    } else if (index >= m_precision.size()) {
        previousValue = m_precision.back().value;
    } else {
        const TimedSample &lower = m_precision.at(index - 1);
        const TimedSample &upper = m_precision.at(index);
        const qint64 spanMs = upper.timestampMs - lower.timestampMs;
        const double fraction = spanMs > 0
            ? (start - lower.timestampMs) / static_cast<double>(spanMs) : 0.0;
        previousValue = lower.value + fraction * (upper.value - lower.value);
    }
    qint64 previousTime = start;

    double integralKPaMs = 0.0;
    for (; index < m_precision.size(); ++index) {
        const TimedSample &current = m_precision.at(index);
        if (current.timestampMs <= previousTime)
            continue;
        if (current.timestampMs >= end)
            break;
        const qint64 segmentMs = current.timestampMs - previousTime;
        integralKPaMs += (previousValue + current.value) * 0.5 * segmentMs;
        previousTime = current.timestampMs;
        previousValue = current.value;
    }

    double endValue = previousValue;
    if (index < m_precision.size()) {
        const TimedSample &next = m_precision.at(index);
        const qint64 spanMs = next.timestampMs - previousTime;
        if (spanMs > 0) {
            const double fraction = (end - previousTime)
                / static_cast<double>(spanMs);
            endValue += fraction * (next.value - endValue);
        }
    }
    integralKPaMs += (previousValue + endValue) * 0.5 * (end - previousTime);
    return integralKPaMs / static_cast<double>(end - start);
}

void PressureSignalProcessor::updatePrecisionModel(qint64 timestampMs, bool force)
{
    if (m_precision.size() < 20)
        return;
    if (!force && m_lastPeriodEvaluationMs >= 0
        && timestampMs - m_lastPeriodEvaluationMs < kPeriodEvaluationIntervalMs) {
        return;
    }
    m_lastPeriodEvaluationMs = timestampMs;

    QVector<double> values;
    QVector<double> times;
    values.reserve(m_precision.size());
    times.reserve(m_precision.size());
    const qint64 originMs = m_precision.front().timestampMs;
    for (const TimedSample &sample : m_precision) {
        values.push_back(sample.value);
        times.push_back((sample.timestampMs - originMs) / 1000.0);
    }

    QVector<double> smoothed(values.size());
    int left = 0;
    int right = 0;
    double windowSum = 0.0;
    for (int index = 0; index < values.size(); ++index) {
        const double centerTime = times.at(index);
        while (right < values.size()
               && times.at(right) <= centerTime + kSmoothingHalfWindowSeconds) {
            windowSum += values.at(right);
            ++right;
        }
        while (left < right
               && times.at(left) < centerTime - kSmoothingHalfWindowSeconds) {
            windowSum -= values.at(left);
            ++left;
        }
        smoothed[index] = windowSum / static_cast<double>(qMax(1, right - left));
    }

    std::vector<double> sorted(smoothed.cbegin(), smoothed.cend());
    std::sort(sorted.begin(), sorted.end());
    const double center = medianOfSorted(sorted);
    m_periodicRangeKPa = quantileOfSorted(sorted, 0.95)
        - quantileOfSorted(sorted, 0.05);
    m_periodicCandidate = m_periodicRangeKPa >= kMinimumPeriodicRangeKPa;

    QVector<double> risingCrossingsMs;
    if (m_periodicCandidate) {
        const double hysteresis = qMax(0.02, m_periodicRangeKPa * 0.12);
        bool crossingArmed = false;
        for (int index = 1; index < smoothed.size(); ++index) {
            if (smoothed.at(index) <= center - hysteresis)
                crossingArmed = true;
            if (!crossingArmed || smoothed.at(index - 1) >= center
                || smoothed.at(index) < center) {
                continue;
            }
            const double valueSpan = smoothed.at(index) - smoothed.at(index - 1);
            const double fraction = valueSpan > 0.0
                ? (center - smoothed.at(index - 1)) / valueSpan : 0.0;
            const double crossingSeconds = times.at(index - 1)
                + fraction * (times.at(index) - times.at(index - 1));
            const double crossingMs = originMs + crossingSeconds * 1000.0;
            if (risingCrossingsMs.isEmpty()
                || (crossingMs - risingCrossingsMs.last()) / 1000.0
                    >= kMinimumPeriodSeconds) {
                risingCrossingsMs.push_back(crossingMs);
            }
            crossingArmed = false;
        }
    }

    QVector<double> cycleMeans;
    QVector<double> cycleMidpointsMs;
    QVector<double> cyclePeriods;
    for (int index = 1; index < risingCrossingsMs.size(); ++index) {
        const double startMs = risingCrossingsMs.at(index - 1);
        const double endMs = risingCrossingsMs.at(index);
        const double periodSeconds = (endMs - startMs) / 1000.0;
        if (periodSeconds < kMinimumPeriodSeconds
            || periodSeconds > kMaximumPeriodSeconds) {
            continue;
        }
        cycleMeans.push_back(precisionMean(qRound64(startMs), qRound64(endMs)));
        cycleMidpointsMs.push_back((startMs + endMs) * 0.5);
        cyclePeriods.push_back(periodSeconds);
    }

    if (cycleMeans.size() >= kRequiredCompleteCycles) {
        std::vector<double> sortedPeriods(cyclePeriods.cbegin(), cyclePeriods.cend());
        std::sort(sortedPeriods.begin(), sortedPeriods.end());
        const double detectedPeriod = medianOfSorted(sortedPeriods);
        m_detectedPeriodSeconds = m_detectedPeriodSeconds > 0.0
            ? m_detectedPeriodSeconds * 0.65 + detectedPeriod * 0.35
            : detectedPeriod;

        const int firstCycle = qMax(0, cycleMeans.size() - 5);
        std::vector<double> recentCycleMeans;
        recentCycleMeans.reserve(cycleMeans.size() - firstCycle);
        for (int index = firstCycle; index < cycleMeans.size(); ++index)
            recentCycleMeans.push_back(cycleMeans.at(index));
        std::sort(recentCycleMeans.begin(), recentCycleMeans.end());
        // The median of complete-cycle centres is robust to the first cycle
        // after a pressure transition.  No slope is estimated or projected.
        m_precisionBaselineKPa = medianOfSorted(recentCycleMeans);
        m_precisionModelTimestampMs = timestampMs;
        m_precisionProgress = 1.0;
        m_precisionReady = true;
        return;
    }

    const qint64 historySpanMs = timestampMs - m_precision.front().timestampMs;
    m_precisionProgress = m_periodicCandidate
        ? qBound(0.0, risingCrossingsMs.size() / 3.0, 0.99)
        : qBound(0.0, historySpanMs / static_cast<double>(kAperiodicWindowMs), 1.0);

    // A period verified by zero calibration remains valid even if the retained
    // window starts near a centre crossing and therefore exposes only one
    // detected crossing-to-crossing interval. Recalculate two exact period
    // centres on every model refresh so readiness cannot fall back to raw data.
    if (m_periodicCandidate && m_detectedPeriodSeconds > 0.0) {
        const qint64 periodMs = qRound64(m_detectedPeriodSeconds * 1000.0);
        if (historySpanMs >= periodMs * kRequiredCompleteCycles) {
            const double previous = precisionMean(timestampMs - periodMs * 2,
                                                  timestampMs - periodMs);
            const double current = precisionMean(timestampMs - periodMs,
                                                 timestampMs);
            m_precisionBaselineKPa = (previous + current) * 0.5;
            m_precisionModelTimestampMs = timestampMs;
            m_precisionProgress = 1.0;
            m_precisionReady = true;
            return;
        }
    }

    // Quiet, non-periodic noise needs no long wait. A timestamp-weighted eight
    // second mean gives a stable indication without pretending that a partial
    // long sine wave has already converged.
    if (!m_periodicCandidate && historySpanMs >= kAperiodicWindowMs) {
        m_precisionBaselineKPa = precisionMean(timestampMs - kAperiodicWindowMs,
                                               timestampMs);
        m_precisionModelTimestampMs = timestampMs;
        m_precisionReady = true;
    } else {
        m_precisionReady = false;
    }
}

void PressureSignalProcessor::resetPrecisionForMotion(double value,
                                                      qint64 timestampMs)
{
    m_precision.clear();
    m_precision.enqueue({timestampMs, value});
    m_precisionProgress = 0.0;
    m_periodicRangeKPa = 0.0;
    m_precisionBaselineKPa = recentMedian(5);
    m_precisionModelTimestampMs = timestampMs;
    m_lastPeriodEvaluationMs = timestampMs;
    m_precisionReady = false;
    m_periodicCandidate = false;
}

double PressureSignalProcessor::precisionEstimate() const
{
    if (!m_precisionReady || m_precisionModelTimestampMs < 0)
        return recentMedian(5);
    return m_precisionBaselineKPa;
}

void PressureSignalProcessor::primePeriodicHistory(const QVector<double> &values,
                                                   const QVector<qint64> &timestamps,
                                                   double detectedPeriodSeconds)
{
    reset();
    if (values.isEmpty() || values.size() != timestamps.size())
        return;

    for (int index = 0; index < values.size(); ++index) {
        if (!qIsFinite(values.at(index)))
            continue;
        if (m_lastTimestampMs >= 0 && timestamps.at(index) <= m_lastTimestampMs)
            continue;
        appendRecent(values.at(index));
        appendPrecision(values.at(index), timestamps.at(index));
        appendStrongObservation(values.at(index), timestamps.at(index));
        m_lastTimestampMs = timestamps.at(index);
    }
    if (m_precision.isEmpty()) {
        reset();
        return;
    }

    m_iirBalanced = m_precision.back().value;
    m_iirStrong = m_precision.back().value;
    m_initialized = true;
    if (detectedPeriodSeconds >= kMinimumPeriodSeconds
        && detectedPeriodSeconds <= kMaximumPeriodSeconds) {
        m_detectedPeriodSeconds = detectedPeriodSeconds;
        m_periodicCandidate = true;
    }
    updatePrecisionModel(m_lastTimestampMs, true);
    updateStrongPeriodicModel(m_lastTimestampMs, true);

    // Crossing detection can miss a boundary when the retained interval begins
    // exactly around the waveform centre. The zero-calibration stage already
    // supplies a verified period, so use two exact period windows as a safe
    // fallback rather than throwing away that traceable evidence.
    if (!m_precisionReady && m_detectedPeriodSeconds > 0.0) {
        const qint64 periodMs = qRound64(m_detectedPeriodSeconds * 1000.0);
        const qint64 spanMs = m_precision.back().timestampMs
            - m_precision.front().timestampMs;
        if (spanMs >= periodMs * kRequiredCompleteCycles) {
            const double previous = precisionMean(m_lastTimestampMs - periodMs * 2,
                                                  m_lastTimestampMs - periodMs);
            const double current = precisionMean(m_lastTimestampMs - periodMs,
                                                 m_lastTimestampMs);
            m_precisionBaselineKPa = (previous + current) * 0.5;
            m_precisionModelTimestampMs = m_lastTimestampMs;
            m_precisionProgress = 1.0;
            m_precisionReady = true;
        }
    }
}
