#include "DatabaseService.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
QString durationText(qint64 durationMs)
{
    const qint64 totalSeconds = qMax<qint64>(0, qRound64(durationMs / 1000.0));
    if (totalSeconds < 60)
        return QStringLiteral("%1 秒").arg(totalSeconds);
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    if (minutes < 60)
        return QStringLiteral("%1 分 %2 秒").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1 小时 %2 分").arg(minutes / 60).arg(minutes % 60);
}

QString relativeTime(qint64 timestampMs)
{
    const qint64 seconds = qMax<qint64>(0,
        (QDateTime::currentMSecsSinceEpoch() - timestampMs) / 1000);
    if (seconds < 60)
        return QStringLiteral("刚刚");
    if (seconds < 3600)
        return QStringLiteral("%1 分钟前").arg(seconds / 60);
    if (seconds < 86400)
        return QStringLiteral("%1 小时前").arg(seconds / 3600);
    if (seconds < 7 * 86400)
        return QStringLiteral("%1 天前").arg(seconds / 86400);
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("MM-dd HH:mm"));
}

QJsonObject parseSettings(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.isObject() ? document.object() : QJsonObject{};
}
}

DatabaseService::DatabaseService(QObject *parent)
    : QObject(parent)
{
    const QString configuredRoot = qEnvironmentVariable("PRESSUREOS_DATA_ROOT").trimmed();
    const QString dataRoot = configuredRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QDir(configuredRoot).absolutePath();
    QDir().mkpath(dataRoot);
    m_databasePath = QDir(dataRoot).filePath(QStringLiteral("pressureos_demo.sqlite"));
    m_connectionName = QStringLiteral("pressureos-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_databasePath);

    if (!m_db.open()) {
        setError(m_db.lastError().text());
        return;
    }

    m_ready = initializeSchema();
    if (m_ready) {
        QSqlQuery count(m_db);
        if (count.exec(QStringLiteral("SELECT COUNT(*) FROM samples")) && count.next())
            m_storedSampleCount = count.value(0).toInt();
    }

    m_flushTimer.setInterval(500);
    connect(&m_flushTimer, &QTimer::timeout, this, &DatabaseService::flush);
    m_flushTimer.start();
    emit readyChanged();
}

DatabaseService::~DatabaseService()
{
    stopSession();
    const QString name = m_connectionName;
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool DatabaseService::startSession(const QString &name)
{
    return startSession(name, QStringLiteral("device-simulator"),
                        QStringLiteral("simulation"), 50);
}

bool DatabaseService::startSession(const QString &name, const QString &source,
                                   const QString &protocol, int sampleRate)
{
    if (!m_ready)
        return false;
    stopSession();
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO measurement_sessions(name, started_at, status, settings_json) "
        "VALUES(?, ?, 'recording', ?)"));
    query.addBindValue(name);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    QJsonObject settings;
    settings.insert(QStringLiteral("interfaceVersion"), QStringLiteral("1.0"));
    settings.insert(QStringLiteral("sampleRate"), qMax(0, sampleRate));
    settings.insert(QStringLiteral("source"), source);
    settings.insert(QStringLiteral("protocol"), protocol);
    query.addBindValue(QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Compact)));
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    m_currentSessionId = query.lastInsertId().toLongLong();
    emit measurementSessionsChanged();
    return true;
}

void DatabaseService::appendSample(const BufferedSample &sample)
{
    if (!m_ready || m_currentSessionId < 0)
        return;
    m_buffer.push_back(sample);
    if (m_buffer.size() >= 100)
        flush();
}

void DatabaseService::stopSession()
{
    if (m_currentSessionId < 0)
        return;
    flush();
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE measurement_sessions SET ended_at=?, status='completed' WHERE id=?"));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(m_currentSessionId);
    if (!query.exec())
        setError(query.lastError().text());
    m_currentSessionId = -1;
    emit measurementSessionsChanged();
}

