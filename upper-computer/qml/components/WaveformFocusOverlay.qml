import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root

    property bool opened: false
    property bool showRaw: true
    property int windowSeconds: 30
    readonly property real effectiveRate: device.sampleRate > 0
                                               ? device.sampleRate
                                               : (device.hardwareMode ? 10 : 50)
    readonly property int selectedPoints: windowSeconds <= 0
                                               ? 0
                                               : Math.max(2, Math.round(effectiveRate * windowSeconds))
    readonly property color safetyAccent: device.safetyLevel === "trip" ? Theme.red
                                                   : (device.safetyLevel === "warning" ? "#E66F49"
                                                   : (device.safetyLevel === "caution" ? "#D9952B"
                                                                                       : Theme.green))

    signal closeRequested()

    visible: opened || opacity > 0.01
    opacity: opened ? 1 : 0
    scale: opened ? 1 : 0.985
    focus: opened

    Behavior on opacity {
        NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
    }

    Keys.onEscapePressed: closeRequested()

    Rectangle {
        anchors.fill: parent
        color: "#F1F8FD"
    }

    AmbientBackdrop { anchors.fill: parent }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        preventStealing: true
    }

    Row {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 15
        height: 62
        spacing: 13

        GlassPanel {
            width: 50
            height: 50
            anchors.verticalCenter: parent.verticalCenter
            radius: 16
            material: "clear"
            elevated: true
            interactive: true
            accessibleName: "退出波形专注模式"
            onClicked: root.closeRequested()

            VectorIcon {
                anchors.centerIn: parent
                width: 18
                height: 18
                name: "back"
                color: Theme.inkMuted
            }
        }

        Column {
            width: 190
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Text {
                text: "波形专注模式"
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Text {
                text: "轻触返回即可恢复完整操作页"
                color: Theme.inkFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMicro
            }
        }

        Rectangle {
            width: 1
            height: 36
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.line
        }

        Row {
            width: 275
            height: parent.height
            spacing: 9
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: device.formattedPressure
                color: Theme.inkStrong
                font.family: Theme.numberFont
                font.pixelSize: text.length > 8 ? 42 : 50
                font.weight: Font.DemiBold
                font.letterSpacing: -1.1
            }
            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 11
                text: device.unit
                color: Theme.inkMuted
                font.family: Theme.numberFont
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
        }

        Item { width: Math.max(0, root.width - 22 * 2 - 50 - 190 - 1 - 275 - 13 * 5 - statusArea.width) }

        Row {
            id: statusArea
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            StatusChip {
                height: 32
                text: Math.round(root.effectiveRate) + " Hz"
                accent: device.sampleRate >= 10 || !device.hardwareMode ? Theme.green : Theme.orange
                iconName: "pulse"
            }
            StatusChip {
                height: 32
                text: device.stable ? "示值稳定" : "波动跟踪中"
                accent: device.stable ? Theme.green : Theme.orange
                iconName: device.stable ? "check" : "trend"
            }
        }
    }

    GlassPanel {
        id: chartPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        anchors.topMargin: 5
        anchors.bottomMargin: 18
        radius: 26
        material: "dense"
        tint: Theme.blue
        tintStrength: 0.035

        Item {
            anchors.fill: parent
            anchors.margins: 17

            Row {
                id: chartToolbar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 42
                spacing: 10

                Column {
                    width: 245
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0
                    Text {
                        text: "实时压力波形"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                    Text {
                        text: "纵轴自适应 · 峰值不裁剪 · 安全区间着色"
                        color: Theme.inkFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textMicro
                    }
                }

                StatusChip {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 220
                    height: 28
                    text: "纵轴 " + focusChart.displayedMinimum.toFixed(Math.abs(focusChart.displayedMaximum-focusChart.displayedMinimum) < 10 ? 2 : 1)
                          + " ～ " + focusChart.displayedMaximum.toFixed(Math.abs(focusChart.displayedMaximum-focusChart.displayedMinimum) < 10 ? 2 : 1) + " kPa"
                    accent: root.safetyAccent
                    iconName: "gauge"
                }

                Item { width: Math.max(0, chartToolbar.width - 245 - 220 - windowSelector.width - rawToggle.width - 40) }

                Row {
                    id: windowSelector
                    anchors.verticalCenter: parent.verticalCenter
                    height: 34
                    spacing: 5

                    Repeater {
                        model: [15, 30, 0]
                        delegate: Rectangle {
                            required property int modelData
                            width: modelData === 0 ? 58 : 49
                            height: 34
                            radius: 11
                            color: root.windowSeconds === modelData ? Theme.blue : "#EAF2F8"
                            border.width: 1
                            border.color: root.windowSeconds === modelData ? Theme.blue : Theme.line

                            Text {
                                anchors.centerIn: parent
                                text: modelData === 0 ? "全部" : modelData + "秒"
                                color: root.windowSeconds === modelData ? "white" : Theme.inkMuted
                                font.family: Theme.numberFont
                                font.pixelSize: Theme.textMicro
                                font.weight: Font.DemiBold
                            }
                            TapHandler {
                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                onTapped: root.windowSeconds = modelData
                            }
                        }
                    }
                }

                Rectangle {
                    id: rawToggle
                    anchors.verticalCenter: parent.verticalCenter
                    width: 112
                    height: 34
                    radius: 11
                    color: root.showRaw ? Theme.blueSoft : "#EAF2F8"
                    border.width: 1
                    border.color: root.showRaw ? "#9FCFFF" : Theme.line

                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 13
                            height: 3
                            radius: 1.5
                            color: root.showRaw ? "#8FBEEA" : Theme.inkFaint
                        }
                        Text {
                            text: root.showRaw ? "原始+滤波" : "仅滤波"
                            color: Theme.inkMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMicro
                            font.weight: Font.DemiBold
                        }
                    }
                    TapHandler {
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: root.showRaw = !root.showRaw
                    }
                }
            }

            RealtimeChart {
                id: focusChart
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: chartToolbar.bottom
                anchors.bottom: legend.top
                anchors.topMargin: 6
                anchors.bottomMargin: 5
                series: device.series
                rawSeries: device.rawSeries
                showRaw: root.showRaw
                showTimeLabels: true
                showSafetyAxis: true
                maxPoints: root.selectedPoints
                sampleRateHz: root.effectiveRate
                resolution: device.resolutionKPa
                rangeMinimum: device.rangeMinKPa
                rangeMaximum: device.rangeMaxKPa
            }

            Row {
                id: legend
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                height: 20
                spacing: 17

                Row {
                    spacing: 6
                    Rectangle { width: 16; height: 3; radius: 1.5; color: Theme.blue; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "滤波示值"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
                }
                Row {
                    visible: root.showRaw
                    spacing: 6
                    Rectangle { width: 16; height: 2; radius: 1; color: "#8FBEEA"; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "原始示值"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
                }
                Text {
                    text: "纵轴刻度：绿色正常 · 黄色提醒 · 橙色警告 · 红色高风险"
                    color: Theme.inkFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textMicro
                }
            }

            Text {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: "当前峰峰值 " + device.stabilityP2P.toFixed(3) + " kPa"
                color: Theme.inkMuted
                font.family: Theme.numberFont
                font.pixelSize: Theme.textMicro
                font.weight: Font.DemiBold
            }
        }
    }
}
