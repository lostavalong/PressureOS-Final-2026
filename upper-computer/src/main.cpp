#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

#include "AppController.h"
#include "AssistantActionDispatcher.h"
#include "AssistantContextService.h"
#include "AssistantController.h"
#include "AssistantKnowledgeRepository.h"
#include "ConnectivityService.h"
#include "DatabaseService.h"
#include "DeviceSimulator.h"
#include "PinyinInputEngine.h"
#include "SerialDeviceGateway.h"
#include "TaskManager.h"
#include "TemplateRepository.h"

int main(int argc, char *argv[])
{
    QGuiApplication::setOrganizationName(QStringLiteral("PressureOS Lab"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("pressureos.local"));
    QGuiApplication::setApplicationName(QStringLiteral("PressureOS"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PRESSUREOS_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication application(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PressureOS 智能精密压力测量任务中心 Demo"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption fullscreenOption(QStringList{QStringLiteral("f"), QStringLiteral("fullscreen")},
                                        QStringLiteral("以全屏模式启动"));
    QCommandLineOption windowedOption(QStringLiteral("windowed"),
                                      QStringLiteral("以 7 英寸屏保守适配基准 1024×600 窗口模式启动"));
    QCommandLineOption pageOption(QStringLiteral("page"),
                                  QStringLiteral("以指定页面启动（home/measure/tasks/runner/templates/data/device）"),
                                  QStringLiteral("route"), QStringLiteral("home"));
    QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                        QStringLiteral("渲染完成后保存窗口截图并退出"),
                                        QStringLiteral("file"));
    QCommandLineOption assistantOption(QStringLiteral("assistant"),
                                       QStringLiteral("启动时打开上下文助手"));
    QCommandLineOption assistantArticleOption(QStringLiteral("assistant-article"),
                                              QStringLiteral("启动时打开指定的内置助手知识条目"),
                                              QStringLiteral("article-id"));
    QCommandLineOption taskStageOption(QStringLiteral("task-stage"),
                                       QStringLiteral("以指定任务阶段启动，仅用于演示验收（0～4）"),
                                       QStringLiteral("stage"));
    QCommandLineOption createTaskDialogOption(QStringLiteral("create-task-dialog"),
                                              QStringLiteral("启动时打开新建任务弹窗，仅用于界面验收"));
    QCommandLineOption quickTaskDialogOption(QStringLiteral("quick-task-dialog"),
                                             QStringLiteral("启动时打开快速空白任务弹窗，仅用于界面验收"));
    QCommandLineOption quickTaskPreviewOption(QStringLiteral("quick-task-preview"),
                                              QStringLiteral("创建自动序号快速任务并进入采集页，仅用于界面验收"));
    QCommandLineOption selfCheckDialogOption(QStringLiteral("self-check-dialog"),
                                             QStringLiteral("启动时打开分层自检结果，仅用于界面验收"));
    QCommandLineOption keyboardPreviewOption(QStringLiteral("keyboard-preview"),
                                             QStringLiteral("启动时预览触屏键盘（text/numeric/symbols）"),
                                             QStringLiteral("mode"));
    QCommandLineOption powerDialogOption(QStringLiteral("power-dialog"),
                                         QStringLiteral("启动时打开系统控制面板，仅用于界面验收"));
    QCommandLineOption waveformFocusOption(QStringLiteral("waveform-focus"),
                                           QStringLiteral("启动时打开波形专注模式，仅用于界面验收"));
    QCommandLineOption demoTargetOption(QStringLiteral("demo-target"),
                                        QStringLiteral("设置模拟压力目标，用于验证大幅波动和纵轴缩放"),
                                        QStringLiteral("kPa"));
    QCommandLineOption deviceSourceOption(QStringLiteral("device-source"),
                                          QStringLiteral("测量数据源：simulation、auto 或 serial"),
                                          QStringLiteral("source"),
                                          qEnvironmentVariable("PRESSUREOS_DEVICE_SOURCE",
                                                               QStringLiteral("simulation")));
    QCommandLineOption serialPortOption(QStringLiteral("serial-port"),
                                        QStringLiteral("覆盖自动发现的下位机串口路径"),
                                        QStringLiteral("path"),
                                        qEnvironmentVariable("PRESSUREOS_SERIAL_PORT"));
    QCommandLineOption autoZeroOption(QStringLiteral("auto-zero"),
                                      QStringLiteral("数据稳定后自动执行一次多周期零点统计校正"));
    QCommandLineOption verifyZeroOption(QStringLiteral("verify-zero"),
                                        QStringLiteral("零点校正完成后独立复采至少两个周期并打印验收结果"));
    parser.addOption(fullscreenOption);
    parser.addOption(windowedOption);
    parser.addOption(pageOption);
    parser.addOption(screenshotOption);
    parser.addOption(assistantOption);
    parser.addOption(assistantArticleOption);
    parser.addOption(taskStageOption);
    parser.addOption(createTaskDialogOption);
    parser.addOption(quickTaskDialogOption);
    parser.addOption(quickTaskPreviewOption);
    parser.addOption(selfCheckDialogOption);
    parser.addOption(keyboardPreviewOption);
    parser.addOption(powerDialogOption);
    parser.addOption(waveformFocusOption);
    parser.addOption(demoTargetOption);
    parser.addOption(deviceSourceOption);
    parser.addOption(serialPortOption);
    parser.addOption(autoZeroOption);
    parser.addOption(verifyZeroOption);
    parser.process(application);

    QString deviceSource = parser.value(deviceSourceOption).trimmed().toLower();
    if (deviceSource != QStringLiteral("simulation")
        && deviceSource != QStringLiteral("auto")
        && deviceSource != QStringLiteral("serial")) {
        std::fprintf(stderr, "PressureOS: unknown --device-source '%s'; using simulation.\n",
                     qPrintable(deviceSource));
        deviceSource = QStringLiteral("simulation");
    }
    const bool hardwareMode = deviceSource != QStringLiteral("simulation");

    AppController appController(!hardwareMode);
    PinyinInputEngine pinyinInput;
    if (parser.value(pageOption) != QStringLiteral("home"))
        appController.navigate(parser.value(pageOption));
    if (parser.isSet(assistantOption))
        appController.setAssistantOpen(true);
    DatabaseService database;
    SerialDeviceGateway deviceLink;
    DeviceSimulator device;
    if (hardwareMode) {
        deviceLink.setPortOverride(parser.value(serialPortOption));
        deviceLink.setChannelThreshold(8000000u);
        device.attachSerialGateway(&deviceLink);
    }
    if (parser.isSet(demoTargetOption)) {
        bool targetOk = false;
        const double targetKPa = parser.value(demoTargetOption).toDouble(&targetOk);
        if (!hardwareMode && targetOk)
            device.setTarget(targetKPa);
        else if (!targetOk)
            std::fprintf(stderr, "PressureOS: --demo-target must be a number in kPa.\n");
        else
            std::fprintf(stderr, "PressureOS: --demo-target is ignored for a hardware data source.\n");
    }
    ConnectivityService connectivity;
    TaskManager taskManager(&database);
    taskManager.attachMeasurementSource(&device, &deviceLink);
    if (parser.isSet(quickTaskPreviewOption)) {
        taskManager.createQuickTask(QStringLiteral("快速空白任务 · 现场记录"), QString(), QString(),
                                    QStringLiteral("auto-index"), 5);
        taskManager.setCurrentStage(1);
        appController.navigate(QStringLiteral("runner"));
    }
    if (parser.isSet(taskStageOption)) {
        bool stageOk = false;
        const int stage = parser.value(taskStageOption).toInt(&stageOk);
        if (stageOk && stage >= 0 && stage <= 4)
            taskManager.setCurrentStage(stage);
        else
            std::fprintf(stderr, "PressureOS: --task-stage must be an integer from 0 to 4.\n");
    }
    TemplateRepository templateRepository(&database);
    AssistantContextService assistantContext(&appController, &taskManager, &device,
                                             &deviceLink, &connectivity, &database);
    AssistantKnowledgeRepository assistantKnowledge;
    AssistantActionDispatcher assistantActions(&appController, &taskManager, &device,
                                               &deviceLink, &connectivity, &database);
    AssistantController assistantController(&assistantContext, &assistantKnowledge,
                                            &assistantActions, &database);
    if (parser.isSet(assistantArticleOption)) {
        appController.setAssistantOpen(true);
        assistantController.ask(parser.value(assistantArticleOption));
    }

    // recordingChanged also notifies the UI once per second while a recording is
    // active so that recordSeconds can update.  Only create/close a database
    // session on the actual false -> true / true -> false state transition.
    bool recordingSessionOpen = false;
    QObject::connect(&device, &DeviceSimulator::recordingChanged, &database, [&] {
        if (device.recording() && !recordingSessionOpen) {
            recordingSessionOpen = database.startSession(
                QStringLiteral("自由测量 · %1")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm"))),
                device.hardwareMode() ? QStringLiteral("stm32-ad7124")
                                      : QStringLiteral("device-simulator"),
                device.hardwareMode() ? deviceLink.protocolName()
                                      : QStringLiteral("simulation"),
                device.sampleRate());
        } else if (!device.recording() && recordingSessionOpen) {
            database.stopSession();
            recordingSessionOpen = false;
        }
    });
    bool zeroVerificationActive = false;
    qint64 zeroVerificationStartedAtMs = -1;
    qint64 zeroVerificationTargetMs = 0;
    QVector<double> zeroVerificationReadings;
    qulonglong zeroVerificationStartCrcErrors = 0;
    qulonglong zeroVerificationStartDroppedFrames = 0;
    qulonglong zeroVerificationStartInvalidFrames = 0;
    QObject::connect(&device, &DeviceSimulator::sampleReady, &database,
                     [&](double raw, double filtered, double reported, double temperature, qint64 timestamp) {
        if (zeroVerificationActive) {
            if (zeroVerificationStartedAtMs < 0)
                zeroVerificationStartedAtMs = timestamp;
            if (timestamp >= zeroVerificationStartedAtMs) {
                zeroVerificationReadings.push_back(reported);
                const qint64 elapsedMs = timestamp - zeroVerificationStartedAtMs;
                if (elapsedMs >= zeroVerificationTargetMs
                    && zeroVerificationReadings.size() >= 200) {
                    const double mean = std::accumulate(zeroVerificationReadings.cbegin(),
                                                        zeroVerificationReadings.cend(), 0.0)
                        / static_cast<double>(zeroVerificationReadings.size());
                    double squaredSum = 0.0;
                    for (double value : zeroVerificationReadings) {
                        const double delta = value - mean;
                        squaredSum += delta * delta;
                    }
                    const double standardDeviation = std::sqrt(squaredSum
                        / static_cast<double>(qMax(1, zeroVerificationReadings.size() - 1)));
                    std::vector<double> sorted(zeroVerificationReadings.cbegin(),
                                               zeroVerificationReadings.cend());
                    std::sort(sorted.begin(), sorted.end());
                    const int trim = sorted.size() >= 200
                        ? qMax(1, static_cast<int>(sorted.size() / 100)) : 0;
                    const double trimmedPeakToPeak = sorted.at(sorted.size() - 1 - trim)
                        - sorted.at(trim);
                    const bool protocolClean = deviceLink.crcErrors()
                            == zeroVerificationStartCrcErrors
                        && deviceLink.droppedFrames() == zeroVerificationStartDroppedFrames
                        && deviceLink.invalidFrames() == zeroVerificationStartInvalidFrames;
                    const bool passed = qAbs(mean) <= 0.02
                        && trimmedPeakToPeak <= 0.05
                        && device.sampleRate() >= 10 && protocolClean;
                    const QString verificationDetails = QStringLiteral(
                        "passed=%1;duration_s=%2;samples=%3;mean_kpa=%4;stddev_kpa=%5;"
                        "trimmed_p2p_kpa=%6;sample_rate_hz=%7;protocol_clean=%8")
                        .arg(passed ? 1 : 0)
                        .arg(elapsedMs / 1000.0, 0, 'f', 3)
                        .arg(zeroVerificationReadings.size())
                        .arg(mean, 0, 'f', 9)
                        .arg(standardDeviation, 0, 'f', 9)
                        .arg(trimmedPeakToPeak, 0, 'f', 9)
                        .arg(device.sampleRate())
                        .arg(protocolClean ? 1 : 0);
                    database.appendAuditLog(QStringLiteral("calibration.zero.verify"),
                                            verificationDetails);
                    std::fprintf(stdout, "ZERO_VERIFICATION_RESULT %s\n",
                                 qPrintable(verificationDetails));
                    std::fflush(stdout);
                    appController.showToast(passed
                        ? QStringLiteral("零点独立复采通过：残余均值 %1 kPa").arg(mean, 0, 'f', 4)
                        : QStringLiteral("零点独立复采未通过，请查看验收日志"));
                    zeroVerificationActive = false;
                }
            }
        }
        if (!device.recording())
            return;
        // The legacy database column is named compensated_kpa for schema compatibility;
        // in ambient mode it stores the reported value, which equals filtered_kpa.
        database.appendSample({timestamp, raw, filtered, reported, temperature});
    });
    QObject::connect(&device, &DeviceSimulator::zeroCalibrationCompleted, &database,
                     [&](double correction, double cumulativeOffset) {
        database.appendAuditLog(
            QStringLiteral("calibration.zero"),
            QStringLiteral("method=adaptive-complete-cycle-mean;correction_kpa=%1;"
                           "cumulative_offset_kpa=%2;samples=%3;mean_kpa=%4;stddev_kpa=%5;"
                           "p2p_kpa=%6;standard_error_kpa=%7;slope_kpa_s=%8;"
                           "cycle_center_spread_kpa=%9;complete_cycles=%10;"
                           "detected_period_s=%11;source=%12")
                .arg(correction, 0, 'f', 6)
                .arg(cumulativeOffset, 0, 'f', 6)
                .arg(device.zeroCalibrationSampleCount())
                .arg(device.zeroCalibrationMeanKPa(), 0, 'f', 6)
                .arg(device.zeroCalibrationStdDevKPa(), 0, 'f', 6)
                .arg(device.zeroCalibrationP2PKPa(), 0, 'f', 6)
                .arg(device.zeroCalibrationStandardErrorKPa(), 0, 'f', 6)
                .arg(device.zeroCalibrationSlopeKPaPerSec(), 0, 'f', 6)
                .arg(device.zeroCalibrationSegmentSpreadKPa(), 0, 'f', 6)
                .arg(device.zeroCalibrationCycleCount())
                .arg(device.zeroCalibrationDetectedPeriodSeconds(), 0, 'f', 6)
                .arg(device.sourceName()));
        std::fprintf(stdout,
                     "ZERO_CALIBRATION_RESULT passed=1 samples=%d mean_kpa=%.9f "
                     "offset_kpa=%.9f stddev_kpa=%.9f p2p_kpa=%.9f "
                     "standard_error_kpa=%.9f slope_kpa_s=%.9f segment_spread_kpa=%.9f\n",
                     device.zeroCalibrationSampleCount(), device.zeroCalibrationMeanKPa(),
                     cumulativeOffset, device.zeroCalibrationStdDevKPa(),
                     device.zeroCalibrationP2PKPa(), device.zeroCalibrationStandardErrorKPa(),
                     device.zeroCalibrationSlopeKPaPerSec(),
                     device.zeroCalibrationSegmentSpreadKPa());
        std::fprintf(stdout,
                     "ZERO_CALIBRATION_PERIOD cycles=%d detected_period_s=%.9f\n",
                     device.zeroCalibrationCycleCount(),
                     device.zeroCalibrationDetectedPeriodSeconds());
        std::fflush(stdout);
        if (parser.isSet(verifyZeroOption)) {
            const double detectedPeriod = device.zeroCalibrationDetectedPeriodSeconds();
            zeroVerificationTargetMs = qBound<qint64>(qint64(30000),
                qRound64(qMax(30.0, detectedPeriod * 2.2) * 1000.0),
                qint64(210000));
            zeroVerificationReadings.clear();
            zeroVerificationReadings.reserve(qMax(400,
                device.sampleRate() * static_cast<int>(zeroVerificationTargetMs / 1000 + 5)));
            zeroVerificationStartedAtMs = -1;
            zeroVerificationStartCrcErrors = deviceLink.crcErrors();
            zeroVerificationStartDroppedFrames = deviceLink.droppedFrames();
            zeroVerificationStartInvalidFrames = deviceLink.invalidFrames();
            zeroVerificationActive = true;
            std::fprintf(stdout,
                         "ZERO_VERIFICATION_AUTO started=1 target_s=%.3f detected_period_s=%.6f\n",
                         zeroVerificationTargetMs / 1000.0, detectedPeriod);
            std::fflush(stdout);
        }
    });
    QObject::connect(&device, &DeviceSimulator::zeroCalibrationFailed, &application,
                     [&](const QString &reason) {
        std::fprintf(stdout,
                     "ZERO_CALIBRATION_RESULT passed=0 reason=%s samples=%d mean_kpa=%.9f "
                     "stddev_kpa=%.9f p2p_kpa=%.9f standard_error_kpa=%.9f "
                     "slope_kpa_s=%.9f segment_spread_kpa=%.9f\n",
                     qPrintable(reason), device.zeroCalibrationSampleCount(),
                     device.zeroCalibrationMeanKPa(), device.zeroCalibrationStdDevKPa(),
                     device.zeroCalibrationP2PKPa(), device.zeroCalibrationStandardErrorKPa(),
                     device.zeroCalibrationSlopeKPaPerSec(),
                     device.zeroCalibrationSegmentSpreadKPa());
        std::fflush(stdout);
    });
    QObject::connect(&device, &DeviceSimulator::userMessage, &appController,
                     [&](const QString &message) { appController.showToast(message); });
    QObject::connect(&taskManager, &TaskManager::userMessage, &appController,
                     [&](const QString &message) { appController.showToast(message); });
    QObject::connect(&templateRepository, &TemplateRepository::userMessage, &appController,
                     [&](const QString &message) { appController.showToast(message); });
    if (hardwareMode)
        deviceLink.start();

    QTimer autoZeroTimer;
    int autoZeroPolls = 0;
    if (parser.isSet(autoZeroOption)) {
        autoZeroTimer.setInterval(500);
        QObject::connect(&autoZeroTimer, &QTimer::timeout, &application, [&] {
            ++autoZeroPolls;
            if (device.canZero() && device.startZeroCalibration()) {
                autoZeroTimer.stop();
                std::fprintf(stdout, "ZERO_CALIBRATION_AUTO started=1\n");
                std::fflush(stdout);
            } else if (autoZeroPolls >= 180) {
                autoZeroTimer.stop();
                std::fprintf(stdout, "ZERO_CALIBRATION_AUTO started=0 reason=timeout\n");
                std::fflush(stdout);
            }
        });
        autoZeroTimer.start();
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &application,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            std::fprintf(stderr, "PressureOS QML: %s\n", qPrintable(warning.toString()));
    });
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &appController);
    engine.rootContext()->setContextProperty(QStringLiteral("pinyin"), &pinyinInput);
    engine.rootContext()->setContextProperty(QStringLiteral("device"), &device);
    engine.rootContext()->setContextProperty(QStringLiteral("deviceLink"), &deviceLink);
    engine.rootContext()->setContextProperty(QStringLiteral("connectivity"), &connectivity);
    engine.rootContext()->setContextProperty(QStringLiteral("database"), &database);
    engine.rootContext()->setContextProperty(QStringLiteral("tasks"), &taskManager);
    engine.rootContext()->setContextProperty(QStringLiteral("templateRepo"), &templateRepository);
    engine.rootContext()->setContextProperty(QStringLiteral("assistant"), &assistantController);
    engine.rootContext()->setContextProperty(QStringLiteral("launchCreateTaskDialog"),
                                             parser.isSet(createTaskDialogOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchQuickTaskDialog"),
                                             parser.isSet(quickTaskDialogOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchSelfCheckDialog"),
                                             parser.isSet(selfCheckDialogOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchKeyboardPreviewMode"),
                                             parser.value(keyboardPreviewOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchPowerDialog"),
                                             parser.isSet(powerDialogOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchWaveformFocus"),
                                             parser.isSet(waveformFocusOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchZeroCalibration"),
                                             parser.isSet(autoZeroOption));
    engine.rootContext()->setContextProperty(QStringLiteral("launchFullscreen"),
                                             parser.isSet(fullscreenOption) && !parser.isSet(windowedOption));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, [] {
        std::fprintf(stderr, "PressureOS: main QML object creation failed.\n");
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/PressureOS/qml/Main.qml")));
    if (parser.isSet(screenshotOption)) {
        const QString screenshotFile = QFileInfo(parser.value(screenshotOption)).absoluteFilePath();
        QTimer::singleShot(1800, &application, [&application, &engine, screenshotFile] {
            QQuickWindow *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
            if (!window) {
                std::fprintf(stderr, "PressureOS: screenshot failed because the main window is unavailable.\n");
                QCoreApplication::exit(2);
                return;
            }
            std::fprintf(stdout, "PressureOS screenshot geometry: %dx%d, DPR %.3f\n",
                         window->width(), window->height(), window->devicePixelRatio());
            QDir().mkpath(QFileInfo(screenshotFile).absolutePath());
            if (!window->grabWindow().save(screenshotFile)) {
                std::fprintf(stderr, "PressureOS: could not save screenshot to %s.\n",
                             qPrintable(QDir::toNativeSeparators(screenshotFile)));
                QCoreApplication::exit(3);
                return;
            }
            application.quit();
        });
    }
    return application.exec();
}