QVariantList DatabaseService::measurementSessions() const
{
    QVariantList result;
    if (!m_ready)
        return result;

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT s.id, s.name, s.started_at, s.ended_at, s.status, s.settings_json, "
            "COUNT(p.id), MIN(p.timestamp_ms), MAX(p.timestamp_ms), "
            "MIN(COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa)), "
            "MAX(COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa)) "
            "FROM measurement_sessions s "
            "LEFT JOIN samples p ON p.session_id=s.id "
            "GROUP BY s.id ORDER BY COALESCE(s.ended_at, s.started_at) DESC LIMIT 100"))) {
        return result;
    }

    while (query.next()) {
        const qint64 sessionId = query.value(0).toLongLong();
        const qint64 startedAt = query.value(2).toLongLong();
        const qint64 endedAt = query.value(3).isNull()
            ? QDateTime::currentMSecsSinceEpoch() : query.value(3).toLongLong();
        const qint64 firstSampleAt = query.value(7).isNull()
            ? startedAt : query.value(7).toLongLong();
        const qint64 lastSampleAt = query.value(8).isNull()
            ? endedAt : query.value(8).toLongLong();
        const int sampleCount = query.value(6).toInt();
        const bool recording = query.value(4).toString() == QStringLiteral("recording");
        const QJsonObject settings = parseSettings(query.value(5).toString());
        const int configuredRate = settings.value(QStringLiteral("sampleRate")).toInt();
        const qint64 durationMs = qMax<qint64>(0, lastSampleAt - firstSampleAt);
        const double measuredRate = sampleCount >= 2 && durationMs > 0
            ? (sampleCount - 1) * 1000.0 / durationMs : configuredRate;

        QVariantMap item;
        item.insert(QStringLiteral("kind"), QStringLiteral("recording"));
        item.insert(QStringLiteral("id"), QStringLiteral("session.%1").arg(sessionId));
        item.insert(QStringLiteral("sessionId"), sessionId);
        item.insert(QStringLiteral("name"), query.value(1));
        item.insert(QStringLiteral("templateName"), QStringLiteral("自由测量记录"));
        item.insert(QStringLiteral("status"), recording ? QStringLiteral("记录中")
                                                         : QStringLiteral("已保存"));
        item.insert(QStringLiteral("statusColor"), recording ? QStringLiteral("#EF625F")
                                                              : QStringLiteral("#19B987"));
        item.insert(QStringLiteral("accent"), QStringLiteral("#20A7D8"));
        item.insert(QStringLiteral("icon"), QStringLiteral("pulse"));
        item.insert(QStringLiteral("progress"), recording ? 0.55 : 1.0);
        item.insert(QStringLiteral("sampleCount"), sampleCount);
        item.insert(QStringLiteral("sampleRate"), measuredRate);
        item.insert(QStringLiteral("createdAt"), startedAt);
        item.insert(QStringLiteral("updatedAt"), endedAt);
        item.insert(QStringLiteral("updated"), relativeTime(endedAt));
        item.insert(QStringLiteral("detail"), QStringLiteral("%1 · %2 帧 · %3 Hz")
            .arg(durationText(durationMs))
            .arg(sampleCount)
            .arg(measuredRate, 0, 'f', 1));
        if (!query.value(9).isNull() && !query.value(10).isNull()) {
            item.insert(QStringLiteral("minimumKPa"), query.value(9));
            item.insert(QStringLiteral("maximumKPa"), query.value(10));
        }
        result.push_back(item);
    }
    return result;
}

