#include "SerialDeviceGateway.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {
constexpr int kReconnectIntervalMs = 1800;
constexpr int kRateWindowMs = 5000;
constexpr int kMaximumReceiveBufferBytes = 8192;
constexpr int kMaximumLineBytes = 256;
constexpr quint32 kPressureInvalidFlag = 0x00000004u;
constexpr quint32 kTemperatureInvalidFlag = 0x00000008u;
constexpr quint32 kTemperatureDisabledFlag = 0x00000800u;
constexpr auto kPreferredDevicePath =
    "/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_"
    "a83bd7f37698ef1197dfca63a8793231-if00-port0";
}

SerialDeviceGateway::SerialDeviceGateway(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(kReconnectIntervalMs);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SerialDeviceGateway::tryOpen);

    m_healthTimer.setInterval(500);
    connect(&m_healthTimer, &QTimer::timeout, this, &SerialDeviceGateway::updateHealth);
}

SerialDeviceGateway::~SerialDeviceGateway()
{
    stop();
}

QString SerialDeviceGateway::protocolName() const
{
    return m_v1Locked ? QStringLiteral("V1 ASCII · CRC16")
                      : QStringLiteral("V0 / V1 自动兼容");
}

QString SerialDeviceGateway::statusText() const
{
    if (!m_running)
        return QStringLiteral("未启用");
    if (m_connected && m_dataFresh)
        return QStringLiteral("实时数据正常");
    if (m_connected)
        return QStringLiteral("串口已开，等待数据");
    if (m_connectionState == QStringLiteral("unsupported"))
        return QStringLiteral("当前平台不支持真实串口");
    if (m_connectionState == QStringLiteral("error"))
        return QStringLiteral("连接异常，正在重试");
    return QStringLiteral("正在查找下位机");
}

qint64 SerialDeviceGateway::lastFrameAgeMs() const
{
    if (!m_runtime.isValid() || m_lastValidFrameElapsedMs < 0)
        return -1;
    return qMax<qint64>(0, m_runtime.elapsed() - m_lastValidFrameElapsedMs);
}

bool SerialDeviceGateway::pressureFresh() const
{
    return m_connected && m_runtime.isValid() && m_lastPressureFrameElapsedMs >= 0
        && m_runtime.elapsed() - m_lastPressureFrameElapsedMs <= m_staleTimeoutMs;
}

bool SerialDeviceGateway::temperatureFresh() const
{
    return temperatureChannelEnabled()
        && m_connected && m_runtime.isValid() && m_lastTemperatureFrameElapsedMs >= 0
        && m_runtime.elapsed() - m_lastTemperatureFrameElapsedMs <= m_staleTimeoutMs;
}

bool SerialDeviceGateway::temperatureChannelEnabled() const
{
    return !m_v1Locked || !(m_lastDeviceStatusFlags & kTemperatureDisabledFlag);
}

void SerialDeviceGateway::setPortOverride(const QString &portName)
{
    m_portOverride = portName.trimmed();
}

void SerialDeviceGateway::setChannelThreshold(quint32 threshold)
{
    m_legacyProtocol.setChannelThreshold(threshold);
}

void SerialDeviceGateway::start()
{
    if (m_running)
        return;
    m_running = true;
    m_runtime.start();
    m_healthTimer.start();
    setConnectionState(QStringLiteral("searching"));
    tryOpen();
}

void SerialDeviceGateway::stop()
{
    if (!m_running && m_descriptor < 0)
        return;
    m_running = false;
    m_reconnectTimer.stop();
    m_healthTimer.stop();
    closePort();
    setConnectionState(QStringLiteral("idle"));
}

void SerialDeviceGateway::reconnect()
{
    if (!m_running) {
        start();
        return;
    }
    closePort();
    resetProtocolState();
    setConnectionState(QStringLiteral("searching"));
    QTimer::singleShot(80, this, &SerialDeviceGateway::tryOpen);
}

