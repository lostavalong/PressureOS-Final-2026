#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <functional>
#include <numeric>

#include "DatabaseService.h"
#include "AppController.h"
#include "AssistantActionDispatcher.h"
#include "AssistantKnowledgeRepository.h"
#include "AssistantRuleEngine.h"
#include "DeviceSimulator.h"
#include "LegacyAsciiProtocol.h"
#include "PinyinInputEngine.h"
#include "PressureSignalProcessor.h"
#include "PressureProtocolV1.h"
#include "SerialDeviceGateway.h"
#include "TaskManager.h"
#include "TemplateRepository.h"

struct SerialDeviceGatewayTestAccess
{
    static void connectSyntheticTransport(SerialDeviceGateway &gateway)
    {
        gateway.m_running = true;
        gateway.m_connected = true;
        gateway.m_connectionState = QStringLiteral("waiting");
        gateway.m_runtime.start();
    }

    static void feed(SerialDeviceGateway &gateway, const QByteArray &bytes)
    {
        gateway.processBytes(bytes);
    }
};

namespace {
bool require(bool condition, const char *message)
{
    if (!condition)
        qCritical() << "FAILED:" << message;
    return condition;
}

bool waitUntil(const std::function<bool()> &predicate, int timeoutMs)
{
    if (predicate())
        return true;
    QElapsedTimer elapsed;
    elapsed.start();
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(20);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (predicate() || elapsed.elapsed() >= timeoutMs)
            loop.quit();
    });
    poll.start();
    loop.exec();
    return predicate();
}

