#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class DatabaseService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY readyChanged)
    Q_PROPERTY(int storedSampleCount READ storedSampleCount NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList measurementSessions READ measurementSessions NOTIFY measurementSessionsChanged)

public:
    struct BufferedSample {
        qint64 timestampMs = 0;
        double rawKPa = 0.0;
        double filteredKPa = 0.0;
        double compensatedKPa = 0.0;
        double temperatureC = 0.0;
    };

    explicit DatabaseService(QObject *parent = nullptr);
    ~DatabaseService() override;

    bool ready() const { return m_ready; }
    QString databasePath() const { return m_databasePath; }
    QString lastError() const { return m_lastError; }
    int storedSampleCount() const { return m_storedSampleCount; }
    QVariantList measurementSessions() const;

    bool startSession(const QString &name);
    bool startSession(const QString &name, const QString &source,
                      const QString &protocol, int sampleRate);
    void appendSample(const BufferedSample &sample);
    void stopSession();
    bool saveTaskPoint(const QString &taskId, double mass, double pressure,
                       const QString &source, qint64 timestampMs);
    QVariantList loadTaskPoints(const QString &taskId) const;
    QVariantList loadTasks() const;
    bool createTask(const QString &taskId, const QString &templateId, const QString &name,
                    const QString &templateName, int targetPoints, const QString &status,
                    int currentStage, const QString &accent, qint64 createdAt);
    bool saveTaskConfiguration(const QString &taskId, const QString &xName,
                               const QString &xUnit, const QString &captureMode,
                               const QString &analysisMode);
    bool updateTaskState(const QString &taskId, const QString &status,
                         int currentStage, qint64 updatedAt);
    bool deleteTask(const QString &taskId);
    bool deleteTaskPoint(const QString &taskId, qint64 pointId);
    bool clearTaskPoints(const QString &taskId);
    bool saveImportedTemplate(const QString &templateId, const QString &name,
                              const QString &version, const QByteArray &json);
    bool appendAssistantHistory(const QString &taskId, const QString &kind,
                                const QString &itemId, const QString &title,
                                const QString &body, const QString &actionId,
                                qint64 createdAt);
    QVariantList loadAssistantHistory(const QString &taskId, int limit = 20) const;
    bool appendAuditLog(const QString &action, const QString &detail);

    Q_INVOKABLE bool flush();
    Q_INVOKABLE QVariantMap runSelfCheck();
    Q_INVOKABLE QVariantMap measurementSession(qint64 sessionId,
                                                int maximumDisplayPoints = 1600) const;
    Q_INVOKABLE bool deleteMeasurementSession(qint64 sessionId);

signals:
    void readyChanged();
    void statisticsChanged();
    void measurementSessionsChanged();

private:
    bool initializeSchema();
    bool execute(const QString &sql);
    void setError(const QString &message);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_db;
    QTimer m_flushTimer;
    QVector<BufferedSample> m_buffer;
    qint64 m_currentSessionId = -1;
    bool m_ready = false;
    int m_storedSampleCount = 0;
};
