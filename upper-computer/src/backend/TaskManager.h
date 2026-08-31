#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

class DatabaseService;
class DeviceSimulator;
class SerialDeviceGateway;

class TaskManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList taskList READ taskList NOTIFY taskListChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(bool hasCurrentTask READ hasCurrentTask NOTIFY currentTaskChanged)
    Q_PROPERTY(QString currentTaskTitle READ currentTaskTitle NOTIFY currentTaskChanged)
    Q_PROPERTY(QString currentTaskId READ currentTaskId NOTIFY currentTaskChanged)
    Q_PROPERTY(QString currentTemplateId READ currentTemplateId NOTIFY currentTaskChanged)
    Q_PROPERTY(QString currentTemplateName READ currentTemplateName NOTIFY currentTaskChanged)
    Q_PROPERTY(QString currentStatus READ currentStatus NOTIFY currentTaskChanged)
    Q_PROPERTY(QString taskDescription READ taskDescription NOTIFY currentTaskChanged)
    Q_PROPERTY(QString taskObjective READ taskObjective NOTIFY currentTaskChanged)
    Q_PROPERTY(QString xVariableName READ xVariableName NOTIFY currentTaskChanged)
    Q_PROPERTY(QString xVariableUnit READ xVariableUnit NOTIFY currentTaskChanged)
    Q_PROPERTY(bool isQuickTask READ isQuickTask NOTIFY currentTaskChanged)
    Q_PROPERTY(bool allowsEngineeringCapture READ allowsEngineeringCapture NOTIFY currentTaskChanged)
    Q_PROPERTY(QString captureMode READ captureMode NOTIFY currentTaskChanged)
    Q_PROPERTY(QString captureTrustLevel READ captureTrustLevel NOTIFY capturePolicyChanged)
    Q_PROPERTY(QString captureBlockReason READ captureBlockReason NOTIFY capturePolicyChanged)
    Q_PROPERTY(bool canCaptureCurrent READ canCaptureCurrent NOTIFY capturePolicyChanged)
    Q_PROPERTY(bool containsEngineeringData READ containsEngineeringData NOTIFY rowsChanged)
    Q_PROPERTY(double nextAutoX READ nextAutoX NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList workflow READ workflow NOTIFY currentTaskChanged)
    Q_PROPERTY(QVariantList preparationItems READ preparationItems NOTIFY currentTaskChanged)
    Q_PROPERTY(QVariantList safetyNotes READ safetyNotes NOTIFY currentTaskChanged)
    Q_PROPERTY(int completedPoints READ completedPoints NOTIFY rowsChanged)
    Q_PROPERTY(int targetPoints READ targetPoints NOTIFY currentTaskChanged)
    Q_PROPERTY(double progress READ progress NOTIFY rowsChanged)
    Q_PROPERTY(int currentStage READ currentStage NOTIFY currentTaskChanged)
    Q_PROPERTY(QString lastSavedText READ lastSavedText NOTIFY currentTaskChanged)
    Q_PROPERTY(double slope READ slope NOTIFY resultsChanged)
    Q_PROPERTY(double intercept READ intercept NOTIFY resultsChanged)
    Q_PROPERTY(double pearsonR READ pearsonR NOTIFY resultsChanged)
    Q_PROPERTY(double rSquared READ rSquared NOTIFY resultsChanged)
    Q_PROPERTY(double residualStd READ residualStd NOTIFY resultsChanged)
    Q_PROPERTY(double maxAbsResidual READ maxAbsResidual NOTIFY resultsChanged)
    Q_PROPERTY(double typeAUncertainty READ typeAUncertainty NOTIFY resultsChanged)
    Q_PROPERTY(double typeBUncertainty READ typeBUncertainty NOTIFY resultsChanged)
    Q_PROPERTY(double combinedUncertainty READ combinedUncertainty NOTIFY resultsChanged)
    Q_PROPERTY(double expandedUncertainty READ expandedUncertainty NOTIFY resultsChanged)
    Q_PROPERTY(int outlierCount READ outlierCount NOTIFY resultsChanged)
    Q_PROPERTY(QString outlierSummary READ outlierSummary NOTIFY resultsChanged)
    Q_PROPERTY(QString fitQuality READ fitQuality NOTIFY resultsChanged)
    Q_PROPERTY(QString equation READ equation NOTIFY resultsChanged)
    Q_PROPERTY(bool hasFit READ hasFit NOTIFY resultsChanged)
    Q_PROPERTY(bool canFinishMeasurement READ canFinishMeasurement NOTIFY rowsChanged)

