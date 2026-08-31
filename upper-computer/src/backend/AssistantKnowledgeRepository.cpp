#include "AssistantKnowledgeRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

AssistantKnowledgeRepository::AssistantKnowledgeRepository(QObject *parent)
    : QObject(parent)
{
    load();
}

QVariantMap AssistantKnowledgeRepository::article(const QString &id) const
{
    return m_articlesById.value(id).toMap();
}

QVariantMap AssistantKnowledgeRepository::search(const QString &query) const
{
    const QString normalized = query.trimmed().toLower();
    if (normalized.isEmpty())
        return article(QStringLiteral("help_scope"));

    int bestScore = 0;
    QVariantMap best;
    for (const QVariant &value : m_articles) {
        const QVariantMap candidate = value.toMap();
        int score = 0;
        const QString title = candidate.value(QStringLiteral("title")).toString().toLower();
        const QString summary = candidate.value(QStringLiteral("summary")).toString().toLower();
        if (title.contains(normalized) || normalized.contains(title))
            score += 12;
        if (summary.contains(normalized))
            score += 4;
        const QVariantList keywords = candidate.value(QStringLiteral("keywords")).toList();
        for (const QVariant &keywordValue : keywords) {
            const QString keyword = keywordValue.toString().trimmed().toLower();
            if (!keyword.isEmpty() && normalized.contains(keyword))
                score += keyword.size() >= 4 ? 7 : 4;
        }
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return bestScore > 0 ? best : article(QStringLiteral("help_scope"));
}

QVariantMap AssistantKnowledgeRepository::templateGuide(const QString &templateId) const
{
    return m_templateGuides.value(templateId).toMap();
}

void AssistantKnowledgeRepository::load()
{
    QFile file(QStringLiteral(":/qt/qml/PressureOS/data/assistant/knowledge_base.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("无法打开内置助手知识库");
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_lastError = QStringLiteral("助手知识库 JSON 无效：%1").arg(parseError.errorString());
        return;
    }
    const QJsonObject root = document.object();
    const QJsonArray articles = root.value(QStringLiteral("articles")).toArray();
    if (articles.isEmpty()) {
        m_lastError = QStringLiteral("助手知识库没有可用条目");
        return;
    }
    for (const QJsonValue &value : articles) {
        const QVariantMap item = value.toObject().toVariantMap();
        const QString id = item.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;
        m_articles.push_back(item);
        m_articlesById.insert(id, item);
    }
    m_templateGuides = root.value(QStringLiteral("templateGuides")).toObject().toVariantMap();
    m_ready = !m_articlesById.isEmpty();
    if (!m_ready)
        m_lastError = QStringLiteral("助手知识条目缺少有效 ID");
}
