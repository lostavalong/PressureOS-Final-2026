#include "AssistantRuleEngine.h"

#include <QtMath>

namespace {
QString pointText(int count)
{
    return QStringLiteral("%1 个有效点").arg(count);
}
}

QVariantMap AssistantRuleEngine::evaluate(const QVariantMap &context) const
{
    const QString page = context.value(QStringLiteral("page")).toString();
    const bool hardware = context.value(QStringLiteral("hardwareMode")).toBool();

    if (!context.value(QStringLiteral("databaseReady")).toBool()) {
        return makeRecommendation(
            QStringLiteral("database_unavailable"), 0, QStringLiteral("critical"),
            QStringLiteral("本地数据服务不可用"),
            QStringLiteral("当前任务无法可靠自动保存，请先停止正式测量。"),
            QStringLiteral("SQLite 数据库未通过初始化，继续操作可能造成记录缺失。"),
            QStringLiteral("open_device"), QStringLiteral("查看系统自检"),
            QStringLiteral("database_recovery"), QStringLiteral("database"));
    }

    if (context.value(QStringLiteral("tripLatched")).toBool()
        || context.value(QStringLiteral("safetyLevel")).toString() == QStringLiteral("trip")) {
        return makeRecommendation(
            QStringLiteral("safety_trip"), 0, QStringLiteral("critical"),
            QStringLiteral("高压风险报警已锁存"),
            context.value(QStringLiteral("safetyMessage")).toString(),
            QStringLiteral("先停止加压并缓慢泄压；压力回到安全区后仍需人工检查和复位。"),
            QStringLiteral("open_measurement"), QStringLiteral("查看安全状态"),
            QStringLiteral("range_safety"), QStringLiteral("warning"));
    }

    const double utilization = context.value(QStringLiteral("utilizationPercent")).toDouble();
    if (utilization >= 98.0) {
        return makeRecommendation(
            QStringLiteral("range_pretrip"), 0, QStringLiteral("critical"),
            QStringLiteral("压力已接近极限边界"),
            QStringLiteral("当前量程使用率 %1%，不要继续加压。")
                .arg(QString::number(utilization, 'f', 1)),
            QStringLiteral("本系统只进行风险报警，不执行自动切断；请人工停止加压并泄压。"),
            QStringLiteral("open_measurement"), QStringLiteral("立即查看"),
            QStringLiteral("range_safety"), QStringLiteral("warning"));
    }

    if (hardware && !context.value(QStringLiteral("serialConnected")).toBool()) {
        return makeRecommendation(
            QStringLiteral("serial_disconnected"), 1, QStringLiteral("warning"),
            QStringLiteral("下位机尚未连接"),
            QStringLiteral("PressureOS 当前没有发现可用串口设备。"),
            QStringLiteral("检查 USB、串口权限和设备端供电，再执行重新连接。"),
            QStringLiteral("reconnect_device"), QStringLiteral("重新检测设备"),
            QStringLiteral("serial_troubleshooting"), QStringLiteral("device"));
    }

    if (hardware && context.value(QStringLiteral("serialConnected")).toBool()
        && !context.value(QStringLiteral("dataFresh")).toBool()) {
        const qint64 age = context.value(QStringLiteral("lastFrameAgeMs")).toLongLong();
        return makeRecommendation(
            QStringLiteral("serial_stale"), 1, QStringLiteral("warning"),
            QStringLiteral("串口已连接，但数据已中断"),
            age >= 0 ? QStringLiteral("最后一个有效帧距今 %1 ms。").arg(age)
                     : QStringLiteral("尚未收到可识别的有效压力帧。"),
            QStringLiteral("正式采集前应恢复连续数据，并确认帧错误计数不再增长。"),
            QStringLiteral("reconnect_device"), QStringLiteral("打开连接诊断"),
            QStringLiteral("serial_troubleshooting"), QStringLiteral("warning"));
    }

    if (utilization >= 90.0) {
        return makeRecommendation(
            QStringLiteral("range_warning"), 1, QStringLiteral("warning"),
            QStringLiteral("已进入量程警告区"),
            QStringLiteral("当前使用率 %1%，请停止升压并准备缓慢泄压。")
                .arg(QString::number(utilization, 'f', 1)),
            QStringLiteral("继续升压会迅速缩小安全余量，并触发高风险报警锁存。"),
            QStringLiteral("open_measurement"), QStringLiteral("查看量程状态"),
            QStringLiteral("range_safety"), QStringLiteral("warning"));
    }

    if (hardware && !context.value(QStringLiteral("protocolIntegrityAvailable")).toBool()
        && (page == QStringLiteral("runner") || page == QStringLiteral("measure"))) {
        return makeRecommendation(
            QStringLiteral("protocol_integrity_missing"), 1, QStringLiteral("warning"),
            QStringLiteral("传输完整性尚未得到证明"),
            QStringLiteral("当前链路尚未锁定为带 CRC16 和帧序号的 V1 协议。"),
            QStringLiteral("V0 裸数字即使能显示，也无法识别传输破损、旧帧重复或序号丢失。"),
            QStringLiteral("open_device"), QStringLiteral("查看协议诊断"),
            QStringLiteral("serial_troubleshooting"), QStringLiteral("device"));
    }

    if (utilization >= 80.0 && page != QStringLiteral("runner")) {
        return makeRecommendation(
            QStringLiteral("range_attention"), 2, QStringLiteral("guide"),
            QStringLiteral("安全余量正在缩小"),
            QStringLiteral("量程使用率已到 %1%，后续操作应减小升压速度。")
                .arg(QString::number(utilization, 'f', 1)),
            QStringLiteral("助手会继续监测变化率、警告区和报警状态。"),
            QStringLiteral("open_measurement"), QStringLiteral("查看实时压力"),
            QStringLiteral("range_safety"), QStringLiteral("shield"));
    }

    if (page == QStringLiteral("runner")) {
        if (!context.value(QStringLiteral("hasTask")).toBool()) {
            return makeRecommendation(
                QStringLiteral("task_missing"), 2, QStringLiteral("guide"),
                QStringLiteral("先创建或选择一个任务"),
                QStringLiteral("任务会保存名称、测点、分析状态和导出记录。"),
                QStringLiteral("命名任务后，即使中途退出或切换任务，数据也会自动恢复。"),
                QStringLiteral("open_tasks"), QStringLiteral("前往任务中心"),
                QStringLiteral("task_create"), QStringLiteral("task"));
        }

        const int stage = context.value(QStringLiteral("stage")).toInt();
        const int completed = context.value(QStringLiteral("completedPoints")).toInt();
        const int target = context.value(QStringLiteral("targetPoints")).toInt();
        const int remaining = context.value(QStringLiteral("remainingPoints")).toInt();
        const QString stageDetail = context.value(QStringLiteral("stageDetail")).toString();

        if (stage == 0) {
            return makeRecommendation(
                QStringLiteral("task_intro"), 2, QStringLiteral("guide"),
                QStringLiteral("先确认任务目标与准备条件"),
                stageDetail.isEmpty() ? QStringLiteral("阅读流程、检查连接并确认安全边界。")
                                      : stageDetail,
                QStringLiteral("准备阶段确认清楚，可以减少归零、采点和变量填写中的返工。"),
                QStringLiteral("task_start_measurement"), QStringLiteral("开始测量阶段"),
                QStringLiteral("task_preparation"), QStringLiteral("check"));
        }

        if (stage == 1) {
            if (!context.value(QStringLiteral("stable")).toBool()) {
                return makeRecommendation(
                    QStringLiteral("reading_unstable"), 2, QStringLiteral("guide"),
                    QStringLiteral("暂不建议采集本点"),
                    QStringLiteral("最近窗口峰峰值为 %1 kPa，读数仍在收敛。")
                        .arg(QString::number(context.value(QStringLiteral("stabilityP2P")).toDouble(),
                                            'f', 3)),
                    QStringLiteral("保持工况不变，避免触碰管路；稳定标识出现后再保存。"),
                    QStringLiteral("open_current_task"), QStringLiteral("继续观察读数"),
                    QStringLiteral("measurement_stability"), QStringLiteral("pulse"));
            }
            if (remaining > 0) {
                QString captureHint = context.value(QStringLiteral("captureMode")).toString()
                        == QStringLiteral("auto-index")
                    ? QStringLiteral("当前读数稳定，可以采集第 %1 点。").arg(completed + 1)
                    : QStringLiteral("填写本点“%1”后，可以保存当前稳定压力。")
                          .arg(context.value(QStringLiteral("xVariableName")).toString());
                if (context.value(QStringLiteral("captureTrustLevel")).toString()
                    == QStringLiteral("engineering")) {
                    captureHint += QStringLiteral(" 本点将明确标记为常温标定验收前的工程记录。");
                }
                return makeRecommendation(
                    QStringLiteral("capture_next_point"), 2, QStringLiteral("guide"),
                    QStringLiteral("可以记录下一个测点"), captureHint,
                    QStringLiteral("采集后会立即写入当前任务并自动更新统计结果。还需 %1 个点达到本模板要求。")
                        .arg(remaining),
                    QStringLiteral("open_current_task"), QStringLiteral("返回采集区"),
                    QStringLiteral("capture_and_autosave"), QStringLiteral("record"));
            }
            return makeRecommendation(
                QStringLiteral("measurement_complete"), 2, QStringLiteral("success"),
                QStringLiteral("测量点数已经满足要求"),
                QStringLiteral("已获得 %1，模板建议目标为 %2 点。")
                    .arg(pointText(completed)).arg(target),
                QStringLiteral("先快速检查是否存在误测点，再进入处理；原始数据不会因分析而丢失。"),
                QStringLiteral("task_finish_measurement"), QStringLiteral("进入数据处理"),
                QStringLiteral("measurement_completion"), QStringLiteral("check"));
        }

        if (stage == 2) {
            if (!context.value(QStringLiteral("hasFit")).toBool()) {
                return makeRecommendation(
                    QStringLiteral("analysis_data_insufficient"), 1, QStringLiteral("warning"),
                    QStringLiteral("有效数据不足以分析"),
                    QStringLiteral("当前只有 %1，至少需要 3 个点建立基础拟合。")
                        .arg(pointText(completed)),
                    QStringLiteral("返回采集阶段补充数据，系统会保留现有测点。"),
                    QStringLiteral("task_review_points"), QStringLiteral("返回补充数据"),
                    QStringLiteral("why_need_points"), QStringLiteral("chart"));
            }
            return makeRecommendation(
                QStringLiteral("processing_ready"), 2, QStringLiteral("guide"),
                QStringLiteral("基础计算已经完成"),
                QStringLiteral("当前模型为 %1，R² = %2。")
                    .arg(context.value(QStringLiteral("equation")).toString(),
                         QString::number(context.value(QStringLiteral("rSquared")).toDouble(), 'f', 5)),
                QStringLiteral("下一步要同时查看残差、异常点和不确定度，不能只依据 R² 下结论。"),
                QStringLiteral("task_begin_analysis"), QStringLiteral("查看结果解释"),
                QStringLiteral("fit_explanation"), QStringLiteral("chart"));
        }

        if (stage == 3) {
            const int outliers = context.value(QStringLiteral("outlierCount")).toInt();
            if (outliers > 0) {
                return makeRecommendation(
                    QStringLiteral("outlier_review"), 1, QStringLiteral("warning"),
                    QStringLiteral("发现需要人工复核的数据点"),
                    context.value(QStringLiteral("outlierSummary")).toString(),
                    QStringLiteral("不要自动删除数据；先检查录入值、工况变化和装置状态，必要时重测。"),
                    QStringLiteral("task_review_points"), QStringLiteral("返回核对数据"),
                    QStringLiteral("residual_outlier"), QStringLiteral("warning"));
            }
            const double r2 = context.value(QStringLiteral("rSquared")).toDouble();
            if (r2 < 0.95) {
                return makeRecommendation(
                    QStringLiteral("model_mismatch"), 1, QStringLiteral("warning"),
                    QStringLiteral("线性模型可能不适合当前数据"),
                    QStringLiteral("当前 R² = %1，数据存在明显非线性或工况差异。")
                        .arg(QString::number(r2, 'f', 5)),
                    QStringLiteral("先查看残差分布并核对模板假设，再决定补测或更换模型。"),
                    QStringLiteral("task_review_points"), QStringLiteral("核对原始数据"),
                    QStringLiteral("fit_explanation"), QStringLiteral("chart"));
            }
            return makeRecommendation(
                QStringLiteral("analysis_review_ready"), 2, QStringLiteral("success"),
                QStringLiteral("结果已具备人工确认条件"),
                QStringLiteral("%1；扩展不确定度为 %2 kPa。")
                    .arg(context.value(QStringLiteral("fitQuality")).toString(),
                         QString::number(context.value(QStringLiteral("expandedUncertainty")).toDouble(),
                                         'f', 4)),
                QStringLiteral("确认前仍应核对实验条件和模板假设；系统不会替你删除异常点。"),
                QStringLiteral("task_confirm_analysis"), QStringLiteral("确认并进入导出"),
                QStringLiteral("analysis_decision"), QStringLiteral("check"));
        }

        if (context.value(QStringLiteral("taskStatus")).toString() == QStringLiteral("已完成")) {
            return makeRecommendation(
                QStringLiteral("task_completed"), 3, QStringLiteral("success"),
                QStringLiteral("任务已经完成并保留在本机"),
                QStringLiteral("你可以再次导出，或返回任务列表继续其他工作。"),
                QStringLiteral("原始测点、分析结果和助手记录仍按任务独立保存。"),
                QStringLiteral("open_tasks"), QStringLiteral("返回任务列表"),
                QStringLiteral("export_contents"), QStringLiteral("check"));
        }
        return makeRecommendation(
            QStringLiteral("export_ready"), 2, QStringLiteral("guide"),
            QStringLiteral("可以生成本次任务数据包"),
            QStringLiteral("将输出 CSV 数据表、JSON 分析摘要和 SVG 矢量图。"),
            QStringLiteral("导出不会删除任务，之后仍可复核或重新生成。"),
            QStringLiteral("task_export_bundle"), QStringLiteral("生成并完成任务"),
            QStringLiteral("export_contents"), QStringLiteral("export"));
    }

    if (page == QStringLiteral("measure")) {
        const bool stable = context.value(QStringLiteral("stable")).toBool();
        return makeRecommendation(
            stable ? QStringLiteral("free_measure_ready") : QStringLiteral("free_measure_unstable"),
            3, stable ? QStringLiteral("success") : QStringLiteral("info"),
            stable ? QStringLiteral("当前读数已经稳定") : QStringLiteral("当前读数仍在变化"),
            stable ? QStringLiteral("若只是临时观察，可以继续使用自由测量。")
                   : QStringLiteral("保持工况不变，等待稳定后再记录或归零。"),
            QStringLiteral("需要可追溯的数据、分析和导出时，建议从任务中心创建任务。"),
            QStringLiteral("open_tasks"), QStringLiteral("创建测量任务"),
            stable ? QStringLiteral("capture_and_autosave")
                   : QStringLiteral("measurement_stability"),
            QStringLiteral("gauge"));
    }

    if (page == QStringLiteral("tasks")) {
        return makeRecommendation(
            QStringLiteral("task_center_help"), 3, QStringLiteral("guide"),
            QStringLiteral("从目标出发选择任务入口"),
            QStringLiteral("有固定实验流程时使用模板；临时现场记录可使用快速空白任务。"),
            QStringLiteral("每次任务都应单独命名，便于多次实验切换和后续检索。"),
            {}, {}, QStringLiteral("task_create"), QStringLiteral("task"));
    }

    if (page == QStringLiteral("templates")) {
        return makeRecommendation(
            QStringLiteral("template_help"), 3, QStringLiteral("info"),
            QStringLiteral("模板定义完整实验流程"),
            QStringLiteral("模板同时规定变量、采集步骤、计算方法、安全说明和导出内容。"),
            QStringLiteral("导入模板前会执行结构检查，但实验方法仍需由使用者审核。"),
            {}, {}, QStringLiteral("template_working"), QStringLiteral("file"));
    }

    if (page == QStringLiteral("device")) {
        const bool healthy = !hardware || context.value(QStringLiteral("dataFresh")).toBool();
        return makeRecommendation(
            healthy ? QStringLiteral("device_healthy") : QStringLiteral("device_attention"),
            healthy ? 3 : 1, healthy ? QStringLiteral("success") : QStringLiteral("warning"),
            healthy ? QStringLiteral("设备链路状态正常") : QStringLiteral("设备链路需要检查"),
            healthy ? QStringLiteral("串口、数据库和当前数据时效均可继续观察。")
                    : context.value(QStringLiteral("serialStatus")).toString(),
            QStringLiteral("正式任务前还应核对设备编号、协议版本和标定状态。"),
            healthy ? QStringLiteral("refresh_connectivity") : QStringLiteral("reconnect_device"),
            healthy ? QStringLiteral("刷新无线状态") : QStringLiteral("重新检测串口"),
            QStringLiteral("serial_troubleshooting"), QStringLiteral("device"));
    }

    return makeRecommendation(
        QStringLiteral("home_start"), 3, QStringLiteral("guide"),
        QStringLiteral("准备开始一次可追溯测量"),
        QStringLiteral("先创建任务，PressureOS 会按步骤保存数据、分析状态和导出结果。"),
        QStringLiteral("若只需临时查看压力，也可以进入实时测量。"),
        QStringLiteral("open_tasks"), QStringLiteral("打开任务中心"),
        QStringLiteral("task_overview"), QStringLiteral("assistant"));
}

