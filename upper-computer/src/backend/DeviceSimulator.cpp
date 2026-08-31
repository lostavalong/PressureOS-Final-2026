#include "DeviceSimulator.h"
#include "SerialDeviceGateway.h"

#include <QDateTime>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace {
constexpr int kSeriesCapacity = 600;
constexpr double kStableP2PLimitKPa = 0.42;
constexpr double kStabilityWindowSeconds = 3.0;
constexpr qint64 kDuplicateHoldMs = 350;
constexpr qint64 kHardwareRateWindowMs = 5000;
constexpr int kZeroSimulationMinimumDurationMs = 25000;
constexpr int kZeroSimulationMaximumDurationMs = 40000;
constexpr int kZeroHardwareMinimumDurationMs = 60000;
constexpr int kZeroHardwareMaximumDurationMs = 180000;
constexpr double kZeroMaximumStandardErrorKPa = 0.03;
constexpr double kZeroMaximumSlopeKPaPerSecond = 0.003;
constexpr double kZeroMaximumSegmentSpreadKPa = 0.08;
constexpr double kZeroPeriodicSignalRangeKPa = 0.12;
constexpr int kZeroMinimumCompleteCycles = 2;
constexpr double kZeroMinimumCycleSeconds = 4.0;
constexpr double kZeroMaximumCycleSeconds = 90.0;
constexpr double kZeroSmoothingHalfWindowSeconds = 0.35;

struct ZeroStatistics {
    bool valid = false;
    bool periodicSignalDetected = false;
    bool periodicAnalysisReady = false;
    int sampleCount = 0;
    int cycleCount = 0;
    double mean = 0.0;
    double standardDeviation = 0.0;
    double peakToPeak = 0.0;
    double standardError = 0.0;
    double slope = 0.0;
    double segmentSpread = 0.0;
    double detectedPeriodSeconds = 0.0;
};

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

double interpolatedValue(const QVector<double> &values, const QVector<double> &times,
                         double timestamp)
{
    if (values.isEmpty() || values.size() != times.size())
        return 0.0;
    if (timestamp <= times.first())
        return values.first();
    if (timestamp >= times.last())
        return values.last();

    const auto upper = std::lower_bound(times.cbegin(), times.cend(), timestamp);
    const int upperIndex = static_cast<int>(std::distance(times.cbegin(), upper));
    const int lowerIndex = qMax(0, upperIndex - 1);
    const double span = times.at(upperIndex) - times.at(lowerIndex);
    if (span <= 0.0)
        return values.at(lowerIndex);
    const double fraction = (timestamp - times.at(lowerIndex)) / span;
    return values.at(lowerIndex)
        + fraction * (values.at(upperIndex) - values.at(lowerIndex));
}

double timeWeightedMean(const QVector<double> &values, const QVector<double> &times,
                        double startTime, double endTime)
{
    if (values.size() < 2 || values.size() != times.size() || endTime <= startTime)
        return values.isEmpty() ? 0.0 : values.last();

    startTime = qMax(startTime, times.first());
    endTime = qMin(endTime, times.last());
    if (endTime <= startTime)
        return interpolatedValue(values, times, endTime);

    double previousTime = startTime;
    double previousValue = interpolatedValue(values, times, startTime);
    double integral = 0.0;
    for (int index = 0; index < times.size(); ++index) {
        const double currentTime = times.at(index);
        if (currentTime <= startTime)
            continue;
        if (currentTime >= endTime)
            break;
        const double currentValue = values.at(index);
        integral += (previousValue + currentValue) * 0.5
            * (currentTime - previousTime);
        previousTime = currentTime;
        previousValue = currentValue;
    }

    const double endValue = interpolatedValue(values, times, endTime);
    integral += (previousValue + endValue) * 0.5 * (endTime - previousTime);
    return integral / (endTime - startTime);
}

void fitTimedMeans(const QVector<double> &means, const QVector<double> &midpoints,
                   double evaluationTime, double *valueAtEvaluation, double *slope,
                   double *residualSpread, double *standardError)
{
    if (!valueAtEvaluation || !slope || !residualSpread || !standardError
        || means.isEmpty() || means.size() != midpoints.size()) {
        return;
    }

    const double meanValue = std::accumulate(means.cbegin(), means.cend(), 0.0)
        / static_cast<double>(means.size());
    const double meanTime = std::accumulate(midpoints.cbegin(), midpoints.cend(), 0.0)
        / static_cast<double>(midpoints.size());
    double timeVariance = 0.0;
    double covariance = 0.0;
    for (int index = 0; index < means.size(); ++index) {
        const double timeDelta = midpoints.at(index) - meanTime;
        timeVariance += timeDelta * timeDelta;
        covariance += timeDelta * (means.at(index) - meanValue);
    }
    *slope = timeVariance > 0.0 ? covariance / timeVariance : 0.0;
    *valueAtEvaluation = meanValue + *slope * (evaluationTime - meanTime);

    QVector<double> residuals;
    residuals.reserve(means.size());
    double squaredResidualSum = 0.0;
    for (int index = 0; index < means.size(); ++index) {
        const double predicted = meanValue + *slope * (midpoints.at(index) - meanTime);
        const double residual = means.at(index) - predicted;
        residuals.push_back(residual);
        squaredResidualSum += residual * residual;
    }
    const auto bounds = std::minmax_element(residuals.cbegin(), residuals.cend());
    *residualSpread = residuals.size() >= 2 ? *bounds.second - *bounds.first : 0.0;
    const double residualDeviation = std::sqrt(squaredResidualSum
        / static_cast<double>(qMax(1, residuals.size() - 2)));
    *standardError = residualDeviation
        / std::sqrt(static_cast<double>(qMax(1, residuals.size())));
}

