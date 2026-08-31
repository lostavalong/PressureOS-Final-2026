#pragma once

#include <QVariantList>
#include <QVariantMap>

class AssistantRuleEngine
{
public:
    QVariantMap evaluate(const QVariantMap &context) const;
    QVariantList quickQuestions(const QVariantMap &context) const;

private:
    static QVariantMap makeRecommendation(const QString &id, int priority,
                                          const QString &level, const QString &title,
                                          const QString &summary, const QString &reason,
                                          const QString &actionId = {},
                                          const QString &actionText = {},
                                          const QString &helpId = {},
                                          const QString &icon = QStringLiteral("assistant"));
    static QVariantMap question(const QString &id, const QString &title,
                                const QString &icon = QStringLiteral("help"));
};