QVariantMap DatabaseService::measurementSession(qint64 sessionId,
                                                 int maximumDisplayPoints) const
{
    QVariantMap result;
    result.insert(QStringLiteral("valid"), false);
    if (!m_ready || sessionId <= 0)
        return result;

    QSqlQuery metadata(m_db);
    metadata.prepare(QStringLiteral(
        "SELECT s.name, s.started_at, s.ended_at, s.status, s.settings_json, "
        "COUNT(p.id), MIN(p.timestamp_ms), MAX(p.timestamp_ms), "
        "AVG(COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa)), "
        "MIN(COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa)), "
        "MAX(COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa)), "
        "SUM(COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa) * "
        "    COALESCE(p.compensated_kpa, p.filtered_kpa, p.raw_kpa)), "
        "AVG(p.temperature_c) "
        "FROM measurement_sessions s LEFT JOIN samples p ON p.session_id=s.id "
        "WHERE s.id=? GROUP BY s.id"));
    metadata.addBindValue(sessionId);
    if (!metadata.exec() || !metadata.next())
        return result;

    const QString name = metadata.value(0).toString();
    const qint64 startedAt = metadata.value(1).toLongLong();
    const qint64 endedAt = metadata.value(2).isNull()
        ? QDateTime::currentMSecsSinceEpoch() : metadata.value(2).toLongLong();
    const QString status = metadata.value(3).toString();
    const QJsonObject settings = parseSettings(metadata.value(4).toString());
    const int sampleCount = metadata.value(5).toInt();
    const qint64 firstSampleAt = metadata.value(6).isNull()
        ? startedAt : metadata.value(6).toLongLong();
    const qint64 lastSampleAt = metadata.value(7).isNull()
        ? endedAt : metadata.value(7).toLongLong();
    const double mean = metadata.value(8).toDouble();
    const double minimum = metadata.value(9).toDouble();
    const double maximum = metadata.value(10).toDouble();
    const double sumSquares = metadata.value(11).toDouble();
    const double variance = sampleCount > 1
        ? qMax(0.0, (sumSquares - sampleCount * mean * mean) / (sampleCount - 1)) : 0.0;
    const double standardDeviation = qSqrt(variance);
    const double averageTemperature = metadata.value(12).toDouble();
    const qint64 durationMs = qMax<qint64>(0, lastSampleAt - firstSampleAt);
    const double measuredRate = sampleCount >= 2 && durationMs > 0
        ? (sampleCount - 1) * 1000.0 / durationMs
        : settings.value(QStringLiteral("sampleRate")).toDouble();

    maximumDisplayPoints = qBound(100, maximumDisplayPoints, 4000);
    const int bucketSize = sampleCount > maximumDisplayPoints
        ? qMax(1, qCeil(sampleCount / (maximumDisplayPoints / 2.0))) : 1;
    QVariantList series;
    QVariantList rawSeries;
    series.reserve(qMin(sampleCount, maximumDisplayPoints + 2));
    rawSeries.reserve(qMin(sampleCount, maximumDisplayPoints + 2));

    struct DisplaySample {
        qint64 timestamp = 0;
        double filtered = 0.0;
        double raw = 0.0;
    };
    QVector<DisplaySample> bucket;
    bucket.reserve(bucketSize);
    auto appendBucket = [&]() {
        if (bucket.isEmpty())
            return;
        auto minimumIt = std::min_element(bucket.cbegin(), bucket.cend(),
            [](const DisplaySample &left, const DisplaySample &right) {
                return left.filtered < right.filtered;
            });
        auto maximumIt = std::max_element(bucket.cbegin(), bucket.cend(),
            [](const DisplaySample &left, const DisplaySample &right) {
                return left.filtered < right.filtered;
            });
        const DisplaySample *first = &*minimumIt;
        const DisplaySample *second = &*maximumIt;
        if (second->timestamp < first->timestamp)
            std::swap(first, second);
        series.push_back(first->filtered);
        rawSeries.push_back(first->raw);
        if (second != first) {
            series.push_back(second->filtered);
            rawSeries.push_back(second->raw);
        }
        bucket.clear();
    };

    QSqlQuery samples(m_db);
    samples.prepare(QStringLiteral(
        "SELECT timestamp_ms, COALESCE(compensated_kpa, filtered_kpa, raw_kpa), raw_kpa "
        "FROM samples WHERE session_id=? ORDER BY timestamp_ms"));
    samples.addBindValue(sessionId);
    if (samples.exec()) {
        while (samples.next()) {
            bucket.push_back({samples.value(0).toLongLong(), samples.value(1).toDouble(),
                              samples.value(2).toDouble()});
            if (bucket.size() >= bucketSize)
                appendBucket();
        }
        appendBucket();
    }

    const double displayRate = durationMs > 0 && series.size() >= 2
        ? (series.size() - 1) * 1000.0 / durationMs : measuredRate;
    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("sessionId"), sessionId);
    result.insert(QStringLiteral("name"), name);
    result.insert(QStringLiteral("status"), status == QStringLiteral("recording")
        ? QStringLiteral("记录中") : QStringLiteral("已保存"));
    result.insert(QStringLiteral("startedAt"), startedAt);
    result.insert(QStringLiteral("endedAt"), endedAt);
    result.insert(QStringLiteral("startedText"),
                  QDateTime::fromMSecsSinceEpoch(startedAt).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    result.insert(QStringLiteral("durationSeconds"), durationMs / 1000.0);
    result.insert(QStringLiteral("durationText"), durationText(durationMs));
    result.insert(QStringLiteral("sampleCount"), sampleCount);
    result.insert(QStringLiteral("sampleRate"), measuredRate);
    result.insert(QStringLiteral("displaySampleRate"), displayRate);
    result.insert(QStringLiteral("meanKPa"), mean);
    result.insert(QStringLiteral("standardDeviationKPa"), standardDeviation);
    result.insert(QStringLiteral("minimumKPa"), minimum);
    result.insert(QStringLiteral("maximumKPa"), maximum);
    result.insert(QStringLiteral("peakToPeakKPa"), maximum - minimum);
    result.insert(QStringLiteral("averageTemperatureC"), averageTemperature);
    result.insert(QStringLiteral("source"), settings.value(QStringLiteral("source")).toString());
    result.insert(QStringLiteral("protocol"), settings.value(QStringLiteral("protocol")).toString());
    result.insert(QStringLiteral("databasePath"), m_databasePath);
    result.insert(QStringLiteral("series"), series);
    result.insert(QStringLiteral("rawSeries"), rawSeries);
    result.insert(QStringLiteral("downsampled"), sampleCount > series.size());
    return result;
}