ZeroStatistics calculateZeroStatistics(const QVector<double> &samples,
                                       const QVector<qint64> &timestamps)
{
    ZeroStatistics result;
    if (samples.size() < 20 || samples.size() != timestamps.size())
        return result;

    std::vector<double> sorted(samples.cbegin(), samples.cend());
    std::sort(sorted.begin(), sorted.end());
    const double median = medianOfSorted(sorted);

    std::vector<double> deviations;
    deviations.reserve(sorted.size());
    for (double value : sorted)
        deviations.push_back(qAbs(value - median));
    std::sort(deviations.begin(), deviations.end());
    const double robustSigma = 1.4826 * medianOfSorted(deviations);
    const double inlierLimit = qMax(0.05, robustSigma * 4.5);

    QVector<double> inlierValues;
    QVector<double> inlierTimes;
    inlierValues.reserve(samples.size());
    inlierTimes.reserve(samples.size());
    const qint64 firstTimestamp = timestamps.first();
    for (int index = 0; index < samples.size(); ++index) {
        if (qAbs(samples.at(index) - median) <= inlierLimit) {
            inlierValues.push_back(samples.at(index));
            inlierTimes.push_back((timestamps.at(index) - firstTimestamp) / 1000.0);
        }
    }
    if (inlierValues.size() < 20)
        return result;

    result.sampleCount = inlierValues.size();
    result.mean = timeWeightedMean(inlierValues, inlierTimes,
                                   inlierTimes.first(), inlierTimes.last());

    double squaredSum = 0.0;
    for (double value : inlierValues) {
        const double delta = value - result.mean;
        squaredSum += delta * delta;
    }
    result.standardDeviation = std::sqrt(squaredSum
        / static_cast<double>(qMax(1, inlierValues.size() - 1)));
    result.standardError = result.standardDeviation
        / std::sqrt(static_cast<double>(inlierValues.size()));

    std::vector<double> sortedInliers(inlierValues.cbegin(), inlierValues.cend());
    std::sort(sortedInliers.begin(), sortedInliers.end());
    const int trim = sortedInliers.size() >= 50
        ? qMax(1, static_cast<int>(sortedInliers.size() / 50)) : 0;
    result.peakToPeak = sortedInliers.at(sortedInliers.size() - 1 - trim)
        - sortedInliers.at(trim);

    QVector<double> smoothed;
    smoothed.resize(inlierValues.size());
    int left = 0;
    int right = 0;
    double windowSum = 0.0;
    for (int index = 0; index < inlierValues.size(); ++index) {
        const double centerTime = inlierTimes.at(index);
        while (right < inlierValues.size()
               && inlierTimes.at(right) <= centerTime + kZeroSmoothingHalfWindowSeconds) {
            windowSum += inlierValues.at(right);
            ++right;
        }
        while (left < right
               && inlierTimes.at(left) < centerTime - kZeroSmoothingHalfWindowSeconds) {
            windowSum -= inlierValues.at(left);
            ++left;
        }
        smoothed[index] = windowSum / static_cast<double>(qMax(1, right - left));
    }

    std::vector<double> sortedSmoothed(smoothed.cbegin(), smoothed.cend());
    std::sort(sortedSmoothed.begin(), sortedSmoothed.end());
    const double waveformCenter = medianOfSorted(sortedSmoothed);
    const double robustWaveformRange = quantileOfSorted(sortedSmoothed, 0.95)
        - quantileOfSorted(sortedSmoothed, 0.05);
    result.periodicSignalDetected = robustWaveformRange >= kZeroPeriodicSignalRangeKPa;

    QVector<double> risingCrossings;
    const double hysteresis = qMax(0.02, robustWaveformRange * 0.12);
    bool crossingArmed = false;
    for (int index = 1; index < smoothed.size(); ++index) {
        if (smoothed.at(index) <= waveformCenter - hysteresis)
            crossingArmed = true;
        if (!crossingArmed || smoothed.at(index - 1) >= waveformCenter
            || smoothed.at(index) < waveformCenter) {
            continue;
        }

        const double valueSpan = smoothed.at(index) - smoothed.at(index - 1);
        const double fraction = valueSpan > 0.0
            ? (waveformCenter - smoothed.at(index - 1)) / valueSpan : 0.0;
        const double crossing = inlierTimes.at(index - 1)
            + fraction * (inlierTimes.at(index) - inlierTimes.at(index - 1));
        if (risingCrossings.isEmpty()
            || crossing - risingCrossings.last() >= kZeroMinimumCycleSeconds) {
            risingCrossings.push_back(crossing);
        }
        crossingArmed = false;
    }

    QVector<double> cycleMeans;
    QVector<double> cycleMidpoints;
    QVector<double> cyclePeriods;
    for (int index = 1; index < risingCrossings.size(); ++index) {
        const double start = risingCrossings.at(index - 1);
        const double end = risingCrossings.at(index);
        const double period = end - start;
        if (period < kZeroMinimumCycleSeconds || period > kZeroMaximumCycleSeconds)
            continue;
        cycleMeans.push_back(timeWeightedMean(inlierValues, inlierTimes, start, end));
        cycleMidpoints.push_back((start + end) * 0.5);
        cyclePeriods.push_back(period);
    }
    result.cycleCount = cycleMeans.size();
    result.periodicAnalysisReady = result.periodicSignalDetected
        && result.cycleCount >= kZeroMinimumCompleteCycles;

    if (result.periodicAnalysisReady) {
        std::vector<double> sortedPeriods(cyclePeriods.cbegin(), cyclePeriods.cend());
        std::sort(sortedPeriods.begin(), sortedPeriods.end());
        result.detectedPeriodSeconds = medianOfSorted(sortedPeriods);
        double fittedStandardError = 0.0;
        fitTimedMeans(cycleMeans, cycleMidpoints, inlierTimes.last(),
                      &result.mean, &result.slope, &result.segmentSpread,
                      &fittedStandardError);
        result.standardError = qMax(result.standardError, fittedStandardError);
    } else {
        // With no material periodic component, use equal-duration blocks. When a
        // large periodic component is present this remains diagnostic only; the
        // caller waits until enough real cycles have been observed.
        constexpr int segmentCount = 5;
        const double duration = inlierTimes.last() - inlierTimes.first();
        QVector<double> segmentMeans;
        QVector<double> segmentMidpoints;
        segmentMeans.reserve(segmentCount);
        segmentMidpoints.reserve(segmentCount);
        for (int segment = 0; segment < segmentCount; ++segment) {
            const double start = inlierTimes.first()
                + duration * segment / static_cast<double>(segmentCount);
            const double end = inlierTimes.first()
                + duration * (segment + 1) / static_cast<double>(segmentCount);
            segmentMeans.push_back(timeWeightedMean(inlierValues, inlierTimes, start, end));
            segmentMidpoints.push_back((start + end) * 0.5);
        }
        double fittedStandardError = 0.0;
        fitTimedMeans(segmentMeans, segmentMidpoints, inlierTimes.last(),
                      &result.mean, &result.slope, &result.segmentSpread,
                      &fittedStandardError);
        result.standardError = qMax(result.standardError, fittedStandardError);
    }
    result.valid = true;
    return result;
}

