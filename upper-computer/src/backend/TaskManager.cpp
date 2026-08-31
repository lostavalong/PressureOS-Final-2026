#include "TaskManager.h"

#include "DatabaseService.h"
#include "DeviceSimulator.h"
#include "SerialDeviceGateway.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
QVariantList strings(std::initializer_list<const char *> values)
{
    QVariantList result;
    for (const char *value : values)
        result.push_back(QString::fromUtf8(value));
    return result;
}

QVariantList makeWorkflow(std::initializer_list<std::pair<const char *, const char *>> values)
{
    QVariantList result;
    int index = 0;
    for (const auto &value : values) {
        result.push_back(QVariantMap{{QStringLiteral("index"), ++index},
                                     {QStringLiteral("title"), QString::fromUtf8(value.first)},
                                     {QStringLiteral("detail"), QString::fromUtf8(value.second)}});
    }
    return result;
}

QString exportRoot()
{
    const QString configuredRoot = qEnvironmentVariable("PRESSUREOS_EXPORT_ROOT").trimmed();
    if (!configuredRoot.isEmpty())
        return QDir(configuredRoot).absolutePath();
    QString root = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (root.isEmpty())
        root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return root;
}
}

TaskManager::TaskManager(DatabaseService *database, QObject *parent)
    : QObject(parent), m_database(database)
{
    connect(this, &TaskManager::currentTaskChanged,
            this, &TaskManager::capturePolicyChanged);
    reloadTaskList();
    if (m_tasks.isEmpty())
        initializeDemoTasks();
    else
        selectTask(m_tasks.first().toMap().value(QStringLiteral("id")).toString());
}

QString TaskManager::currentTaskTitle() const
{
    return m_currentTask.value(QStringLiteral("name")).toString();
}

QString TaskManager::currentTaskId() const
{
    return m_currentTask.value(QStringLiteral("id")).toString();
}

QString TaskManager::currentTemplateId() const
{
    return m_currentTask.value(QStringLiteral("templateId")).toString();
}

QString TaskManager::currentTemplateName() const
{
    return m_currentTask.value(QStringLiteral("templateName")).toString();
}

QString TaskManager::currentStatus() const
{
    return m_currentTask.value(QStringLiteral("status")).toString();
}

QString TaskManager::taskDescription() const
{
    return definitionFor(currentTemplateId()).value(QStringLiteral("description")).toString();
}

QString TaskManager::taskObjective() const
{
    return definitionFor(currentTemplateId()).value(QStringLiteral("objective")).toString();
}

QString TaskManager::xVariableName() const
{
    const QString configured = m_currentTask.value(QStringLiteral("xName")).toString().trimmed();
    if (!configured.isEmpty())
        return configured;
    return definitionFor(currentTemplateId()).value(QStringLiteral("xName")).toString();
}

QString TaskManager::xVariableUnit() const
{
    if (m_currentTask.contains(QStringLiteral("xUnit"))
        && m_currentTask.value(QStringLiteral("xUnit")).isValid())
        return m_currentTask.value(QStringLiteral("xUnit")).toString().trimmed();
    return definitionFor(currentTemplateId()).value(QStringLiteral("xUnit")).toString();
}

bool TaskManager::isQuickTask() const
{
    return currentTemplateId() == QStringLiteral("quick.blank");
}

bool TaskManager::allowsEngineeringCapture() const
{
    const QString templateId = currentTemplateId();
    return templateId == QStringLiteral("quality.zero-drift")
        || templateId == QStringLiteral("metrology.multi-point");
}

QString TaskManager::captureTrustLevel() const
{
    if (!hasCurrentTask())
        return QStringLiteral("blocked");
    if (!m_device || !m_device->hardwareMode())
        return QStringLiteral("formal");
    if (m_device->tripLatched() || !m_gateway || !m_gateway->dataFresh()
        || !m_gateway->pressureFresh())
        return QStringLiteral("blocked");
    return QStringLiteral("formal");
}

QString TaskManager::captureBlockReason() const
{
    if (!hasCurrentTask())
        return QStringLiteral("请先创建或选择任务");
    if (!m_device)
        return {};
    if (m_device->tripLatched())
        return QStringLiteral("高压报警已锁存；请先人工停止加压、泄压检查并复位");
    if (!m_device->hardwareMode())
        return m_device->stable() ? QString{} : QStringLiteral("当前读数仍在收敛，请等待稳定后采集");
    if (!m_gateway || !m_gateway->dataFresh() || !m_gateway->pressureFresh())
        return QStringLiteral("下位机压力数据已中断或尚未到达，不能记录旧读数");
    if (currentTemplateId() == QStringLiteral("quality.zero-drift")
        && qAbs(m_device->pressureKPa()) > 5.0)
        return QStringLiteral("零点观察要求压力端卸压并连通大气；当前示值偏离零点超过 5 kPa");
    if (!m_device->stable())
        return QStringLiteral("当前读数仍在收敛，请保持零压工况并等待稳定");
    return {};
}

bool TaskManager::canCaptureCurrent() const
{
    return captureBlockReason().isEmpty();
}

bool TaskManager::containsEngineeringData() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [](const QVariant &item) {
        return item.toMap().value(QStringLiteral("source")).toString()
            .contains(QStringLiteral("工程记录"));
    });
}

