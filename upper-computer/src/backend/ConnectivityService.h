#pragma once

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

class ConnectivityService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported NOTIFY statusChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QString backendName READ backendName NOTIFY statusChanged)
    Q_PROPERTY(QString lastUpdatedText READ lastUpdatedText NOTIFY statusChanged)
    Q_PROPERTY(QString statusError READ statusError NOTIFY statusChanged)

    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY statusChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY statusChanged)
    Q_PROPERTY(QString wifiSsid READ wifiSsid NOTIFY statusChanged)
    Q_PROPERTY(QString wifiInterface READ wifiInterface NOTIFY statusChanged)
    Q_PROPERTY(QString wifiIpv4 READ wifiIpv4 NOTIFY statusChanged)
    Q_PROPERTY(int wifiSignalPercent READ wifiSignalPercent NOTIFY statusChanged)
    Q_PROPERTY(int wifiFrequencyMHz READ wifiFrequencyMHz NOTIFY statusChanged)
    Q_PROPERTY(QString wifiBand READ wifiBand NOTIFY statusChanged)
    Q_PROPERTY(QString wifiStatusText READ wifiStatusText NOTIFY statusChanged)
    Q_PROPERTY(QString wifiPrimaryText READ wifiPrimaryText NOTIFY statusChanged)
    Q_PROPERTY(QString wifiDetailText READ wifiDetailText NOTIFY statusChanged)

    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable NOTIFY statusChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString bluetoothAdapterName READ bluetoothAdapterName NOTIFY statusChanged)
    Q_PROPERTY(QString bluetoothAddress READ bluetoothAddress NOTIFY statusChanged)
    Q_PROPERTY(int bluetoothConnectedCount READ bluetoothConnectedCount NOTIFY statusChanged)
    Q_PROPERTY(QString bluetoothDeviceSummary READ bluetoothDeviceSummary NOTIFY statusChanged)
    Q_PROPERTY(QString bluetoothStatusText READ bluetoothStatusText NOTIFY statusChanged)
    Q_PROPERTY(QString bluetoothPrimaryText READ bluetoothPrimaryText NOTIFY statusChanged)
    Q_PROPERTY(QString bluetoothDetailText READ bluetoothDetailText NOTIFY statusChanged)

public:
    explicit ConnectivityService(QObject *parent = nullptr);

    bool supported() const { return m_supported; }
    bool refreshing() const { return m_refreshing; }
    QString backendName() const { return m_backendName; }
    QString lastUpdatedText() const;
    QString statusError() const { return m_statusError; }

    bool wifiAvailable() const { return m_wifiAvailable; }
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool wifiConnected() const { return m_wifiConnected; }
    QString wifiSsid() const { return m_wifiSsid; }
    QString wifiInterface() const { return m_wifiInterface; }
    QString wifiIpv4() const { return m_wifiIpv4; }
    int wifiSignalPercent() const { return m_wifiSignalPercent; }
    int wifiFrequencyMHz() const { return m_wifiFrequencyMHz; }
    QString wifiBand() const;
    QString wifiStatusText() const;
    QString wifiPrimaryText() const;
    QString wifiDetailText() const;

    bool bluetoothAvailable() const { return m_bluetoothAvailable; }
    bool bluetoothEnabled() const { return m_bluetoothEnabled; }
    QString bluetoothAdapterName() const { return m_bluetoothAdapterName; }
    QString bluetoothAddress() const { return m_bluetoothAddress; }
    int bluetoothConnectedCount() const { return m_bluetoothDeviceNames.size(); }
    QString bluetoothDeviceSummary() const;
    QString bluetoothStatusText() const;
    QString bluetoothPrimaryText() const;
    QString bluetoothDetailText() const;

    Q_INVOKABLE void refresh();

signals:
    void statusChanged();
    void refreshingChanged();

private slots:
    void handleProbeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProbeError(QProcess::ProcessError error);
    void handleProbeTimeout();

private:
    void parseProbeOutput(const QString &output);
    void setRefreshing(bool refreshing);
    void setProbeError(const QString &message);
    static QStringList splitNmcliFields(const QString &line);

    QProcess m_probe;
    QTimer m_refreshTimer;
    QTimer m_timeoutTimer;
    QDateTime m_lastUpdated;
    bool m_probeTimedOut = false;
    bool m_supported = false;
    bool m_refreshing = false;
    QString m_backendName = QStringLiteral("等待系统检测");
    QString m_statusError;

    bool m_wifiAvailable = false;
    bool m_wifiEnabled = false;
    bool m_wifiConnected = false;
    QString m_wifiSsid;
    QString m_wifiInterface;
    QString m_wifiIpv4;
    int m_wifiSignalPercent = -1;
    int m_wifiFrequencyMHz = 0;

    bool m_bluetoothAvailable = false;
    bool m_bluetoothEnabled = false;
    QString m_bluetoothAdapterName;
    QString m_bluetoothAddress;
    QStringList m_bluetoothDeviceNames;
};
