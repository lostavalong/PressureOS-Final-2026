#include "ConnectivityService.h"

#include <QRegularExpression>

#include <utility>

namespace {
constexpr int kRefreshIntervalMs = 5000;
constexpr int kProbeTimeoutMs = 3500;

struct NetworkInterfaceInfo
{
    QString device;
    QString type;
    QString state;
    QString connection;
    QString ipv4;
};

QString valueAfterColon(const QString &line)
{
    const qsizetype separator = line.indexOf(QLatin1Char(':'));
    return separator < 0 ? QString() : line.mid(separator + 1).trimmed();
}
}

ConnectivityService::ConnectivityService(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    m_refreshTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ConnectivityService::refresh);

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &ConnectivityService::handleProbeTimeout);
    connect(&m_probe, &QProcess::finished, this, &ConnectivityService::handleProbeFinished);
    connect(&m_probe, &QProcess::errorOccurred, this, &ConnectivityService::handleProbeError);

#ifdef Q_OS_LINUX
    m_refreshTimer.start();
#endif
    QTimer::singleShot(0, this, &ConnectivityService::refresh);
}

QString ConnectivityService::lastUpdatedText() const
{
    return m_lastUpdated.isValid() ? m_lastUpdated.toString(QStringLiteral("HH:mm:ss"))
                                   : QStringLiteral("尚未读取");
}

QString ConnectivityService::wifiBand() const
{
    if (m_wifiFrequencyMHz >= 5925)
        return QStringLiteral("6 GHz");
    if (m_wifiFrequencyMHz >= 4900)
        return QStringLiteral("5 GHz");
    if (m_wifiFrequencyMHz >= 2300)
        return QStringLiteral("2.4 GHz");
    return QString();
}

QString ConnectivityService::wifiStatusText() const
{
    if (m_refreshing && !m_lastUpdated.isValid())
        return QStringLiteral("正在读取");
    if (!m_wifiAvailable)
        return QStringLiteral("未检测到");
    if (!m_wifiEnabled)
        return QStringLiteral("Wi-Fi 已关闭");
    return m_wifiConnected ? QStringLiteral("已连接") : QStringLiteral("未连接");
}

QString ConnectivityService::wifiPrimaryText() const
{
    if (!m_wifiAvailable)
        return QStringLiteral("未检测到无线网卡");
    if (!m_wifiEnabled)
        return QStringLiteral("无线功能已关闭");
    if (!m_wifiConnected)
        return QStringLiteral("尚未连接网络");
    return m_wifiSsid.isEmpty() ? QStringLiteral("已连接的 Wi-Fi") : m_wifiSsid;
}

QString ConnectivityService::wifiDetailText() const
{
    QStringList details;
    if (m_wifiConnected) {
        if (!wifiBand().isEmpty())
            details.push_back(wifiBand());
        if (m_wifiSignalPercent >= 0)
            details.push_back(QStringLiteral("信号 %1%").arg(m_wifiSignalPercent));
        if (!m_wifiIpv4.isEmpty())
            details.push_back(m_wifiIpv4);
    } else if (!m_wifiInterface.isEmpty()) {
        details.push_back(m_wifiInterface);
        details.push_back(QStringLiteral("NetworkManager"));
    }
    return details.isEmpty() ? QStringLiteral("等待系统状态") : details.join(QStringLiteral(" · "));
}

QString ConnectivityService::bluetoothDeviceSummary() const
{
    if (m_bluetoothDeviceNames.isEmpty())
        return QStringLiteral("暂无已连接设备");
    return m_bluetoothDeviceNames.join(QStringLiteral("、"));
}

QString ConnectivityService::bluetoothStatusText() const
{
    if (m_refreshing && !m_lastUpdated.isValid())
        return QStringLiteral("正在读取");
    if (!m_bluetoothAvailable)
        return QStringLiteral("未检测到");
    return m_bluetoothEnabled ? QStringLiteral("蓝牙已开启") : QStringLiteral("蓝牙已关闭");
}