int zeroDurationFromEnvironment(const char *name, int fallback, int minimum, int maximum)
{
    bool ok = false;
    const int configured = qEnvironmentVariableIntValue(name, &ok);
    return ok ? qBound(minimum, configured, maximum) : fallback;
}
}

DeviceSimulator::DeviceSimulator(QObject *parent)
    : QObject(parent)
{
    m_series.reserve(kSeriesCapacity);
    m_rawSeries.reserve(kSeriesCapacity);
    for (int i = 0; i < 120; ++i) {
        const double value = 524.15 + i * 0.004 + qSin(i * 0.18) * 0.10;
        m_series.push_back(value);
        m_rawSeries.push_back(value + qSin(i * 0.55) * 0.06);
    }

    m_runtime.start();
    m_sampleTimer.setTimerType(Qt::PreciseTimer);
    m_sampleTimer.setInterval(20);
    connect(&m_sampleTimer, &QTimer::timeout, this, &DeviceSimulator::generateSample);
    m_sampleTimer.start();
}

bool DeviceSimulator::connected() const
{
    return !m_hardwareMode || (m_serialGateway && m_serialGateway->connected());
}

QString DeviceSimulator::transportName() const
{
    if (!m_hardwareMode)
        return QStringLiteral("内置模拟器 · 50 Hz");
    if (!m_serialGateway)
        return QStringLiteral("USB 串口 · 未配置");
    if (m_serialGateway->portName().isEmpty())
        return QStringLiteral("USB 串口 · 自动发现");
    return QStringLiteral("USB 串口 · %1").arg(QFileInfo(m_serialGateway->portName()).fileName());
}

QString DeviceSimulator::sourceName() const
{
    return m_hardwareMode ? QStringLiteral("真实下位机") : QStringLiteral("Demo 模拟器");
}

void DeviceSimulator::attachSerialGateway(SerialDeviceGateway *gateway)
{
    if (!gateway || m_serialGateway == gateway)
        return;

    m_serialGateway = gateway;
    m_hardwareMode = true;
    m_valueTrustedForSafety = qEnvironmentVariableIntValue("PRESSUREOS_VALUE_TRUSTED") == 1;
    m_zeroCalibrationMinimumDurationMs = kZeroHardwareMinimumDurationMs;
    m_zeroCalibrationMaximumDurationMs = kZeroHardwareMaximumDurationMs;
    m_sampleTimer.stop();
    m_series.clear();
    m_rawSeries.clear();
    m_hardwareSampleTimes.clear();
    m_signalProcessor.reset();
    m_pressureKPa = 0.0;
    m_rawPressureKPa = 0.0;
    m_filteredPressureKPa = 0.0;
    m_temperature = 0.0;
    m_stabilityP2P = 0.0;
    m_pressureRateKPaPerSec = 0.0;
    m_sampleRate = 0;
    m_lastMeasurementTimestampMs = -1;
    m_lastPressureAcceptedAtMs = -1;
    m_lastTemperatureAcceptedAtMs = -1;
    m_hasMeasurement = false;
    m_stable = false;

    connect(gateway, &SerialDeviceGateway::pressureCodeReady,
            this, &DeviceSimulator::ingestPressureCode);
    connect(gateway, &SerialDeviceGateway::temperatureCodeReady,
            this, &DeviceSimulator::ingestTemperatureCode);
    connect(gateway, &SerialDeviceGateway::connectionChanged,
            this, &DeviceSimulator::relayGatewayState);
    connect(gateway, &SerialDeviceGateway::userMessage,
            this, &DeviceSimulator::userMessage);

    emit measurementChanged();
    emit seriesChanged();
    emit connectionChanged();
}

double DeviceSimulator::rangePercent() const
{
    return qBound(0.0, (m_pressureKPa + 100.0) / 700.0 * 100.0, 100.0);
}

double DeviceSimulator::utilizationPercent() const
{
    const double positive = m_pressureKPa > 0.0 ? m_pressureKPa / rangeMaxKPa() : 0.0;
    const double negative = m_pressureKPa < 0.0 ? -m_pressureKPa / qAbs(rangeMinKPa()) : 0.0;
    return qBound(0.0, qMax(positive, negative) * 100.0, 120.0);
}

QString DeviceSimulator::formattedPressure() const
{
    return formatValue(m_pressureKPa);
}

QString DeviceSimulator::formattedRawPressure() const
{
    return formatValue(m_rawPressureKPa);
}

QVariantList DeviceSimulator::series() const
{
    QVariantList result;
    result.reserve(m_series.size());
    for (double value : m_series)
        result.push_back(value);
    return result;
}

QVariantList DeviceSimulator::rawSeries() const
{
    QVariantList result;
    result.reserve(m_rawSeries.size());
    for (double value : m_rawSeries)
        result.push_back(value);
    return result;
}

QVariantList DeviceSimulator::unitOptions() const
{
    return {QStringLiteral("Pa"), QStringLiteral("kPa"), QStringLiteral("MPa"),
            QStringLiteral("bar"), QStringLiteral("mbar"), QStringLiteral("psi")};
}

QVariantList DeviceSimulator::filterOptions() const
{
    return {QStringLiteral("周期抑制 · 精密"),
            QStringLiteral("稳态周期锁定 · 强抑制"),
            QStringLiteral("无滤波（原始）"),
            QStringLiteral("中值 · 5点"),
            QStringLiteral("滑动平均 · 10点"), QStringLiteral("IIR · 平衡"),
            QStringLiteral("IIR · 强抑制")};
}

bool DeviceSimulator::canZero() const
{
    return !m_recording && !m_tripLatched && !m_zeroCalibrationActive
        && m_hasMeasurement && m_stable
        && (!m_hardwareMode || (m_serialGateway && m_serialGateway->dataFresh()
                                && m_serialGateway->protocolIntegrityAvailable()
                                && m_sampleRate >= 10))
        && m_stabilityP2P < kStableP2PLimitKPa
        && qAbs(m_pressureKPa) <= 2.0;
}

