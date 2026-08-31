import QtQuick 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root

    readonly property bool compactLayout: width < 1160 || height < 600
    readonly property bool modalActive: selfCheckDialog.opened
    readonly property int pageMargin: compactLayout ? 20 : 32
    readonly property int sectionGap: compactLayout ? 8 : 13
    readonly property int panelTitleSize: compactLayout ? Theme.textLabel : 15

    ColumnLayout {
        enabled: !selfCheckDialog.opened
        anchors.fill: parent
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        anchors.topMargin: 3
        anchors.bottomMargin: 4
        spacing: root.sectionGap

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactLayout ? 60 : 64
            Layout.minimumHeight: Layout.preferredHeight
            Layout.maximumHeight: Layout.preferredHeight
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 260
                spacing: 0

                Text {
                    text: "硬件、无线与数据链路"
                    color: Theme.cyan
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSmall
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "设备与连接"
                    color: Theme.inkStrong
                    font.family: Theme.fontFamily
                    font.pixelSize: root.compactLayout ? 23 : 25
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    text: device.hardwareMode
                          ? "接口 1.0 已接入 STM32 下位机；当前使用 " + deviceLink.protocolName + "。"
                          : "测量链路保持 Demo；Wi-Fi 与蓝牙已读取树莓派真实系统状态。"
                    color: Theme.inkMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textSmall
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            StatusChip {
                Layout.preferredHeight: 34
                text: device.hardwareMode
                      ? (deviceLink.dataFresh ? "实时数据正常"
                                              : (deviceLink.connected ? "串口已连接" : "串口未连接"))
                      : "PX-01 模拟在线"
                accent: device.hardwareMode
                        ? (deviceLink.dataFresh ? Theme.green
                                                : (deviceLink.connected ? Theme.orange : Theme.red))
                        : Theme.green
                iconName: "pulse"
            }
            PremiumButton {
                Layout.preferredWidth: 124
                Layout.preferredHeight: 48
                Layout.minimumWidth: 124
                Layout.minimumHeight: 48
                compact: true
                text: connectivity.refreshing ? "读取中" : "刷新连接"
                iconName: "sync"
                busy: connectivity.refreshing
                enabled: !connectivity.refreshing
                variant: "secondary"
                onClicked: {
                    connectivity.refresh()
                    if (device.hardwareMode)
                        deviceLink.reconnect()
                }
            }
            PremiumButton {
                Layout.preferredWidth: 124
                Layout.preferredHeight: 48
                Layout.minimumWidth: 124
                Layout.minimumHeight: 48
                compact: true
                text: "分层自检"
                iconName: "shield"
                variant: "primary"
                onClicked: selfCheckDialog.open()
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactLayout ? 96 : 142
            Layout.minimumHeight: Layout.preferredHeight
            Layout.maximumHeight: Layout.preferredHeight
            radius: 23

            Item {
                anchors.fill: parent
                anchors.margins: root.compactLayout ? 11 : 18

                Text {
                    text: "当前测量链"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: root.panelTitleSize
                    font.weight: Font.Bold
                }

                Row {
                    id: chainRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: root.compactLayout ? 52 : 72
                    spacing: root.compactLayout ? 4 : 8

                    Repeater {
                        model: [
                            { i: "pulse", n: "压力桥 + PT100", d: "ConST-EDU" },
                            { i: "filter", n: "24 位 ADC", d: "AD7124-8" },
                            { i: "device", n: "下位机", d: "STM32F103" },
                            { i: "usb", n: "设备网关", d: device.transportName },
                            { i: "database", n: "任务与数据", d: "Raspberry Pi · SQLite" }
                        ]

                        delegate: Item {
                            width: (chainRow.width - chainRow.spacing * 4) / 5
                            height: chainRow.height

                            GlassPanel {
                                width: parent.width - (index < 4 ? (root.compactLayout ? 16 : 22) : 0)
                                height: parent.height
                                radius: 14
                                elevated: false
                                panelColor: "#F4FBFFFF"
                                panelColorBottom: "#ECF7FCFF"

                                Row {
                                    anchors.fill: parent
                                    anchors.margins: root.compactLayout ? 8 : 12
                                    spacing: root.compactLayout ? 8 : 10

                                    Rectangle {
                                        width: root.compactLayout ? 30 : 34
                                        height: width
                                        radius: 9
                                        color: "#E8F3FD"
                                        anchors.verticalCenter: parent.verticalCenter

                                        VectorIcon {
                                            anchors.centerIn: parent
                                            width: root.compactLayout ? 16 : 17
                                            height: width
                                            name: modelData.i
                                            color: index === 4 ? Theme.violet : Theme.blue
                                        }
                                    }

                                    Column {
                                        width: Math.max(0, parent.width - parent.spacing
                                                        - (root.compactLayout ? 30 : 34))
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 1

                                        Text {
                                            width: parent.width
                                            text: modelData.n
                                            color: Theme.ink
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.textBody
                                            font.weight: Font.DemiBold
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            width: parent.width
                                            text: modelData.d
                                            color: Theme.inkMuted
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.textMicro
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }

                            VectorIcon {
                                visible: index < 4
                                anchors.right: parent.right
                                anchors.rightMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 12
                                height: 12
                                name: "arrow"
                                color: Theme.inkFaint
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.compactLayout ? 10 : 13

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.compactLayout ? 8 : 12

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compactLayout ? 164 : 224
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    radius: 22

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 12 : 18

                        Row {
                            spacing: 9
                            VectorIcon { width: 19; height: 19; name: "usb"; color: Theme.blue }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "串口与协议诊断"
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: root.panelTitleSize
                                font.weight: Font.Bold
                            }
                        }
                        StatusChip {
                            anchors.right: parent.right
                            height: 25
                            text: device.hardwareMode
                                  ? (deviceLink.dataFresh ? "实时数据正常" : deviceLink.statusText)
                                  : "模拟器 50 Hz"
                            accent: device.hardwareMode
                                    ? (deviceLink.dataFresh ? Theme.green : Theme.orange)
                                    : Theme.orange
                            iconName: "info"
                        }

                        Grid {
                            id: diagGrid
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: root.compactLayout ? 110 : 154
                            columns: 3
                            columnSpacing: root.compactLayout ? 6 : 10
                            rowSpacing: root.compactLayout ? 6 : 10

                            MetricCard {
                                width: (diagGrid.width - diagGrid.columnSpacing * 2) / 3
                                height: root.compactLayout ? 52 : 72
                                label: "接收行"
                                value: device.hardwareMode ? deviceLink.receivedLines.toString() : "128,450"
                                hint: device.hardwareMode ? "串口原始行" : "持续增加"
                                iconName: "pulse"
                                accent: Theme.blue
                            }
                            MetricCard {
                                width: (diagGrid.width - diagGrid.columnSpacing * 2) / 3
                                height: root.compactLayout ? 52 : 72
                                label: device.hardwareMode && !deviceLink.temperatureChannelEnabled
                                       ? "压力帧 / 温度通道" : "压力 / 温度帧"
                                value: device.hardwareMode
                                       ? (deviceLink.temperatureChannelEnabled
                                          ? deviceLink.pressureFrames + " / " + deviceLink.temperatureFrames
                                          : deviceLink.pressureFrames + " / 停用")
                                       : "64,225 / 64,225"
                                hint: device.hardwareMode ? "有效 " + deviceLink.validFrames : "模拟帧"
                                iconName: "shield"
                                accent: Theme.green
                            }
                            MetricCard {
                                width: (diagGrid.width - diagGrid.columnSpacing * 2) / 3
                                height: root.compactLayout ? 52 : 72
                                label: "线速率"
                                value: device.hardwareMode ? deviceLink.wireRateHz.toFixed(1) : "50.0"
                                unit: "Hz"
                                iconName: "clock"
                                accent: Theme.violet
                            }
                            MetricCard {
                                width: (diagGrid.width - diagGrid.columnSpacing * 2) / 3
                                height: root.compactLayout ? 52 : 72
                                label: "异常行"
                                value: device.hardwareMode ? deviceLink.invalidFrames.toString() : "0"
                                iconName: "info"
                                accent: deviceLink.invalidFrames > 0 ? Theme.orange : Theme.green
                            }
                            MetricCard {
                                width: (diagGrid.width - diagGrid.columnSpacing * 2) / 3
                                height: root.compactLayout ? 52 : 72
                                label: "接口协议"
                                value: device.hardwareMode ? deviceLink.protocolName : "Demo 模式"
                                hint: device.hardwareMode ? "接口 1.0" : "仿真链路"
                                textValue: true
                                iconName: "file"
                                accent: Theme.cyan
                            }
                            MetricCard {
                                width: (diagGrid.width - diagGrid.columnSpacing * 2) / 3
                                height: root.compactLayout ? 52 : 72
                                label: "最近原码"
                                value: device.hardwareMode && deviceLink.lastPressureRaw > 0
                                       ? "P " + deviceLink.lastPressureRaw : "—"
                                hint: device.hardwareMode && deviceLink.lastTemperatureRaw > 0
                                      ? "T " + deviceLink.lastTemperatureRaw : ""
                                iconName: "lock"
                                accent: Theme.orange
                            }
                        }
                    }
                }

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 22

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 12 : 18
                        clip: true

                        Text {
                            text: "最近事件"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: root.panelTitleSize
                            font.weight: Font.Bold
                        }
                        Text {
                            anchors.right: parent.right
                            text: "诊断日志可导出"
                            color: Theme.blueDeep
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSmall
                            font.weight: Font.DemiBold
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: root.compactLayout ? 26 : 34
                            spacing: root.compactLayout ? 1 : 4

                            Repeater {
                                model: device.hardwareMode
                                       ? [
                                             { t: "实时", n: deviceLink.dataFresh ? (deviceLink.temperatureChannelEnabled ? "压力与温度原始帧持续到达" : "压力单通道高速帧持续到达") : deviceLink.statusText, c: "#19B987" },
                                             { t: "端口", n: deviceLink.portName !== "" ? deviceLink.portName : "自动发现 CP2102N", c: "#1683FF" },
                                             { t: "协议", n: "115200 / 8N1 / " + deviceLink.protocolName, c: "#7964F4" },
                                             { t: "诊断", n: deviceLink.lastDiagnostic !== "" ? deviceLink.lastDiagnostic : "暂无下位机诊断文本", c: "#20BED5" }
                                         ]
                                       : [
                                             { t: "刚刚", n: "模拟采样帧校验通过", c: "#19B987" },
                                             { t: "14:32", n: "SQLite WAL 检查点完成", c: "#1683FF" },
                                             { t: "14:30", n: "读取 Demo 参数版本", c: "#7964F4" },
                                             { t: "14:28", n: "PX-01 模拟链路建立", c: "#20BED5" }
                                         ]

                                delegate: Row {
                                    width: parent.width
                                    height: root.compactLayout ? 20 : 32
                                    spacing: 9

                                    Rectangle {
                                        width: 52
                                        height: root.compactLayout ? 19 : 24
                                        radius: 7
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: "#1A" + modelData.c.slice(1)
                                        border.width: 1
                                        border.color: "#38" + modelData.c.slice(1)

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.t
                                            color: modelData.c
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.textMicro
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                    Text {
                                        width: Math.max(0, parent.width - 61)
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.n
                                        color: Theme.inkMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textSmall
                                        maximumLineCount: 1
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: root.compactLayout ? 336 : 380
                Layout.minimumWidth: Layout.preferredWidth
                Layout.maximumWidth: Layout.preferredWidth
                Layout.fillHeight: true
                spacing: root.compactLayout ? 8 : 12

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compactLayout ? 100 : 170
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    radius: 22

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 11 : 18

                        Row {
                            spacing: 9
                            VectorIcon { width: 19; height: 19; name: "wifi"; color: Theme.blue }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Wi-Fi"
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: root.panelTitleSize
                                font.weight: Font.Bold
                            }
                        }
                        StatusChip {
                            anchors.right: parent.right
                            height: 25
                            text: connectivity.wifiStatusText
                            accent: connectivity.wifiConnected ? Theme.green
                                                               : (connectivity.wifiAvailable ? Theme.orange : Theme.inkFaint)
                            dot: true
                        }

                        GlassPanel {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: root.compactLayout ? 50 : 72
                            radius: 16
                            elevated: false
                            panelColor: "#F4FBFFFF"
                            panelColorBottom: "#ECF7FCFF"

                            Row {
                                anchors.fill: parent
                                anchors.margins: root.compactLayout ? 8 : 13
                                spacing: root.compactLayout ? 8 : 11

                                Rectangle {
                                    id: wifiIcon
                                    width: root.compactLayout ? 34 : 40
                                    height: width
                                    radius: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.blueSoft

                                    VectorIcon {
                                        anchors.centerIn: parent
                                        width: root.compactLayout ? 18 : 21
                                        height: width
                                        name: "wifi"
                                        color: connectivity.wifiConnected ? Theme.blue : Theme.inkFaint
                                    }
                                }
                                Column {
                                    width: Math.max(0, parent.width - wifiIcon.width
                                                    - (connectivity.wifiConnected ? 54 : 0)
                                                    - parent.spacing * (connectivity.wifiConnected ? 2 : 1))
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2

                                    Text {
                                        width: parent.width
                                        text: connectivity.wifiPrimaryText
                                        color: Theme.ink
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textBody
                                        font.weight: Font.DemiBold
                                        maximumLineCount: 1
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: connectivity.wifiDetailText
                                        color: Theme.inkMuted
                                        font.family: Theme.numberFont
                                        font.pixelSize: Theme.textMicro
                                        maximumLineCount: 1
                                        elide: Text.ElideRight
                                    }
                                }
                                Column {
                                    visible: connectivity.wifiConnected
                                    width: 54
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 0

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: connectivity.wifiSignalPercent >= 0
                                              ? connectivity.wifiSignalPercent + "%" : "—"
                                        color: Theme.blueDeep
                                        font.family: Theme.numberFont
                                        font.pixelSize: Theme.textBody
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "信号"
                                        color: Theme.inkFaint
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textMicro
                                    }
                                }
                            }
                        }
                    }
                }

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compactLayout ? 124 : 170
                    Layout.minimumHeight: Layout.preferredHeight
                    Layout.maximumHeight: Layout.preferredHeight
                    radius: 22

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 11 : 18

                        Row {
                            spacing: 9
                            VectorIcon { width: 19; height: 19; name: "bluetooth"; color: Theme.violet }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "蓝牙与手机"
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: root.panelTitleSize
                                font.weight: Font.Bold
                            }
                        }
                        StatusChip {
                            anchors.right: parent.right
                            height: 25
                            text: connectivity.bluetoothStatusText
                            accent: connectivity.bluetoothEnabled ? Theme.violet
                                                                  : (connectivity.bluetoothAvailable ? Theme.orange : Theme.inkFaint)
                            dot: true
                        }

                        GlassPanel {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            y: root.compactLayout ? 28 : 34
                            height: root.compactLayout ? 47 : 62
                            radius: 16
                            elevated: false

                            Row {
                                anchors.fill: parent
                                anchors.margins: root.compactLayout ? 7 : 10
                                spacing: 8

                                Rectangle {
                                    id: bluetoothIcon
                                    width: root.compactLayout ? 32 : 36
                                    height: width
                                    radius: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "#EEEAFD"

                                    VectorIcon {
                                        anchors.centerIn: parent
                                        width: root.compactLayout ? 17 : 19
                                        height: width
                                        name: "bluetooth"
                                        color: connectivity.bluetoothEnabled ? Theme.violet : Theme.inkFaint
                                    }
                                }
                                Column {
                                    width: Math.max(0, parent.width - bluetoothIcon.width - parent.spacing)
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1

                                    Text {
                                        width: parent.width
                                        text: connectivity.bluetoothPrimaryText
                                        color: Theme.ink
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textBody
                                        font.weight: Font.DemiBold
                                        maximumLineCount: 1
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: connectivity.bluetoothDetailText
                                        color: Theme.inkMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textMicro
                                        maximumLineCount: 1
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 24
                            spacing: 8

                            StatusChip {
                                Layout.preferredHeight: 24
                                text: "已连接 " + connectivity.bluetoothConnectedCount + " 台"
                                accent: connectivity.bluetoothConnectedCount > 0 ? Theme.green : Theme.inkFaint
                                dot: true
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: (connectivity.bluetoothAddress !== "" ? connectivity.bluetoothAddress : "BlueZ")
                                      + " · " + connectivity.lastUpdatedText
                                color: Theme.inkFaint
                                font.family: Theme.numberFont
                                font.pixelSize: Theme.textMicro
                                horizontalAlignment: Text.AlignRight
                                maximumLineCount: 1
                                elide: Text.ElideLeft
                            }
                        }
                    }
                }

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: root.compactLayout ? 68 : 76
                    radius: 21
                    elevated: false
                    panelColor: "#EEF8FFFF"
                    panelColorBottom: "#E8F4FBFF"

                    Row {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 11 : 16
                        spacing: 10

                        Rectangle {
                            width: root.compactLayout ? 36 : 42
                            height: width
                            radius: 12
                            anchors.verticalCenter: parent.verticalCenter
                            color: connectivity.supported ? Theme.greenSoft : Theme.orangeSoft

                            VectorIcon {
                                anchors.centerIn: parent
                                width: root.compactLayout ? 18 : 21
                                height: width
                                name: connectivity.supported ? "shield" : "warning"
                                color: connectivity.supported ? Theme.green : Theme.orange
                            }
                        }
                        Column {
                            width: Math.max(0, parent.width - 46)
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2

                            Text {
                                width: parent.width
                                text: connectivity.supported ? "系统无线状态已接入" : "无线状态服务待检查"
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textBody
                                font.weight: Font.DemiBold
                                maximumLineCount: 1
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: connectivity.statusError !== ""
                                      ? connectivity.statusError
                                      : connectivity.backendName + " · 每 5 秒自动刷新"
                                color: Theme.inkMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textMicro
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                wrapMode: Text.WordWrap
                                lineHeight: 1.2
                            }
                        }
                    }
                }
            }
        }
    }

    SafetySelfCheckDialog {
        id: selfCheckDialog
        anchors.fill: parent
    }
}