QVariantList AssistantRuleEngine::quickQuestions(const QVariantMap &context) const
{
    QVariantList items;
    const QString page = context.value(QStringLiteral("page")).toString();
    if (page == QStringLiteral("runner") && context.value(QStringLiteral("hasTask")).toBool()) {
        const int stage = context.value(QStringLiteral("stage")).toInt();
        items << question(QStringLiteral("task_next_step"), QStringLiteral("我现在下一步该做什么？"),
                          QStringLiteral("arrow"));
        if (stage == 0) {
            items << question(QStringLiteral("task_preparation"), QStringLiteral("开始前要检查哪些内容？"),
                              QStringLiteral("check"))
                  << question(QStringLiteral("zero_calibration"), QStringLiteral("什么时候需要零点校正？"),
                              QStringLiteral("zero"));
        } else if (stage == 1) {
            items << question(QStringLiteral("reading_trust"), QStringLiteral("当前读数可以采集吗？"),
                              QStringLiteral("pulse"))
                  << question(QStringLiteral("capture_and_autosave"), QStringLiteral("采错、退出或切换任务怎么办？"),
                              QStringLiteral("database"));
        } else if (stage == 2) {
            items << question(QStringLiteral("fit_explanation"), QStringLiteral("系统是怎样拟合的？"),
                              QStringLiteral("chart"))
                  << question(QStringLiteral("why_need_points"), QStringLiteral("为什么至少需要三个点？"),
                              QStringLiteral("help"));
        } else if (stage == 3) {
            items << question(QStringLiteral("residual_outlier"), QStringLiteral("异常点和残差怎么看？"),
                              QStringLiteral("warning"))
                  << question(QStringLiteral("uncertainty"), QStringLiteral("不确定度结果怎么理解？"),
                              QStringLiteral("chart"));
        } else {
            items << question(QStringLiteral("export_contents"), QStringLiteral("导出包里包含什么？"),
                              QStringLiteral("export"))
                  << question(QStringLiteral("task_history"), QStringLiteral("完成后还能修改和复核吗？"),
                              QStringLiteral("database"));
        }
        return items;
    }

    if (page == QStringLiteral("measure")) {
        return {question(QStringLiteral("reading_trust"), QStringLiteral("这份读数现在可信吗？"),
                         QStringLiteral("pulse")),
                question(QStringLiteral("zero_calibration"), QStringLiteral("如何正确进行零点校正？"),
                         QStringLiteral("zero")),
                question(QStringLiteral("range_safety"), QStringLiteral("量程风险提示怎样工作？"),
                         QStringLiteral("shield"))};
    }
    if (page == QStringLiteral("device")) {
        return {question(QStringLiteral("serial_troubleshooting"), QStringLiteral("串口没有数据怎么排查？"),
                         QStringLiteral("device")),
                question(QStringLiteral("ambient_calibration"), QStringLiteral("常温标定还要完成什么？"),
                         QStringLiteral("target")),
                question(QStringLiteral("wifi_bluetooth"), QStringLiteral("Wi-Fi 和蓝牙分别做什么？"),
                         QStringLiteral("wifi"))};
    }
    if (page == QStringLiteral("templates")) {
        return {question(QStringLiteral("template_working"), QStringLiteral("任务模板到底定义了什么？"),
                         QStringLiteral("file")),
                question(QStringLiteral("template_import"), QStringLiteral("自定义模板如何导入？"),
                         QStringLiteral("download")),
                question(QStringLiteral("quick_task"), QStringLiteral("来不及做模板时怎么办？"),
                         QStringLiteral("edit"))};
    }
    return {question(QStringLiteral("task_overview"), QStringLiteral("任务模式应该怎么使用？"),
                     QStringLiteral("task")),
            question(QStringLiteral("quick_task"), QStringLiteral("怎样快速开始临时测量？"),
                     QStringLiteral("edit")),
            question(QStringLiteral("reading_trust"), QStringLiteral("怎样判断读数是否可靠？"),
                     QStringLiteral("pulse"))};
}

QVariantMap AssistantRuleEngine::makeRecommendation(
    const QString &id, int priority, const QString &level, const QString &title,
    const QString &summary, const QString &reason, const QString &actionId,
    const QString &actionText, const QString &helpId, const QString &icon)
{
    return QVariantMap{{QStringLiteral("id"), id},
                       {QStringLiteral("priority"), priority},
                       {QStringLiteral("level"), level},
                       {QStringLiteral("title"), title},
                       {QStringLiteral("summary"), summary},
                       {QStringLiteral("reason"), reason},
                       {QStringLiteral("actionId"), actionId},
                       {QStringLiteral("actionText"), actionText},
                       {QStringLiteral("helpId"), helpId},
                       {QStringLiteral("icon"), icon},
                       {QStringLiteral("hasAction"), !actionId.isEmpty()},
                       {QStringLiteral("requiresAttention"), priority <= 1},
                       {QStringLiteral("shouldNudge"), priority <= 2}};
}

QVariantMap AssistantRuleEngine::question(const QString &id, const QString &title,
                                          const QString &icon)
{
    return QVariantMap{{QStringLiteral("id"), id},
                       {QStringLiteral("title"), title},
                       {QStringLiteral("icon"), icon}};
}