void TaskManager::attachMeasurementSource(DeviceSimulator *device,
                                          SerialDeviceGateway *gateway)
{
    if (m_device)
        disconnect(m_device, nullptr, this, nullptr);
    if (m_gateway)
        disconnect(m_gateway, nullptr, this, nullptr);
    m_device = device;
    m_gateway = gateway;
    if (m_device) {
        connect(m_device, &DeviceSimulator::measurementChanged,
                this, &TaskManager::capturePolicyChanged);
        connect(m_device, &DeviceSimulator::safetyChanged,
                this, &TaskManager::capturePolicyChanged);
        connect(m_device, &DeviceSimulator::connectionChanged,
                this, &TaskManager::capturePolicyChanged);
    }
    if (m_gateway) {
        connect(m_gateway, &SerialDeviceGateway::connectionChanged,
                this, &TaskManager::capturePolicyChanged);
        connect(m_gateway, &SerialDeviceGateway::statisticsChanged,
                this, &TaskManager::capturePolicyChanged);
    }
    emit capturePolicyChanged();
}

QString TaskManager::captureMode() const
{
    const QString mode = m_currentTask.value(QStringLiteral("captureMode")).toString();
    return mode == QStringLiteral("auto-index") ? mode : QStringLiteral("manual-x");
}

double TaskManager::nextAutoX() const
{
    double maximum = 0.0;
    for (const QVariant &item : m_rows)
        maximum = qMax(maximum, item.toMap().value(QStringLiteral("mass")).toDouble());
    return qFloor(maximum) + 1.0;
}

QVariantList TaskManager::workflow() const
{
    return definitionFor(currentTemplateId()).value(QStringLiteral("workflow")).toList();
}

QVariantList TaskManager::preparationItems() const
{
    return definitionFor(currentTemplateId()).value(QStringLiteral("preparation")).toList();
}

QVariantList TaskManager::safetyNotes() const
{
    return definitionFor(currentTemplateId()).value(QStringLiteral("safety")).toList();
}

int TaskManager::targetPoints() const
{
    return qMax(1, m_currentTask.value(QStringLiteral("targetPoints"), 6).toInt());
}

double TaskManager::progress() const
{
    return qMin(1.0, completedPoints() / static_cast<double>(targetPoints()));
}

int TaskManager::currentStage() const
{
    return qBound(0, m_currentTask.value(QStringLiteral("currentStage")).toInt(), 4);
}

QString TaskManager::lastSavedText() const
{
    if (!hasCurrentTask())
        return QStringLiteral("尚未创建任务");
    return relativeTime(m_currentTask.value(QStringLiteral("updatedAt")).toLongLong())
        + QStringLiteral("自动保存");
}

QString TaskManager::equation() const
{
    if (!hasFit())
        return QStringLiteral("至少需要 3 个有效点");
    return QStringLiteral("p = %1x %2 %3")
        .arg(QString::number(m_slope, 'f', 5))
        .arg(m_intercept >= 0 ? QStringLiteral("+") : QStringLiteral("−"))
        .arg(QString::number(qAbs(m_intercept), 'f', 4));
}

QString TaskManager::outlierSummary() const
{
    if (!hasFit())
        return QStringLiteral("数据不足，尚不能执行残差筛查");
    if (m_outlierCount == 0)
        return QStringLiteral("未发现 |标准化残差| > 2.5 的粗大误差嫌疑点");
    return QStringLiteral("发现 %1 个粗大误差嫌疑点；系统不会自动删除，请核对工况后人工处理")
        .arg(m_outlierCount);
}

QString TaskManager::fitQuality() const
{
    if (!hasFit())
        return QStringLiteral("等待数据");
    if (m_r2 >= 0.999)
        return QStringLiteral("线性关系高度显著");
    if (m_r2 >= 0.99)
        return QStringLiteral("线性关系良好");
    if (m_r2 >= 0.95)
        return QStringLiteral("存在可见非线性");
    return QStringLiteral("线性模型可能不适用");
}

bool TaskManager::createTask(const QString &templateId, const QString &customName)
{
    const QString name = customName.trimmed();
    if (name.isEmpty()) {
        emit userMessage(QStringLiteral("请先为本次任务命名"));
        return false;
    }
    if (name.size() > 40) {
        emit userMessage(QStringLiteral("任务名称请控制在 40 个字符以内"));
        return false;
    }
    const QVariantMap definition = definitionFor(templateId);
    if (definition.isEmpty()) {
        emit userMessage(QStringLiteral("该模板暂不可用于创建任务"));
        return false;
    }
    const QString id = QStringLiteral("task.%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_database || !m_database->createTask(
            id, templateId, name, definition.value(QStringLiteral("templateName")).toString(),
            definition.value(QStringLiteral("targetPoints")).toInt(), QStringLiteral("待测数据"),
            0, definition.value(QStringLiteral("accent")).toString(), now)) {
        emit userMessage(QStringLiteral("任务创建失败，请检查本地数据库"));
        return false;
    }
    reloadTaskList();
    selectTask(id);
    emit userMessage(QStringLiteral("“%1”已创建，任务胶囊会持续自动保存").arg(name));
    return true;
}

