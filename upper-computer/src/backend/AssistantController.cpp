#include "AssistantController.h"

#include "AssistantActionDispatcher.h"
#include "AssistantContextService.h"
#include "AssistantKnowledgeRepository.h"
#include "DatabaseService.h"

#include <QDateTime>
#include <QTimer>

namespace {
QString pageLabel(const QString &page)
{
    if (page == QStringLiteral("measure"))
        return QStringLiteral("实时测量");
    if (page == QStringLiteral("tasks"))
        return QStringLiteral("任务中心");
    if (page == QStringLiteral("runner"))
        return QStringLiteral("任务运行");
    if (page == QStringLiteral("templates"))
        return QStringLiteral("模板库");
    if (page == QStringLiteral("data"))
        return QStringLiteral("数据工作室");
    if (page == QStringLiteral("device"))
        return QStringLiteral("设备与连接");
    return QStringLiteral("桌面");
}

QVariantList stringList(std::initializer_list<QString> values)
{
    QVariantList result;
    for (const QString &value : values) {
        if (!value.isEmpty())
            result.push_back(value);
    }
    return result;
}
}

AssistantController::AssistantController(AssistantContextService *contextService,
                                         AssistantKnowledgeRepository *knowledge,
                                         AssistantActionDispatcher *actions,
                                         DatabaseService *database,
                                         QObject *parent)
    : QObject(parent),
      m_contextService(contextService),
      m_knowledge(knowledge),
      m_actions(actions),
      m_database(database)
{
    m_nudgeTimer.setSingleShot(true);
    m_nudgeTimer.setInterval(8500);
    connect(&m_nudgeTimer, &QTimer::timeout, this, [this] { setNudgeVisible(false); });
    if (m_contextService) {
        connect(m_contextService, &AssistantContextService::contextChanged,
                this, &AssistantController::refresh);
    }
    refresh();
}

QString AssistantController::statusLevel() const
{
    return m_recommendation.value(QStringLiteral("level"), QStringLiteral("info")).toString();
}

QString AssistantController::contextLabel() const
{
    const QString page = m_context.value(QStringLiteral("page")).toString();
    if (page == QStringLiteral("runner") && m_context.value(QStringLiteral("hasTask")).toBool()) {
        return QStringLiteral("第 %1/5 步 · %2 · %3")
            .arg(QString::number(m_context.value(QStringLiteral("stageNumber")).toInt()),
                 m_context.value(QStringLiteral("stageTitle")).toString(),
                 m_context.value(QStringLiteral("taskTitle")).toString());
    }
    return pageLabel(page) + QStringLiteral(" · 上下文已同步");
}

bool AssistantController::hasAttention() const
{
    return m_recommendation.value(QStringLiteral("priority"), 3).toInt() <= 1;
}

bool AssistantController::knowledgeReady() const
{
    return m_knowledge && m_knowledge->ready();
}

void AssistantController::ask(const QString &articleId)
{
    QVariantMap article = m_knowledge ? m_knowledge->article(articleId) : QVariantMap{};
    if (article.isEmpty() && m_knowledge)
        article = m_knowledge->article(QStringLiteral("help_scope"));
    if (article.isEmpty())
        return;
    m_currentAnswer = enrichArticle(article);
    emit answerChanged();
    appendHistory(QStringLiteral("question"), articleId,
                  m_currentAnswer.value(QStringLiteral("title")).toString(),
                  m_currentAnswer.value(QStringLiteral("body")).toString(),
                  m_currentAnswer.value(QStringLiteral("actionId")).toString());
}

void AssistantController::askText(const QString &question)
{
    const QString trimmed = question.trimmed();
    if (trimmed.isEmpty())
        return;
    QVariantMap article = m_knowledge ? m_knowledge->search(trimmed) : QVariantMap{};
    if (article.isEmpty())
        return;
    m_currentAnswer = enrichArticle(article, trimmed);
    emit answerChanged();
    appendHistory(QStringLiteral("question"),
                  article.value(QStringLiteral("id")).toString(),
                  m_currentAnswer.value(QStringLiteral("title")).toString(),
                  m_currentAnswer.value(QStringLiteral("body")).toString(),
                  m_currentAnswer.value(QStringLiteral("actionId")).toString());
}

void AssistantController::explainRecommendation()
{
    const QString helpId = m_recommendation.value(QStringLiteral("helpId")).toString();
    ask(helpId.isEmpty() ? QStringLiteral("task_next_step") : helpId);
}

QVariantMap AssistantController::executeAction(const QString &actionId)
{
    if (!m_actions || actionId.isEmpty())
        return {};
    const QVariantMap result = m_actions->dispatch(actionId);
    appendHistory(QStringLiteral("action"), actionId,
                  result.value(QStringLiteral("success")).toBool()
                      ? QStringLiteral("已执行助手操作") : QStringLiteral("助手操作未执行"),
                  result.value(QStringLiteral("message")).toString(), actionId);
    QTimer::singleShot(0, this, &AssistantController::refresh);
    return result;
}