QString DeviceSimulator::safetyTitle() const
{
    if (m_hardwareMode && (!m_serialGateway || !m_serialGateway->dataFresh()))
        return QStringLiteral("等待下位机实时数据");
    if (m_hardwareMode && !m_valueTrustedForSafety)
        return QStringLiteral("量程状态监测");
    if (m_safetyLevel == QStringLiteral("trip")) return QStringLiteral("高压风险报警");
    if (m_safetyLevel == QStringLiteral("warning")) return QStringLiteral("压力接近量程边界");
    if (m_safetyLevel == QStringLiteral("caution")) return QStringLiteral("量程余量正在减小");
    return QStringLiteral("测量处于安全区间");
}

QString DeviceSimulator::safetyMessage() const
{
    if (m_hardwareMode && (!m_serialGateway || !m_serialGateway->dataFresh()))
        return QStringLiteral("下位机数据已中断或尚未到达；当前读数不得用于记录与安全判断。");
    if (m_hardwareMode && !m_valueTrustedForSafety)
        return QStringLiteral("实时示值已接入；量程颜色用于趋势提示，正式标定版本仍待确认。");
    if (m_safetyLevel == QStringLiteral("trip"))
        return QStringLiteral("高风险报警已锁存；请立即人工停止加压并缓慢泄压，本设备不执行自动切断。");
    if (m_safetyLevel == QStringLiteral("warning"))
        return QStringLiteral("请立即停止继续加压并准备泄压；系统正在预测量程越界风险。");
    if (m_safetyLevel == QStringLiteral("caution"))
        return QStringLiteral("已超过量程的 80%，请放慢加压速度并持续观察趋势。");
    return QStringLiteral("量程、变化率与采样状态正常。");
}

void DeviceSimulator::cycleUnit()
{
    const QVariantList options = unitOptions();
    int index = options.indexOf(m_unit);
    setUnit(options.at((index + 1) % options.size()).toString());
}

void DeviceSimulator::setUnit(const QString &unit)
{
    if (!unitOptions().contains(unit) || unit == m_unit)
        return;
    m_unit = unit;
    emit unitChanged();
    emit measurementChanged();
    emit userMessage(QStringLiteral("显示单位已切换为 %1").arg(unit));
}

void DeviceSimulator::cycleFilter()
{
    const QVariantList options = filterOptions();
    int index = options.indexOf(m_filterName);
    setFilter(options.at((index + 1) % options.size()).toString());
}

void DeviceSimulator::setFilter(const QString &filter)
{
    if (!filterOptions().contains(filter) || filter == m_filterName)
        return;
    m_filterName = filter;
    m_signalProcessor.reset();
    emit filterChanged();
    if (m_filterName.startsWith(QStringLiteral("稳态周期锁定"))) {
        emit userMessage(QStringLiteral("强周期锁定已启动：连续确认规律振荡后压制，压力变化时自动解锁"));
    } else if (m_filterName.startsWith(QStringLiteral("周期抑制"))) {
        emit userMessage(QStringLiteral("精密滤波已启动：自动识别振荡周期，真实压力变化时快速跟随"));
    } else {
        emit userMessage(QStringLiteral("滤波方案：%1").arg(m_filterName));
    }
}

bool DeviceSimulator::zero()
{
    return startZeroCalibration();
}

bool DeviceSimulator::startZeroCalibration()
{
    if (m_zeroCalibrationActive)
        return true;
    if (m_recording) {
        emit userMessage(QStringLiteral("正在记录，不能修改零点；请先停止记录"));
        return false;
    }
    if (m_tripLatched) {
        emit userMessage(QStringLiteral("高压报警尚未人工复位，禁止执行零点校正"));
        return false;
    }
    if (m_hardwareMode && (!m_serialGateway || !m_serialGateway->dataFresh())) {
        emit userMessage(QStringLiteral("下位机数据尚未持续到达，不能开始零点采集"));
        return false;
    }
    if (m_hardwareMode && !m_serialGateway->protocolIntegrityAvailable()) {
        emit userMessage(QStringLiteral("协议完整性尚未锁定，不能开始正式零点校正"));
        return false;
    }
    if (m_hardwareMode && m_sampleRate < 10) {
        emit userMessage(QStringLiteral("采样率低于 10 Hz，不能开始正式零点校正"));
        return false;
    }
    if (!m_stable || m_stabilityP2P >= kStableP2PLimitKPa || qAbs(m_pressureKPa) > 2.0) {
        emit userMessage(QStringLiteral("零点条件未满足：请先卸压连通大气，并等待完整稳定窗口"));
        return false;
    }

    const int defaultMinimumDuration = m_hardwareMode
        ? kZeroHardwareMinimumDurationMs : kZeroSimulationMinimumDurationMs;
    const int defaultMaximumDuration = m_hardwareMode
        ? kZeroHardwareMaximumDurationMs : kZeroSimulationMaximumDurationMs;
    m_zeroCalibrationMinimumDurationMs = zeroDurationFromEnvironment(
        "PRESSUREOS_ZERO_MIN_MS", defaultMinimumDuration, 500, 90000);
    m_zeroCalibrationMaximumDurationMs = zeroDurationFromEnvironment(
        "PRESSUREOS_ZERO_MAX_MS", defaultMaximumDuration,
        m_zeroCalibrationMinimumDurationMs, 240000);
    m_zeroCalibrationSamples.clear();
    m_zeroCalibrationTimes.clear();
    m_zeroCalibrationSamples.reserve(qMax(500, m_sampleRate * 210));
    m_zeroCalibrationTimes.reserve(qMax(500, m_sampleRate * 210));
    m_zeroCalibrationStartedAtMs = -1;
    m_zeroCalibrationLastEvaluationAtMs = -1;
    m_zeroCalibrationProgress = 0;
    m_zeroCalibrationElapsedSeconds = 0;
    m_zeroCalibrationSampleCount = 0;
    m_zeroCalibrationCycleCount = 0;
    m_zeroCalibrationMeanKPa = 0.0;
    m_zeroCalibrationStdDevKPa = 0.0;
    m_zeroCalibrationP2PKPa = 0.0;
    m_zeroCalibrationStandardErrorKPa = 0.0;
    m_zeroCalibrationSlopeKPaPerSec = 0.0;
    m_zeroCalibrationSegmentSpreadKPa = 0.0;
    m_zeroCalibrationDetectedPeriodSeconds = 0.0;
    m_zeroCalibrationStatus = QStringLiteral("等待首个有效采样帧");
    m_zeroCalibrationStartCrcErrors = m_serialGateway ? m_serialGateway->crcErrors() : 0;
    m_zeroCalibrationStartDroppedFrames = m_serialGateway ? m_serialGateway->droppedFrames() : 0;
    m_zeroCalibrationStartInvalidFrames = m_serialGateway ? m_serialGateway->invalidFrames() : 0;
    m_zeroCalibrationActive = true;
    emit zeroCalibrationChanged();
    emit measurementChanged();
    emit userMessage(QStringLiteral("零点统计采集已开始：%1 秒首次判定，长周期时自动延长")
                         .arg(zeroCalibrationTargetSeconds()));
    return true;
}