bool DatabaseService::deleteMeasurementSession(qint64 sessionId)
{
    if (!m_ready || sessionId <= 0 || sessionId == m_currentSessionId
        || !m_db.transaction()) {
        return false;
    }
    QSqlQuery samples(m_db);
    samples.prepare(QStringLiteral("DELETE FROM samples WHERE session_id=?"));
    samples.addBindValue(sessionId);
    if (!samples.exec()) {
        m_db.rollback();
        setError(samples.lastError().text());
        return false;
    }
    QSqlQuery session(m_db);
    session.prepare(QStringLiteral("DELETE FROM measurement_sessions WHERE id=?"));
    session.addBindValue(sessionId);
    if (!session.exec() || session.numRowsAffected() <= 0 || !m_db.commit()) {
        m_db.rollback();
        setError(session.lastError().text().isEmpty()
            ? m_db.lastError().text() : session.lastError().text());
        return false;
    }
    emit measurementSessionsChanged();
    return true;
}

bool DatabaseService::saveTaskPoint(const QString &taskId, double mass, double pressure,
                                    const QString &source, qint64 timestampMs)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO task_points(task_id, mass_g, pressure_kpa, source, captured_at) "
        "VALUES(?, ?, ?, ?, ?)"));
    query.addBindValue(taskId);
    query.addBindValue(mass);
    query.addBindValue(pressure);
    query.addBindValue(source);
    query.addBindValue(timestampMs);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    QSqlQuery touch(m_db);
    touch.prepare(QStringLiteral("UPDATE task_runs SET updated_at=? WHERE task_id=?"));
    touch.addBindValue(timestampMs);
    touch.addBindValue(taskId);
    touch.exec();
    return true;
}

