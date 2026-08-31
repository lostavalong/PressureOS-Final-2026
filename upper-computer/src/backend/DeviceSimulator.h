#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "PressureSignalProcessor.h"

class SerialDeviceGateway;

class DeviceSimulator final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double pressureKPa READ pressureKPa NOTIFY measurementChanged)
    Q_PROPERTY(double rawPressureKPa READ rawPressureKPa NOTIFY measurementChanged)
    Q_PROPERTY(double filteredPressureKPa READ filteredPressureKPa NOTIFY measurementChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY measurementChanged)
    Q_PROPERTY(bool temperatureCompensationEnabled READ temperatureCompensationEnabled CONSTANT)
    Q_PROPERTY(QString temperatureModeText READ temperatureModeText CONSTANT)
    Q_PROPERTY(double stabilityP2P READ stabilityP2P NOTIFY measurementChanged)
    Q_PROPERTY(double rangePercent READ rangePercent NOTIFY measurementChanged)
    Q_PROPERTY(double utilizationPercent READ utilizationPercent NOTIFY safetyChanged)
    Q_PROPERTY(double pressureRateKPaPerSec READ pressureRateKPaPerSec NOTIFY safetyChanged)
    Q_PROPERTY(double rangeMinKPa READ rangeMinKPa CONSTANT)
    Q_PROPERTY(double rangeMaxKPa READ rangeMaxKPa CONSTANT)
    Q_PROPERTY(double resolutionKPa READ resolutionKPa CONSTANT)
    Q_PROPERTY(QString rangeText READ rangeText CONSTANT)
    Q_PROPERTY(QString resolutionText READ resolutionText CONSTANT)
    Q_PROPERTY(QString formattedPressure READ formattedPressure NOTIFY measurementChanged)
    Q_PROPERTY(QString formattedRawPressure READ formattedRawPressure NOTIFY measurementChanged)
    Q_PROPERTY(QString unit READ unit NOTIFY unitChanged)
    Q_PROPERTY(QVariantList unitOptions READ unitOptions CONSTANT)
    Q_PROPERTY(QString filterName READ filterName NOTIFY filterChanged)
    Q_PROPERTY(QVariantList filterOptions READ filterOptions CONSTANT)
    Q_PROPERTY(bool precisionFilterReady READ precisionFilterReady NOTIFY measurementChanged)
    Q_PROPERTY(double precisionFilterPeriodSeconds READ precisionFilterPeriodSeconds NOTIFY measurementChanged)
    Q_PROPERTY(double zeroOffsetKPa READ zeroOffsetKPa NOTIFY zeroOffsetChanged)
    Q_PROPERTY(bool canZero READ canZero NOTIFY measurementChanged)
    Q_PROPERTY(bool zeroCalibrationActive READ zeroCalibrationActive NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(int zeroCalibrationProgress READ zeroCalibrationProgress NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(int zeroCalibrationElapsedSeconds READ zeroCalibrationElapsedSeconds NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(int zeroCalibrationTargetSeconds READ zeroCalibrationTargetSeconds NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(int zeroCalibrationSampleCount READ zeroCalibrationSampleCount NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(int zeroCalibrationCycleCount READ zeroCalibrationCycleCount NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(QString zeroCalibrationStatus READ zeroCalibrationStatus NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationMeanKPa READ zeroCalibrationMeanKPa NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationStdDevKPa READ zeroCalibrationStdDevKPa NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationP2PKPa READ zeroCalibrationP2PKPa NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationStandardErrorKPa READ zeroCalibrationStandardErrorKPa NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationSlopeKPaPerSec READ zeroCalibrationSlopeKPaPerSec NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationSegmentSpreadKPa READ zeroCalibrationSegmentSpreadKPa NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(double zeroCalibrationDetectedPeriodSeconds READ zeroCalibrationDetectedPeriodSeconds NOTIFY zeroCalibrationChanged)
    Q_PROPERTY(QString safetyLevel READ safetyLevel NOTIFY safetyChanged)
    Q_PROPERTY(QString safetyTitle READ safetyTitle NOTIFY safetyChanged)
    Q_PROPERTY(QString safetyMessage READ safetyMessage NOTIFY safetyChanged)
    Q_PROPERTY(bool tripLatched READ tripLatched NOTIFY safetyChanged)
    Q_PROPERTY(bool controlOutputEnabled READ controlOutputEnabled NOTIFY safetyChanged)
    Q_PROPERTY(bool stable READ stable NOTIFY measurementChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int recordSeconds READ recordSeconds NOTIFY recordingChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool hardwareMode READ hardwareMode NOTIFY connectionChanged)
    Q_PROPERTY(bool valueTrustedForSafety READ valueTrustedForSafety CONSTANT)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY measurementChanged)
    Q_PROPERTY(QVariantList series READ series NOTIFY seriesChanged)
    Q_PROPERTY(QVariantList rawSeries READ rawSeries NOTIFY seriesChanged)
    Q_PROPERTY(QString transportName READ transportName NOTIFY connectionChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY connectionChanged)

public:
    explicit DeviceSimulator(QObject *parent = nullptr);

    double pressureKPa() const { return m_pressureKPa; }
    double rawPressureKPa() const { return m_rawPressureKPa; }
    double filteredPressureKPa() const { return m_filteredPressureKPa; }
    double temperature() const { return m_temperature; }
    bool temperatureCompensationEnabled() const { return false; }
    QString temperatureModeText() const { return QStringLiteral("常温模式 · 未启用温补"); }
    double stabilityP2P() const { return m_stabilityP2P; }
    double rangePercent() const;
    double utilizationPercent() const;
    double pressureRateKPaPerSec() const { return m_pressureRateKPaPerSec; }
    double rangeMinKPa() const { return -100.0; }
    double rangeMaxKPa() const { return 600.0; }
    double resolutionKPa() const { return 0.1; }
    QString rangeText() const { return QStringLiteral("−100 ～ 600 kPa"); }
    QString resolutionText() const { return QStringLiteral("0.1 kPa"); }
    QString formattedPressure() const;
    QString formattedRawPressure() const;
    QString unit() const { return m_unit; }
    QVariantList unitOptions() const;
    QString filterName() const { return m_filterName; }
    QVariantList filterOptions() const;
    bool precisionFilterReady() const { return m_signalProcessor.precisionReady(); }
    double precisionFilterPeriodSeconds() const
    {
        return m_signalProcessor.precisionDetectedPeriodSeconds();
    }
    double zeroOffsetKPa() const { return m_zeroOffset; }
    bool canZero() const;
    bool zeroCalibrationActive() const { return m_zeroCalibrationActive; }
    int zeroCalibrationProgress() const { return m_zeroCalibrationProgress; }
    int zeroCalibrationElapsedSeconds() const { return m_zeroCalibrationElapsedSeconds; }
    int zeroCalibrationTargetSeconds() const
    {
        return (m_zeroCalibrationMinimumDurationMs + 999) / 1000;
    }
    int zeroCalibrationSampleCount() const { return m_zeroCalibrationSampleCount; }
    int zeroCalibrationCycleCount() const { return m_zeroCalibrationCycleCount; }
    QString zeroCalibrationStatus() const { return m_zeroCalibrationStatus; }
    double zeroCalibrationMeanKPa() const { return m_zeroCalibrationMeanKPa; }
    double zeroCalibrationStdDevKPa() const { return m_zeroCalibrationStdDevKPa; }
    double zeroCalibrationP2PKPa() const { return m_zeroCalibrationP2PKPa; }
    double zeroCalibrationStandardErrorKPa() const { return m_zeroCalibrationStandardErrorKPa; }
    double zeroCalibrationSlopeKPaPerSec() const { return m_zeroCalibrationSlopeKPaPerSec; }
    double zeroCalibrationSegmentSpreadKPa() const { return m_zeroCalibrationSegmentSpreadKPa; }
    double zeroCalibrationDetectedPeriodSeconds() const
    {
        return m_zeroCalibrationDetectedPeriodSeconds;
    }
    QString safetyLevel() const { return m_safetyLevel; }
    QString safetyTitle() const;
    QString safetyMessage() const;
    bool tripLatched() const { return m_tripLatched; }
    bool controlOutputEnabled() const { return m_controlOutputEnabled; }
    bool stable() const { return m_stable; }
    bool recording() const { return m_recording; }
    int recordSeconds() const { return m_recordSeconds; }
    bool connected() const;
    bool hardwareMode() const { return m_hardwareMode; }
    bool valueTrustedForSafety() const { return m_valueTrustedForSafety; }
    int sampleRate() const { return m_sampleRate; }
    QVariantList series() const;
    QVariantList rawSeries() const;
    QString transportName() const;
    QString sourceName() const;

    void attachSerialGateway(SerialDeviceGateway *gateway);

    Q_INVOKABLE void cycleUnit();
    Q_INVOKABLE void setUnit(const QString &unit);
    Q_INVOKABLE void cycleFilter();
    Q_INVOKABLE void setFilter(const QString &filter);
    Q_INVOKABLE bool zero();
    Q_INVOKABLE bool startZeroCalibration();
    Q_INVOKABLE void cancelZeroCalibration();
    Q_INVOKABLE void simulateVentToAtmosphere();
    Q_INVOKABLE bool acknowledgeTrip();
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void setTarget(double targetKPa);
signals:
    void measurementChanged();
    void seriesChanged();
    void unitChanged();
    void filterChanged();
    void zeroOffsetChanged();
    void zeroCalibrationCompleted(double correctionKPa, double cumulativeOffsetKPa);
    void zeroCalibrationChanged();
    void zeroCalibrationFailed(const QString &reason);
    void safetyChanged();
    void recordingChanged();
    void connectionChanged();
    void sampleReady(double rawKPa, double filteredKPa, double reportedKPa,
                     double temperatureC, qint64 timestampMs);
    void userMessage(const QString &message);
    void emergencyTripRequested(double pressureKPa, const QString &reason);

private slots:
    void generateSample();
    void ingestPressureCode(quint32 rawCode, qint64 timestampMs);
    void ingestTemperatureCode(quint32 rawCode, qint64 timestampMs);
    void relayGatewayState();

private:
    QString formatValue(double kPa) const;
    void processMeasurement(double rawPressureKPa, double temperatureC, qint64 timestampMs);
    void collectZeroCalibrationSample(double uncorrectedPressureKPa, qint64 timestampMs);
    bool evaluateZeroCalibration(bool finalAttempt);
    void failZeroCalibration(const QString &reason);
    void updateHardwareSampleRate(qint64 timestampMs);
    void updateStability();
    void updateSafety(double previousPressureKPa, double intervalSeconds);
    static double temperatureFromRawCode(quint32 rawCode);
    static double boundedNoise();

    QTimer m_sampleTimer;
    QElapsedTimer m_runtime;
    QElapsedTimer m_recordingRuntime;
    QVector<double> m_series;
    QVector<double> m_rawSeries;
    QQueue<qint64> m_hardwareSampleTimes;
    QPointer<SerialDeviceGateway> m_serialGateway;
    PressureSignalProcessor m_signalProcessor;
    double m_targetKPa = 524.7;
    double m_pressureKPa = 524.7;
    double m_rawPressureKPa = 524.7;
    double m_filteredPressureKPa = 524.7;
    double m_temperature = 24.6;
    double m_zeroOffset = 0.0;
    QVector<double> m_zeroCalibrationSamples;
    QVector<qint64> m_zeroCalibrationTimes;
    qint64 m_zeroCalibrationStartedAtMs = -1;
    qint64 m_zeroCalibrationLastEvaluationAtMs = -1;
    int m_zeroCalibrationMinimumDurationMs = 25000;
    int m_zeroCalibrationMaximumDurationMs = 40000;
    int m_zeroCalibrationProgress = 0;
    int m_zeroCalibrationElapsedSeconds = 0;
    int m_zeroCalibrationSampleCount = 0;
    int m_zeroCalibrationCycleCount = 0;
    QString m_zeroCalibrationStatus = QStringLiteral("尚未开始");
    double m_zeroCalibrationMeanKPa = 0.0;
    double m_zeroCalibrationStdDevKPa = 0.0;
    double m_zeroCalibrationP2PKPa = 0.0;
    double m_zeroCalibrationStandardErrorKPa = 0.0;
    double m_zeroCalibrationSlopeKPaPerSec = 0.0;
    double m_zeroCalibrationSegmentSpreadKPa = 0.0;
    double m_zeroCalibrationDetectedPeriodSeconds = 0.0;
    qulonglong m_zeroCalibrationStartCrcErrors = 0;
    qulonglong m_zeroCalibrationStartDroppedFrames = 0;
    qulonglong m_zeroCalibrationStartInvalidFrames = 0;
    bool m_zeroCalibrationActive = false;
    double m_stabilityP2P = 0.0;
    double m_pressureRateKPaPerSec = 0.0;
    QString m_unit = QStringLiteral("MPa");
    QString m_filterName = QStringLiteral("周期抑制 · 精密");
    bool m_stable = true;
    bool m_recording = false;
    int m_recordSeconds = 0;
    int m_tick = 0;
    int m_sampleRate = 50;
    qint64 m_lastMeasurementTimestampMs = -1;
    qint64 m_lastPressureAcceptedAtMs = -1;
    qint64 m_lastTemperatureAcceptedAtMs = -1;
    quint32 m_lastPressureCode = 0;
    quint32 m_lastTemperatureCode = 0;
    bool m_hasMeasurement = true;
    bool m_hardwareMode = false;
    bool m_valueTrustedForSafety = true;
    QString m_safetyLevel = QStringLiteral("normal");
    bool m_tripLatched = false;
    bool m_controlOutputEnabled = true;
};