void DeviceSimulator::cancelZeroCalibration()
{
    if (!m_zeroCalibrationActive)
        return;
    m_zeroCalibrationActive = false;
    m_zeroCalibrationStatus = QStringLiteral("采集已取消，零点偏移未改变");
    emit zeroCalibrationChanged();
    emit measurementChanged();
    emit userMessage(m_zeroCalibrationStatus);
}

void DeviceSimulator::failZeroCalibration(const QString &reason)
{
    if (!m_zeroCalibrationActive)
        return;
    m_zeroCalibrationActive = false;
    m_zeroCalibrationStatus = reason;
    emit zeroCalibrationChanged();
    emit measurementChanged();
    emit zeroCalibrationFailed(reason);
    emit userMessage(QStringLiteral("零点校正未写入：%1").arg(reason));
}

bool DeviceSimulator::evaluateZeroCalibration(bool finalAttempt)
{
    if (!m_zeroCalibrationActive || m_zeroCalibrationSamples.isEmpty())
        return false;

    // Keep every sample collected in this zero attempt. A short-cycle waveform
    // normally passes at the first 60-second evaluation; a thermally stretched
    // 45-60 second cycle needs the automatically extended history to expose at
    // least two complete cycles. A rolling 60-second cutoff would discard the
    // very evidence required for that decision.
    QVector<double> windowSamples;
    QVector<qint64> windowTimes;
    windowSamples.reserve(m_zeroCalibrationSamples.size());
    windowTimes.reserve(m_zeroCalibrationTimes.size());
    for (int index = 0; index < m_zeroCalibrationSamples.size(); ++index) {
        windowSamples.push_back(m_zeroCalibrationSamples.at(index));
        windowTimes.push_back(m_zeroCalibrationTimes.at(index));
    }

    const ZeroStatistics stats = calculateZeroStatistics(windowSamples, windowTimes);
    if (stats.valid) {
        m_zeroCalibrationSampleCount = stats.sampleCount;
        m_zeroCalibrationCycleCount = stats.cycleCount;
        m_zeroCalibrationMeanKPa = stats.mean;
        m_zeroCalibrationStdDevKPa = stats.standardDeviation;
        m_zeroCalibrationP2PKPa = stats.peakToPeak;
        m_zeroCalibrationStandardErrorKPa = stats.standardError;
        m_zeroCalibrationSlopeKPaPerSec = stats.slope;
        m_zeroCalibrationSegmentSpreadKPa = stats.segmentSpread;
        m_zeroCalibrationDetectedPeriodSeconds = stats.detectedPeriodSeconds;
    }

    QStringList reasons;
    const int minimumSamples = qMax(20, qCeil(10.0
        * m_zeroCalibrationMinimumDurationMs / 1000.0 * 0.9));
    if (!stats.valid || stats.sampleCount < minimumSamples)
        reasons.push_back(QStringLiteral("有效样本不足（%1/%2）")
                              .arg(stats.sampleCount).arg(minimumSamples));
    if (stats.valid && stats.periodicSignalDetected && !stats.periodicAnalysisReady) {
        reasons.push_back(QStringLiteral("完整振荡周期不足（%1/%2）")
                              .arg(stats.cycleCount).arg(kZeroMinimumCompleteCycles));
    }
    if (m_hardwareMode && m_sampleRate < 10)
        reasons.push_back(QStringLiteral("采样率低于 10 Hz"));
    if (stats.valid && qAbs(stats.mean) > 2.0)
        reasons.push_back(QStringLiteral("平均压力偏离零点"));
    if (stats.valid && stats.standardError > kZeroMaximumStandardErrorKPa)
        reasons.push_back(QStringLiteral("均值不确定度过大"));
    if (stats.valid && qAbs(stats.slope) > kZeroMaximumSlopeKPaPerSecond)
        reasons.push_back(QStringLiteral("零点仍存在趋势漂移"));
    if (stats.valid && stats.segmentSpread > kZeroMaximumSegmentSpreadKPa)
        reasons.push_back(QStringLiteral("各完整周期的中心值尚未收敛"));
    if (m_serialGateway) {
        if (m_serialGateway->crcErrors() != m_zeroCalibrationStartCrcErrors)
            reasons.push_back(QStringLiteral("采集期间出现 CRC 错误"));
        if (m_serialGateway->droppedFrames() != m_zeroCalibrationStartDroppedFrames)
            reasons.push_back(QStringLiteral("采集期间出现丢帧"));
        if (m_serialGateway->invalidFrames() != m_zeroCalibrationStartInvalidFrames)
            reasons.push_back(QStringLiteral("采集期间出现无效帧"));
    }

    emit zeroCalibrationChanged();
    if (!reasons.isEmpty()) {
        if (finalAttempt) {
            failZeroCalibration(reasons.join(QStringLiteral("；")));
        } else {
            m_zeroCalibrationStatus = QStringLiteral("稳定性复核中，已自动延长采集：%1")
                                          .arg(reasons.join(QStringLiteral("；")));
            emit zeroCalibrationChanged();
        }
        return false;
    }

    const double previousOffset = m_zeroOffset;
    const double absoluteCurrentPressure = m_rawPressureKPa + previousOffset;
    m_zeroOffset = stats.mean;
    const double correction = m_zeroOffset - previousOffset;
    m_rawPressureKPa = absoluteCurrentPressure - m_zeroOffset;
    m_filteredPressureKPa = 0.0;
    m_pressureKPa = 0.0;
    QVector<double> centredZeroHistory;
    centredZeroHistory.reserve(m_zeroCalibrationSamples.size());
    for (double sample : m_zeroCalibrationSamples)
        centredZeroHistory.push_back(sample - m_zeroOffset);
    // Reuse the traceable zero interval to prime the adaptive periodic filter.
    // The post-calibration display therefore starts from a verified cycle
    // centre instead of learning the same interference again from scratch.
    m_signalProcessor.primePeriodicHistory(centredZeroHistory,
                                           m_zeroCalibrationTimes,
                                           stats.detectedPeriodSeconds);
    m_series.clear();
    m_rawSeries.clear();
    if (!m_hardwareMode)
        m_targetKPa = m_zeroOffset;
    m_zeroCalibrationActive = false;
    m_zeroCalibrationProgress = 100;
    m_zeroCalibrationStatus = QStringLiteral("校正完成：自适应完整周期统计结果已写入");
    emit zeroOffsetChanged();
    emit zeroCalibrationChanged();
    emit zeroCalibrationCompleted(correction, m_zeroOffset);
    emit measurementChanged();
    emit seriesChanged();
    emit userMessage(QStringLiteral("零点校正完成：%1 个样本、%2 个完整周期，偏移 %3 kPa")
                         .arg(stats.sampleCount).arg(stats.cycleCount)
                         .arg(m_zeroOffset, 0, 'f', 4));
    return true;
}