QVariantList DatabaseService::loadTaskPoints(const QString &taskId) const
{
    QVariantList result;
    if (!m_ready)
        return result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, mass_g, pressure_kpa, source, captured_at "
        "FROM task_points WHERE task_id=? ORDER BY id"));
    query.addBindValue(taskId);
    if (!query.exec())
        return result;
    int index = 1;
    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("databaseId"), query.value(0));
        row.insert(QStringLiteral("index"), index++);
        row.insert(QStringLiteral("mass"), query.value(1));
        row.insert(QStringLiteral("pressure"), query.value(2));
        row.insert(QStringLiteral("source"), query.value(3));
        row.insert(QStringLiteral("timestamp"), query.value(4));
        row.insert(QStringLiteral("status"), QStringLiteral("已采集"));
        result.push_back(row);
    }
    return result;
}

QVariantList DatabaseService::loadTasks() const
{
    QVariantList result;
    if (!m_ready)
        return result;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT t.task_id, t.template_id, t.name, t.template_name, t.status, t.current_stage, "
            "t.target_points, t.accent, t.created_at, t.updated_at, "
            "(SELECT COUNT(*) FROM task_points p WHERE p.task_id=t.task_id), "
            "c.x_name, c.x_unit, c.capture_mode, c.analysis_mode "
            "FROM task_runs t LEFT JOIN task_configs c ON c.task_id=t.task_id "
            "ORDER BY t.updated_at DESC")))
        return result;
    while (query.next()) {
        QVariantMap task;
        task.insert(QStringLiteral("id"), query.value(0));
        task.insert(QStringLiteral("templateId"), query.value(1));
        task.insert(QStringLiteral("name"), query.value(2));
        task.insert(QStringLiteral("templateName"), query.value(3));
        task.insert(QStringLiteral("status"), query.value(4));
        task.insert(QStringLiteral("currentStage"), query.value(5));
        task.insert(QStringLiteral("targetPoints"), query.value(6));
        task.insert(QStringLiteral("accent"), query.value(7));
        task.insert(QStringLiteral("createdAt"), query.value(8));
        task.insert(QStringLiteral("updatedAt"), query.value(9));
        task.insert(QStringLiteral("completedPoints"), query.value(10));
        task.insert(QStringLiteral("xName"), query.value(11));
        task.insert(QStringLiteral("xUnit"), query.value(12));
        task.insert(QStringLiteral("captureMode"), query.value(13));
        task.insert(QStringLiteral("analysisMode"), query.value(14));
        result.push_back(task);
    }
    return result;
}