void SerialDeviceGateway::tryOpen()
{
    if (!m_running || m_connected)
        return;

#ifdef Q_OS_UNIX
    const QString devicePath = discoverPort();
    if (devicePath.isEmpty()) {
        setConnectionState(QStringLiteral("searching"), QStringLiteral("未发现 CP2102N 串口设备"));
        scheduleReconnect();
        return;
    }

    const QByteArray nativePath = QFile::encodeName(devicePath);
    const int descriptor = ::open(nativePath.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) {
        setConnectionState(QStringLiteral("error"),
                           QStringLiteral("无法打开 %1：%2")
                               .arg(devicePath, QString::fromLocal8Bit(std::strerror(errno))));
        scheduleReconnect();
        return;
    }
    if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
        const QString error = QStringLiteral("串口 %1 正被其他程序占用").arg(devicePath);
        ::close(descriptor);
        setConnectionState(QStringLiteral("error"), error);
        scheduleReconnect();
        return;
    }
    if (!configurePort(descriptor)) {
        const QString error = QStringLiteral("无法配置 %1 为 115200/8N1").arg(devicePath);
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
        setConnectionState(QStringLiteral("error"), error);
        scheduleReconnect();
        return;
    }

    m_descriptor = descriptor;
    m_portName = devicePath;
    m_connected = true;
    m_dataFresh = false;
    m_lastValidFrameElapsedMs = -1;
    m_lastPressureFrameElapsedMs = -1;
    m_lastTemperatureFrameElapsedMs = -1;
    resetProtocolState();
    m_readNotifier = new QSocketNotifier(m_descriptor, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
            this, &SerialDeviceGateway::handleReadable);
    setConnectionState(QStringLiteral("waiting"));
    emit userMessage(QStringLiteral("下位机串口已连接：%1 · 115200/8N1").arg(devicePath));
#else
    setConnectionState(QStringLiteral("unsupported"),
                       QStringLiteral("真实串口采集当前仅在树莓派 Linux 构建中启用"));
#endif
}

void SerialDeviceGateway::handleReadable()
{
#ifdef Q_OS_UNIX
    if (m_descriptor < 0)
        return;

    char bytes[4096];
    while (true) {
        const ssize_t count = ::read(m_descriptor, bytes, sizeof(bytes));
        if (count > 0) {
            processBytes(QByteArray(bytes, static_cast<qsizetype>(count)));
            continue;
        }
        // A non-blocking tty configured with VMIN=0 may return 0 after all
        // currently available bytes have been drained.  This is not an EOF
        // indication (unlike a regular file/socket), so keep the port open and
        // wait for the next QSocketNotifier activation.  A real USB-serial
        // removal is reported by Linux as an error such as EIO/ENODEV below.
        if (count == 0)
            break;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        const QString reason = QStringLiteral("串口读取失败：%1")
                                   .arg(QString::fromLocal8Bit(std::strerror(errno)));
        closePort(reason);
        scheduleReconnect();
        break;
    }
#endif
}

void SerialDeviceGateway::updateHealth()
{
    if (!m_running)
        return;

    const qint64 now = m_runtime.elapsed();
    while (!m_validFrameTimes.isEmpty() && now - m_validFrameTimes.head() > kRateWindowMs)
        m_validFrameTimes.dequeue();

    if (m_validFrameTimes.size() >= 2) {
        const qint64 duration = m_validFrameTimes.back() - m_validFrameTimes.front();
        m_wireRateHz = duration > 0
            ? (m_validFrameTimes.size() - 1) * 1000.0 / static_cast<double>(duration)
            : 0.0;
    } else {
        m_wireRateHz = 0.0;
    }

    const bool fresh = m_connected && m_lastValidFrameElapsedMs >= 0
        && now - m_lastValidFrameElapsedMs <= m_staleTimeoutMs;
    const bool freshnessChanged = fresh != m_dataFresh;
    m_dataFresh = fresh;

    if (m_connected) {
        const QString nextState = m_dataFresh ? QStringLiteral("streaming") : QStringLiteral("waiting");
        if (nextState != m_connectionState)
            setConnectionState(nextState);
        else if (freshnessChanged)
            emit connectionChanged();
    }

    emit statisticsChanged();
}

QString SerialDeviceGateway::discoverPort() const
{
#ifdef Q_OS_UNIX
    if (!m_portOverride.isEmpty())
        return QFileInfo::exists(m_portOverride) ? m_portOverride : QString{};

    const QString environmentPath = QString::fromLocal8Bit(qgetenv("PRESSUREOS_SERIAL_PORT")).trimmed();
    if (!environmentPath.isEmpty() && QFileInfo::exists(environmentPath))
        return environmentPath;

    const QString preferred = QString::fromLatin1(kPreferredDevicePath);
    if (QFileInfo::exists(preferred))
        return preferred;

    const QDir deviceDirectory(QStringLiteral("/dev"));
    const QStringList candidates = deviceDirectory.entryList(
        {QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*")},
        QDir::System | QDir::Files | QDir::Readable, QDir::Name);
    if (!candidates.isEmpty())
        return deviceDirectory.absoluteFilePath(candidates.first());
#endif
    return {};
}

bool SerialDeviceGateway::configurePort(int descriptor)
{
#ifdef Q_OS_UNIX
    termios settings{};
    if (::tcgetattr(descriptor, &settings) != 0)
        return false;
    ::cfmakeraw(&settings);
    ::cfsetispeed(&settings, B115200);
    ::cfsetospeed(&settings, B115200);
    settings.c_cflag |= CLOCAL | CREAD;
    settings.c_cflag &= ~CSIZE;
    settings.c_cflag |= CS8;
    settings.c_cflag &= ~(PARENB | CSTOPB);
#ifdef CRTSCTS
    settings.c_cflag &= ~CRTSCTS;
#endif
    settings.c_iflag &= ~(IXON | IXOFF | IXANY);
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (::tcsetattr(descriptor, TCSANOW, &settings) != 0)
        return false;
    ::tcflush(descriptor, TCIFLUSH);
    return true;
#else
    Q_UNUSED(descriptor)
    return false;
#endif
}

void SerialDeviceGateway::closePort(const QString &reason)
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }
#ifdef Q_OS_UNIX
    if (m_descriptor >= 0) {
        ::flock(m_descriptor, LOCK_UN);
        ::close(m_descriptor);
    }