void DeviceSimulator::collectZeroCalibrationSample(double uncorrectedPressureKPa,
                                                   qint64 timestampMs)
{
    if (!m_zeroCalibrationActive)
        return;
    if (!qIsFinite(uncorrectedPressureKPa) || qAbs(uncorrectedPressureKPa) > 2.0) {
        failZeroCalibration(QStringLiteral("采集期间压力离开零点范围（±2 kPa）"));
        return;
    }
    if (m_zeroCalibrationStartedAtMs < 0)
        m_zeroCalibrationStartedAtMs = timestampMs;
    if (!m_zeroCalibrationTimes.isEmpty() && timestampMs <= m_zeroCalibrationTimes.last())
        return;

    m_zeroCalibrationSamples.push_back(uncorrectedPressureKPa);
    m_zeroCalibrationTimes.push_back(timestampMs);
    const qint64 elapsedMs = timestampMs - m_zeroCalibrationStartedAtMs;
    m_zeroCalibrationElapsedSeconds = static_cast<int>(elapsedMs / 1000);
    m_zeroCalibrationSampleCount = m_zeroCalibrationSamples.size();
    m_zeroCalibrationProgress = qMin(99, static_cast<int>(elapsedMs * 100
        / qMax(1, m_zeroCalibrationMinimumDurationMs)));
    m_zeroCalibrationStatus = QStringLiteral("正在自适应识别振荡周期 · %1/%2 秒")
                                  .arg(qMin(m_zeroCalibrationElapsedSeconds + 1,
                                            qCeil(m_zeroCalibrationMinimumDurationMs / 1000.0)))
                                  .arg(qCeil(m_zeroCalibrationMinimumDurationMs / 1000.0));

    const bool shouldEvaluate = elapsedMs >= m_zeroCalibrationMinimumDurationMs
        && (m_zeroCalibrationLastEvaluationAtMs < 0
            || timestampMs - m_zeroCalibrationLastEvaluationAtMs >= 1000);
    if (shouldEvaluate) {
        m_zeroCalibrationLastEvaluationAtMs = timestampMs;
        evaluateZeroCalibration(elapsedMs >= m_zeroCalibrationMaximumDurationMs);
        return;
    }

    const int statisticsIntervalSamples = qMax(1, qMax(10, m_sampleRate) / 2);
    const bool updateStatistics = m_zeroCalibrationSamples.size()
        % statisticsIntervalSamples == 0;
    if (updateStatistics) {
        const ZeroStatistics interim = calculateZeroStatistics(m_zeroCalibrationSamples,
                                                                m_zeroCalibrationTimes);
        if (interim.valid) {
            m_zeroCalibrationCycleCount = interim.cycleCount;
            m_zeroCalibrationMeanKPa = interim.mean;
            m_zeroCalibrationStdDevKPa = interim.standardDeviation;
            m_zeroCalibrationP2PKPa = interim.peakToPeak;
            m_zeroCalibrationStandardErrorKPa = interim.standardError;
            m_zeroCalibrationSlopeKPaPerSec = interim.slope;
            m_zeroCalibrationSegmentSpreadKPa = interim.segmentSpread;
            m_zeroCalibrationDetectedPeriodSeconds = interim.detectedPeriodSeconds;
        }
        emit zeroCalibrationChanged();
    }
}

void DeviceSimulator::simulateVentToAtmosphere()
{
    if (m_hardwareMode) {
        emit userMessage(QStringLiteral("真实硬件模式：请人工停止加压并缓慢泄压至大气"));
        return;
    }
    m_targetKPa = m_zeroOffset;
    m_stable = false;
    emit measurementChanged();
    emit userMessage(QStringLiteral("Demo：压力源正在缓慢泄压至大气"));
}

bool DeviceSimulator::acknowledgeTrip()
{
    if (!m_tripLatched)
        return true;
    if (utilizationPercent() > 75.0) {
        emit userMessage(QStringLiteral("当前压力仍高于安全复位阈值，请先泄压至 75% 量程以下"));
        return false;
    }
    m_tripLatched = false;
    m_controlOutputEnabled = true;
    m_safetyLevel = QStringLiteral("normal");
    emit safetyChanged();
    emit userMessage(QStringLiteral("高压报警已人工复位，恢复测量操作"));
    return true;
}

void DeviceSimulator::toggleRecording()
{
    if (!m_recording && m_zeroCalibrationActive) {
        emit userMessage(QStringLiteral("零点统计采集中，完成或取消后才能开始记录"));
        return;
    }
    if (!m_recording && m_hardwareMode
        && (!m_serialGateway || !m_serialGateway->dataFresh())) {
        emit userMessage(QStringLiteral("尚未收到有效下位机数据，暂不能开始记录"));
        return;
    }
    m_recording = !m_recording;
    if (m_recording) {
        m_recordSeconds = 0;
        m_recordingRuntime.restart();
    }
    emit recordingChanged();
    emit userMessage(m_recording ? QStringLiteral("已开始记录测量数据")
                                 : QStringLiteral("记录已停止并写入数据库"));
}

void DeviceSimulator::setTarget(double targetKPa)
{
    if (m_tripLatched || !m_controlOutputEnabled) {
        emit userMessage(QStringLiteral("高压报警锁存中，禁止施加新的压力目标"));
        return;
    }
    if (m_hardwareMode) {
        Q_UNUSED(targetKPa)
        emit userMessage(QStringLiteral("真实硬件模式暂不发送控压命令，请人工施加工况后等待读数稳定"));
        return;
    }
    m_targetKPa = qBound(-100.0, targetKPa + m_zeroOffset, 600.0 + m_zeroOffset);
    m_stable = false;
    emit measurementChanged();
}

