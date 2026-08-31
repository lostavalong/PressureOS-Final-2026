#include "AssistantContextService.h"

#include "AppController.h"
#include "ConnectivityService.h"
#include "DatabaseService.h"
#include "DeviceSimulator.h"
#include "SerialDeviceGateway.h"
#include "TaskManager.h"

#include <QtMath>

AssistantContextService::AssistantContextService(AppController *app, TaskManager *tasks,
                                                 DeviceSimulator *device,
                                                 SerialDeviceGateway *deviceLink,
                                                 ConnectivityService *connectivity,
                                                 DatabaseService *database,
                                                 QObject *parent)
    : QObject(parent),
      m_app(app),
      m_tasks(tasks),
      m_device(device),
      m_deviceLink(deviceLink),
      m_connectivity(connectivity),
      m_database(database)
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(180);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AssistantContextService::contextChanged);

    if (m_app) {
        connect(m_app, &AppController::currentPageChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_app, &AppController::expertModeChanged,
                this, &AssistantContextService::scheduleRefresh);
    }
    if (m_tasks) {
        connect(m_tasks, &TaskManager::taskListChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_tasks, &TaskManager::currentTaskChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_tasks, &TaskManager::rowsChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_tasks, &TaskManager::resultsChanged,
                this, &AssistantContextService::scheduleRefresh);
    }
    if (m_device) {
        connect(m_device, &DeviceSimulator::measurementChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_device, &DeviceSimulator::safetyChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_device, &DeviceSimulator::connectionChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_device, &DeviceSimulator::recordingChanged,
                this, &AssistantContextService::scheduleRefresh);
    }
    if (m_deviceLink) {
        connect(m_deviceLink, &SerialDeviceGateway::connectionChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_deviceLink, &SerialDeviceGateway::statisticsChanged,
                this, &AssistantContextService::scheduleRefresh);
    }
    if (m_connectivity) {
        connect(m_connectivity, &ConnectivityService::statusChanged,
                this, &AssistantContextService::scheduleRefresh);
    }
    if (m_database) {
        connect(m_database, &DatabaseService::readyChanged,
                this, &AssistantContextService::scheduleRefresh);
        connect(m_database, &DatabaseService::statisticsChanged,
                this, &AssistantContextService::scheduleRefresh);
    }
}