#endif
    const bool wasConnected = m_connected;
    m_descriptor = -1;
    m_connected = false;
    m_dataFresh = false;
    m_portName.clear();
    if (!reason.isEmpty()) {
        setConnectionState(QStringLiteral("error"), reason);
        emit userMessage(reason + QStringLiteral("，系统将自动重连"));
    } else if (wasConnected) {
        emit connectionChanged();
    }
}

void SerialDeviceGateway::scheduleReconnect()
{
    if (m_running && !m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}

void SerialDeviceGateway::processBytes(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return;

    m_receiveBuffer.append(bytes);
    if (m_receiveBuffer.size() > kMaximumReceiveBufferBytes) {
        const int lastLineBreak = m_receiveBuffer.lastIndexOf('\n');
        m_receiveBuffer = lastLineBreak >= 0
            ? m_receiveBuffer.mid(lastLineBreak + 1) : QByteArray{};
        ++m_invalidFrames;
        m_lastDiagnostic = QStringLiteral("串口接收缓存超过上限，已重新同步");
    }

    int lineBreak = -1;
    while ((lineBreak = m_receiveBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_receiveBuffer.left(lineBreak);
        m_receiveBuffer.remove(0, lineBreak + 1);
        line = line.trimmed();
        if (!line.isEmpty())
            processLine(line);
    }

    if (!m_dataFresh && m_lastValidFrameElapsedMs >= 0) {
        m_dataFresh = true;
        setConnectionState(QStringLiteral("streaming"));
    }
    emit statisticsChanged();
}

void SerialDeviceGateway::processLine(const QByteArray &line)
{
    ++m_receivedLines;
    const qint64 wallClock = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = m_runtime.elapsed();

    if (line.startsWith("@PS1,")) {
        const PressureProtocolV1::Event event = m_v1Protocol.parseLine(line);
        if (event.type == PressureProtocolV1::EventType::Invalid) {
            ++m_invalidFrames;
            if (event.crcError)
                ++m_crcErrors;
            m_lastDiagnostic = event.text;
            return;
        }
        processV1Event(event, wallClock, elapsed);
        return;
    }

    if (m_v1Locked) {
        ++m_invalidFrames;
        m_lastDiagnostic = QStringLiteral("V1 锁定后忽略无协议前缀数据：%1")
                               .arg(QString::fromLatin1(line.left(48)));
        return;
    }

    const QList<LegacyAsciiProtocol::Event> events = m_legacyProtocol.feed(line + '\n');
    for (const LegacyAsciiProtocol::Event &event : events)
        processLegacyEvent(event, wallClock, elapsed);
}

void SerialDeviceGateway::processLegacyEvent(const LegacyAsciiProtocol::Event &event,
                                             qint64 wallClock, qint64 elapsed)
{
    switch (event.type) {
    case LegacyAsciiProtocol::EventType::Pressure:
        ++m_validFrames;
        ++m_pressureFrames;
        m_lastPressureRaw = event.rawCode;
        m_lastValidFrameElapsedMs = elapsed;
        m_lastPressureFrameElapsedMs = elapsed;
        m_validFrameTimes.enqueue(elapsed);
        emit pressureCodeReady(event.rawCode, wallClock);
        break;
    case LegacyAsciiProtocol::EventType::Temperature:
        ++m_validFrames;
        ++m_temperatureFrames;
        m_lastTemperatureRaw = event.rawCode;
        m_lastValidFrameElapsedMs = elapsed;
        m_lastTemperatureFrameElapsedMs = elapsed;
        m_validFrameTimes.enqueue(elapsed);
        emit temperatureCodeReady(event.rawCode, wallClock);
        break;
    case LegacyAsciiProtocol::EventType::Diagnostic:
        ++m_diagnosticLines;
        m_lastDiagnostic = event.text;
        emit diagnosticLine(event.text);
        break;
    case LegacyAsciiProtocol::EventType::Invalid:
        ++m_invalidFrames;
        m_lastDiagnostic = event.text;
        break;
    }
}

void SerialDeviceGateway::processV1Event(const PressureProtocolV1::Event &event,
                                         qint64 wallClock, qint64 elapsed)
{
    if (!m_v1Locked) {
        m_v1Locked = true;
        emit userMessage(QStringLiteral("已识别下位机 V1 协议，CRC16 完整性校验已启用"));
    }

    m_lastDeviceStatusFlags = event.statusFlags;
    switch (event.type) {
    case PressureProtocolV1::EventType::Measurement: {
        ++m_validFrames;
        m_validFrameTimes.enqueue(elapsed);

        if (m_hasV1Sequence) {
            const quint32 expected = m_lastV1Sequence + 1u;
            if (event.sequence != expected) {
                const quint32 missing = event.sequence - expected;
                if (missing < 0x80000000u)
                    m_droppedFrames += missing;
            }
        }
        m_hasV1Sequence = true;
        m_lastV1Sequence = event.sequence;
        m_lastDeviceUptimeMs = event.uptimeMs;
        m_lastPressureRaw = event.pressureRaw;
        m_lastTemperatureRaw = event.temperatureRaw;

        const bool pressureValid = !(event.statusFlags & kPressureInvalidFlag);
        const bool temperatureEnabled = !(event.statusFlags & kTemperatureDisabledFlag);
        const bool temperatureValid = temperatureEnabled
            && !(event.statusFlags & kTemperatureInvalidFlag);
        if (pressureValid) {
            ++m_pressureFrames;
            m_lastPressureFrameElapsedMs = elapsed;
            emit pressureCodeReady(event.pressureRaw, wallClock);
        }
        if (temperatureValid) {
            ++m_temperatureFrames;
            m_lastTemperatureFrameElapsedMs = elapsed;
            emit temperatureCodeReady(event.temperatureRaw, wallClock);
        }
        // Pressure is the primary measured quantity. A deliberately disabled
        // temperature channel must not make an otherwise valid pressure stream stale.
        if (pressureValid)
            m_lastValidFrameElapsedMs = elapsed;

        if (event.statusFlags == kTemperatureDisabledFlag) {
            m_lastDiagnostic = QStringLiteral("常温高速模式：温度通道已按项目配置停用");
        } else if (event.statusFlags != 0u) {
            m_lastDiagnostic = QStringLiteral("下位机状态位：0x%1")
                                   .arg(event.statusFlags, 8, 16, QLatin1Char('0')).toUpper();
        }
        requestDeviceInfo();
        break;
    }
    case PressureProtocolV1::EventType::Info:
        m_infoRequested = true;
        m_lastDeviceUptimeMs = event.uptimeMs;
        m_firmwareVersion = event.firmwareVersion;
        m_deviceId = event.deviceId;
        m_lastDiagnostic = QStringLiteral("%1 · %2 · %3 · reset=%4")
                               .arg(event.deviceId, event.adcModel,
                                    event.firmwareVersion, event.resetReason);
        ++m_diagnosticLines;
        emit diagnosticLine(m_lastDiagnostic);
        break;
    case PressureProtocolV1::EventType::Ack:
        m_lastDiagnostic = QStringLiteral("命令 ACK #%1 %2：%3")
                               .arg(event.commandId).arg(event.command, event.result);
        ++m_diagnosticLines;
        emit diagnosticLine(m_lastDiagnostic);
        break;
    case PressureProtocolV1::EventType::Nack:
        m_lastDiagnostic = QStringLiteral("命令 NACK #%1 %2：%3")
                               .arg(event.commandId).arg(event.command, event.result);
        ++m_diagnosticLines;
        emit diagnosticLine(m_lastDiagnostic);
        break;
    case PressureProtocolV1::EventType::Invalid:
        break;
    }
}

void SerialDeviceGateway::resetProtocolState()
{
    m_receiveBuffer.clear();
    m_legacyProtocol.reset();
    m_v1Locked = false;
    m_infoRequested = false;
    m_hasV1Sequence = false;
    m_lastV1Sequence = 0u;
    m_lastDeviceUptimeMs = 0u;
    m_lastDeviceStatusFlags = 0u;
    m_firmwareVersion.clear();
    m_deviceId.clear();
}

void SerialDeviceGateway::requestDeviceInfo()
{
#ifdef Q_OS_UNIX
    if (m_infoRequested || m_descriptor < 0)
        return;
    const QByteArray frame = PressureProtocolV1::encodeCommand(1u, QByteArrayLiteral("GET_INFO"));
    if (frame.isEmpty())
        return;
    const ssize_t written = ::write(m_descriptor, frame.constData(), static_cast<size_t>(frame.size()));
    if (written == frame.size())
        m_infoRequested = true;
#endif
}

void SerialDeviceGateway::setConnectionState(const QString &state, const QString &error)
{
    const bool changed = state != m_connectionState || error != m_lastError;
    m_connectionState = state;
    m_lastError = error;
    if (changed)
        emit connectionChanged();
}