void DeviceSimulator::generateSample()
{
    const double seconds = m_runtime.elapsed() / 1000.0;
    const double approach = (m_targetKPa - (m_filteredPressureKPa + m_zeroOffset)) * 0.045;
    const double wave = qSin(seconds * 1.35) * 0.085 + qSin(seconds * 0.31) * 0.035;
    const double rawAbsolute = m_filteredPressureKPa + m_zeroOffset + approach + wave * 0.12 + boundedNoise();
    const double rawPressureKPa = rawAbsolute - m_zeroOffset;
    const double temperatureC = 24.6 + qSin(seconds * 0.075) * 0.14 + boundedNoise() * 0.03;
    processMeasurement(rawPressureKPa, temperatureC, QDateTime::currentMSecsSinceEpoch());
}

void DeviceSimulator::ingestPressureCode(quint32 rawCode, qint64 timestampMs)
{
    if (!m_hardwareMode)
        return;

    const bool legacyProtocol = !m_serialGateway
        || !m_serialGateway->protocolIntegrityAvailable();
    const bool duplicateBurst = legacyProtocol && m_lastPressureAcceptedAtMs >= 0
        && rawCode == m_lastPressureCode
        && timestampMs - m_lastPressureAcceptedAtMs < kDuplicateHoldMs;
    if (duplicateBurst)
        return;

    const double calibratedPressure = PressureSignalProcessor::calibratedPressure(rawCode);
    if (!qIsFinite(calibratedPressure) || qAbs(calibratedPressure) > 5000.0) {
        emit userMessage(QStringLiteral("压力原始码换算结果异常，已忽略本帧：%1").arg(rawCode));
        return;
    }

    m_lastPressureCode = rawCode;
    m_lastPressureAcceptedAtMs = timestampMs;
    updateHardwareSampleRate(timestampMs);
    processMeasurement(calibratedPressure - m_zeroOffset, m_temperature, timestampMs);
}

void DeviceSimulator::ingestTemperatureCode(quint32 rawCode, qint64 timestampMs)
{
    if (!m_hardwareMode)
        return;

    const bool legacyProtocol = !m_serialGateway
        || !m_serialGateway->protocolIntegrityAvailable();
    const bool duplicateBurst = legacyProtocol && m_lastTemperatureAcceptedAtMs >= 0
        && rawCode == m_lastTemperatureCode
        && timestampMs - m_lastTemperatureAcceptedAtMs < kDuplicateHoldMs;
    if (duplicateBurst)
        return;

    const double measuredTemperature = temperatureFromRawCode(rawCode);
    if (!qIsFinite(measuredTemperature) || measuredTemperature < -80.0 || measuredTemperature > 180.0) {
        emit userMessage(QStringLiteral("温度原始码换算结果异常，已忽略本帧：%1").arg(rawCode));
        return;
    }

    m_lastTemperatureCode = rawCode;
    m_lastTemperatureAcceptedAtMs = timestampMs;
    m_temperature = m_temperature == 0.0
        ? measuredTemperature : m_temperature * 0.8 + measuredTemperature * 0.2;
    emit measurementChanged();
}

void DeviceSimulator::relayGatewayState()
{
    if (m_hardwareMode && (!m_serialGateway || !m_serialGateway->dataFresh())) {
        m_stable = false;
        if (m_zeroCalibrationActive)
            failZeroCalibration(QStringLiteral("采集期间下位机数据中断"));
        emit measurementChanged();
    }
    emit connectionChanged();
}

void DeviceSimulator::processMeasurement(double rawPressureKPa, double temperatureC,
                                         qint64 timestampMs)
{
    ++m_tick;
    const bool hadMeasurement = m_hasMeasurement;
    const double previousSafetyPressure = hadMeasurement ? m_rawPressureKPa : rawPressureKPa;
    const double intervalSeconds = m_lastMeasurementTimestampMs >= 0
        ? qBound(0.001, (timestampMs - m_lastMeasurementTimestampMs) / 1000.0, 5.0)
        : (m_hardwareMode ? 0.5 : 0.02);
    m_lastMeasurementTimestampMs = timestampMs;

    const double uncorrectedPressureKPa = rawPressureKPa + m_zeroOffset;
    const bool zeroCalibrationWasActive = m_zeroCalibrationActive;
    collectZeroCalibrationSample(uncorrectedPressureKPa, timestampMs);
    const bool zeroCalibrationCompletedNow = zeroCalibrationWasActive
        && !m_zeroCalibrationActive && m_zeroCalibrationProgress == 100;
    rawPressureKPa = uncorrectedPressureKPa - m_zeroOffset;
    m_rawPressureKPa = rawPressureKPa;
    // The accepted zero interval already primes the signal processor including
    // this exact timestamp. Processing the same frame again would look like a
    // non-monotonic clock and reset the newly learned periodic model.
    if (zeroCalibrationCompletedNow) {
        m_filteredPressureKPa = 0.0;
    } else {
        m_filteredPressureKPa = m_signalProcessor.process(
            m_rawPressureKPa, timestampMs,
            PressureSignalProcessor::modeFromName(m_filterName));
    }
    m_hasMeasurement = true;
    if (qIsFinite(temperatureC) && temperatureC != 0.0)
        m_temperature = temperatureC;
    // The current project decision is ambient/room-temperature operation only.
    // Temperature remains a traceability/monitoring channel and must not alter the
    // pressure result until a real temperature-characterization experiment exists.
    m_pressureKPa = m_filteredPressureKPa;

    m_series.push_back(m_pressureKPa);
    m_rawSeries.push_back(m_rawPressureKPa);
    if (m_series.size() > kSeriesCapacity) {
        m_series.removeFirst();
        m_rawSeries.removeFirst();
    }

    updateStability();
    // Safety intentionally follows the calibrated raw path, not the long-window
    // precision indication. Filtering may improve readability but must never
    // delay an over-range warning or its rate prediction.
    updateSafety(previousSafetyPressure, intervalSeconds);
    if (m_recording && m_recordingRuntime.isValid()) {
        const int secondsRecorded = static_cast<int>(m_recordingRuntime.elapsed() / 1000);
        if (secondsRecorded != m_recordSeconds) {
            m_recordSeconds = secondsRecorded;
            emit recordingChanged();
        }
    }

    emit sampleReady(m_rawPressureKPa, m_filteredPressureKPa, m_pressureKPa,
                     m_temperature, timestampMs);
    emit measurementChanged();
    if (m_hardwareMode || m_tick % 3 == 0)
        emit seriesChanged();
}

