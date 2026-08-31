#pragma once

#include <QString>
#include <QVariantMap>

class AppController;
class ConnectivityService;
class DatabaseService;
class DeviceSimulator;
class SerialDeviceGateway;
class TaskManager;

class AssistantActionDispatcher
{
public:
    AssistantActionDispatcher(AppController *app, TaskManager *tasks,
                              DeviceSimulator *device, SerialDeviceGateway *deviceLink,
                              ConnectivityService *connectivity, DatabaseService *database);

    QVariantMap dispatch(const QString &actionId);

private:
    QVariantMap navigate(const QString &page, const QString &message);
    QVariantMap finish(bool success, const QString &message, bool closeAssistant = true,
                       const QString &detail = {});
    void audit(const QString &actionId, const QString &detail);

    AppController *m_app = nullptr;
    TaskManager *m_tasks = nullptr;
    DeviceSimulator *m_device = nullptr;
    SerialDeviceGateway *m_deviceLink = nullptr;
    ConnectivityService *m_connectivity = nullptr;
    DatabaseService *m_database = nullptr;
};
