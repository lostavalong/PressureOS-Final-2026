#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QTimer>

#include "LegacyAsciiProtocol.h"
#include "PressureProtocolV1.h"

class QSocketNotifier;

class SerialDeviceGateway final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString interfaceVersion READ interfaceVersion CONSTANT)
    Q_PROPERTY(QString protocolName READ protocolName NOTIFY statisticsChanged)
    Q_PROPERTY(bool running READ running NOTIFY connectionChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool dataFresh READ dataFresh NOTIFY connectionChanged)
    Q_PROPERTY(bool pressureFresh READ pressureFresh NOTIFY statisticsChanged)
    Q_PROPERTY(bool temperatureFresh READ temperatureFresh NOTIFY statisticsChanged)
    Q_PROPERTY(bool temperatureChannelEnabled READ temperatureChannelEnabled NOTIFY statisticsChanged)
    Q_PROPERTY(bool protocolIntegrityAvailable READ protocolIntegrityAvailable NOTIFY statisticsChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY connectionChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY connectionChanged)
    Q_PROPERTY(int baudRate READ baudRate CONSTANT)
    Q_PROPERTY(qulonglong receivedLines READ receivedLines NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong validFrames READ validFrames NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong pressureFrames READ pressureFrames NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong temperatureFrames READ temperatureFrames NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong diagnosticLines READ diagnosticLines NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong invalidFrames READ invalidFrames NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong crcErrors READ crcErrors NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong droppedFrames READ droppedFrames NOTIFY statisticsChanged)
    Q_PROPERTY(double wireRateHz READ wireRateHz NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 lastFrameAgeMs READ lastFrameAgeMs NOTIFY statisticsChanged)
    Q_PROPERTY(quint32 lastPressureRaw READ lastPressureRaw NOTIFY statisticsChanged)
    Q_PROPERTY(quint32 lastTemperatureRaw READ lastTemperatureRaw NOTIFY statisticsChanged)
    Q_PROPERTY(QString lastDiagnostic READ lastDiagnostic NOTIFY statisticsChanged)
    Q_PROPERTY(quint32 lastDeviceStatusFlags READ lastDeviceStatusFlags NOTIFY statisticsChanged)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion NOTIFY statisticsChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY statisticsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY connectionChanged)

public:
    explicit SerialDeviceGateway(QObject *parent = nullptr);
    ~SerialDeviceGateway() override;

    QString interfaceVersion() const { return QStringLiteral("1.0"); }
    QString protocolName() const;
    bool running() const { return m_running; }
    bool connected() const { return m_connected; }
    bool dataFresh() const { return m_dataFresh; }
    bool pressureFresh() const;
    bool temperatureFresh() const;
    bool temperatureChannelEnabled() const;
    bool protocolIntegrityAvailable() const { return m_v1Locked; }
    QString connectionState() const { return m_connectionState; }
    QString statusText() const;
    QString portName() const { return m_portName; }
    int baudRate() const { return 115200; }
    qulonglong receivedLines() const { return m_receivedLines; }
    qulonglong validFrames() const { return m_validFrames; }
    qulonglong pressureFrames() const { return m_pressureFrames; }
    qulonglong temperatureFrames() const { return m_temperatureFrames; }
    qulonglong diagnosticLines() const { return m_diagnosticLines; }
    qulonglong invalidFrames() const { return m_invalidFrames; }
    qulonglong crcErrors() const { return m_crcErrors; }
    qulonglong droppedFrames() const { return m_droppedFrames; }
    double wireRateHz() const { return m_wireRateHz; }
    qint64 lastFrameAgeMs() const;
    quint32 lastPressureRaw() const { return m_lastPressureRaw; }
    quint32 lastTemperatureRaw() const { return m_lastTemperatureRaw; }
    QString lastDiagnostic() const { return m_lastDiagnostic; }
    quint32 lastDeviceStatusFlags() const { return m_lastDeviceStatusFlags; }
    QString firmwareVersion() const { return m_firmwareVersion; }
    QString deviceId() const { return m_deviceId; }
    QString lastError() const { return m_lastError; }

    void setPortOverride(const QString &portName);
    void setChannelThreshold(quint32 threshold);
    void start();
    void stop();

    Q_INVOKABLE void reconnect();

signals:
    void connectionChanged();
    void statisticsChanged();
    void pressureCodeReady(quint32 rawCode, qint64 timestampMs);
    void temperatureCodeReady(quint32 rawCode, qint64 timestampMs);
    void diagnosticLine(const QString &line);
    void userMessage(const QString &message);

private slots:
    void tryOpen();
    void handleReadable();
    void updateHealth();

private:
    friend struct SerialDeviceGatewayTestAccess;

    QString discoverPort() const;
    bool configurePort(int descriptor);
    void closePort(const QString &reason = {});
    void scheduleReconnect();
    void processBytes(const QByteArray &bytes);
    void processLine(const QByteArray &line);
    void processLegacyEvent(const LegacyAsciiProtocol::Event &event,
                            qint64 wallClock, qint64 elapsed);
    void processV1Event(const PressureProtocolV1::Event &event,
                        qint64 wallClock, qint64 elapsed);
    void resetProtocolState();
    void requestDeviceInfo();
    void setConnectionState(const QString &state, const QString &error = {});

    LegacyAsciiProtocol m_legacyProtocol;
    PressureProtocolV1 m_v1Protocol;
    QByteArray m_receiveBuffer;
    QTimer m_reconnectTimer;
    QTimer m_healthTimer;
    QElapsedTimer m_runtime;
    QPointer<QSocketNotifier> m_readNotifier;
    QQueue<qint64> m_validFrameTimes;
    QString m_portOverride;
    QString m_portName;
    QString m_connectionState = QStringLiteral("idle");
    QString m_lastDiagnostic;
    QString m_lastError;
    int m_descriptor = -1;
    int m_staleTimeoutMs = 2200;
    bool m_running = false;
    bool m_connected = false;
    bool m_dataFresh = false;
    bool m_v1Locked = false;
    bool m_infoRequested = false;
    bool m_hasV1Sequence = false;
    qint64 m_lastValidFrameElapsedMs = -1;
    qint64 m_lastPressureFrameElapsedMs = -1;
    qint64 m_lastTemperatureFrameElapsedMs = -1;
    double m_wireRateHz = 0.0;
    qulonglong m_receivedLines = 0;
    qulonglong m_validFrames = 0;
    qulonglong m_pressureFrames = 0;
    qulonglong m_temperatureFrames = 0;
    qulonglong m_diagnosticLines = 0;
    qulonglong m_invalidFrames = 0;
    qulonglong m_crcErrors = 0;
    qulonglong m_droppedFrames = 0;
    quint32 m_lastPressureRaw = 0;
    quint32 m_lastTemperatureRaw = 0;
    quint32 m_lastV1Sequence = 0;
    quint32 m_lastDeviceUptimeMs = 0;
    quint32 m_lastDeviceStatusFlags = 0;
    QString m_firmwareVersion;
    QString m_deviceId;
};