void AssistantController::clearAnswer()
{
    if (m_currentAnswer.isEmpty())
        return;
    m_currentAnswer.clear();
    emit answerChanged();
}

void AssistantController::dismissNudge()
{
    if (statusLevel() == QStringLiteral("critical"))
        return;
    setNudgeVisible(false);
}

void AssistantController::refresh()
{
    if (!m_contextService)
        return;
    const QVariantMap newContext = m_contextService->snapshot();
    const QVariantMap newRecommendation = m_ruleEngine.evaluate(newContext);
    const QVariantList newQuestions = m_ruleEngine.quickQuestions(newContext);
    const QString oldRecommendationId = m_recommendation.value(QStringLiteral("id")).toString();
    const QString newRecommendationId = newRecommendation.value(QStringLiteral("id")).toString();
    const QString oldScope = m_historyScope;
    const QString oldContextLabel = contextLabel();

    const bool recommendationContentChanged = newRecommendation != m_recommendation
        || newQuestions != m_quickQuestions;
    m_context = newContext;
    m_recommendation = newRecommendation;
    m_quickQuestions = newQuestions;
    m_historyScope = historyScope();
    if (recommendationContentChanged || oldContextLabel != contextLabel())
        emit recommendationChanged();

    if (oldScope != m_historyScope)
        reloadHistory();

    if (!newRecommendationId.isEmpty() && newRecommendationId != oldRecommendationId
        && (!oldRecommendationId.isEmpty()
            || newRecommendation.value(QStringLiteral("level")).toString()
                == QStringLiteral("critical"))
        && newRecommendation.value(QStringLiteral("shouldNudge")).toBool()) {
        setNudgeVisible(true);
        if (newRecommendation.value(QStringLiteral("level")).toString()
            == QStringLiteral("critical")) {
            m_nudgeTimer.stop();
        } else {
            m_nudgeTimer.start();
        }
    }
}

