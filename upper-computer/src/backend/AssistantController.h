#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "AssistantRuleEngine.h"

class AssistantActionDispatcher;
class AssistantContextService;
class AssistantKnowledgeRepository;
class DatabaseService;

class AssistantController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap recommendation READ recommendation NOTIFY recommendationChanged)
    Q_PROPERTY(QVariantList quickQuestions READ quickQuestions NOTIFY recommendationChanged)
    Q_PROPERTY(QVariantMap currentAnswer READ currentAnswer NOTIFY answerChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString statusLevel READ statusLevel NOTIFY recommendationChanged)
    Q_PROPERTY(QString contextLabel READ contextLabel NOTIFY recommendationChanged)
    Q_PROPERTY(bool hasAttention READ hasAttention NOTIFY recommendationChanged)
    Q_PROPERTY(bool nudgeVisible READ nudgeVisible NOTIFY nudgeVisibleChanged)
    Q_PROPERTY(bool knowledgeReady READ knowledgeReady CONSTANT)

public:
    AssistantController(AssistantContextService *contextService,
                        AssistantKnowledgeRepository *knowledge,
                        AssistantActionDispatcher *actions,
                        DatabaseService *database,
                        QObject *parent = nullptr);

    QVariantMap recommendation() const { return m_recommendation; }
    QVariantList quickQuestions() const { return m_quickQuestions; }
    QVariantMap currentAnswer() const { return m_currentAnswer; }
    QVariantList history() const { return m_history; }
    QString statusLevel() const;
    QString contextLabel() const;
    bool hasAttention() const;
    bool nudgeVisible() const { return m_nudgeVisible; }
    bool knowledgeReady() const;

    Q_INVOKABLE void ask(const QString &articleId);
    Q_INVOKABLE void askText(const QString &question);
    Q_INVOKABLE void explainRecommendation();
    Q_INVOKABLE QVariantMap executeAction(const QString &actionId);
    Q_INVOKABLE void clearAnswer();
    Q_INVOKABLE void dismissNudge();
    Q_INVOKABLE void refresh();

signals:
    void recommendationChanged();
    void answerChanged();
    void historyChanged();
    void nudgeVisibleChanged();

private:
    QVariantMap enrichArticle(QVariantMap article, const QString &question = {}) const;
    QString historyScope() const;
    void appendHistory(const QString &kind, const QString &itemId,
                       const QString &title, const QString &body,
                       const QString &actionId = {});
    void reloadHistory();
    void setNudgeVisible(bool visible);

    AssistantContextService *m_contextService = nullptr;
    AssistantKnowledgeRepository *m_knowledge = nullptr;
    AssistantActionDispatcher *m_actions = nullptr;
    DatabaseService *m_database = nullptr;
    AssistantRuleEngine m_ruleEngine;
    QVariantMap m_context;
    QVariantMap m_recommendation;
    QVariantList m_quickQuestions;
    QVariantMap m_currentAnswer;
    QVariantList m_history;
    QString m_historyScope;
    bool m_nudgeVisible = false;
    QTimer m_nudgeTimer;
};
