#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantMap>

class AppController;
class ConnectivityService;
class DatabaseService;
class DeviceSimulator;
class SerialDeviceGateway;
class TaskManager;

class AssistantContextService final : public QObject
{
    Q_OBJECT

public:
    AssistantContextService(AppController *app, TaskManager *tasks,
                            DeviceSimulator *device, SerialDeviceGateway *deviceLink,
                            ConnectivityService *connectivity, DatabaseService *database,
                            QObject *parent = nullptr);

    QVariantMap snapshot() const;

signals:
    void contextChanged();

private slots:
    void scheduleRefresh();

private:
    AppController *m_app = nullptr;
    TaskManager *m_tasks = nullptr;
    DeviceSimulator *m_device = nullptr;
    SerialDeviceGateway *m_deviceLink = nullptr;
    ConnectivityService *m_connectivity = nullptr;
    DatabaseService *m_database = nullptr;
    QTimer m_refreshTimer;
};