QString ConnectivityService::bluetoothPrimaryText() const
{
    if (!m_bluetoothAvailable)
        return QStringLiteral("未检测到蓝牙适配器");
    if (!m_bluetoothAdapterName.isEmpty())
        return m_bluetoothAdapterName;
    return QStringLiteral("本机蓝牙适配器");
}

QString ConnectivityService::bluetoothDetailText() const
{
    if (!m_bluetoothAvailable)
        return QStringLiteral("BlueZ 未返回控制器");
    if (!m_bluetoothEnabled)
        return QStringLiteral("适配器存在 · 当前关闭");
    if (m_bluetoothDeviceNames.isEmpty())
        return QStringLiteral("已开启 · 暂无已连接设备");
    return QStringLiteral("已连接 %1 台 · %2")
        .arg(m_bluetoothDeviceNames.size())
        .arg(bluetoothDeviceSummary());
}

void ConnectivityService::refresh()
{
#ifdef Q_OS_LINUX
    if (m_probe.state() != QProcess::NotRunning)
        return;

    static const QString probeScript = QStringLiteral(R"SH(
export LC_ALL=C
printf '%s\n' '__META__'
if command -v nmcli >/dev/null 2>&1; then printf '%s\n' 'NMCLI=1'; else printf '%s\n' 'NMCLI=0'; fi
if command -v bluetoothctl >/dev/null 2>&1; then printf '%s\n' 'BLUEZ=1'; else printf '%s\n' 'BLUEZ=0'; fi
printf '%s\n' '__NM_GENERAL__'
if command -v nmcli >/dev/null 2>&1; then
    nmcli -t --escape yes -f GENERAL.DEVICE,GENERAL.TYPE,GENERAL.STATE,GENERAL.CONNECTION,IP4.ADDRESS device show 2>/dev/null
fi
printf '%s\n' '__NM_RADIO__'
if command -v nmcli >/dev/null 2>&1; then nmcli -t -f WIFI general 2>/dev/null; fi
printf '%s\n' '__NM_WIFI__'
if command -v nmcli >/dev/null 2>&1; then
    nmcli -t --escape yes -f IN-USE,SSID,SIGNAL,FREQ,DEVICE device wifi list --rescan no 2>/dev/null
fi
printf '%s\n' '__BT_SHOW__'
if command -v bluetoothctl >/dev/null 2>&1; then bluetoothctl show 2>/dev/null; fi
printf '%s\n' '__BT_CONNECTED__'
if command -v bluetoothctl >/dev/null 2>&1; then bluetoothctl devices Connected 2>/dev/null; fi
printf '%s\n' '__END__'
)SH");

    m_probeTimedOut = false;
    setRefreshing(true);
    m_probe.setProgram(QStringLiteral("/bin/sh"));
    m_probe.setArguments({QStringLiteral("-c"), probeScript});
    m_probe.start();
    m_timeoutTimer.start(kProbeTimeoutMs);
#else
    m_supported = false;
    m_backendName = QStringLiteral("仅树莓派系统检测");
    m_statusError = QStringLiteral("当前预览平台不提供 NetworkManager 与 BlueZ 状态");
    m_lastUpdated = QDateTime::currentDateTime();
    emit statusChanged();
#endif
}

void ConnectivityService::handleProbeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer.stop();
    const QString output = QString::fromUtf8(m_probe.readAllStandardOutput());
    if (m_probeTimedOut) {
        setProbeError(QStringLiteral("无线状态读取超时"));
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0 || !output.contains(QStringLiteral("__END__"))) {
        setProbeError(QStringLiteral("无线状态读取失败"));
        return;
    }

    parseProbeOutput(output);
    setRefreshing(false);
}

void ConnectivityService::handleProbeError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart)
        return;
    m_timeoutTimer.stop();
    setProbeError(QStringLiteral("无法启动系统状态检测进程"));
}

void ConnectivityService::handleProbeTimeout()
{
    if (m_probe.state() == QProcess::NotRunning)
        return;
    m_probeTimedOut = true;
    m_probe.kill();
}