bool TaskManager::createQuickTask(const QString &customName, const QString &xName,
                                  const QString &xUnit, const QString &captureMode,
                                  int suggestedPoints)
{
    const QString name = customName.trimmed();
    if (name.isEmpty() || name.size() > 40) {
        emit userMessage(name.isEmpty() ? QStringLiteral("请先为快速任务命名")
                                        : QStringLiteral("任务名称请控制在 40 个字符以内"));
        return false;
    }
    const QString mode = captureMode == QStringLiteral("auto-index")
        ? QStringLiteral("auto-index") : QStringLiteral("manual-x");
    QString variableName = xName.trimmed();
    QString variableUnit = xUnit.trimmed();
    if (mode == QStringLiteral("auto-index")) {
        variableName = QStringLiteral("测点序号");
        variableUnit.clear();
    } else if (variableName.isEmpty()) {
        emit userMessage(QStringLiteral("请填写横轴变量名称，例如质量、时间或位移"));
        return false;
    }
    if (variableName.size() > 16 || variableUnit.size() > 10) {
        emit userMessage(QStringLiteral("横轴名称或单位过长，请使用简短名称"));
        return false;
    }

    const QVariantMap definition = definitionFor(QStringLiteral("quick.blank"));
    const QString id = QStringLiteral("task.%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int target = qBound(3, suggestedPoints, 30);
    if (!m_database || !m_database->createTask(
            id, QStringLiteral("quick.blank"), name,
            definition.value(QStringLiteral("templateName")).toString(), target,
            QStringLiteral("待测数据"), 0,
            definition.value(QStringLiteral("accent")).toString(), now)
        || !m_database->saveTaskConfiguration(id, variableName, variableUnit, mode,
                                               QStringLiteral("linear"))) {
        if (m_database)
            m_database->deleteTask(id);
        emit userMessage(QStringLiteral("快速任务创建失败，请检查本地数据库"));
        return false;
    }
    reloadTaskList();
    selectTask(id);
    emit userMessage(mode == QStringLiteral("auto-index")
                         ? QStringLiteral("快速任务已创建，直接采集即可自动编号")
                         : QStringLiteral("快速任务已创建，填写横轴值后即可采集"));
    return true;
}

bool TaskManager::selectTask(const QString &taskId)
{
    auto iterator = std::find_if(m_tasks.cbegin(), m_tasks.cend(), [&](const QVariant &item) {
        return item.toMap().value(QStringLiteral("id")).toString() == taskId;
    });
    if (iterator == m_tasks.cend())
        return false;
    m_currentTask = iterator->toMap();
    m_rows = m_database ? m_database->loadTaskPoints(taskId) : QVariantList{};
    calculateRegression();
    emit currentTaskChanged();
    emit rowsChanged();
    emit resultsChanged();
    return true;
}

bool TaskManager::deleteTask(const QString &taskId)
{
    if (taskId.isEmpty() || !m_database || !m_database->deleteTask(taskId)) {
        emit userMessage(QStringLiteral("任务删除失败"));
        return false;
    }
    const bool deletedCurrent = taskId == currentTaskId();
    reloadTaskList();
    if (deletedCurrent) {
        if (m_tasks.isEmpty()) {
            m_currentTask.clear();
            m_rows.clear();
            calculateRegression();
            emit currentTaskChanged();
            emit rowsChanged();
            emit resultsChanged();
        } else {
            selectTask(m_tasks.first().toMap().value(QStringLiteral("id")).toString());
        }
    }
    emit userMessage(QStringLiteral("任务及其本地数据已删除"));
    return true;
}

bool TaskManager::capturePoint(double xValue, double pressureKPa, bool stable)
{
    if (!hasCurrentTask()) {
        emit userMessage(QStringLiteral("请先创建或选择任务"));
        return false;
    }
    if (m_device && !canCaptureCurrent()) {
        emit userMessage(captureBlockReason());
        return false;
    }
    if (isQuickTask() && captureMode() == QStringLiteral("auto-index"))
        xValue = nextAutoX();
    if (!std::isfinite(xValue) || xValue < -100000.0 || xValue > 100000.0) {
        emit userMessage(QStringLiteral("请输入有效的%1").arg(xVariableName()));
        return false;
    }
    if (currentTemplateId() == QStringLiteral("edu.pressure-mass.linear") && xValue <= 0.0) {
        emit userMessage(QStringLiteral("质量必须大于 0 g"));
        return false;
    }
    if (pressureKPa < -100.0 || pressureKPa > 600.0) {
        emit userMessage(QStringLiteral("压力超出项目工作量程，已拒绝写入任务"));
        return false;
    }
    if (!stable) {
        emit userMessage(QStringLiteral("当前读数仍在收敛，请等待“稳定”后采集"));
        return false;
    }
    if (m_rows.size() >= 100) {
        emit userMessage(QStringLiteral("当前演示任务已达到 100 个测量点上限"));
        return false;
    }
    const QString source = captureTrustLevel() == QStringLiteral("engineering")
        ? QStringLiteral("内置压力通道 · 工程记录（常温标定待验收） · V1 CRC")
        : QStringLiteral("内置压力通道 · 稳定窗口均值");
    appendPoint(xValue, pressureKPa, source,
                QDateTime::currentMSecsSinceEpoch(), true);
    touchCurrent(QStringLiteral("待测数据"), 1);
    emit userMessage(QStringLiteral("第 %1 个测量点已自动保存").arg(m_rows.size()));
    return true;
}

bool TaskManager::deletePoint(qint64 databaseId)
{
    if (!hasCurrentTask() || databaseId <= 0 || !m_database
        || !m_database->deleteTaskPoint(currentTaskId(), databaseId)) {
        emit userMessage(QStringLiteral("数据点删除失败"));
        return false;
    }
    m_rows = m_database->loadTaskPoints(currentTaskId());
    calculateRegression();
    touchCurrent(QStringLiteral("待测数据"), 1);
    emit rowsChanged();
    emit resultsChanged();
    emit userMessage(QStringLiteral("测错的数据点已删除，拟合结果已重新计算"));
    return true;
}

void TaskManager::setCurrentStage(int stage)
{
    if (!hasCurrentTask())
        return;
    stage = qBound(0, stage, 4);
    touchCurrent(currentStatus(), stage);
}

bool TaskManager::finishMeasurement()
{
    if (!canFinishMeasurement()) {
        const int minimum = isQuickTask() ? 3 : targetPoints();
        emit userMessage(QStringLiteral("还需采集 %1 个有效点后才能完成本步骤")
                         .arg(qMax(0, minimum - completedPoints())));
        return false;
    }
    touchCurrent(QStringLiteral("待数据分析"), 2);
    emit userMessage(isQuickTask()
                         ? QStringLiteral("采集已暂存，进入基础线性分析；请结合残差判断模型是否适用")
                         : QStringLiteral("测量阶段完成，已进入模板规定的数据处理流程"));
    return true;
}

bool TaskManager::confirmAnalysis()
{
    if (!hasFit()) {
        emit userMessage(QStringLiteral("有效数据不足，无法确认分析结果"));
        return false;
    }
    touchCurrent(QStringLiteral("待导出结果"), 4);
    emit userMessage(QStringLiteral("分析结果已确认，可以生成任务产物"));
    return true;
}

void TaskManager::restoreExample()
{
    if (!hasCurrentTask())
        return;
    if (m_database)
        m_database->clearTaskPoints(currentTaskId());
    m_rows.clear();
    seedCurrentExample();
    calculateRegression();
    touchCurrent(QStringLiteral("待测数据"), 1);
    emit rowsChanged();
    emit resultsChanged();
    emit userMessage(QStringLiteral("已恢复 4 个演示测量点"));
}

QString TaskManager::exportCsv()
{
    const QString directory = exportRoot();
    QDir().mkpath(directory);
    const QString path = writeCsv(directory);
    emit userMessage(path.isEmpty() ? QStringLiteral("CSV 导出失败")
                                    : QStringLiteral("CSV 已导出到下载目录"));
    return QDir::toNativeSeparators(path);
}

QString TaskManager::exportBundle()
{
    if (!hasFit()) {
        emit userMessage(QStringLiteral("至少需要 3 个有效点才能生成分析数据包"));
        return {};
    }
    const QString root = exportRoot();
    const QString folder = QStringLiteral("PressureOS_%1_%2")
        .arg(safeFileName(currentTaskTitle()),
             QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString directory = QDir(root).filePath(folder);
    if (!QDir().mkpath(directory)) {
        emit userMessage(QStringLiteral("无法创建导出目录"));
        return {};
    }
    const QString csv = writeCsv(directory);
    const QString json = writeAnalysisJson(directory);
    const QString svg = writeChartSvg(directory);
    if (csv.isEmpty() || json.isEmpty() || svg.isEmpty()) {
        emit userMessage(QStringLiteral("任务数据包生成不完整，请检查磁盘空间"));
        return {};
    }
    touchCurrent(QStringLiteral("已完成"), 4);
    emit userMessage(QStringLiteral("已生成数据表、分析摘要与矢量拟合图"));
    return QDir::toNativeSeparators(directory);
}

QVariantMap TaskManager::definitionFor(const QString &templateId) const
{
    QVariantMap definition;
    if (templateId == QStringLiteral("metrology.multi-point")) {
        definition = {{"templateName", QStringLiteral("多点压力标定")},
                      {"description", QStringLiteral("按上、下行程逐点输入标准压力并采集示值，保留原始点、残差和参数版本。")},
                      {"objective", QStringLiteral("评价示值误差、重复性与回程误差，为后续标定模型提供可追溯证据。")},
                      {"xName", QStringLiteral("标准压力")}, {"xUnit", QStringLiteral("kPa")},
                      {"targetPoints", 7}, {"accent", QStringLiteral("#7964F4")},
                      {"preparation", strings({"连接标准压力源并检查密封", "确认设备编号与参数版本", "完成零点漂移校正"})},
                      {"safety", strings({"标准源和被检表均不得超量程", "先升压后降压，切换工况前缓慢泄压"})},
                      {"workflow", makeWorkflow({{"确认标定配置", "核对量程、标准点和上下行程"}, {"逐点稳定采集", "录入标准值，自动读取被检表示值"}, {"误差计算", "计算示值误差、残差和拟合模型"}, {"审核异常点", "检查回程差与粗大误差嫌疑"}, {"冻结并导出", "生成参数候选与审计记录"}})}};
    } else if (templateId == QStringLiteral("quality.zero-drift")) {
        definition = {{"templateName", QStringLiteral("零点漂移观察")},
                      {"description", QStringLiteral("在卸压并连通大气的状态下定时采样，观察零点随时间和温度的变化。")},
                      {"objective", QStringLiteral("量化零点极差、趋势斜率和A类统计特征，判断预热是否充分。")},
                      {"xName", QStringLiteral("经过时间")}, {"xUnit", QStringLiteral("min")},
                      {"targetPoints", 10}, {"accent", QStringLiteral("#15B98A")},
                      {"preparation", strings({"压力端完全卸压并连通大气", "设备至少预热 10 min", "避免触摸传感器和快速温变"})},
                      {"safety", strings({"确认无残余压力后再拆装管路", "零点校正与零点观察是两个不同操作"})},
                      {"workflow", makeWorkflow({{"建立零压条件", "卸压、静置并确认读数稳定"}, {"定时记录", "按模板间隔采集零点与温度"}, {"漂移计算", "计算极差、标准差与趋势斜率"}, {"判断稳定性", "检查预热段和异常扰动"}, {"导出记录", "形成趋势图与稳定性摘要"}})}};
    } else if (templateId == QStringLiteral("engineering.leak")) {
        definition = {{"templateName", QStringLiteral("定压泄漏测试")},
                      {"description", QStringLiteral("达到目标压力并隔离压力源后，按时间记录压降，依据模板阈值给出判定。")},
                      {"objective", QStringLiteral("用可追溯压降曲线评价被测系统的保压能力。")},
                      {"xName", QStringLiteral("保压时间")}, {"xUnit", QStringLiteral("min")},
                      {"targetPoints", 8}, {"accent", QStringLiteral("#F29A43")},
                      {"preparation", strings({"检查接头与软管额定压力", "设定目标压力和保压时长", "确认泄压通道可用"})},
                      {"safety", strings({"加压期间人员远离接头正前方", "出现快速压降或异响应立即停止"})},
                      {"workflow", makeWorkflow({{"设置测试条件", "确认目标压力、时长和判据"}, {"升压并隔离", "稳定后切断压力源开始计时"}, {"记录压降", "按时间点自动或人工采点"}, {"计算泄漏率", "拟合压力—时间关系并检查异常"}, {"输出判定", "导出曲线、结果与环境信息"}})}};
    } else if (templateId == QStringLiteral("edu.pressure-mass.linear")) {
        definition = {{"templateName", QStringLiteral("压力—质量关系实验")},
                      {"description", QStringLiteral("逐点填写外部质量，由内置压力通道等待稳定后采集；系统自动完成线性拟合与误差分析。")},
                      {"objective", QStringLiteral("验证质量与压力之间的函数关系，并理解斜率、截距、残差和测量不确定度。")},
                      {"xName", QStringLiteral("质量")}, {"xUnit", QStringLiteral("g")},
                      {"targetPoints", 6}, {"accent", QStringLiteral("#1683FF")},
                      {"preparation", strings({"检查压力接口与实验装置连接", "确认秤码或质量值可靠", "在无载荷状态完成零点漂移校正"})},
                      {"safety", strings({"不得超过 −100～600 kPa 工作量程", "加卸载应缓慢进行，读数稳定后再采集"})},
                      {"workflow", makeWorkflow({{"了解任务", "阅读目标、变量和安全要求"}, {"逐点测量", "填写质量并采集稳定压力"}, {"数据处理", "按模板执行普通最小二乘拟合"}, {"结果分析", "审查关系式、残差、不确定度和异常点"}, {"导出归档", "生成 CSV、分析 JSON 与矢量图"}})}};
    } else if (templateId == QStringLiteral("quick.blank")) {
        definition = {{"templateName", QStringLiteral("快速空白任务")},
                      {"description", QStringLiteral("无需预先制作模板，选择自动编号或自定义横轴后即可开始采集；数据会逐点自动保存。")},
                      {"objective", QStringLiteral("先把现场数据可靠记录下来，再用基础表格、线性拟合、残差图和导出功能快速形成结果。")},
                      {"xName", QStringLiteral("测点序号")}, {"xUnit", QString()},
                      {"targetPoints", 5}, {"accent", QStringLiteral("#20A7D8")},
                      {"preparation", strings({"确认压力接口连接可靠", "确认当前量程满足现场工况", "需要自变量时提前确定名称和单位"})},
                      {"safety", strings({"空白任务不会绕过量程和稳定性检查", "线性拟合只是基础工具，必须结合 R² 与残差判断是否适用"})},
                      {"workflow", makeWorkflow({{"选择记录方式", "自动编号可直接采集，自定义横轴需逐点填写"}, {"快速采点", "稳定后点击采集，误测点可随时删除"}, {"形成数据表", "达到 3 点即可提前进入基础分析"}, {"判断模型", "查看拟合、残差和异常点提示"}, {"一键导出", "生成 CSV、分析 JSON 与 SVG 图"}})}};
    }
    return definition;
}

void TaskManager::initializeDemoTasks()
{
    if (!m_database)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QVariantMap main = definitionFor(QStringLiteral("edu.pressure-mass.linear"));
    m_database->createTask(QStringLiteral("demo.pressure-mass.001"),
                           QStringLiteral("edu.pressure-mass.linear"),
                           QStringLiteral("压力—质量关系实验 · 第一次"),
                           main.value(QStringLiteral("templateName")).toString(), 6,
                           QStringLiteral("待测数据"), 1,
                           main.value(QStringLiteral("accent")).toString(), now);

    const QVariantMap calibration = definitionFor(QStringLiteral("metrology.multi-point"));
    m_database->createTask(QStringLiteral("task.calibration.004"),
                           QStringLiteral("metrology.multi-point"),
                           QStringLiteral("PX-01 首轮多点标定"),
                           calibration.value(QStringLiteral("templateName")).toString(), 7,
                           QStringLiteral("待测数据"), 0,
                           calibration.value(QStringLiteral("accent")).toString(), now - 3 * 3600 * 1000);

    const QVariantMap drift = definitionFor(QStringLiteral("quality.zero-drift"));
    m_database->createTask(QStringLiteral("task.zero-drift.018"),
                           QStringLiteral("quality.zero-drift"),
                           QStringLiteral("预热后零点漂移观察"),
                           drift.value(QStringLiteral("templateName")).toString(), 6,
                           QStringLiteral("已完成"), 4,
                           drift.value(QStringLiteral("accent")).toString(), now - 24 * 3600 * 1000);
    for (int i = 0; i < 6; ++i)
        m_database->saveTaskPoint(QStringLiteral("task.zero-drift.018"), i * 5.0,
                                  0.03 + qSin(i * 0.8) * 0.025,
                                  QStringLiteral("内置压力通道 · 定时采样"),
                                  now - (24 * 3600 - i * 300) * 1000LL);
    m_database->updateTaskState(QStringLiteral("task.zero-drift.018"), QStringLiteral("已完成"), 4,
                                now - 23 * 3600 * 1000);

    reloadTaskList();
    selectTask(QStringLiteral("demo.pressure-mass.001"));
    if (m_rows.isEmpty()) {
        seedCurrentExample();
        touchCurrent(QStringLiteral("待测数据"), 1);
        emit rowsChanged();
        emit resultsChanged();
    }
}

void TaskManager::reloadTaskList()
{
    const QString selectedId = currentTaskId();
    m_tasks = m_database ? m_database->loadTasks() : QVariantList{};
    for (QVariant &item : m_tasks) {
        QVariantMap task = item.toMap();
        enrichTask(task);
        item = task;
        if (!selectedId.isEmpty() && task.value(QStringLiteral("id")).toString() == selectedId)
            m_currentTask = task;
    }
    emit taskListChanged();
    if (!selectedId.isEmpty())
        emit currentTaskChanged();
}

void TaskManager::enrichTask(QVariantMap &task) const
{
    const int completed = task.value(QStringLiteral("completedPoints")).toInt();
    const int target = qMax(1, task.value(QStringLiteral("targetPoints")).toInt());
    task.insert(QStringLiteral("progress"), qMin(1.0, completed / static_cast<double>(target)));
    const bool quick = task.value(QStringLiteral("templateId")).toString() == QStringLiteral("quick.blank");
    task.insert(QStringLiteral("detail"), quick
                    ? QStringLiteral("%1 点 · 建议 %2 点").arg(completed).arg(target)
                    : QStringLiteral("%1 / %2 个数据点").arg(completed).arg(target));
    task.insert(QStringLiteral("updated"), relativeTime(task.value(QStringLiteral("updatedAt")).toLongLong()));
    const QString status = task.value(QStringLiteral("status")).toString();
    task.insert(QStringLiteral("statusColor"), status == QStringLiteral("已完成") ? QStringLiteral("#19B987")
        : status == QStringLiteral("待导出结果") ? QStringLiteral("#7964F4")
        : status == QStringLiteral("待数据分析") ? QStringLiteral("#F29A43") : QStringLiteral("#1683FF"));
    const QString templateId = task.value(QStringLiteral("templateId")).toString();
    task.insert(QStringLiteral("icon"), templateId == QStringLiteral("quick.blank") ? QStringLiteral("edit")
        : templateId.contains(QStringLiteral("calibration")) ? QStringLiteral("target")
        : templateId.contains(QStringLiteral("drift")) ? QStringLiteral("trend")
        : templateId.contains(QStringLiteral("leak")) ? QStringLiteral("shield") : QStringLiteral("flask"));
}

void TaskManager::touchCurrent(const QString &status, int stage)
{
    if (!hasCurrentTask() || !m_database)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_database->updateTaskState(currentTaskId(), status, stage, now))
        return;
    m_currentTask.insert(QStringLiteral("status"), status);
    m_currentTask.insert(QStringLiteral("currentStage"), stage);
    m_currentTask.insert(QStringLiteral("updatedAt"), now);
    reloadTaskList();
    emit currentTaskChanged();
}

void TaskManager::seedCurrentExample()
{
    const QList<QPair<double, double>> seed {
        {100.0, 94.32}, {200.0, 176.41}, {300.0, 258.37}, {400.0, 340.51}
    };
    qint64 time = QDateTime::currentMSecsSinceEpoch() - 240000;
    for (const auto &point : seed) {
        appendPoint(point.first, point.second, QStringLiteral("内置压力通道 · 稳定窗口均值"),
                    time, true);
        time += 45000;
    }
}

void TaskManager::appendPoint(double xValue, double pressureKPa, const QString &source,
                              qint64 timestampMs, bool persist)
{
    if (persist && m_database) {
        m_database->saveTaskPoint(currentTaskId(), xValue, pressureKPa, source, timestampMs);
        m_rows = m_database->loadTaskPoints(currentTaskId());
    } else {
        QVariantMap row;
        row.insert(QStringLiteral("databaseId"), -1);
        row.insert(QStringLiteral("index"), m_rows.size() + 1);
        row.insert(QStringLiteral("mass"), xValue);
        row.insert(QStringLiteral("pressure"), pressureKPa);
        row.insert(QStringLiteral("source"), source);
        row.insert(QStringLiteral("timestamp"), timestampMs);
        row.insert(QStringLiteral("status"), QStringLiteral("已采集"));
        m_rows.push_back(row);
    }
    calculateRegression();
    emit rowsChanged();
    emit resultsChanged();
}

void TaskManager::calculateRegression()
{
    const int n = m_rows.size();
    m_slope = m_intercept = m_r = m_r2 = m_residualStd = 0.0;
    m_maxAbsResidual = m_typeA = m_combined = m_expanded = 0.0;
    m_typeB = 0.1 / qSqrt(12.0);
    m_outlierCount = 0;
    if (n < 2)
        return;

    double sumX = 0.0;
    double sumY = 0.0;
    for (const QVariant &item : std::as_const(m_rows)) {
        const QVariantMap row = item.toMap();
        sumX += row.value(QStringLiteral("mass")).toDouble();
        sumY += row.value(QStringLiteral("pressure")).toDouble();
    }
    const double meanX = sumX / n;
    const double meanY = sumY / n;
    double sxx = 0.0;
    double syy = 0.0;
    double sxy = 0.0;
    for (const QVariant &item : std::as_const(m_rows)) {
        const QVariantMap row = item.toMap();
        const double dx = row.value(QStringLiteral("mass")).toDouble() - meanX;
        const double dy = row.value(QStringLiteral("pressure")).toDouble() - meanY;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }
    m_slope = qFuzzyIsNull(sxx) ? 0.0 : sxy / sxx;
    m_intercept = meanY - m_slope * meanX;
    m_r = (qFuzzyIsNull(sxx) || qFuzzyIsNull(syy)) ? 0.0 : sxy / qSqrt(sxx * syy);
    m_r2 = m_r * m_r;

    double residualSum = 0.0;
    for (const QVariant &item : std::as_const(m_rows)) {
        const QVariantMap row = item.toMap();
        const double predicted = m_slope * row.value(QStringLiteral("mass")).toDouble() + m_intercept;
        const double residual = row.value(QStringLiteral("pressure")).toDouble() - predicted;
        residualSum += residual * residual;
    }
    m_residualStd = n > 2 ? qSqrt(residualSum / (n - 2)) : 0.0;
    m_typeA = n > 0 ? m_residualStd / qSqrt(static_cast<double>(n)) : 0.0;
    m_combined = qSqrt(m_typeA * m_typeA + m_typeB * m_typeB);
    m_expanded = 2.0 * m_combined;

    for (int i = 0; i < m_rows.size(); ++i) {
        QVariantMap row = m_rows.at(i).toMap();
        const double predicted = m_slope * row.value(QStringLiteral("mass")).toDouble() + m_intercept;
        const double residual = row.value(QStringLiteral("pressure")).toDouble() - predicted;
        const double standardized = qFuzzyIsNull(m_residualStd) ? 0.0 : residual / m_residualStd;
        const bool suspect = n >= 4 && qAbs(standardized) > 2.5;
        row.insert(QStringLiteral("predicted"), predicted);
        row.insert(QStringLiteral("residual"), residual);
        row.insert(QStringLiteral("standardizedResidual"), standardized);
        row.insert(QStringLiteral("suspect"), suspect);
        row.insert(QStringLiteral("status"), suspect ? QStringLiteral("待核对") : QStringLiteral("有效"));
        m_rows[i] = row;
        m_maxAbsResidual = qMax(m_maxAbsResidual, qAbs(residual));
        if (suspect)
            ++m_outlierCount;
    }
}

QString TaskManager::writeCsv(const QString &directory) const
{
    const QString path = QDir(directory).filePath(safeFileName(currentTaskTitle()) + QStringLiteral("_数据.csv"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    file.write("\xEF\xBB\xBF");
    QTextStream out(&file);
    out << QStringLiteral("序号,%1(%2),压力(kPa),预测值(kPa),残差(kPa),状态,数据来源,采集时间\n")
               .arg(xVariableName(), xVariableUnit());
    for (const QVariant &item : m_rows) {
        const QVariantMap row = item.toMap();
        out << row.value(QStringLiteral("index")).toInt() << ','
            << QString::number(row.value(QStringLiteral("mass")).toDouble(), 'f', 4) << ','
            << QString::number(row.value(QStringLiteral("pressure")).toDouble(), 'f', 4) << ','
            << QString::number(row.value(QStringLiteral("predicted")).toDouble(), 'f', 4) << ','
            << QString::number(row.value(QStringLiteral("residual")).toDouble(), 'f', 4) << ','
            << row.value(QStringLiteral("status")).toString() << ','
            << '"' << row.value(QStringLiteral("source")).toString() << '"' << ','
            << QDateTime::fromMSecsSinceEpoch(row.value(QStringLiteral("timestamp")).toLongLong()).toString(Qt::ISODate)
            << '\n';
    }
    return file.commit() ? path : QString{};
}

QString TaskManager::writeAnalysisJson(const QString &directory) const
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("PressureOS.Analysis/1.0"));
    root.insert(QStringLiteral("taskId"), currentTaskId());
    root.insert(QStringLiteral("taskName"), currentTaskTitle());
    root.insert(QStringLiteral("templateId"), currentTemplateId());
    root.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("dataTrustLevel"),
                containsEngineeringData() ? QStringLiteral("engineering")
                                          : QStringLiteral("formal"));
    root.insert(QStringLiteral("trustNotice"), containsEngineeringData()
                    ? QStringLiteral("包含常温标定验收前工程记录；可用于零点漂移或标定分析，不可直接作为正式检定结论。")
                    : QStringLiteral("未发现工程数据标记；正式使用仍以设备参数版本和审核记录为准。"));
    QJsonObject fit;
    fit.insert(QStringLiteral("method"), QStringLiteral("ordinaryLeastSquares"));
    fit.insert(QStringLiteral("equation"), equation());
    fit.insert(QStringLiteral("slope"), m_slope);
    fit.insert(QStringLiteral("intercept"), m_intercept);
    fit.insert(QStringLiteral("pearsonR"), m_r);
    fit.insert(QStringLiteral("rSquared"), m_r2);
    fit.insert(QStringLiteral("residualStdKPa"), m_residualStd);
    fit.insert(QStringLiteral("outlierRule"), QStringLiteral("abs(standardizedResidual) > 2.5; manual review required"));
    fit.insert(QStringLiteral("outlierCount"), m_outlierCount);
    root.insert(QStringLiteral("fit"), fit);
    QJsonObject uncertainty;
    uncertainty.insert(QStringLiteral("typeA_kPa"), m_typeA);
    uncertainty.insert(QStringLiteral("typeB_resolution_kPa"), m_typeB);
    uncertainty.insert(QStringLiteral("combined_kPa"), m_combined);
    uncertainty.insert(QStringLiteral("expanded_k2_kPa"), m_expanded);
    uncertainty.insert(QStringLiteral("note"), QStringLiteral("演示口径；正式报告需由模板声明全部B类分量和自由度"));
    root.insert(QStringLiteral("uncertainty"), uncertainty);
    root.insert(QStringLiteral("points"), QJsonArray::fromVariantList(m_rows));

    const QString path = QDir(directory).filePath(safeFileName(currentTaskTitle()) + QStringLiteral("_分析摘要.json"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit() ? path : QString{};
}

QString TaskManager::writeChartSvg(const QString &directory) const
{
    if (m_rows.isEmpty())
        return {};
    double minX = m_rows.first().toMap().value(QStringLiteral("mass")).toDouble();
    double maxX = minX;
    double minY = m_rows.first().toMap().value(QStringLiteral("pressure")).toDouble();
    double maxY = minY;
    for (const QVariant &item : m_rows) {
        const QVariantMap row = item.toMap();
        minX = qMin(minX, row.value(QStringLiteral("mass")).toDouble());
        maxX = qMax(maxX, row.value(QStringLiteral("mass")).toDouble());
        minY = qMin(minY, row.value(QStringLiteral("pressure")).toDouble());
        maxY = qMax(maxY, row.value(QStringLiteral("pressure")).toDouble());
    }
    if (qFuzzyCompare(minX, maxX)) maxX = minX + 1.0;
    if (qFuzzyCompare(minY, maxY)) maxY = minY + 1.0;
    const double xPad = (maxX - minX) * 0.08;
    const double yPad = (maxY - minY) * 0.12;
    minX -= xPad; maxX += xPad; minY -= yPad; maxY += yPad;
    const auto sx = [&](double x) { return 110.0 + (x - minX) / (maxX - minX) * 1010.0; };
    const auto sy = [&](double y) { return 600.0 - (y - minY) / (maxY - minY) * 470.0; };

    QString svg;
    QTextStream out(&svg);
    out << QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"700\" viewBox=\"0 0 1200 700\">\n")
        << QStringLiteral("<rect width=\"1200\" height=\"700\" rx=\"28\" fill=\"#F7FBFF\"/>\n")
        << QStringLiteral("<text x=\"70\" y=\"62\" font-family=\"Microsoft YaHei, sans-serif\" font-size=\"28\" font-weight=\"700\" fill=\"#07264B\">")
        << currentTaskTitle().toHtmlEscaped() << QStringLiteral("</text>\n")
        << QStringLiteral("<text x=\"70\" y=\"94\" font-family=\"Microsoft YaHei, sans-serif\" font-size=\"16\" fill=\"#657C96\">")
        << equation().toHtmlEscaped() << QStringLiteral("　R² = ") << QString::number(m_r2, 'f', 6)
        << QStringLiteral("</text>\n");
    if (containsEngineeringData()) {
        out << QStringLiteral("<rect x=\"900\" y=\"38\" width=\"230\" height=\"42\" rx=\"14\" fill=\"#FFF0DC\"/>\n")
            << QStringLiteral("<text x=\"1015\" y=\"65\" text-anchor=\"middle\" font-family=\"Microsoft YaHei, sans-serif\" font-size=\"16\" font-weight=\"700\" fill=\"#B96A16\">工程数据 · 待标定验收</text>\n");
    }
    for (int i = 0; i <= 5; ++i) {
        const double y = 130 + i * 94;
        out << QStringLiteral("<line x1=\"110\" y1=\"%1\" x2=\"1120\" y2=\"%1\" stroke=\"#DCEAF5\"/>\n").arg(y);
    }
    out << QStringLiteral("<line x1=\"110\" y1=\"600\" x2=\"1120\" y2=\"600\" stroke=\"#7891AA\" stroke-width=\"2\"/>\n")
        << QStringLiteral("<line x1=\"110\" y1=\"130\" x2=\"110\" y2=\"600\" stroke=\"#7891AA\" stroke-width=\"2\"/>\n")
        << QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\" stroke=\"#1683FF\" stroke-width=\"4\" stroke-linecap=\"round\"/>\n")
               .arg(sx(minX)).arg(sy(m_slope * minX + m_intercept)).arg(sx(maxX)).arg(sy(m_slope * maxX + m_intercept));
    for (const QVariant &item : m_rows) {
        const QVariantMap row = item.toMap();
        const QString color = row.value(QStringLiteral("suspect")).toBool() ? QStringLiteral("#E95A6F") : QStringLiteral("#19B987");
        out << QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"8\" fill=\"%3\" stroke=\"white\" stroke-width=\"4\"/>\n")
               .arg(sx(row.value(QStringLiteral("mass")).toDouble()))
               .arg(sy(row.value(QStringLiteral("pressure")).toDouble()))
               .arg(color);
    }
    out << QStringLiteral("<text x=\"1080\" y=\"654\" text-anchor=\"end\" font-family=\"Microsoft YaHei, sans-serif\" font-size=\"16\" fill=\"#657C96\">")
        << xVariableName().toHtmlEscaped() << QStringLiteral(" / ") << xVariableUnit().toHtmlEscaped() << QStringLiteral("</text>\n")
        << QStringLiteral("<text x=\"34\" y=\"160\" font-family=\"Microsoft YaHei, sans-serif\" font-size=\"16\" fill=\"#657C96\">压力 / kPa</text>\n")
        << QStringLiteral("</svg>\n");

    const QString path = QDir(directory).filePath(safeFileName(currentTaskTitle()) + QStringLiteral("_拟合图.svg"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    file.write(svg.toUtf8());
    return file.commit() ? path : QString{};
}

QString TaskManager::relativeTime(qint64 timestampMs)
{
    const qint64 seconds = qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - timestampMs) / 1000);
    if (seconds < 20) return QStringLiteral("刚刚");
    if (seconds < 60) return QStringLiteral("%1 秒前").arg(seconds);
    if (seconds < 3600) return QStringLiteral("%1 分钟前").arg(seconds / 60);
    if (seconds < 24 * 3600) return QStringLiteral("%1 小时前").arg(seconds / 3600);
    if (seconds < 7 * 24 * 3600) return QStringLiteral("%1 天前").arg(seconds / (24 * 3600));
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("M月d日"));
}

QString TaskManager::safeFileName(const QString &name)
{
    QString result = name.trimmed();
    result.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    return result.left(60).isEmpty() ? QStringLiteral("未命名任务") : result.left(60);
}