QVariantMap AssistantContextService::snapshot() const
{
    QVariantMap context;
    const QString page = m_app ? m_app->currentPage() : QStringLiteral("home");
    context.insert(QStringLiteral("page"), page);
    context.insert(QStringLiteral("expertMode"), m_app && m_app->expertMode());
    context.insert(QStringLiteral("demoMode"), !m_app || m_app->demoMode());

    const bool hasTask = m_tasks && m_tasks->hasCurrentTask();
    context.insert(QStringLiteral("hasTask"), hasTask);
    context.insert(QStringLiteral("taskId"), hasTask ? m_tasks->currentTaskId() : QString());
    context.insert(QStringLiteral("taskTitle"), hasTask ? m_tasks->currentTaskTitle() : QString());
    context.insert(QStringLiteral("templateId"), hasTask ? m_tasks->currentTemplateId() : QString());
    context.insert(QStringLiteral("templateName"), hasTask ? m_tasks->currentTemplateName() : QString());
    context.insert(QStringLiteral("taskStatus"), hasTask ? m_tasks->currentStatus() : QString());
    context.insert(QStringLiteral("taskDescription"), hasTask ? m_tasks->taskDescription() : QString());
    context.insert(QStringLiteral("taskObjective"), hasTask ? m_tasks->taskObjective() : QString());
    context.insert(QStringLiteral("isQuickTask"), hasTask && m_tasks->isQuickTask());
    context.insert(QStringLiteral("allowsEngineeringCapture"),
                   hasTask && m_tasks->allowsEngineeringCapture());
    context.insert(QStringLiteral("captureMode"), hasTask ? m_tasks->captureMode() : QString());
    context.insert(QStringLiteral("captureTrustLevel"),
                   hasTask ? m_tasks->captureTrustLevel() : QStringLiteral("blocked"));
    context.insert(QStringLiteral("captureBlockReason"),
                   hasTask ? m_tasks->captureBlockReason() : QString());
    context.insert(QStringLiteral("canCaptureCurrent"),
                   hasTask && m_tasks->canCaptureCurrent());
    context.insert(QStringLiteral("containsEngineeringData"),
                   hasTask && m_tasks->containsEngineeringData());
    context.insert(QStringLiteral("xVariableName"), hasTask ? m_tasks->xVariableName() : QString());
    context.insert(QStringLiteral("xVariableUnit"), hasTask ? m_tasks->xVariableUnit() : QString());

    const int stage = hasTask ? m_tasks->currentStage() : 0;
    const QVariantList workflow = hasTask ? m_tasks->workflow() : QVariantList{};
    QVariantMap step;
    if (stage >= 0 && stage < workflow.size())
        step = workflow.at(stage).toMap();
    context.insert(QStringLiteral("stage"), stage);
    context.insert(QStringLiteral("stageNumber"), stage + 1);
    context.insert(QStringLiteral("stageTitle"), step.value(QStringLiteral("title")));
    context.insert(QStringLiteral("stageDetail"), step.value(QStringLiteral("detail")));
    context.insert(QStringLiteral("workflow"), workflow);
    context.insert(QStringLiteral("preparationItems"),
                   hasTask ? m_tasks->preparationItems() : QVariantList{});
    context.insert(QStringLiteral("safetyNotes"),
                   hasTask ? m_tasks->safetyNotes() : QVariantList{});

    const int completed = hasTask ? m_tasks->completedPoints() : 0;
    const int target = hasTask ? m_tasks->targetPoints() : 0;
    const int minimum = hasTask ? (m_tasks->isQuickTask() ? 3 : target) : 0;
    context.insert(QStringLiteral("completedPoints"), completed);
    context.insert(QStringLiteral("targetPoints"), target);
    context.insert(QStringLiteral("minimumPoints"), minimum);
    context.insert(QStringLiteral("remainingPoints"), qMax(0, minimum - completed));
    context.insert(QStringLiteral("canFinishMeasurement"),
                   hasTask && m_tasks->canFinishMeasurement());
    context.insert(QStringLiteral("hasFit"), hasTask && m_tasks->hasFit());
    context.insert(QStringLiteral("equation"), hasTask ? m_tasks->equation() : QString());
    context.insert(QStringLiteral("fitQuality"), hasTask ? m_tasks->fitQuality() : QString());
    context.insert(QStringLiteral("rSquared"), hasTask ? m_tasks->rSquared() : 0.0);
    context.insert(QStringLiteral("pearsonR"), hasTask ? m_tasks->pearsonR() : 0.0);
    context.insert(QStringLiteral("residualStd"), hasTask ? m_tasks->residualStd() : 0.0);
    context.insert(QStringLiteral("maxAbsResidual"), hasTask ? m_tasks->maxAbsResidual() : 0.0);
    context.insert(QStringLiteral("typeAUncertainty"),
                   hasTask ? m_tasks->typeAUncertainty() : 0.0);
    context.insert(QStringLiteral("typeBUncertainty"),
                   hasTask ? m_tasks->typeBUncertainty() : 0.0);
    context.insert(QStringLiteral("combinedUncertainty"),
                   hasTask ? m_tasks->combinedUncertainty() : 0.0);
    context.insert(QStringLiteral("expandedUncertainty"),
                   hasTask ? m_tasks->expandedUncertainty() : 0.0);
    context.insert(QStringLiteral("outlierCount"), hasTask ? m_tasks->outlierCount() : 0);
    context.insert(QStringLiteral("outlierSummary"),
                   hasTask ? m_tasks->outlierSummary() : QString());

    if (m_device) {
        context.insert(QStringLiteral("pressureKPa"), m_device->pressureKPa());
        context.insert(QStringLiteral("formattedPressure"), m_device->formattedPressure());
        context.insert(QStringLiteral("pressureUnit"), m_device->unit());
        context.insert(QStringLiteral("temperature"), m_device->temperature());
        context.insert(QStringLiteral("temperatureCompensationEnabled"),
                       m_device->temperatureCompensationEnabled());
        context.insert(QStringLiteral("temperatureModeText"),
                       m_device->temperatureModeText());
        context.insert(QStringLiteral("stable"), m_device->stable());
        context.insert(QStringLiteral("stabilityP2P"), m_device->stabilityP2P());
        context.insert(QStringLiteral("utilizationPercent"), m_device->utilizationPercent());
        context.insert(QStringLiteral("pressureRate"), m_device->pressureRateKPaPerSec());
        context.insert(QStringLiteral("safetyLevel"), m_device->safetyLevel());
        context.insert(QStringLiteral("safetyTitle"), m_device->safetyTitle());
        context.insert(QStringLiteral("safetyMessage"), m_device->safetyMessage());
        context.insert(QStringLiteral("tripLatched"), m_device->tripLatched());
        context.insert(QStringLiteral("controlOutputEnabled"), m_device->controlOutputEnabled());
        context.insert(QStringLiteral("recording"), m_device->recording());
        context.insert(QStringLiteral("deviceConnected"), m_device->connected());
        context.insert(QStringLiteral("hardwareMode"), m_device->hardwareMode());
        context.insert(QStringLiteral("valueTrustedForSafety"),
                       m_device->valueTrustedForSafety());
        context.insert(QStringLiteral("rangeText"), m_device->rangeText());
        context.insert(QStringLiteral("resolutionText"), m_device->resolutionText());
    }

    if (m_deviceLink) {
        context.insert(QStringLiteral("serialRunning"), m_deviceLink->running());
        context.insert(QStringLiteral("serialConnected"), m_deviceLink->connected());
        context.insert(QStringLiteral("dataFresh"), m_deviceLink->dataFresh());
        context.insert(QStringLiteral("pressureFresh"), m_deviceLink->pressureFresh());
        context.insert(QStringLiteral("temperatureFresh"), m_deviceLink->temperatureFresh());
        context.insert(QStringLiteral("temperatureChannelEnabled"),
                       m_deviceLink->temperatureChannelEnabled());
        context.insert(QStringLiteral("protocolIntegrityAvailable"),
                       m_deviceLink->protocolIntegrityAvailable());
        context.insert(QStringLiteral("serialState"), m_deviceLink->connectionState());
        context.insert(QStringLiteral("serialStatus"), m_deviceLink->statusText());
        context.insert(QStringLiteral("serialPort"), m_deviceLink->portName());
        context.insert(QStringLiteral("protocolName"), m_deviceLink->protocolName());
        context.insert(QStringLiteral("lastFrameAgeMs"), m_deviceLink->lastFrameAgeMs());
        context.insert(QStringLiteral("invalidFrames"),
                       QVariant::fromValue(m_deviceLink->invalidFrames()));
        context.insert(QStringLiteral("crcErrors"),
                       QVariant::fromValue(m_deviceLink->crcErrors()));
        context.insert(QStringLiteral("droppedFrames"),
                       QVariant::fromValue(m_deviceLink->droppedFrames()));
        context.insert(QStringLiteral("lastSerialError"), m_deviceLink->lastError());
    }

    if (m_connectivity) {
        context.insert(QStringLiteral("wifiConnected"), m_connectivity->wifiConnected());
        context.insert(QStringLiteral("wifiSsid"), m_connectivity->wifiSsid());
        context.insert(QStringLiteral("wifiSignalPercent"),
                       m_connectivity->wifiSignalPercent());
        context.insert(QStringLiteral("bluetoothEnabled"),
                       m_connectivity->bluetoothEnabled());
        context.insert(QStringLiteral("bluetoothConnectedCount"),
                       m_connectivity->bluetoothConnectedCount());
    }
    context.insert(QStringLiteral("databaseReady"), m_database && m_database->ready());
    return context;
}

void AssistantContextService::scheduleRefresh()
{
    if (!m_refreshTimer.isActive())
        m_refreshTimer.start();
}