QByteArray measurementFrame(quint32 sequence, quint32 uptimeMs,
                            quint32 pressureRaw, quint32 temperatureRaw,
                            quint32 statusFlags)
{
    const QByteArray payload = QByteArrayLiteral("PS1,M,")
        + QByteArray::number(sequence) + ',' + QByteArray::number(uptimeMs) + ','
        + QByteArray::number(pressureRaw) + ',' + QByteArray::number(temperatureRaw) + ','
        + QByteArray::number(statusFlags, 16).rightJustified(8, '0').toUpper();
    return QByteArrayLiteral("@") + payload + '*'
        + QByteArray::number(PressureProtocolV1::crc16CcittFalse(payload), 16)
              .rightJustified(4, '0').toUpper()
        + QByteArrayLiteral("\r\n");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PressureOS Test"));
    QCoreApplication::setApplicationName(QStringLiteral("BackendSmoke"));
    QTemporaryDir dataRoot;
    QTemporaryDir exportRoot;
    if (!require(dataRoot.isValid(), "temporary database directory")
        || !require(exportRoot.isValid(), "temporary export directory"))
        return 1;
    qputenv("PRESSUREOS_DATA_ROOT", dataRoot.path().toUtf8());
    qputenv("PRESSUREOS_EXPORT_ROOT", exportRoot.path().toUtf8());

    DatabaseService database;
    if (!require(database.ready(), "SQLite schema initialization"))
        return 2;
    const QVariantMap databaseCheck = database.runSelfCheck();
    if (!require(databaseCheck.value(QStringLiteral("integrityOk")).toBool(),
                 "SQLite integrity self-check")
        || !require(databaseCheck.value(QStringLiteral("writeOk")).toBool(),
                    "SQLite write/readback self-check"))
        return 25;

    AssistantKnowledgeRepository assistantKnowledge;
    if (!require(assistantKnowledge.ready(), "assistant knowledge base loads")
        || !require(assistantKnowledge.article(QStringLiteral("task_overview"))
                        .value(QStringLiteral("title")).toString().contains(QStringLiteral("任务")),
                    "assistant article lookup")
        || !require(assistantKnowledge.search(QStringLiteral("为什么现在不能归零"))
                        .value(QStringLiteral("id")).toString()
                        == QStringLiteral("zero_calibration"),
                    "assistant keyword retrieval"))
        return 31;

    PinyinInputEngine pinyin;
    pinyin.appendLetter(QStringLiteral("r"));
    pinyin.appendLetter(QStringLiteral("e"));
    pinyin.appendLetter(QStringLiteral("n"));
    pinyin.appendLetter(QStringLiteral("w"));
    pinyin.appendLetter(QStringLiteral("u"));
    if (!require(pinyin.ready(), "offline pinyin lexicon loads")
        || !require(pinyin.takeFirstCandidate() == QStringLiteral("任务"),
                    "offline pinyin resolves renwu to task"))
        return 36;
    for (const QChar letter : QStringLiteral("yali"))
        pinyin.appendLetter(QString(letter));
    if (!require(pinyin.takeFirstCandidate() == QStringLiteral("压力"),
                 "offline pinyin resolves yali to pressure"))
        return 37;

    AssistantRuleEngine assistantRules;
    QVariantMap assistantContext{{QStringLiteral("databaseReady"), true},
                                 {QStringLiteral("page"), QStringLiteral("runner")},
                                 {QStringLiteral("hardwareMode"), false},
                                 {QStringLiteral("safetyLevel"), QStringLiteral("normal")},
                                 {QStringLiteral("tripLatched"), false},
                                 {QStringLiteral("utilizationPercent"), 20.0},
                                 {QStringLiteral("hasTask"), true},
                                 {QStringLiteral("stage"), 1},
                                 {QStringLiteral("completedPoints"), 1},
                                 {QStringLiteral("targetPoints"), 6},
                                 {QStringLiteral("remainingPoints"), 5},
                                 {QStringLiteral("stable"), false},
                                 {QStringLiteral("stabilityP2P"), 0.18},
                                 {QStringLiteral("captureMode"), QStringLiteral("manual-x")},
                                 {QStringLiteral("xVariableName"), QStringLiteral("质量")}};
    if (!require(assistantRules.evaluate(assistantContext)
                     .value(QStringLiteral("id")).toString()
                     == QStringLiteral("reading_unstable"),
                 "assistant task-stage rule"))
        return 32;
    assistantContext.insert(QStringLiteral("hardwareMode"), true);
    assistantContext.insert(QStringLiteral("serialConnected"), true);
    assistantContext.insert(QStringLiteral("dataFresh"), true);
    assistantContext.insert(QStringLiteral("protocolIntegrityAvailable"), true);
    assistantContext.insert(QStringLiteral("valueTrustedForSafety"), false);
    if (!require(assistantRules.evaluate(assistantContext)
                     .value(QStringLiteral("id")).toString()
                     == QStringLiteral("reading_unstable"),
                 "delivery assistant does not show interface-debug calibration gate"))
        return 45;
    assistantContext.insert(QStringLiteral("tripLatched"), true);
    if (!require(assistantRules.evaluate(assistantContext)
                     .value(QStringLiteral("id")).toString()
                     == QStringLiteral("safety_trip"),
                 "assistant safety priority"))
        return 33;

    if (!require(database.appendAssistantHistory(
                     QStringLiteral("test.task"), QStringLiteral("question"),
                     QStringLiteral("task_overview"), QStringLiteral("测试标题"),
                     QStringLiteral("测试回答"), QString(), 1000),
                 "assistant history write")
        || !require(database.loadAssistantHistory(QStringLiteral("test.task"), 5).size() == 1,
                    "assistant history read"))
        return 34;

    LegacyAsciiProtocol legacyProtocol;
    if (!require(legacyProtocol.feed(QByteArrayLiteral("491")).isEmpty(),
                 "legacy protocol retains partial frames"))
        return 23;
    const QList<LegacyAsciiProtocol::Event> protocolEvents = legacyProtocol.feed(
        QByteArrayLiteral("117\r\n9507497\r\nID = 0x14\r\n16777216\r\n"));
    if (!require(protocolEvents.size() == 4, "legacy protocol emits four complete lines")
        || !require(protocolEvents.at(0).type == LegacyAsciiProtocol::EventType::Pressure
                    && protocolEvents.at(0).rawCode == 491117u,
                    "legacy pressure frame classification")
        || !require(protocolEvents.at(1).type == LegacyAsciiProtocol::EventType::Temperature
                    && protocolEvents.at(1).rawCode == 9507497u,
                    "legacy temperature frame classification")
        || !require(protocolEvents.at(2).type == LegacyAsciiProtocol::EventType::Diagnostic,
                    "legacy diagnostic line classification")
        || !require(protocolEvents.at(3).type == LegacyAsciiProtocol::EventType::Invalid,
                    "legacy ADC range validation"))
        return 24;

    PressureProtocolV1 v1Protocol;
    const PressureProtocolV1::Event v1Measurement = v1Protocol.parseLine(
        QByteArrayLiteral("@PS1,M,1523,483920,490980,9514440,00000000*3EC2\r\n"));
    if (!require(v1Measurement.type == PressureProtocolV1::EventType::Measurement,
                 "V1 measurement frame classification")
        || !require(v1Measurement.sequence == 1523u && v1Measurement.uptimeMs == 483920u,
                    "V1 sequence and uptime fields")
        || !require(v1Measurement.pressureRaw == 490980u
                        && v1Measurement.temperatureRaw == 9514440u,
                    "V1 paired ADC fields")
        || !require(v1Measurement.statusFlags == 0u, "V1 status flags")
        || !require(PressureProtocolV1::encodeCommand(43u, QByteArrayLiteral("GET_INFO"))
                        == QByteArrayLiteral("@PS1,C,43,GET_INFO*4B8F\r\n"),
                    "V1 command CRC encoding"))
        return 29;

    QByteArray corruptedV1 = QByteArrayLiteral(
        "@PS1,M,1523,483920,490980,9514440,00000000*3EC2\r\n");
    corruptedV1[15] = corruptedV1.at(15) == '9' ? '8' : '9';
    const PressureProtocolV1::Event badCrc = v1Protocol.parseLine(corruptedV1);
    const PressureProtocolV1::Event v1Info = v1Protocol.parseLine(
        QByteArrayLiteral("@PS1,I,0,125,1.0.0,STM32F103RC,AD7124-8,POR,00000000*27BD\r\n"));
    if (!require(badCrc.type == PressureProtocolV1::EventType::Invalid && badCrc.crcError,
                 "V1 damaged frame rejected by CRC")
        || !require(v1Info.type == PressureProtocolV1::EventType::Info,
                    "V1 INFO frame classification")
        || !require(v1Info.firmwareVersion == QStringLiteral("1.0.0")
                        && v1Info.deviceId == QStringLiteral("STM32F103RC"),
                    "V1 INFO identity fields"))
        return 30;

    SerialDeviceGateway pressureOnlyGateway;
    DeviceSimulator pressureOnlyDevice;
    pressureOnlyDevice.attachSerialGateway(&pressureOnlyGateway);
    SerialDeviceGatewayTestAccess::connectSyntheticTransport(pressureOnlyGateway);
    SerialDeviceGatewayTestAccess::feed(
        pressureOnlyGateway, measurementFrame(1u, 1000u, 490980u, 0u, 0x00000800u));
    QEventLoop pressureOnlyWait;
    QTimer::singleShot(30, &pressureOnlyWait, &QEventLoop::quit);
    pressureOnlyWait.exec();
    SerialDeviceGatewayTestAccess::feed(
        pressureOnlyGateway, measurementFrame(2u, 1025u, 490980u, 0u, 0x00000800u));
    if (!require(pressureOnlyGateway.protocolIntegrityAvailable(),
                 "pressure-only V1 stream locks protocol integrity")
        || !require(pressureOnlyGateway.dataFresh() && pressureOnlyGateway.pressureFresh(),
                    "temperature-disabled frame keeps pressure data fresh")
        || !require(!pressureOnlyGateway.temperatureChannelEnabled()
                        && !pressureOnlyGateway.temperatureFresh(),
                    "temperature-disabled status is explicit")
        || !require(pressureOnlyGateway.temperatureFrames() == 0u,
                    "disabled temperature field is not counted as a sample")
        || !require(pressureOnlyDevice.series().size() == 2,
                    "equal V1 pressure codes remain two distinct timed samples"))
        return 39;

    DeviceSimulator calibratedDevice;
    SerialDeviceGateway calibrationGateway;
    calibratedDevice.attachSerialGateway(&calibrationGateway);
    const bool calibrationInvoked = QMetaObject::invokeMethod(
        &calibratedDevice, "ingestPressureCode", Qt::DirectConnection,
        Q_ARG(quint32, 490980u), Q_ARG(qint64, 1000));
    if (!require(calibrationInvoked, "invoke hardware pressure calibration path")
        || !require(qAbs(calibratedDevice.rawPressureKPa() - 0.07168704466736876) < 1.0e-9,
                    "final 2026-08-24 pressure calibration coefficients"))
        return 28;

    struct CalibrationCheck {
        quint32 rawCode;
        double standardKPa;
    };
    const CalibrationCheck calibrationChecks[] = {
        {59545u, -90.0}, {490702u, 0.0}, {849703u, 75.0},
        {1566447u, 225.0}, {1924222u, 300.0}, {2282151u, 375.0},
        {2638870u, 450.0}, {3352234u, 600.0}
    };
    double maximumCalibrationError = 0.0;
    for (const CalibrationCheck &check : calibrationChecks) {
        const double error = PressureSignalProcessor::calibratedPressure(check.rawCode)
            - check.standardKPa;
        maximumCalibrationError = qMax(maximumCalibrationError, qAbs(error));
    }
    if (!require(QString::fromLatin1(PressureSignalProcessor::calibrationVersion())
                     == QStringLiteral("CAL-Q2-UP-20260824-R1"),
                 "calibration version is traceable")
        || !require(maximumCalibrationError < 0.08,
                    "selected calibration nodes remain below 0.08 kPa")
        || !require(PressureSignalProcessor::calibratedPressure(3352234u)
                        > PressureSignalProcessor::calibratedPressure(2638870u),
                    "calibration remains monotonic"))
        return 40;

    const auto precisionMode = PressureSignalProcessor::FilterMode::PeriodicPrecision;
    constexpr double pi = 3.14159265358979323846;
    constexpr qint64 sampleIntervalMs = 80;
    struct PrecisionResult {
        bool ready = false;
        bool readyAfterStep = false;
        double detectedPeriod = 0.0;
        double mean = 0.0;
        double peakToPeak = 0.0;
        double output = 0.0;
        int sampleCount = 0;
        qint64 nextTimestampMs = 0;
    };
    auto exercisePeriodicFilter = [&](double periodSeconds, double durationSeconds) {
        PressureSignalProcessor filter;
        QVector<double> settled;
        const int sampleCount = qCeil(durationSeconds * 1000.0 / sampleIntervalMs);
        double output = 0.0;
        for (int index = 0; index < sampleCount; ++index) {
            const double seconds = index * sampleIntervalMs / 1000.0;
            const double input = 100.0
                + 0.275 * qSin(2.0 * pi * seconds / periodSeconds);
            output = filter.process(input, index * sampleIntervalMs, precisionMode);
            if (seconds >= durationSeconds - qMin(15.0, periodSeconds * 0.25))
                settled.push_back(output);
        }
        const auto bounds = std::minmax_element(settled.cbegin(), settled.cend());
        PrecisionResult result;
        result.ready = filter.precisionReady();
        result.detectedPeriod = filter.precisionDetectedPeriodSeconds();
        result.mean = std::accumulate(settled.cbegin(), settled.cend(), 0.0)
            / qMax(1, settled.size());
        result.peakToPeak = settled.isEmpty() ? 0.0 : *bounds.second - *bounds.first;
        result.output = output;
        result.sampleCount = sampleCount;
        result.nextTimestampMs = sampleCount * sampleIntervalMs;

        // Keep the long-period instance for the subsequent step-response check.
        if (periodSeconds > 40.0) {
            for (int index = 0; index < 5; ++index) {
                result.output = filter.process(125.0,
                    result.nextTimestampMs + index * sampleIntervalMs, precisionMode);
            }
            result.readyAfterStep = filter.precisionReady();
        }
        return result;
    };

    const PrecisionResult tenSecondFilter = exercisePeriodicFilter(10.0, 45.0);
    const PrecisionResult fiftySecondFilter = exercisePeriodicFilter(50.0, 175.0);
    qInfo() << "precision filter regression"
            << tenSecondFilter.ready << tenSecondFilter.detectedPeriod
            << tenSecondFilter.mean << tenSecondFilter.peakToPeak
            << fiftySecondFilter.ready << fiftySecondFilter.detectedPeriod
            << fiftySecondFilter.mean << fiftySecondFilter.peakToPeak
            << fiftySecondFilter.output;
    if (!require(tenSecondFilter.ready && fiftySecondFilter.ready,
                 "adaptive precision filter reaches ready state")
        || !require(qAbs(tenSecondFilter.detectedPeriod - 10.0) < 1.0,
                    "ten-second interference period is identified")
        || !require(qAbs(fiftySecondFilter.detectedPeriod - 50.0) < 4.0,
                    "fifty-second interference period is identified")
        || !require(qAbs(tenSecondFilter.mean - 100.0) < 0.01
                        && tenSecondFilter.peakToPeak < 0.02,
                    "ten-second oscillation is suppressed without bias")
        || !require(qAbs(fiftySecondFilter.mean - 100.0) < 0.015
                        && fiftySecondFilter.peakToPeak < 0.03,
                    "long-period oscillation is suppressed without bias"))
        return 41;

    // A true pressure step must follow within the five-sample robust-median
    // latency. The old plateau centre is deliberately invalidated after the
    // jump; precision mode is reacquired only from samples on the new plateau.
    const double precisionOutput = fiftySecondFilter.output;
    if (!require(precisionOutput > 124.9,
                 "precision filter rapidly follows a real pressure step")
        || !require(!fiftySecondFilter.readyAfterStep,
                    "pressure step invalidates the old precision centre"))
        return 42;

    PressureSignalProcessor quietFilter;
    double quietOutput = 0.0;
    for (int index = 0; index < 750; ++index) {
        const double seconds = index * sampleIntervalMs / 1000.0;
        const double input = 12.0 + 0.012 * qSin(seconds * 1.35)
            + 0.004 * qSin(seconds * 0.31);
        quietOutput = quietFilter.process(input, index * sampleIntervalMs, precisionMode);
    }
    if (!require(quietFilter.precisionReady(),
                 "quiet signal reaches short-window precision mode")
        || !require(qAbs(quietOutput - 12.0) < 0.02,
                    "quiet short-window filter remains unbiased"))
        return 44;

    DeviceSimulator device;
    QEventLoop sampleWait;
    QTimer::singleShot(180, &sampleWait, &QEventLoop::quit);
    sampleWait.exec();
    if (!require(device.series().size() >= 120, "simulator produces chart series")
        || !require(device.temperature() > 20.0, "simulator temperature is plausible"))
        return 3;

    device.setUnit(QStringLiteral("kPa"));
    device.setFilter(QStringLiteral("IIR · 平衡"));
    if (!require(device.unit() == QStringLiteral("kPa"), "explicit unit selection")
        || !require(device.filterName() == QStringLiteral("IIR · 平衡"), "explicit filter selection")
        || !require(device.formattedPressure().section(QLatin1Char('.'), 1).size() == 3,
                    "kPa display keeps three decimal places"))
        return 4;

    const int beforeSamples = database.storedSampleCount();
    if (!require(database.startSession(QStringLiteral("smoke-test")), "start measurement session"))
        return 5;
    database.appendSample({1, 100.0, 99.9, 99.8, 24.6});
    if (!require(database.flush(), "flush sample transaction"))
        return 6;
    database.stopSession();
    if (!require(database.storedSampleCount() == beforeSamples + 1, "sample count after flush"))
        return 7;
    const QVariantList measurementSessions = database.measurementSessions();
    if (!require(!measurementSessions.isEmpty(), "free measurement session appears in history"))
        return 36;
    const qint64 measurementSessionId = measurementSessions.first().toMap()
        .value(QStringLiteral("sessionId")).toLongLong();
    const QVariantMap measurementDetail = database.measurementSession(measurementSessionId);
    if (!require(measurementDetail.value(QStringLiteral("valid")).toBool(),
                 "free measurement detail query")
        || !require(measurementDetail.value(QStringLiteral("sampleCount")).toInt() == 1,
                    "free measurement detail sample count")
        || !require(measurementDetail.value(QStringLiteral("series")).toList().size() == 1,
                    "free measurement waveform query"))
        return 37;

    TaskManager tasks(&database);
    qInfo() << "task smoke state" << tasks.currentTaskId() << tasks.completedPoints()
            << tasks.taskList().size() << tasks.currentStatus();
    if (!require(tasks.completedPoints() == 4, "example task seed")
        || !require(tasks.hasFit(), "regression available")
        || !require(tasks.rSquared() > 0.999, "regression result"))
        return 8;

    AppController assistantShell;
    AssistantActionDispatcher assistantActions(&assistantShell, &tasks, &device,
                                               nullptr, nullptr, &database);
    tasks.setCurrentStage(0);
    const QVariantMap guidedStart = assistantActions.dispatch(
        QStringLiteral("task_start_measurement"));
    if (!require(guidedStart.value(QStringLiteral("success")).toBool(),
                 "assistant whitelisted task action")
        || !require(tasks.currentStage() == 1
                        && assistantShell.currentPage() == QStringLiteral("runner"),
                    "assistant action updates stage and navigation")
        || !require(!assistantActions.dispatch(QStringLiteral("delete_everything"))
                         .value(QStringLiteral("success")).toBool(),
                    "assistant rejects non-whitelisted action"))
        return 35;

    const QString originalTaskId = tasks.currentTaskId();
    const int initialTaskCount = tasks.taskList().size();
    if (!require(tasks.createTask(QStringLiteral("edu.pressure-mass.linear"),
                                  QStringLiteral("自动回归测试任务")), "create named task")
        || !require(tasks.currentTaskTitle() == QStringLiteral("自动回归测试任务"),
                    "custom task name retained")
        || !require(tasks.completedPoints() == 0, "new task starts empty")
        || !require(tasks.taskList().size() == initialTaskCount + 1, "task list grows"))
        return 9;
    const QString regressionTaskId = tasks.currentTaskId();
    if (!require(!tasks.capturePoint(100.0, 94.3, false), "unstable point rejected")
        || !require(!tasks.capturePoint(100.0, 601.0, true), "out-of-range point rejected")
        || !require(!tasks.finishMeasurement(), "incomplete measurement cannot finish"))
        return 10;

    if (!require(tasks.capturePoint(100.0, 94.32, true), "capture point 1")
        || !require(tasks.capturePoint(200.0, 176.41, true), "capture point 2")
        || !require(tasks.capturePoint(300.0, 258.37, true), "capture point 3")
        || !require(tasks.hasFit(), "fit becomes available at three points"))
        return 11;
    const qint64 deletedPointId = tasks.rows().at(1).toMap().value(QStringLiteral("databaseId")).toLongLong();
    if (!require(tasks.deletePoint(deletedPointId), "delete captured point")
        || !require(tasks.completedPoints() == 2, "point deletion persisted")
        || !require(!tasks.hasFit(), "fit invalidated after point deletion"))
        return 12;

    if (!require(tasks.capturePoint(200.0, 176.41, true), "recapture deleted point")
        || !require(tasks.capturePoint(400.0, 340.51, true), "capture point 4")
        || !require(tasks.capturePoint(500.0, 422.43, true), "capture point 5")
        || !require(tasks.capturePoint(600.0, 504.28, true), "capture point 6")
        || !require(tasks.canFinishMeasurement(), "target point count reached")
        || !require(tasks.finishMeasurement(), "measurement stage completion")
        || !require(tasks.currentStatus() == QStringLiteral("待数据分析"), "analysis status")
        || !require(tasks.confirmAnalysis(), "analysis confirmation")
        || !require(tasks.currentStatus() == QStringLiteral("待导出结果"), "export status"))
        return 13;

    if (!require(tasks.selectTask(originalTaskId), "switch to another task")
        || !require(tasks.selectTask(regressionTaskId), "switch back without losing data")
        || !require(tasks.completedPoints() == 6, "task data restored after switch"))
        return 14;

    const QString bundlePath = tasks.exportBundle();
    const QDir bundle(bundlePath);
    const QStringList bundleFiles = bundle.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (!require(!bundlePath.isEmpty() && bundle.exists(), "export bundle directory")
        || !require(bundleFiles.size() == 3, "export bundle contains three artifacts")
        || !require(tasks.currentStatus() == QStringLiteral("已完成"), "export completes task"))
        return 15;

    if (!require(tasks.createTask(QStringLiteral("engineering.leak"),
                                  QStringLiteral("待删除测试任务")), "create disposable task"))
        return 16;
    const QString disposableId = tasks.currentTaskId();
    if (!require(tasks.deleteTask(disposableId), "delete task and its local data")
        || !require(tasks.taskList().size() == initialTaskCount + 1, "task list shrinks after delete")
        || !require(tasks.hasCurrentTask(), "another task selected after current deletion"))
        return 17;

    if (!require(tasks.createQuickTask(QStringLiteral("现场快速记录"), QString(), QString(),
                                       QStringLiteral("auto-index"), 5),
                 "create quick blank task")
        || !require(tasks.isQuickTask(), "quick task identity")
        || !require(tasks.captureMode() == QStringLiteral("auto-index"),
                    "quick task automatic index mode")
        || !require(tasks.xVariableName() == QStringLiteral("测点序号"),
                    "quick task axis defaults"))
        return 26;
    if (!require(tasks.capturePoint(999.0, 10.0, true), "quick point 1")
        || !require(tasks.capturePoint(999.0, 12.0, true), "quick point 2")
        || !require(tasks.capturePoint(999.0, 14.0, true), "quick point 3")
        || !require(tasks.rows().at(0).toMap().value(QStringLiteral("mass")).toDouble() == 1.0,
                    "automatic x index overrides caller value")
        || !require(tasks.canFinishMeasurement(), "quick task may analyze from three points")
        || !require(tasks.finishMeasurement(), "quick task early analysis transition"))
        return 27;

    if (!require(tasks.createTask(QStringLiteral("quality.zero-drift"),
                                  QStringLiteral("预热零点权限测试")),
                 "create zero-drift task")
        || !require(tasks.allowsEngineeringCapture(),
                    "zero-drift task permits pre-acceptance engineering capture"))
        return 38;

    bool emergencyRequested = false;
    QObject::connect(&device, &DeviceSimulator::emergencyTripRequested, &app,
                     [&](double, const QString &) { emergencyRequested = true; });
    device.setFilter(QStringLiteral("无滤波（原始）"));
    device.setTarget(600.0);
    if (!require(waitUntil([&] { return device.tripLatched(); }, 3000), "98 percent safety trip")
        || !require(emergencyRequested, "hardware trip request emitted")
        || !require(!device.controlOutputEnabled(), "control output permission removed"))
        return 18;
    device.simulateVentToAtmosphere();
    if (!require(waitUntil([&] { return device.utilizationPercent() < 75.0; }, 2500),
                 "vent below reset threshold")
        || !require(device.acknowledgeTrip(), "manual trip reset")
        || !require(device.controlOutputEnabled(), "control output permission restored"))
        return 19;
    // Exercise the formal zero workflow with a deterministic healthy V1 stream.
    // The interference period slowly changes from roughly 45 s to 60 s, matching
    // the thermally stretched waveform observed after a high CPU-load build.
    // Synthetic timestamps exercise automatic extension without wall-clock wait.
    SerialDeviceGateway zeroGateway;
    DeviceSimulator zeroDevice;
    zeroDevice.attachSerialGateway(&zeroGateway);
    SerialDeviceGatewayTestAccess::connectSyntheticTransport(zeroGateway);
    SerialDeviceGatewayTestAccess::feed(
        zeroGateway, measurementFrame(1u, 1000u, 490702u, 0u, 0x00000800u));
    constexpr qint64 zeroIntervalMs = 75;
    constexpr int zeroWarmupSamples = 320;
    const qint64 zeroBaseTimestamp = QDateTime::currentMSecsSinceEpoch() + zeroIntervalMs;
    auto injectZeroSample = [&](int index) {
        const double seconds = index * zeroIntervalMs / 1000.0;
        constexpr double initialFrequencyHz = 1.0 / 45.0;
        constexpr double frequencySlopeHzPerSecond = (1.0 / 60.0 - 1.0 / 45.0) / 210.0;
        const double phase = 2.0 * pi
            * (initialFrequencyHz * seconds
               + 0.5 * frequencySlopeHzPerSecond * seconds * seconds);
        const quint32 code = static_cast<quint32>(490702
            + qRound(1400.0 * qSin(phase) + 0.6 * seconds));
        return QMetaObject::invokeMethod(
            &zeroDevice, "ingestPressureCode", Qt::DirectConnection,
            Q_ARG(quint32, code),
            Q_ARG(qint64, zeroBaseTimestamp + index * zeroIntervalMs));
    };
    for (int index = 0; index < zeroWarmupSamples; ++index) {
        if (!require(injectZeroSample(index), "inject zero warmup sample"))
            return 20;
    }
    const bool zeroReady = zeroDevice.canZero() && zeroDevice.stable();
    qInfo() << "zero readiness" << zeroReady << zeroDevice.pressureKPa()
            << zeroDevice.stabilityP2P() << zeroDevice.sampleRate()
            << zeroDevice.stable();
    bool zeroCompleted = false;
    QString zeroFailure;
    QObject::connect(&zeroDevice, &DeviceSimulator::zeroCalibrationCompleted, &app,
                     [&](double, double) { zeroCompleted = true; });
    QObject::connect(&zeroDevice, &DeviceSimulator::zeroCalibrationFailed, &app,
                     [&](const QString &reason) { zeroFailure = reason; });
    const bool zeroStarted = zeroReady && zeroDevice.zero();
    int lastZeroIndex = zeroWarmupSamples - 1;
    if (zeroStarted) {
        for (int index = zeroWarmupSamples;
             index < zeroWarmupSamples + 2800 && !zeroCompleted && zeroFailure.isEmpty();
             ++index) {
            lastZeroIndex = index;
            if (!require(injectZeroSample(index), "inject zero calibration sample"))
                return 20;
        }
    }
    const bool zeroDecided = zeroCompleted || !zeroFailure.isEmpty();
    qInfo() << "zero decision" << zeroStarted << zeroDecided << zeroCompleted
            << zeroDevice.zeroCalibrationSampleCount()
            << zeroDevice.zeroCalibrationMeanKPa()
            << zeroDevice.zeroCalibrationStdDevKPa()
            << zeroDevice.zeroCalibrationP2PKPa()
            << zeroDevice.zeroCalibrationStandardErrorKPa()
            << zeroDevice.zeroCalibrationSlopeKPaPerSec()
            << zeroDevice.zeroCalibrationSegmentSpreadKPa()
            << zeroDevice.zeroCalibrationCycleCount()
            << zeroDevice.zeroCalibrationDetectedPeriodSeconds()
            << zeroDevice.precisionFilterReady()
            << zeroDevice.precisionFilterPeriodSeconds()
            << zeroFailure;
    if (!require(zeroReady, "zero conditions become valid")
        || !require(zeroStarted, "guided zero correction starts")
        || !require(zeroDecided,
                    "multi-sample zero correction reaches a decision")
        || !require(zeroFailure.isEmpty(),
                    qPrintable(QStringLiteral("multi-sample zero correction accepted: %1")
                                   .arg(zeroFailure)))
        || !require(zeroCompleted,
                    "multi-sample zero correction completes")
        || !require(zeroDevice.zeroCalibrationSampleCount() >= 1500,
                    "zero correction uses an interval of samples")
        || !require(zeroDevice.zeroCalibrationCycleCount() >= 2,
                    "zero correction identifies at least two complete cycles")
        || !require(zeroDevice.zeroCalibrationDetectedPeriodSeconds() >= 35.0
                        && zeroDevice.zeroCalibrationDetectedPeriodSeconds() <= 70.0,
                    "zero correction identifies the thermally stretched period")
        || !require(zeroDevice.zeroCalibrationStandardErrorKPa() < 0.03,
                    "zero correction reports mean uncertainty"))
        return 20;

    // Do not validate a zero correction against the samples that created it.
    // Continue the independent synthetic stream for more than two long cycles
    // and verify both residual centre and filtered stability.
    QVector<double> independentZeroReadings;
    for (int index = lastZeroIndex + 1; index <= lastZeroIndex + 1900; ++index) {
        if (!require(injectZeroSample(index), "inject independent zero verification sample"))
            return 43;
        independentZeroReadings.push_back(zeroDevice.pressureKPa());
    }
    const auto independentBounds = std::minmax_element(independentZeroReadings.cbegin(),
                                                       independentZeroReadings.cend());
    const double independentMean = std::accumulate(independentZeroReadings.cbegin(),
                                                   independentZeroReadings.cend(), 0.0)
        / independentZeroReadings.size();
    qInfo() << "independent zero verification" << independentMean
            << (*independentBounds.second - *independentBounds.first)
            << independentZeroReadings.size()
            << zeroDevice.precisionFilterReady()
            << zeroDevice.precisionFilterPeriodSeconds();
    if (!require(qAbs(independentMean) < 0.02,
                 "independent post-correction zero has no material bias")
        || !require(*independentBounds.second - *independentBounds.first < 0.04,
                    "adaptive filter suppresses the independent zero waveform"))
        return 43;

    TemplateRepository templates(&database);
    QTemporaryFile templateFile(QDir::tempPath() + QStringLiteral("/pressureos-template-XXXXXX.json"));
    if (!templateFile.open())
        return 21;
    templateFile.write(R"JSON({
      "schemaVersion":"1.0","templateId":"test.smoke","name":"测试模板","version":"1.0.0",
      "variables":[{"id":"pressure","unit":"kPa"}],
      "workflow":[{"id":"acquire","type":"acquire"}]
    })JSON");
    templateFile.flush();
    if (!require(templates.importTemplate(QUrl::fromLocalFile(templateFile.fileName())), "template import")
        || !require(templates.templateCount() == 5, "template list update"))
        return 22;

    qInfo() << "PressureOS backend smoke test passed";
    return 0;
}