bool DatabaseService::createTask(const QString &taskId, const QString &templateId,
                                 const QString &name, const QString &templateName,
                                 int targetPoints, const QString &status,
                                 int currentStage, const QString &accent, qint64 createdAt)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO task_runs(task_id, template_id, name, template_name, status, current_stage, "
        "target_points, accent, created_at, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(taskId);
    query.addBindValue(templateId);
    query.addBindValue(name);
    query.addBindValue(templateName);
    query.addBindValue(status);
    query.addBindValue(currentStage);
    query.addBindValue(targetPoints);
    query.addBindValue(accent);
    query.addBindValue(createdAt);
    query.addBindValue(createdAt);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseService::saveTaskConfiguration(const QString &taskId, const QString &xName,
                                            const QString &xUnit, const QString &captureMode,
                                            const QString &analysisMode)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO task_configs(task_id, x_name, x_unit, capture_mode, analysis_mode) "
        "VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(task_id) DO UPDATE SET x_name=excluded.x_name, x_unit=excluded.x_unit, "
        "capture_mode=excluded.capture_mode, analysis_mode=excluded.analysis_mode"));
    query.addBindValue(taskId);
    query.addBindValue(xName);
    query.addBindValue(xUnit);
    query.addBindValue(captureMode);
    query.addBindValue(analysisMode);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseService::updateTaskState(const QString &taskId, const QString &status,
                                      int currentStage, qint64 updatedAt)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE task_runs SET status=?, current_stage=?, updated_at=? WHERE task_id=?"));
    query.addBindValue(status);
    query.addBindValue(currentStage);
    query.addBindValue(updatedAt);
    query.addBindValue(taskId);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool DatabaseService::deleteTask(const QString &taskId)
{
    if (!m_ready || !m_db.transaction())
        return false;
    QSqlQuery points(m_db);
    points.prepare(QStringLiteral("DELETE FROM task_points WHERE task_id=?"));
    points.addBindValue(taskId);
    if (!points.exec()) {
        m_db.rollback();
        setError(points.lastError().text());
        return false;
    }
    QSqlQuery config(m_db);
    config.prepare(QStringLiteral("DELETE FROM task_configs WHERE task_id=?"));
    config.addBindValue(taskId);
    if (!config.exec()) {
        m_db.rollback();
        setError(config.lastError().text());
        return false;
    }
    QSqlQuery assistantHistory(m_db);
    assistantHistory.prepare(QStringLiteral("DELETE FROM assistant_history WHERE task_id=?"));
    assistantHistory.addBindValue(taskId);
    if (!assistantHistory.exec()) {
        m_db.rollback();
        setError(assistantHistory.lastError().text());
        return false;
    }
    QSqlQuery task(m_db);
    task.prepare(QStringLiteral("DELETE FROM task_runs WHERE task_id=?"));
    task.addBindValue(taskId);
    if (!task.exec() || !m_db.commit()) {
        m_db.rollback();
        setError(task.lastError().text().isEmpty() ? m_db.lastError().text() : task.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseService::deleteTaskPoint(const QString &taskId, qint64 pointId)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM task_points WHERE task_id=? AND id=?"));
    query.addBindValue(taskId);
    query.addBindValue(pointId);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool DatabaseService::clearTaskPoints(const QString &taskId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM task_points WHERE task_id=?"));
    query.addBindValue(taskId);
    return query.exec();
}

bool DatabaseService::saveImportedTemplate(const QString &templateId, const QString &name,
                                           const QString &version, const QByteArray &json)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO imported_templates(template_id, name, version, json, installed_at) "
        "VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(template_id) DO UPDATE SET name=excluded.name, version=excluded.version, "
        "json=excluded.json, installed_at=excluded.installed_at"));
    query.addBindValue(templateId);
    query.addBindValue(name);
    query.addBindValue(version);
    query.addBindValue(QString::fromUtf8(json));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseService::appendAssistantHistory(const QString &taskId, const QString &kind,
                                             const QString &itemId, const QString &title,
                                             const QString &body, const QString &actionId,
                                             qint64 createdAt)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO assistant_history(task_id, kind, item_id, title, body, action_id, created_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(taskId.isEmpty() ? QStringLiteral("global") : taskId);
    query.addBindValue(kind);
    query.addBindValue(itemId);
    query.addBindValue(title);
    query.addBindValue(body);
    query.addBindValue(actionId);
    query.addBindValue(createdAt);
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

QVariantList DatabaseService::loadAssistantHistory(const QString &taskId, int limit) const
{
    QVariantList result;
    if (!m_ready)
        return result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT kind, item_id, title, body, action_id, created_at "
        "FROM assistant_history WHERE task_id=? ORDER BY id DESC LIMIT ?"));
    query.addBindValue(taskId.isEmpty() ? QStringLiteral("global") : taskId);
    query.addBindValue(qBound(1, limit, 100));
    if (!query.exec())
        return result;
    while (query.next()) {
        QVariantMap item;
        item.insert(QStringLiteral("kind"), query.value(0));
        item.insert(QStringLiteral("itemId"), query.value(1));
        item.insert(QStringLiteral("title"), query.value(2));
        item.insert(QStringLiteral("body"), query.value(3));
        item.insert(QStringLiteral("actionId"), query.value(4));
        item.insert(QStringLiteral("createdAt"), query.value(5));
        item.insert(QStringLiteral("timeText"),
                    QDateTime::fromMSecsSinceEpoch(query.value(5).toLongLong())
                        .toString(QStringLiteral("MM-dd HH:mm")));
        result.push_back(item);
    }
    return result;
}

