import QtQuick 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root
    readonly property bool compactLayout: height < 460
    readonly property bool sourceReady: device.hardwareMode ? deviceLink.dataFresh : true
    readonly property bool systemReady: sourceReady && database.ready
    readonly property string greeting: {
        const hour = new Date().getHours()
        return hour < 6 ? "夜间测量" : (hour < 12 ? "上午好" : (hour < 18 ? "下午好" : "晚上好"))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compactLayout ? 24 : 32
        anchors.rightMargin: root.compactLayout ? 24 : 32
        anchors.topMargin: 4
        anchors.bottomMargin: 6
        spacing: root.compactLayout ? 6 : 8

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactLayout ? 60 : 70
            Layout.minimumHeight: Layout.preferredHeight
            Layout.maximumHeight: Layout.preferredHeight
            ColumnLayout {
                spacing: 1
                RowLayout {
                    spacing: 8
                    Rectangle { width: 19; height: 19; radius: 6; gradient: Gradient { GradientStop { position: 0; color: "#37A3FF" } GradientStop { position: 1; color: "#0877EF" } } VectorIcon { anchors.centerIn: parent; width: 12; height: 12; name: "gauge"; color: "white" } }
                    Text { text: "PressureOS"; color: Theme.inkMuted; font.family: Theme.numberFont; font.pixelSize: 11; font.weight: Font.DemiBold }
                }
                Text { text: root.greeting + "，操作员"; color: Theme.inkStrong; font.family: Theme.fontFamily; font.pixelSize: root.compactLayout ? 22 : 24; font.weight: Font.Bold }
                Text { Layout.maximumWidth: 560; text: root.systemReady ? "关键测量链路可用，可以开始新的测量任务。" : "部分测量链路尚未就绪，请先查看设备状态。"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody; elide: Text.ElideRight }
            }
            Item { Layout.fillWidth: true }
            GlassPanel {
                Layout.preferredWidth: 150; Layout.preferredHeight: 48; radius: 17; elevated: false; interactive: true; material: "floating"
                Row { anchors.fill: parent; anchors.margins: 8; spacing: 10
                    Rectangle { width: 32; height: 32; radius: 11; gradient: Gradient { GradientStop { position: 0; color: "#5CB1FF" } GradientStop { position: 1; color: "#4E78F0" } } VectorIcon { anchors.centerIn: parent; width: 16; height: 16; name: "assistant"; color: "white" } }
                    Column { anchors.verticalCenter: parent.verticalCenter; spacing: 1; Text { text: "本机操作员"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold } Text { text: "受控操作模式"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro } }
                    VectorIcon { anchors.verticalCenter: parent.verticalCenter; width: 13; height: 13; name: "arrow"; color: Theme.inkFaint }
                }
                onClicked: app.showToast("当前为操作员权限；专家参数已安全收纳")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactLayout ? 205 : 230
            Layout.minimumHeight: Layout.preferredHeight
            Layout.maximumHeight: Layout.preferredHeight
            spacing: root.compactLayout ? 10 : 12

            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 24
                material: "thick"
                tint: Theme.blue
                clip: true
                Item {
                    anchors.fill: parent; anchors.margins: root.compactLayout ? 15 : 18
                    Row {
                        anchors.left: parent.left; anchors.top: parent.top; spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4; color: root.sourceReady ? Theme.green : Theme.orange; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "当前示值"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }
                    }
                    StatusChip {
                        anchors.right: parent.right; anchors.top: parent.top
                        text: device.hardwareMode
                              ? (deviceLink.dataFresh
                                 ? (deviceLink.protocolIntegrityAvailable ? "实时 · CRC 已校验" : "实时 · V0 兼容")
                                 : deviceLink.statusText)
                              : "模拟数据"
                        accent: root.sourceReady ? (device.hardwareMode ? Theme.green : Theme.orange) : Theme.red
                        iconName: root.sourceReady ? "pulse" : "warning"
                    }
                    Row {
                        anchors.left: parent.left; anchors.top: parent.top; anchors.topMargin: 34; spacing: 12
                        Text { text: device.formattedPressure; color: Theme.inkStrong; font.family: Theme.numberFont; font.pixelSize: root.compactLayout ? 54 : 62; font.weight: Font.DemiBold; font.letterSpacing: -1.6 }
                        Text { anchors.baseline: parent.children[0].baseline; text: device.unit; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: root.compactLayout ? 17 : 19; font.weight: Font.DemiBold }
                    }
                    Row {
                        anchors.left: parent.left; anchors.top: parent.top; anchors.topMargin: root.compactLayout ? 94 : 101; spacing: root.compactLayout ? 10 : 13
                        Text { text: device.hardwareMode && !deviceLink.temperatureChannelEnabled
                                     ? "常温高速模式 · 温度通道停用"
                                     : "环境温度  " + device.temperature.toFixed(1) + " °C"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody }
                        Rectangle { width: 1; height: 12; color: Theme.line }
                        Text { text: "波动  ±" + (device.stabilityP2P/2).toFixed(3) + " kPa"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody }
                        Rectangle { width: 1; height: 12; color: Theme.line }
                        Text { text: device.stable ? "稳定窗口通过" : "读数收敛中"; color: device.stable ? Theme.green : Theme.orange; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody }
                    }
                    RealtimeChart { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: root.compactLayout ? 78 : 91; compact: true; series: device.series; rawSeries: device.rawSeries }
                    PremiumButton { anchors.right: parent.right; anchors.bottom: parent.bottom; width: 148; height: 48; compact: true; text: "进入测量中心"; iconName: "arrow"; variant: "secondary"; onClicked: app.navigate("measure") }
                }
            }

            GlassPanel {
                Layout.preferredWidth: 350
                Layout.minimumWidth: 350
                Layout.maximumWidth: 350
                Layout.fillHeight: true
                radius: 24
                material: "content"
                tint: root.systemReady ? Theme.green : Theme.orange
                clip: true
                Item {
                    anchors.fill: parent; anchors.margins: 15
                    Text { text: "系统概览"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.Bold }
                    VectorIcon { anchors.right: parent.right; anchors.top: parent.top; width: 17; height: 17; name: "more"; color: Theme.inkFaint }
                    Row {
                        id: overviewSummary
                        y: 24
                        width: parent.width
                        height: 66
                        spacing: 12
                        Item {
                            id: healthGauge
                            width: 66; height: 66
                            Rectangle { anchors.fill: parent; radius: width/2; color: root.systemReady ? Theme.greenSoft : Theme.orangeSoft; border.width: 1; border.color: root.systemReady ? "#BCEBDD" : "#F5D9B7" }
                            VectorIcon { anchors.horizontalCenter: parent.horizontalCenter; y: 12; width: 26; height: 26; name: root.systemReady ? "check" : "warning"; color: root.systemReady ? Theme.green : Theme.orange; lineWidth: 2 }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; y: 42; text: root.systemReady ? "链路可用" : "待检查"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.DemiBold }
                        }
                        Column {
                            width: overviewSummary.width - healthGauge.width - overviewSummary.spacing
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text { width: parent.width; text: root.systemReady ? "关键模块检查通过" : "存在未就绪模块"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { width: parent.width; text: root.sourceReady ? "测量数据持续到达" : "等待下位机有效数据"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; elide: Text.ElideRight }
                            Text { width: parent.width; text: database.ready ? "●  本地数据库可写" : "●  数据库异常"; color: database.ready ? Theme.green : Theme.red; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; elide: Text.ElideRight }
                        }
                    }
                    Rectangle { y: 96; width: parent.width; height: 1; color: Theme.lineSoft }
                    Column {
                        y: 104; width: parent.width; spacing: 3
                        Repeater {
                            model: [
                                {icon:"device",name:"压力数据源",detail:device.hardwareMode?device.transportName:"内置交互模拟器",value:root.sourceReady?"实时":"等待",color:"#1683FF"},
                                {icon:"thermometer",name:device.hardwareMode&&!deviceLink.temperatureChannelEnabled?"采样模式":"环境温度",detail:device.hardwareMode&&!deviceLink.temperatureChannelEnabled?"压力单通道 · 常温":"PT100 · 三线制",value:device.hardwareMode&&!deviceLink.temperatureChannelEnabled?"高速":device.temperature.toFixed(1)+" °C",color:"#20BED5"},
                                {icon:"database",name:"本地数据",detail:"SQLite · WAL",value:database.ready?"可用":"异常",color:"#7964F4"}
                            ]
                            delegate: Item {
                                width: parent.width; height: 28
                                Rectangle { id: overviewIcon; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; width: 26; height: 26; radius: 8; color: Qt.rgba(Qt.color(modelData.color).r,Qt.color(modelData.color).g,Qt.color(modelData.color).b,.09); VectorIcon { anchors.centerIn: parent; width: 14; height: 14; name: modelData.icon; color: modelData.color } }
                                Text { id: overviewValue; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: (modelData.value==="实时"||modelData.value==="可用")?Theme.green:(modelData.value==="异常"?Theme.red:Theme.inkMuted); font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; font.weight: Font.DemiBold }
                                Column {
                                    anchors.left: overviewIcon.right; anchors.leftMargin: 8
                                    anchors.right: overviewValue.left; anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: -1
                                    Text { width: parent.width; text: modelData.name; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                    Text { width: parent.width; text: modelData.detail; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro; elide: Text.ElideMiddle }
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            Layout.minimumHeight: 24
            Layout.maximumHeight: 24
            Text { text: "应用"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 17; font.weight: Font.Bold }
            Text { text: "围绕测量任务组织工具与数据"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
            Item { Layout.fillWidth: true }
            Text { text: "全部应用  ›"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.compactLayout ? 104 : 120
            spacing: 12
            AppTile { Layout.fillWidth: true; Layout.fillHeight: true; title: "实时测量"; subtitle: "自由测量、安全监护与记录"; iconName: "gauge"; accent: "#1683FF"; onActivated: app.navigate("measure") }
            AppTile { Layout.fillWidth: true; Layout.fillHeight: true; title: "任务工作台"; subtitle: "模板→测量→分析→导出"; iconName: "task"; accent: "#20BED5"; newBadge: true; onActivated: app.navigate("tasks") }
            AppTile { Layout.fillWidth: true; Layout.fillHeight: true; title: "安全与校正"; subtitle: "量程风险提示和零点漂移向导"; iconName: "shield"; accent: "#7964F4"; onActivated: { app.navigate("measure"); app.showToast("安全监测与零点向导已收拢到实时测量页") } }
            AppTile { Layout.fillWidth: true; Layout.fillHeight: true; title: "设备与连接"; subtitle: "硬件、Wi-Fi、蓝牙与诊断"; iconName: "device"; accent: "#5B8CB8"; onActivated: app.navigate("device") }
        }
    }
}