public:
    explicit TaskManager(DatabaseService *database, QObject *parent = nullptr);

    QVariantList taskList() const { return m_tasks; }
    QVariantList rows() const { return m_rows; }
    bool hasCurrentTask() const { return !m_currentTask.isEmpty(); }
    QString currentTaskTitle() const;
    QString currentTaskId() const;
    QString currentTemplateId() const;
    QString currentTemplateName() const;
    QString currentStatus() const;
    QString taskDescription() const;
    QString taskObjective() const;
    QString xVariableName() const;
    QString xVariableUnit() const;
    bool isQuickTask() const;
    bool allowsEngineeringCapture() const;
    QString captureMode() const;
    QString captureTrustLevel() const;
    QString captureBlockReason() const;
    bool canCaptureCurrent() const;
    bool containsEngineeringData() const;
    double nextAutoX() const;
    QVariantList workflow() const;
    QVariantList preparationItems() const;
    QVariantList safetyNotes() const;
    int completedPoints() const { return m_rows.size(); }
    int targetPoints() const;
    double progress() const;
    int currentStage() const;
    QString lastSavedText() const;
    double slope() const { return m_slope; }
    double intercept() const { return m_intercept; }
    double pearsonR() const { return m_r; }
    double rSquared() const { return m_r2; }
    double residualStd() const { return m_residualStd; }
    double maxAbsResidual() const { return m_maxAbsResidual; }
    double typeAUncertainty() const { return m_typeA; }
    double typeBUncertainty() const { return m_typeB; }
    double combinedUncertainty() const { return m_combined; }
    double expandedUncertainty() const { return m_expanded; }
    int outlierCount() const { return m_outlierCount; }
    QString outlierSummary() const;
    QString fitQuality() const;
    QString equation() const;
    bool hasFit() const { return m_rows.size() >= 3; }
    bool canFinishMeasurement() const
    {
        return completedPoints() >= (isQuickTask() ? 3 : targetPoints());
    }

    void attachMeasurementSource(DeviceSimulator *device, SerialDeviceGateway *gateway);

    Q_INVOKABLE bool createTask(const QString &templateId, const QString &customName);
    Q_INVOKABLE bool createQuickTask(const QString &customName, const QString &xName,
                                     const QString &xUnit, const QString &captureMode,
                                     int suggestedPoints);
    Q_INVOKABLE bool selectTask(const QString &taskId);
    Q_INVOKABLE bool deleteTask(const QString &taskId);
    Q_INVOKABLE bool capturePoint(double xValue, double pressureKPa, bool stable);
    Q_INVOKABLE bool deletePoint(qint64 databaseId);
    Q_INVOKABLE void setCurrentStage(int stage);
    Q_INVOKABLE bool finishMeasurement();
    Q_INVOKABLE bool confirmAnalysis();
    Q_INVOKABLE void restoreExample();
    Q_INVOKABLE QString exportCsv();
    Q_INVOKABLE QString exportBundle();

signals:
    void taskListChanged();
    void currentTaskChanged();
    void rowsChanged();
    void resultsChanged();
    void capturePolicyChanged();
    void userMessage(const QString &message);

private:
    QVariantMap definitionFor(const QString &templateId) const;
    void initializeDemoTasks();
    void reloadTaskList();
    void enrichTask(QVariantMap &task) const;
    void touchCurrent(const QString &status, int stage);
    void seedCurrentExample();
    void appendPoint(double xValue, double pressureKPa, const QString &source,
                     qint64 timestampMs, bool persist);
    void calculateRegression();
    QString writeCsv(const QString &directory) const;
    QString writeAnalysisJson(const QString &directory) const;
    QString writeChartSvg(const QString &directory) const;
    static QString relativeTime(qint64 timestampMs);
    static QString safeFileName(const QString &name);

    DatabaseService *m_database = nullptr;
    QPointer<DeviceSimulator> m_device;
    QPointer<SerialDeviceGateway> m_gateway;
    QVariantList m_tasks;
    QVariantMap m_currentTask;
    QVariantList m_rows;
    double m_slope = 0.0;
    double m_intercept = 0.0;
    double m_r = 0.0;
    double m_r2 = 0.0;
    double m_residualStd = 0.0;
    double m_maxAbsResidual = 0.0;
    double m_typeA = 0.0;
    double m_typeB = 0.0;
    double m_combined = 0.0;
    double m_expanded = 0.0;
    int m_outlierCount = 0;
};
