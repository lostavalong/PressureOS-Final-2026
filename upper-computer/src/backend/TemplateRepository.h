#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

class DatabaseService;

class TemplateRepository final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList templates READ templates NOTIFY templatesChanged)
    Q_PROPERTY(int templateCount READ templateCount NOTIFY templatesChanged)
    Q_PROPERTY(QString lastImportMessage READ lastImportMessage NOTIFY importResultChanged)
    Q_PROPERTY(bool lastImportSucceeded READ lastImportSucceeded NOTIFY importResultChanged)

public:
    explicit TemplateRepository(DatabaseService *database, QObject *parent = nullptr);

    QVariantList templates() const { return m_templates; }
    int templateCount() const { return m_templates.size(); }
    QString lastImportMessage() const { return m_lastImportMessage; }
    bool lastImportSucceeded() const { return m_lastImportSucceeded; }

    Q_INVOKABLE bool importTemplate(const QUrl &fileUrl);

signals:
    void templatesChanged();
    void importResultChanged();
    void userMessage(const QString &message);

private:
    void addBuiltInTemplates();
    void failImport(const QString &message);

    DatabaseService *m_database = nullptr;
    QVariantList m_templates;
    QString m_lastImportMessage;
    bool m_lastImportSucceeded = false;
};