bool DatabaseService::appendAuditLog(const QString &action, const QString &detail)
{
    if (!m_ready)
        return false;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO audit_logs(action, detail, created_at) VALUES(?, ?, ?)"));
    query.addBindValue(action);
    query.addBindValue(detail);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        setError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseService::flush()
{
    if (!m_ready || m_buffer.isEmpty() || m_currentSessionId < 0)
        return true;
    if (!m_db.transaction()) {
        setError(m_db.lastError().text());
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO samples(session_id, timestamp_ms, raw_kpa, filtered_kpa, "
        "compensated_kpa, temperature_c, status) VALUES(?, ?, ?, ?, ?, ?, 'valid')"));
    int inserted = 0;
    for (const BufferedSample &sample : std::as_const(m_buffer)) {
        query.addBindValue(m_currentSessionId);
        query.addBindValue(sample.timestampMs);
        query.addBindValue(sample.rawKPa);
        query.addBindValue(sample.filteredKPa);
        query.addBindValue(sample.compensatedKPa);
        query.addBindValue(sample.temperatureC);
        if (!query.exec()) {
            m_db.rollback();
            setError(query.lastError().text());
            return false;
        }
        ++inserted;
    }
    if (!m_db.commit()) {
        setError(m_db.lastError().text());
        return false;
    }
    m_buffer.clear();
    m_storedSampleCount += inserted;
    emit statisticsChanged();
    return true;
}

QVariantMap DatabaseService::runSelfCheck()
{
    QVariantMap result;
    result.insert(QStringLiteral("databaseReady"), m_ready);
    result.insert(QStringLiteral("integrityOk"), false);
    result.insert(QStringLiteral("writeOk"), false);
    result.insert(QStringLiteral("freeSpaceOk"), false);
    result.insert(QStringLiteral("freeBytes"), 0LL);
    result.insert(QStringLiteral("message"), m_ready ? QStringLiteral("正在检查") : m_lastError);
    if (!m_ready)
        return result;

    QSqlQuery integrity(m_db);
    const bool integrityOk = integrity.exec(QStringLiteral("PRAGMA quick_check"))
        && integrity.next() && integrity.value(0).toString() == QStringLiteral("ok");
    result.insert(QStringLiteral("integrityOk"), integrityOk);

    bool writeOk = false;
    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (m_db.transaction()) {
        QSqlQuery write(m_db);
        write.prepare(QStringLiteral(
            "INSERT INTO audit_logs(action, detail, created_at) VALUES('self_check', ?, ?)"));
        write.addBindValue(token);
        write.addBindValue(QDateTime::currentMSecsSinceEpoch());
        const bool inserted = write.exec();
        QSqlQuery cleanup(m_db);
        cleanup.prepare(QStringLiteral("DELETE FROM audit_logs WHERE action='self_check' AND detail=?"));
        cleanup.addBindValue(token);
        const bool removed = inserted && cleanup.exec();
        writeOk = removed && m_db.commit();
        if (!writeOk)
            m_db.rollback();
    }
    result.insert(QStringLiteral("writeOk"), writeOk);

    const QStorageInfo storage(QFileInfo(m_databasePath).absolutePath());
    const qint64 freeBytes = storage.isValid() && storage.isReady()
        ? storage.bytesAvailable() : 0;
    const bool freeSpaceOk = freeBytes >= 50LL * 1024LL * 1024LL;
    result.insert(QStringLiteral("freeBytes"), freeBytes);
    result.insert(QStringLiteral("freeSpaceOk"), freeSpaceOk);
    result.insert(QStringLiteral("ok"), integrityOk && writeOk && freeSpaceOk);
    result.insert(QStringLiteral("message"), integrityOk && writeOk && freeSpaceOk
                      ? QStringLiteral("数据库完整性、写入回读和剩余空间检查通过")
                      : QStringLiteral("数据库自检存在异常，请查看分项结果"));
    return result;
}

bool DatabaseService::initializeSchema()
{
    if (!execute(QStringLiteral("PRAGMA journal_mode=WAL"))
        || !execute(QStringLiteral("PRAGMA synchronous=NORMAL"))
        || !execute(QStringLiteral("PRAGMA foreign_keys=ON")))
        return false;

    const QStringList statements {
        QStringLiteral("CREATE TABLE IF NOT EXISTS measurement_sessions("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, "
                       "started_at INTEGER NOT NULL, ended_at INTEGER, status TEXT NOT NULL, "
                       "settings_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS samples("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, session_id INTEGER NOT NULL, "
                       "timestamp_ms INTEGER NOT NULL, raw_kpa REAL, filtered_kpa REAL, "
                       "compensated_kpa REAL, temperature_c REAL, status TEXT, "
                       "FOREIGN KEY(session_id) REFERENCES measurement_sessions(id))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_session_time "
                       "ON samples(session_id, timestamp_ms)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS task_points("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT NOT NULL, "
                       "mass_g REAL NOT NULL, pressure_kpa REAL NOT NULL, source TEXT NOT NULL, "
                       "captured_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_task_points_task ON task_points(task_id)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS task_runs("
                       "task_id TEXT PRIMARY KEY, template_id TEXT NOT NULL, name TEXT NOT NULL, "
                       "template_name TEXT NOT NULL, status TEXT NOT NULL, current_stage INTEGER NOT NULL DEFAULT 0, "
                       "target_points INTEGER NOT NULL DEFAULT 6, accent TEXT NOT NULL DEFAULT '#1683FF', "
                       "created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS task_configs("
                       "task_id TEXT PRIMARY KEY, x_name TEXT NOT NULL, x_unit TEXT, "
                       "capture_mode TEXT NOT NULL DEFAULT 'manual', "
                       "analysis_mode TEXT NOT NULL DEFAULT 'linear', "
                       "FOREIGN KEY(task_id) REFERENCES task_runs(task_id))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_task_runs_updated ON task_runs(updated_at DESC)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS imported_templates("
                       "template_id TEXT PRIMARY KEY, name TEXT NOT NULL, version TEXT NOT NULL, "
                       "json TEXT NOT NULL, installed_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS devices("
                       "device_id TEXT PRIMARY KEY, hardware_version TEXT, firmware_version TEXT, "
                       "protocol_version TEXT, last_seen_at INTEGER, metadata_json TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS calibration_profiles("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, device_id TEXT NOT NULL, "
                       "version TEXT NOT NULL, algorithm TEXT NOT NULL, coefficients_json TEXT NOT NULL, "
                       "range_min_kpa REAL, range_max_kpa REAL, parameter_crc TEXT, "
                       "created_at INTEGER NOT NULL, active INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_calibration_device_version "
                       "ON calibration_profiles(device_id, version)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS reports("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT, report_type TEXT NOT NULL, "
                       "file_path TEXT, summary_json TEXT, created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings("
                       "key TEXT PRIMARY KEY, value_json TEXT NOT NULL, updated_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS audit_logs("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, action TEXT NOT NULL, "
                       "detail TEXT, created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS assistant_history("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, task_id TEXT NOT NULL, "
                       "kind TEXT NOT NULL, item_id TEXT, title TEXT NOT NULL, body TEXT, "
                       "action_id TEXT, created_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_assistant_history_task_time "
                       "ON assistant_history(task_id, created_at DESC)")
    };
    for (const QString &statement : statements) {
        if (!execute(statement))
            return false;
    }
    return true;
}

bool DatabaseService::execute(const QString &sql)
{
    QSqlQuery query(m_db);
    if (query.exec(sql))
        return true;
    setError(query.lastError().text());
    return false;
}

void DatabaseService::setError(const QString &message)
{
    m_lastError = message;
    emit readyChanged();
}