void DeviceSimulator::updateHardwareSampleRate(qint64 timestampMs)
{
    m_hardwareSampleTimes.enqueue(timestampMs);
    while (!m_hardwareSampleTimes.isEmpty()
           && timestampMs - m_hardwareSampleTimes.head() > kHardwareRateWindowMs) {
        m_hardwareSampleTimes.dequeue();
    }

    int nextRate = 0;
    if (m_hardwareSampleTimes.size() >= 2) {
        const qint64 duration = m_hardwareSampleTimes.back() - m_hardwareSampleTimes.front();
        if (duration > 0) {
            nextRate = qMax(1, qRound((m_hardwareSampleTimes.size() - 1) * 1000.0
                                      / static_cast<double>(duration)));
        }
    }
    m_sampleRate = nextRate;
}

QString DeviceSimulator::formatValue(double kPa) const
{
    if (m_unit == QStringLiteral("Pa"))
        return QString::number(kPa * 1000.0, 'f', 0);
    if (m_unit == QStringLiteral("MPa"))
        return QString::number(kPa / 1000.0, 'f', 4);
    if (m_unit == QStringLiteral("bar"))
        return QString::number(kPa / 100.0, 'f', 3);
    if (m_unit == QStringLiteral("mbar"))
        return QString::number(kPa * 10.0, 'f', 1);
    if (m_unit == QStringLiteral("psi"))
        return QString::number(kPa * 0.1450377377, 'f', 3);
    // Keep the primary kPa indication detailed enough for calibration work.
    // This is display precision only; the verified effective resolution remains
    // a metrology result and is not inferred from the number of shown digits.
    return QString::number(kPa, 'f', 3);
}

void DeviceSimulator::updateStability()
{
    const int effectiveRate = m_hardwareMode ? qMax(1, m_sampleRate) : 50;
    const int targetWindow = qBound(10,
                                    qRound(effectiveRate * kStabilityWindowSeconds),
                                    kSeriesCapacity);
    const int window = qMin(targetWindow, m_series.size());
    if (window < 2) {
        m_stable = false;
        return;
    }
    auto begin = m_series.constEnd() - window;
    std::vector<double> sortedSamples(begin, m_series.constEnd());
    std::sort(sortedSamples.begin(), sortedSamples.end());
    const int trim = window >= 20 ? qMax(1, window / 20) : 0;
    m_stabilityP2P = sortedSamples.at(window - 1 - trim) - sortedSamples.at(trim);
    if (window < targetWindow) {
        m_stable = false;
        return;
    }
    const bool closeToTarget = m_hardwareMode
        || qAbs((m_pressureKPa + m_zeroOffset) - m_targetKPa) < 0.25;
    m_stable = closeToTarget && m_stabilityP2P < kStableP2PLimitKPa;
}

void DeviceSimulator::updateSafety(double previousPressureKPa, double intervalSeconds)
{
    const double instantaneousRate = intervalSeconds > 0.0
        ? (m_rawPressureKPa - previousPressureKPa) / intervalSeconds : 0.0;
    m_pressureRateKPaPerSec = m_pressureRateKPaPerSec * 0.92 + instantaneousRate * 0.08;
    if (m_hardwareMode && !m_valueTrustedForSafety && !m_tripLatched) {
        const bool levelChanged = m_safetyLevel != QStringLiteral("limited");
        m_safetyLevel = QStringLiteral("limited");
        if (levelChanged || m_tick % 5 == 0)
            emit safetyChanged();
        return;
    }
    const double positiveUtilization = m_rawPressureKPa > 0.0
        ? m_rawPressureKPa / rangeMaxKPa() : 0.0;
    const double negativeUtilization = m_rawPressureKPa < 0.0
        ? -m_rawPressureKPa / qAbs(rangeMinKPa()) : 0.0;
    const double utilization = qBound(0.0,
                                      qMax(positiveUtilization, negativeUtilization) * 100.0,
                                      120.0);
    const bool headingPositiveBoundary = m_pressureRateKPaPerSec > 1.5
        && m_rawPressureKPa + m_pressureRateKPaPerSec * 2.0 >= rangeMaxKPa();
    const bool headingNegativeBoundary = m_pressureRateKPaPerSec < -1.5
        && m_rawPressureKPa + m_pressureRateKPaPerSec * 2.0 <= rangeMinKPa();

    QString nextLevel = QStringLiteral("normal");
    if (m_tripLatched)
        nextLevel = QStringLiteral("trip");
    else if (utilization >= 98.0) {
        m_tripLatched = true;
        m_controlOutputEnabled = false;
        if (m_zeroCalibrationActive)
            failZeroCalibration(QStringLiteral("采集期间触发量程高风险报警"));
        if (!m_hardwareMode)
            m_targetKPa = m_zeroOffset;
        nextLevel = QStringLiteral("trip");
        if (m_recording) {
            m_recording = false;
            emit recordingChanged();
        }
        emit emergencyTripRequested(m_rawPressureKPa,
                                    QStringLiteral("压力达到 98% 高风险阈值"));
        emit userMessage(QStringLiteral("高压风险报警：请立即人工停止加压并缓慢泄压"));
    } else if (utilization >= 90.0 || headingPositiveBoundary || headingNegativeBoundary) {
        nextLevel = QStringLiteral("warning");
    } else if (utilization >= 80.0) {
        nextLevel = QStringLiteral("caution");
    }

    const bool levelChanged = nextLevel != m_safetyLevel;
    m_safetyLevel = nextLevel;
    if (levelChanged || m_tick % 5 == 0)
        emit safetyChanged();
}

double DeviceSimulator::temperatureFromRawCode(quint32 rawCode)
{
    const double value = static_cast<double>(rawCode);
    const double voltage = ((value / 8388608.0) - 1.0) / 4.0 * 0.66;
    const double resistance = voltage / 200.0 * 1000000.0;
    return (resistance - 100.0) / 0.385;
}

double DeviceSimulator::boundedNoise()
{
    // Keep the demo trace visibly alive while allowing the three-second
    // stability window to represent a settled instrument rather than noise spikes.
    return (QRandomGenerator::global()->generateDouble() - 0.5) * 0.12;
}
