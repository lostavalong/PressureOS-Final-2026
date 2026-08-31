#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class AssistantKnowledgeRepository final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString lastError READ lastError CONSTANT)

public:
    explicit AssistantKnowledgeRepository(QObject *parent = nullptr);

    bool ready() const { return m_ready; }
    QString lastError() const { return m_lastError; }
    QVariantMap article(const QString &id) const;
    QVariantMap search(const QString &query) const;
    QVariantMap templateGuide(const QString &templateId) const;
    QVariantList articles() const { return m_articles; }

private:
    void load();

    bool m_ready = false;
    QString m_lastError;
    QVariantList m_articles;
    QVariantMap m_articlesById;
    QVariantMap m_templateGuides;
};
