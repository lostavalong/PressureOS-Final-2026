#include "AssistantActionDispatcher.h"

#include "AppController.h"
#include "ConnectivityService.h"
#include "DatabaseService.h"
#include "DeviceSimulator.h"
#include "SerialDeviceGateway.h"
#include "TaskManager.h"

AssistantActionDispatcher::AssistantActionDispatcher(
    AppController *app, TaskManager *tasks, DeviceSimulator *device,
    SerialDeviceGateway *deviceLink, ConnectivityService *connectivity,
    DatabaseService *database)
    : m_app(app),
      m_tasks(tasks),
      m_device(device),
      m_deviceLink(deviceLink),
      m_connectivity(connectivity),
      m_database(database)
{
}

QVariantMap AssistantActionDispatcher::dispatch(const QString &actionId)
{
    if (actionId == QStringLiteral("open_home"))
        return navigate(QStringLiteral("home"), QStringLiteral("已返回桌面"));
    if (actionId == QStringLiteral("open_measurement"))
        return navigate(QStringLiteral("measure"), QStringLiteral("已打开实时测量"));
    if (actionId == QStringLiteral("open_tasks"))
        return navigate(QStringLiteral("tasks"), QStringLiteral("已打开任务中心"));
    if (actionId == QStringLiteral("open_templates"))
        return navigate(QStringLiteral("templates"), QStringLiteral("已打开模板库"));
    if (actionId == QStringLiteral("open_device"))
        return navigate(QStringLiteral("device"), QStringLiteral("已打开设备与连接"));

    if (actionId == QStringLiteral("open_current_task")) {
        if (!m_tasks || !m_tasks->hasCurrentTask())
            return finish(false, QStringLiteral("当前没有可打开的任务"), false);
        return navigate(QStringLiteral("runner"), QStringLiteral("已返回当前任务"));
    }

    if (actionId == QStringLiteral("reconnect_device")) {
        if (m_deviceLink)
            m_deviceLink->reconnect();
        if (m_app)
            m_app->navigate(QStringLiteral("device"));
        audit(actionId, QStringLiteral("请求重新检测串口并打开设备诊断"));
        return finish(true, QStringLiteral("正在重新检测下位机"));
    }

    if (actionId == QStringLiteral("refresh_connectivity")) {
        if (m_connectivity)
            m_connectivity->refresh();
        if (m_app)
            m_app->navigate(QStringLiteral("device"));
        audit(actionId, QStringLiteral("刷新 NetworkManager 与 BlueZ 只读状态"));
        return finish(true, QStringLiteral("正在刷新 Wi-Fi 与蓝牙状态"));
    }

    if (!m_tasks || !m_tasks->hasCurrentTask())
        return finish(false, QStringLiteral("请先创建或选择任务"), false);

    if (actionId == QStringLiteral("task_start_measurement")) {
        m_tasks->setCurrentStage(1);
        if (m_app)
            m_app->navigate(QStringLiteral("runner"));
        audit(actionId, QStringLiteral("从任务说明进入数据采集阶段"));
        return finish(true, QStringLiteral("已进入数据采集阶段"));
    }

    if (actionId == QStringLiteral("task_finish_measurement")) {
        if (!m_tasks->finishMeasurement())
            return finish(false, QStringLiteral("当前测点数量尚未满足流程要求"), false);
        if (m_app)
            m_app->navigate(QStringLiteral("runner"));
        audit(actionId, QStringLiteral("确认测量阶段完成并进入数据处理"));
        return finish(true, QStringLiteral("已进入数据处理阶段"));
    }

    if (actionId == QStringLiteral("task_review_points")) {
        m_tasks->setCurrentStage(1);
        if (m_app)
            m_app->navigate(QStringLiteral("runner"));
        audit(actionId, QStringLiteral("返回采集页人工核对原始测点"));
        return finish(true, QStringLiteral("已返回原始数据采集页"));
    }

    if (actionId == QStringLiteral("task_begin_analysis")) {
        if (!m_tasks->hasFit())
            return finish(false, QStringLiteral("至少需要三个有效点才能进入分析"), false);
        m_tasks->setCurrentStage(3);
        if (m_app)
            m_app->navigate(QStringLiteral("runner"));
        audit(actionId, QStringLiteral("打开拟合、残差与不确定度解释"));
        return finish(true, QStringLiteral("已打开结果分析"));
    }

    if (actionId == QStringLiteral("task_confirm_analysis")) {
        if (!m_tasks->confirmAnalysis())
            return finish(false, QStringLiteral("当前结果还不能确认"), false);
        if (m_app)
            m_app->navigate(QStringLiteral("runner"));
        audit(actionId, QStringLiteral("人工确认分析结果并进入导出阶段"));
        return finish(true, QStringLiteral("分析已确认，可以导出"));
    }

    if (actionId == QStringLiteral("task_export_bundle")) {
        const QString path = m_tasks->exportBundle();
        if (path.isEmpty())
            return finish(false, QStringLiteral("数据包生成失败，请检查数据和磁盘空间"), false);
        if (m_app)
            m_app->navigate(QStringLiteral("runner"));
        audit(actionId, QStringLiteral("生成任务数据包：%1").arg(path));
        return finish(true, QStringLiteral("任务数据包已生成"), true, path);
    }

    return finish(false, QStringLiteral("该操作尚未加入安全白名单"), false);
}

QVariantMap AssistantActionDispatcher::navigate(const QString &page, const QString &message)
{
    if (m_app)
        m_app->navigate(page);
    audit(QStringLiteral("navigate_%1").arg(page), message);
    return finish(true, message);
}

QVariantMap AssistantActionDispatcher::finish(bool success, const QString &message,
                                              bool closeAssistant, const QString &detail)
{
    if (m_app) {
        m_app->showToast(message);
        if (success && closeAssistant)
            m_app->setAssistantOpen(false);
    }
    return QVariantMap{{QStringLiteral("success"), success},
                       {QStringLiteral("message"), message},
                       {QStringLiteral("closeAssistant"), closeAssistant},
                       {QStringLiteral("detail"), detail}};
}

void AssistantActionDispatcher::audit(const QString &actionId, const QString &detail)
{
    if (!m_database)
        return;
    const QString taskId = m_tasks && m_tasks->hasCurrentTask()
        ? m_tasks->currentTaskId() : QStringLiteral("global");
    m_database->appendAuditLog(QStringLiteral("assistant.%1").arg(actionId),
                               QStringLiteral("task=%1; %2").arg(taskId, detail));
}
