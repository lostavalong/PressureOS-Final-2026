import QtQuick 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0

Item {
    id: root

    property bool opened: false
    property int sessionId: -1
    property var recordData: ({})
    visible: opened
    z: 145

    function reload() {
        recordData = sessionId > 0 ? database.measurementSession(sessionId, 1800) : ({})
    }

    function close() {
        opened = false
    }

    onOpenedChanged: if (opened) reload()
    onSessionIdChanged: if (opened) reload()

    Rectangle {
        anchors.fill: parent
        color: Theme.modalScrim
        TapHandler { }
    }

    GlassPanel {
        anchors.centerIn: parent
        width: Math.min(root.width - 30, 940)
        height: Math.min(root.height - 18, 438)
        radius: 27
        material: "modal"
        tint: Theme.cyan
        tintStrength: 0.012

        Item {
            anchors.fill: parent
            anchors.margins: 20

            Rectangle {
                width: 42
                height: 42
                radius: 13
                gradient: Gradient {
                    GradientStop { position: 0; color: "#55B9EA" }
                    GradientStop { position: 1; color: "#188AC5" }
                }
                VectorIcon { anchors.centerIn: parent; width: 21; height: 21; name: "pulse"; color: "white" }
            }

            Text {
                x: 55
                y: -1
                width: parent.width - 150
                text: root.recordData.name || "自由测量记录"
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Text {
                x: 55
                y: 24
                width: parent.width - 150
                text: (root.recordData.startedText || "等待记录信息")
                      + "  ·  " + (root.recordData.source === "stm32-ad7124" ? "真实下位机" : "模拟数据源")
                      + "  ·  " + (root.recordData.protocol || "未知协议")
                color: Theme.inkMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMicro
                elide: Text.ElideRight
            }

            StatusChip {
                anchors.right: closeButton.left
                anchors.rightMargin: 8
                y: 7
                height: 28
                text: root.recordData.status || "已保存"
                accent: Theme.green
                dot: true
            }
            GlassPanel {
                id: closeButton
                anchors.right: parent.right
                y: 0
                width: 42
                height: 42
                radius: 13
                material: "clear"
                elevated: false
                interactive: true
                accessibleName: "关闭自由测量记录"
                VectorIcon { anchors.centerIn: parent; width: 14; height: 14; name: "close"; color: Theme.inkMuted }
                onClicked: root.close()
            }

            RowLayout {
                id: metrics
                anchors.left: parent.left
                anchors.right: parent.right
                y: 56
                height: 68
                spacing: 7

                MetricCard {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "记录时长"; value: root.recordData.durationText || "—"
                    textValue: true; iconName: "clock"; accent: Theme.blue
                }
                MetricCard {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "采样规模"
                    value: (root.recordData.sampleCount || 0) + " 帧 · "
                           + Number(root.recordData.sampleRate || 0).toFixed(1) + " Hz"
                    textValue: true; iconName: "database"; accent: Theme.cyan
                }
                MetricCard {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "全程均值"; value: Number(root.recordData.meanKPa || 0).toFixed(3)
                    unit: "kPa"; iconName: "target"; accent: Theme.green
                }
                MetricCard {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "标准差"; value: Number(root.recordData.standardDeviationKPa || 0).toFixed(3)
                    unit: "kPa"; iconName: "chart"; accent: Theme.violet
                }
                MetricCard {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    label: "峰峰值"; value: Number(root.recordData.peakToPeakKPa || 0).toFixed(3)
                    unit: "kPa"; iconName: "trend"; accent: Theme.orange
                }
            }

            GlassPanel {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: metrics.bottom
                anchors.topMargin: 9
                anchors.bottom: footer.top
                anchors.bottomMargin: 7
                radius: 18
                material: "dense"
                elevated: false
                tint: Theme.blue
                tintStrength: 0.025

                Item {
                    anchors.fill: parent
                    anchors.margins: 11
                    Text {
                        text: "完整波形趋势"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }
                    Row {
                        anchors.right: parent.right
                        y: 1
                        spacing: 11
                        Row { spacing: 5; Rectangle { width: 13; height: 3; radius: 2; color: Theme.blue; anchors.verticalCenter: parent.verticalCenter } Text { text: "保存示值"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro } }
                        Row { spacing: 5; Rectangle { width: 13; height: 2; radius: 1; color: "#8FBEEA"; anchors.verticalCenter: parent.verticalCenter } Text { text: "原始值"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro } }
                        Text {
                            text: root.recordData.downsampled ? "全程极值保真抽样" : "全部采样点"
                            color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro
                        }
                    }
                    RealtimeChart {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 22
                        anchors.bottom: parent.bottom
                        series: root.recordData.series || []
                        rawSeries: root.recordData.rawSeries || []
                        showRaw: true
                        showTimeLabels: true
                        showSafetyAxis: true
                        maxPoints: 0
                        sampleRateHz: Number(root.recordData.displaySampleRate || 1)
                        resolution: device.resolutionKPa
                        rangeMinimum: device.rangeMinKPa
                        rangeMaximum: device.rangeMaxKPa
                    }
                    Text {
                        visible: !root.recordData.valid || (root.recordData.sampleCount || 0) < 2
                        anchors.centerIn: parent
                        text: "这段记录还没有足够的有效采样点"
                        color: Theme.inkMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textBody
                    }
                }
            }

            Item {
                id: footer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 25
                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7
                    Rectangle { width: 18; height: 18; radius: 6; color: Theme.greenSoft; VectorIcon { anchors.centerIn: parent; width: 10; height: 10; name: "check"; color: Theme.green } }
                    Text {
                        text: "已自动保存到 PressureOS 本机数据仓库；完整统计按全部采样点计算。"
                        color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "范围  " + Number(root.recordData.minimumKPa || 0).toFixed(3)
                          + " ～ " + Number(root.recordData.maximumKPa || 0).toFixed(3) + " kPa"
                    color: Theme.blueDeep; font.family: Theme.numberFont; font.pixelSize: Theme.textMicro; font.weight: Font.DemiBold
                }
            }
        }
    }
}