void ConnectivityService::parseProbeOutput(const QString &output)
{
    enum class Section { None, Meta, NetworkGeneral, NetworkRadio, WifiAccessPoint, BluetoothShow, BluetoothConnected };
    Section section = Section::None;
    bool nmcliPresent = false;
    bool bluezPresent = false;
    QString radioState;
    QList<NetworkInterfaceInfo> interfaces;
    NetworkInterfaceInfo currentInterface;
    bool hasCurrentInterface = false;

    bool nextWifiAvailable = false;
    bool nextWifiConnected = false;
    QString nextWifiSsid;
    QString nextWifiInterface;
    QString nextWifiIpv4;
    int nextWifiSignalPercent = -1;
    int nextWifiFrequencyMHz = 0;

    bool nextBluetoothAvailable = false;
    bool nextBluetoothEnabled = false;
    QString nextBluetoothAdapterName;
    QString nextBluetoothAddress;
    QStringList nextBluetoothDevices;

    const auto flushInterface = [&] {
        if (hasCurrentInterface && !currentInterface.device.isEmpty())
            interfaces.push_back(currentInterface);
        currentInterface = NetworkInterfaceInfo{};
        hasCurrentInterface = false;
    };

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line == QStringLiteral("__META__")) { flushInterface(); section = Section::Meta; continue; }
        if (line == QStringLiteral("__NM_GENERAL__")) { flushInterface(); section = Section::NetworkGeneral; continue; }
        if (line == QStringLiteral("__NM_RADIO__")) { flushInterface(); section = Section::NetworkRadio; continue; }
        if (line == QStringLiteral("__NM_WIFI__")) { flushInterface(); section = Section::WifiAccessPoint; continue; }
        if (line == QStringLiteral("__BT_SHOW__")) { flushInterface(); section = Section::BluetoothShow; continue; }
        if (line == QStringLiteral("__BT_CONNECTED__")) { flushInterface(); section = Section::BluetoothConnected; continue; }
        if (line == QStringLiteral("__END__")) { flushInterface(); section = Section::None; continue; }

        switch (section) {
        case Section::Meta:
            if (line == QStringLiteral("NMCLI=1")) nmcliPresent = true;
            if (line == QStringLiteral("BLUEZ=1")) bluezPresent = true;
            break;
        case Section::NetworkGeneral:
            if (line.isEmpty()) {
                flushInterface();
            } else if (line.startsWith(QStringLiteral("GENERAL.DEVICE:"))) {
                flushInterface();
                hasCurrentInterface = true;
                currentInterface.device = valueAfterColon(line);
            } else if (hasCurrentInterface && line.startsWith(QStringLiteral("GENERAL.TYPE:"))) {
                currentInterface.type = valueAfterColon(line);
            } else if (hasCurrentInterface && line.startsWith(QStringLiteral("GENERAL.STATE:"))) {
                currentInterface.state = valueAfterColon(line);
            } else if (hasCurrentInterface && line.startsWith(QStringLiteral("GENERAL.CONNECTION:"))) {
                currentInterface.connection = valueAfterColon(line);
            } else if (hasCurrentInterface && line.startsWith(QStringLiteral("IP4.ADDRESS"))) {
                currentInterface.ipv4 = valueAfterColon(line).section(QLatin1Char('/'), 0, 0);
            }
            break;
        case Section::NetworkRadio:
            if (!line.isEmpty())
                radioState = line;
            break;
        case Section::WifiAccessPoint: {
            const QStringList fields = splitNmcliFields(rawLine.trimmed());
            if (fields.size() >= 5 && fields.at(0).trimmed() == QStringLiteral("*")) {
                nextWifiConnected = true;
                nextWifiSsid = fields.at(1).trimmed();
                nextWifiSignalPercent = fields.at(2).trimmed().toInt();
                const QRegularExpressionMatch frequency = QRegularExpression(QStringLiteral("(\\d+)")).match(fields.at(3));
                if (frequency.hasMatch())
                    nextWifiFrequencyMHz = frequency.captured(1).toInt();
                nextWifiInterface = fields.at(4).trimmed();
            }
            break;
        }
        case Section::BluetoothShow:
            if (line.startsWith(QStringLiteral("Controller "))) {
                nextBluetoothAvailable = true;
                nextBluetoothAddress = line.section(QLatin1Char(' '), 1, 1);
            } else if (line.startsWith(QStringLiteral("Alias:"))) {
                nextBluetoothAdapterName = valueAfterColon(line);
            } else if (nextBluetoothAdapterName.isEmpty() && line.startsWith(QStringLiteral("Name:"))) {
                nextBluetoothAdapterName = valueAfterColon(line);
            } else if (line.startsWith(QStringLiteral("Powered:"))) {
                nextBluetoothEnabled = valueAfterColon(line).compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
            }
            break;
        case Section::BluetoothConnected:
            if (line.startsWith(QStringLiteral("Device "))) {
                const QString deviceName = line.section(QLatin1Char(' '), 2);
                if (!deviceName.isEmpty())
                    nextBluetoothDevices.push_back(deviceName);
            }
            break;
        case Section::None:
            break;
        }
    }
    flushInterface();

    for (const NetworkInterfaceInfo &interfaceInfo : std::as_const(interfaces)) {
        if (interfaceInfo.type != QStringLiteral("wifi"))
            continue;
        nextWifiAvailable = true;
        if (nextWifiInterface.isEmpty())
            nextWifiInterface = interfaceInfo.device;
        if (nextWifiInterface == interfaceInfo.device) {
            nextWifiIpv4 = interfaceInfo.ipv4;
            if (interfaceInfo.state.startsWith(QStringLiteral("100")))
                nextWifiConnected = true;
        }
    }

    m_supported = nmcliPresent || bluezPresent;
    if (nmcliPresent && bluezPresent)
        m_backendName = QStringLiteral("NetworkManager · BlueZ");
    else if (nmcliPresent)
        m_backendName = QStringLiteral("NetworkManager");
    else if (bluezPresent)
        m_backendName = QStringLiteral("BlueZ");
    else
        m_backendName = QStringLiteral("系统后端不可用");
    m_statusError = m_supported ? QString() : QStringLiteral("未安装 NetworkManager 或 BlueZ 命令行工具");

    m_wifiAvailable = nextWifiAvailable;
    m_wifiEnabled = radioState.compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0;
    m_wifiConnected = nextWifiConnected;
    m_wifiSsid = nextWifiSsid;
    m_wifiInterface = nextWifiInterface;
    m_wifiIpv4 = nextWifiIpv4;
    m_wifiSignalPercent = nextWifiSignalPercent;
    m_wifiFrequencyMHz = nextWifiFrequencyMHz;

    m_bluetoothAvailable = nextBluetoothAvailable;
    m_bluetoothEnabled = nextBluetoothEnabled;
    m_bluetoothAdapterName = nextBluetoothAdapterName;
    m_bluetoothAddress = nextBluetoothAddress;
    m_bluetoothDeviceNames = nextBluetoothDevices;
    m_lastUpdated = QDateTime::currentDateTime();
    emit statusChanged();
}

void ConnectivityService::setRefreshing(bool refreshing)
{
    if (m_refreshing == refreshing)
        return;
    m_refreshing = refreshing;
    emit refreshingChanged();
}

void ConnectivityService::setProbeError(const QString &message)
{
    m_statusError = message;
    m_lastUpdated = QDateTime::currentDateTime();
    setRefreshing(false);
    emit statusChanged();
}

QStringList ConnectivityService::splitNmcliFields(const QString &line)
{
    QStringList fields;
    QString field;
    bool escaped = false;
    for (const QChar character : line) {
        if (escaped) {
            field.append(character);
            escaped = false;
        } else if (character == QLatin1Char('\\')) {
            escaped = true;
        } else if (character == QLatin1Char(':')) {
            fields.push_back(field);
            field.clear();
        } else {
            field.append(character);
        }
    }
    if (escaped)
        field.append(QLatin1Char('\\'));
    fields.push_back(field);
    return fields;
}