QVariantMap AssistantController::enrichArticle(QVariantMap article,
                                               const QString &question) const
{
    if (!question.isEmpty())
        article.insert(QStringLiteral("question"), question);
    const QString id = article.value(QStringLiteral("id")).toString();
    const bool hasTask = m_context.value(QStringLiteral("hasTask")).toBool();
    const QVariantMap templateGuide = m_knowledge
        ? m_knowledge->templateGuide(m_context.value(QStringLiteral("templateId")).toString())
        : QVariantMap{};

    if (id == QStringLiteral("task_next_step")) {
        article.insert(QStringLiteral("title"), m_recommendation.value(QStringLiteral("title")));
        article.insert(QStringLiteral("body"), m_recommendation.value(QStringLiteral("summary")));
        article.insert(QStringLiteral("bullets"),
                       stringList({m_recommendation.value(QStringLiteral("reason")).toString(),
                                   m_context.value(QStringLiteral("stageDetail")).toString()}));
        article.insert(QStringLiteral("actionId"),
                       m_recommendation.value(QStringLiteral("actionId")));
        article.insert(QStringLiteral("actionText"),
                       m_recommendation.value(QStringLiteral("actionText")));
    } else if (id == QStringLiteral("task_overview") && hasTask) {
        article.insert(QStringLiteral("body"),
                       QStringLiteral("当前任务“%1”使用“%2”模板，状态为“%3”，现在位于第 %4/5 步。")
                           .arg(m_context.value(QStringLiteral("taskTitle")).toString(),
                                m_context.value(QStringLiteral("templateName")).toString(),
                                m_context.value(QStringLiteral("taskStatus")).toString(),
                                QString::number(m_context.value(QStringLiteral("stageNumber")).toInt())));
        article.insert(QStringLiteral("actionId"), QStringLiteral("open_current_task"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("打开当前任务"));
    } else if (id == QStringLiteral("task_preparation") && hasTask) {
        QVariantList bullets = m_context.value(QStringLiteral("preparationItems")).toList();
        const QString stageTip = templateGuide.value(QStringLiteral("preparationTip")).toString();
        if (!stageTip.isEmpty())
            bullets.push_back(stageTip);
        article.insert(QStringLiteral("bullets"), bullets);
        article.insert(QStringLiteral("body"),
                       QStringLiteral("“%1”开始前应逐项确认。准备项不是形式步骤，它们直接影响零点、稳定性和结果可追溯性。")
                           .arg(m_context.value(QStringLiteral("templateName")).toString()));
        article.insert(QStringLiteral("actionId"), QStringLiteral("open_current_task"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("返回任务说明"));
    } else if (id == QStringLiteral("reading_trust")) {
        const bool hardware = m_context.value(QStringLiteral("hardwareMode")).toBool();
        const bool fresh = !hardware || m_context.value(QStringLiteral("dataFresh")).toBool();
        const bool integrity = !hardware
            || m_context.value(QStringLiteral("protocolIntegrityAvailable")).toBool();
        const bool trusted = !hardware
            || m_context.value(QStringLiteral("valueTrustedForSafety")).toBool();
        const bool engineering = m_context.value(QStringLiteral("captureTrustLevel")).toString()
            == QStringLiteral("engineering");
        const bool stable = m_context.value(QStringLiteral("stable")).toBool();
        article.insert(QStringLiteral("body"),
                       fresh && integrity && trusted && stable
                           ? QStringLiteral("当前数据时效、协议完整性、常温标定可信状态和短时稳定性均满足软件采集条件。")
                           : (fresh && integrity && engineering && stable
                              ? QStringLiteral("当前数据可写入零点观察或多点标定任务，并会标记为工程记录；它不等同于正式检定结论。")
                              : QStringLiteral("当前还不能把读数直接视为正式有效数据，请先处理未满足项。")));
        article.insert(QStringLiteral("bullets"), stringList({
            fresh ? QStringLiteral("数据时效：有效") : QStringLiteral("数据时效：中断或尚未收到有效帧"),
            integrity ? QStringLiteral("协议完整性：V1 CRC 与帧序号已校验")
                      : QStringLiteral("协议完整性：尚未锁定 V1，不可证明传输无损"),
            trusted ? QStringLiteral("常温零点/标定可信状态：可用于正式任务")
                    : (engineering
                       ? QStringLiteral("常温零点/标定可信状态：允许工程任务采集并保留标记")
                       : QStringLiteral("常温零点/标定可信状态：正式任务暂不可采集")),
            stable ? QStringLiteral("短时稳定性：已稳定，峰峰值 %1 kPa")
                         .arg(QString::number(m_context.value(QStringLiteral("stabilityP2P")).toDouble(),
                                             'f', 3))
                   : QStringLiteral("短时稳定性：仍在变化，峰峰值 %1 kPa")
                         .arg(QString::number(m_context.value(QStringLiteral("stabilityP2P")).toDouble(),
                                             'f', 3)),
            QStringLiteral("量程使用率：%1%")
                .arg(QString::number(m_context.value(QStringLiteral("utilizationPercent")).toDouble(),
                                     'f', 1))}));
        article.insert(QStringLiteral("actionId"),
                       hasTask ? QStringLiteral("open_current_task")
                               : QStringLiteral("open_measurement"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("查看实时状态"));
    } else if (id == QStringLiteral("measurement_stability")) {
        article.insert(QStringLiteral("body"),
                       m_context.value(QStringLiteral("stable")).toBool()
                           ? QStringLiteral("当前稳定标识已经满足，可以结合量程和标定状态决定是否采集。")
                           : QStringLiteral("当前峰峰值为 %1 kPa，保持工况不变并等待稳定窗口重新判定。")
                                 .arg(QString::number(
                                     m_context.value(QStringLiteral("stabilityP2P")).toDouble(),
                                     'f', 3)));
    } else if (id == QStringLiteral("capture_and_autosave") && hasTask) {
        article.insert(QStringLiteral("body"),
                       QStringLiteral("当前任务已保存 %1 个测点。每次采点、删点和阶段切换都会写入 SQLite；切换任务或退出页面不会清空数据。")
                           .arg(m_context.value(QStringLiteral("completedPoints")).toInt()));
        article.insert(QStringLiteral("actionId"), QStringLiteral("open_current_task"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("返回数据记录"));
    } else if (id == QStringLiteral("fit_explanation") && hasTask) {
        article.insert(QStringLiteral("body"),
                       m_context.value(QStringLiteral("hasFit")).toBool()
                           ? QStringLiteral("当前普通最小二乘结果为 %1，R² = %2，残差标准差 = %3 kPa。")
                                 .arg(m_context.value(QStringLiteral("equation")).toString(),
                                      QString::number(m_context.value(QStringLiteral("rSquared")).toDouble(),
                                                      'f', 6),
                                      QString::number(m_context.value(QStringLiteral("residualStd")).toDouble(),
                                                      'f', 4))
                           : QStringLiteral("当前有效点不足，至少需要三个不同横轴位置的点建立基础线性拟合。"));
        const QString modelTip = templateGuide.value(QStringLiteral("analysisTip")).toString();
        if (!modelTip.isEmpty()) {
            QVariantList bullets = article.value(QStringLiteral("bullets")).toList();
            bullets.push_front(modelTip);
            article.insert(QStringLiteral("bullets"), bullets);
        }
    } else if (id == QStringLiteral("residual_outlier") && hasTask) {
        article.insert(QStringLiteral("body"), m_context.value(QStringLiteral("outlierSummary")).toString());
        article.insert(QStringLiteral("bullets"), stringList({
            QStringLiteral("最大绝对残差：%1 kPa")
                .arg(QString::number(m_context.value(QStringLiteral("maxAbsResidual")).toDouble(), 'f', 4)),
            QStringLiteral("残差标准差：%1 kPa")
                .arg(QString::number(m_context.value(QStringLiteral("residualStd")).toDouble(), 'f', 4)),
            QStringLiteral("异常点数量：%1；系统不会自动删除")
                .arg(m_context.value(QStringLiteral("outlierCount")).toInt())}));
        article.insert(QStringLiteral("actionId"), QStringLiteral("task_review_points"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("核对原始数据"));
    } else if (id == QStringLiteral("uncertainty") && hasTask) {
        article.insert(QStringLiteral("body"),
                       QStringLiteral("当前 A 类 = %1 kPa，B 类 = %2 kPa，合成标准不确定度 = %3 kPa，扩展不确定度 = %4 kPa。")
                           .arg(QString::number(m_context.value(QStringLiteral("typeAUncertainty")).toDouble(), 'f', 4),
                                QString::number(m_context.value(QStringLiteral("typeBUncertainty")).toDouble(), 'f', 4),
                                QString::number(m_context.value(QStringLiteral("combinedUncertainty")).toDouble(), 'f', 4),
                                QString::number(m_context.value(QStringLiteral("expandedUncertainty")).toDouble(), 'f', 4)));
    } else if (id == QStringLiteral("range_safety")) {
        article.insert(QStringLiteral("body"),
                       QStringLiteral("当前量程为 %1，使用率 %2%，安全状态为“%3”。")
                           .arg(m_context.value(QStringLiteral("rangeText")).toString(),
                                QString::number(m_context.value(QStringLiteral("utilizationPercent")).toDouble(), 'f', 1),
                                m_context.value(QStringLiteral("safetyTitle")).toString()));
        article.insert(QStringLiteral("actionId"), QStringLiteral("open_measurement"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("查看量程状态"));
    } else if (id == QStringLiteral("serial_troubleshooting")) {
        article.insert(QStringLiteral("body"),
                       QStringLiteral("当前串口状态：%1；端口：%2；协议：%3；最后帧年龄：%4 ms。")
                           .arg(m_context.value(QStringLiteral("serialStatus")).toString(),
                                m_context.value(QStringLiteral("serialPort")).toString().isEmpty()
                                    ? QStringLiteral("未发现")
                                    : m_context.value(QStringLiteral("serialPort")).toString(),
                                m_context.value(QStringLiteral("protocolName")).toString(),
                                QString::number(m_context.value(QStringLiteral("lastFrameAgeMs")).toLongLong())));
        article.insert(QStringLiteral("actionId"), QStringLiteral("reconnect_device"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("重新检测设备"));
    } else if (id == QStringLiteral("task_history") && hasTask) {
        article.insert(QStringLiteral("body"),
                       QStringLiteral("“%1”的测点、阶段、分析结果和助手问答均按任务独立保存。导出不会删除任务，之后可以再次打开、核对和生成文件。")
                           .arg(m_context.value(QStringLiteral("taskTitle")).toString()));
        article.insert(QStringLiteral("actionId"), QStringLiteral("open_current_task"));
        article.insert(QStringLiteral("actionText"), QStringLiteral("查看当前任务"));
    }

    if (!article.contains(QStringLiteral("source")))
        article.insert(QStringLiteral("source"), QStringLiteral("PressureOS 内置操作手册"));
    return article;
}

QString AssistantController::historyScope() const
{
    if (m_context.value(QStringLiteral("page")).toString() == QStringLiteral("runner")) {
        const QString taskId = m_context.value(QStringLiteral("taskId")).toString();
        if (!taskId.isEmpty())
            return taskId;
    }
    return QStringLiteral("global");
}

void AssistantController::appendHistory(const QString &kind, const QString &itemId,
                                        const QString &title, const QString &body,
                                        const QString &actionId)
{
    if (m_database) {
        m_database->appendAssistantHistory(historyScope(), kind, itemId, title, body,
                                           actionId, QDateTime::currentMSecsSinceEpoch());
    }
    reloadHistory();
}

void AssistantController::reloadHistory()
{
    m_history = m_database ? m_database->loadAssistantHistory(historyScope(), 12)
                           : QVariantList{};
    emit historyChanged();
}

void AssistantController::setNudgeVisible(bool visible)
{
    if (m_nudgeVisible == visible)
        return;
    m_nudgeVisible = visible;
    emit nudgeVisibleChanged();
}
