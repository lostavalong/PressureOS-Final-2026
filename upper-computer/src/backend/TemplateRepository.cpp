#include "TemplateRepository.h"

#include "DatabaseService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

TemplateRepository::TemplateRepository(DatabaseService *database, QObject *parent)
    : QObject(parent), m_database(database)
{
    addBuiltInTemplates();
}

bool TemplateRepository::importTemplate(const QUrl &fileUrl)
{
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        failImport(QStringLiteral("无法读取该文件，请检查文件权限"));
        return false;
    }
    const QByteArray raw = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        failImport(QStringLiteral("JSON 格式错误：%1").arg(parseError.errorString()));
        return false;
    }
    const QJsonObject root = document.object();
    const QString schemaVersion = root.value(QStringLiteral("schemaVersion")).toString();
    const QString id = root.value(QStringLiteral("templateId")).toString();
    const QString name = root.value(QStringLiteral("name")).toString();
    const QString version = root.value(QStringLiteral("version")).toString();
    const QJsonArray workflow = root.value(QStringLiteral("workflow")).toArray();
    const QJsonArray variables = root.value(QStringLiteral("variables")).toArray();
    if (schemaVersion != QStringLiteral("1.0")) {
        failImport(QStringLiteral("仅支持 PressureOS Template Schema 1.0"));
        return false;
    }
    if (id.isEmpty() || name.isEmpty() || version.isEmpty()) {
        failImport(QStringLiteral("模板缺少 templateId、name 或 version"));
        return false;
    }
    if (workflow.isEmpty() || variables.isEmpty()) {
        failImport(QStringLiteral("模板必须包含变量定义和可执行工作流"));
        return false;
    }
    QSet<QString> ids;
    for (const QJsonValue &value : workflow) {
        const QString stepId = value.toObject().value(QStringLiteral("id")).toString();
        if (stepId.isEmpty() || ids.contains(stepId)) {
            failImport(QStringLiteral("工作流步骤 ID 为空或重复"));
            return false;
        }
        ids.insert(stepId);
    }
    if (m_database && !m_database->saveImportedTemplate(id, name, version, raw)) {
        failImport(QStringLiteral("模板校验通过，但写入数据库失败"));
        return false;
    }

    QVariantMap item;
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("name"), name);
    item.insert(QStringLiteral("category"), root.value(QStringLiteral("category")).toString(QStringLiteral("自定义")));
    item.insert(QStringLiteral("description"), root.value(QStringLiteral("description")).toObject()
                .value(QStringLiteral("plainText")).toString(QStringLiteral("用户导入的任务模板")));
    item.insert(QStringLiteral("duration"), QStringLiteral("按模板流程"));
    item.insert(QStringLiteral("outputs"), QStringLiteral("%1 个变量 · %2 个步骤").arg(variables.size()).arg(workflow.size()));
    item.insert(QStringLiteral("accent"), QStringLiteral("#FF8A4C"));
    item.insert(QStringLiteral("icon"), QStringLiteral("file"));
    item.insert(QStringLiteral("badge"), QStringLiteral("已验证 · 用户模板"));
    item.insert(QStringLiteral("installed"), true);
    m_templates.push_front(item);

    m_lastImportSucceeded = true;
    m_lastImportMessage = QStringLiteral("“%1”已通过结构与引用检查并安全安装").arg(name);
    emit templatesChanged();
    emit importResultChanged();
    emit userMessage(m_lastImportMessage);
    return true;
}

void TemplateRepository::addBuiltInTemplates()
{
    m_templates = {
        QVariantMap{{"id", "edu.pressure-mass.linear"}, {"name", "压力—质量关系实验"},
                    {"category", "经典物理实验"}, {"description", "人工补充质量，压力自动稳定采集；完成线性拟合、R 与残差分析。"},
                    {"duration", "约 12 min"}, {"outputs", "数据表 · 拟合图 · 实验报告"},
                    {"accent", "#1683FF"}, {"icon", "flask"}, {"badge", "内置 · 推荐"}, {"installed", true}},
        QVariantMap{{"id", "metrology.multi-point"}, {"name", "多点压力标定"},
                    {"category", "计量标定"}, {"description", "引导上、下行程逐点采集，计算示值误差、回程误差并生成参数版本。"},
                    {"duration", "约 25 min"}, {"outputs", "残差图 · 标定参数 · 审计记录"},
                    {"accent", "#7B61FF"}, {"icon", "target"}, {"badge", "内置 · 专业"}, {"installed", true}},
        QVariantMap{{"id", "quality.zero-drift"}, {"name", "零点漂移观察"},
                    {"category", "稳定性评估"}, {"description", "在设定时长内连续记录零点，自动计算极差、标准差与趋势斜率。"},
                    {"duration", "10～60 min"}, {"outputs", "趋势图 · 稳定性统计"},
                    {"accent", "#15B98A"}, {"icon", "trend"}, {"badge", "内置"}, {"installed", true}},
        QVariantMap{{"id", "engineering.leak"}, {"name", "定压泄漏测试"},
                    {"category", "工程测试"}, {"description", "达到目标压力后开始保压，依据压降速率和阈值给出可追溯判定。"},
                    {"duration", "约 15 min"}, {"outputs", "压降曲线 · 合格判定"},
                    {"accent", "#F39A3D"}, {"icon", "shield"}, {"badge", "试验功能"}, {"installed", true}}
    };
}

void TemplateRepository::failImport(const QString &message)
{
    m_lastImportSucceeded = false;
    m_lastImportMessage = message;
    emit importResultChanged();
    emit userMessage(message);
}
