import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root
    property bool opened: false
    property int step: 0
    property string failureText: ""
    visible: opened
    z: 130

    Rectangle {
        anchors.fill: parent
        color: Theme.modalScrim
        TapHandler { }
    }

    GlassPanel {
        anchors.centerIn: parent
        width: 640
        height: 452
        radius: 28
        material: "modal"
        tint: Theme.blue
        tintStrength: 0.012

        Item {
            anchors.fill: parent
            anchors.margins: 26

            Rectangle {
                width: 46; height: 46; radius: 15
                color: step === 3 ? Theme.greenSoft : Theme.blueSoft
                VectorIcon {
                    anchors.centerIn: parent
                    width: 23; height: 23
                    name: step === 3 ? "check" : "zero"
                    color: step === 3 ? Theme.green : Theme.blue
                }
            }
            Text {
                x: 60; y: 1
                text: step === 3 ? "多周期零点校正已完成" : "零点漂移校正向导"
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            Text {
                x: 60; y: 29
                text: "系统会等待足够的完整周期，再安全更新零点"
                color: Theme.inkMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSmall
            }
            GlassPanel {
                anchors.right: parent.right
                y: 0; width: 44; height: 44; radius: 15
                material: "clear"
                interactive: !device.zeroCalibrationActive
                elevated: false
                opacity: device.zeroCalibrationActive ? 0.35 : 1.0
                accessibleName: "关闭"
                VectorIcon { anchors.centerIn: parent; width: 14; height: 14; name: "close"; color: Theme.inkMuted }
                onClicked: root.opened = false
            }

            Item {
                visible: step === 0
                anchors.left: parent.left; anchors.right: parent.right
                y: 76; height: 292

                Rectangle {
                    width: parent.width; height: 64; radius: 17; color: "#EDF6FF"
                    Rectangle {
                        x: 13; anchors.verticalCenter: parent.verticalCenter
                        width: 38; height: 38; radius: 12; color: Theme.blue
                        VectorIcon { anchors.centerIn: parent; width: 19; height: 19; name: "pulse"; color: "white" }
                    }
                    Text { x: 64; y: 12; text: "不是取一次读数，而是观察一段完整波形"; color: Theme.inkStrong; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text { x: 64; y: 35; text: "通常约 " + device.zeroCalibrationTargetSeconds + " 秒；周期较长时系统会自动继续。"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                }
                Row {
                    y: 78; width: parent.width; spacing: 10
                    Repeater {
                        model: [
                            { number: "01", title: "确认零压", detail: "校验仪置 0 kPa\n保持接口与软管静止" },
                            { number: "02", title: "自动采集", detail: "识别规律振荡\n剔除偶发异常帧" },
                            { number: "03", title: "安全写入", detail: "统计条件通过后\n才更新零点偏移" }
                        ]
                        delegate: Rectangle {
                            width: (parent.width - 20) / 3; height: 122; radius: 17; color: "#F7FAFD"; border.width: 1; border.color: Theme.lineSoft
                            Text { x: 13; y: 11; text: modelData.number; color: Theme.blue; font.family: Theme.numberFont; font.pixelSize: Theme.textSmall; font.weight: Font.Bold }
                            Text { x: 13; y: 36; text: modelData.title; color: Theme.inkStrong; font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Text { x: 13; y: 65; width: parent.width - 26; text: modelData.detail; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; lineHeight: 1.35; wrapMode: Text.WordWrap }
                        }
                    }
                }
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 48; radius: 14; color: "#FFF7EB"
                    VectorIcon { x: 14; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; name: "warning"; color: Theme.orange }
                    Text {
                        x: 45; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 60
                        text: "开始后只需保持零压和静止，系统会自动完成其余判断。"
                        color: "#80501C"; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody
                    }
                }
            }

            Item {
                visible: step === 1
                anchors.left: parent.left; anchors.right: parent.right
                y: 76; height: 292

                Text { text: "开始前条件检查"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 15; font.weight: Font.DemiBold }
                Text { y: 28; text: "确认校验仪为 0 kPa，并保持当前连接不变。"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                Row {
                    y: 64; spacing: 12
                    Rectangle {
                        width: 258; height: 112; radius: 18; color: "#F2F8FC"
                        Text { x: 16; y: 14; text: "当前压力"; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                        Text { x: 16; y: 39; text: device.pressureKPa.toFixed(3); color: Theme.inkStrong; font.family: Theme.numberFont; font.pixelSize: 31; font.weight: Font.DemiBold }
                        Text { anchors.right: parent.right; anchors.rightMargin: 16; y: 61; text: "kPa"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: 11 }
                        Text { x: 16; y: 88; text: "采样 " + device.sampleRate + " Hz"; color: device.sampleRate >= 10 ? Theme.green : Theme.orange; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                    }
                    Rectangle {
                        width: 306; height: 112; radius: 18; color: "#F2F8FC"
                        Column {
                            x: 16; y: 12; spacing: 9
                            Row { spacing: 8; Rectangle { width: 8; height: 8; radius: 4; color: Math.abs(device.pressureKPa) <= 2 ? Theme.green : Theme.orange } Text { text: "零压范围（±2 kPa）"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall } }
                            Row { spacing: 8; Rectangle { width: 8; height: 8; radius: 4; color: device.stable ? Theme.green : Theme.orange } Text { text: "短时状态稳定"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall } }
                            Row { spacing: 8; Rectangle { width: 8; height: 8; radius: 4; color: deviceLink.protocolIntegrityAvailable ? Theme.green : Theme.orange } Text { text: "V1 协议与 CRC 已锁定"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall } }
                            Row { spacing: 8; Rectangle { width: 8; height: 8; radius: 4; color: !device.recording ? Theme.green : Theme.red } Text { text: "未在记录其他任务"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall } }
                        }
                    }
                }
                Rectangle {
                    y: 193; width: parent.width; height: 54; radius: 15
                    color: failureText.length > 0 ? "#FDECEF" : "#EAF4FF"
                    Text {
                        anchors.centerIn: parent; width: parent.width - 30
                        text: failureText.length > 0 ? failureText : "当前偏移记录：" + (device.zeroOffsetKPa >= 0 ? "+" : "") + device.zeroOffsetKPa.toFixed(4) + " kPa"
                        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                        color: failureText.length > 0 ? Theme.red : Theme.blueDeep
                        font.family: Theme.fontFamily; font.pixelSize: Theme.textBody
                    }
                }
                Text {
                    visible: !device.canZero
                    anchors.bottom: parent.bottom
                    text: "所有条件变绿后才能开始正式统计采集。"
                    color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall
                }
            }

            Item {
                visible: step === 2
                anchors.left: parent.left; anchors.right: parent.right
                y: 76; height: 292

                Text { text: "正在采集零点统计样本"; color: Theme.inkStrong; font.family: Theme.fontFamily; font.pixelSize: 16; font.weight: Font.DemiBold }
                Text { anchors.right: parent.right; text: device.zeroCalibrationProgress + "%"; color: Theme.blueDeep; font.family: Theme.numberFont; font.pixelSize: 16; font.weight: Font.DemiBold }
                Rectangle { y: 34; width: parent.width; height: 10; radius: 5; color: "#DDEAF4" }
                Rectangle {
                    y: 34; width: parent.width * device.zeroCalibrationProgress / 100; height: 10; radius: 5
                    color: Theme.blue
                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
                Text { y: 55; width: parent.width; text: device.zeroCalibrationStatus; horizontalAlignment: Text.AlignHCenter; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }

                Grid {
                    y: 88; columns: 2; spacing: 10
                    Repeater {
                        model: [
                            { label: "采集进度", value: device.zeroCalibrationElapsedSeconds + " 秒 · " + device.zeroCalibrationSampleCount + " 帧" },
                            { label: "当前零点估计", value: device.zeroCalibrationMeanKPa.toFixed(4) + " kPa" },
                            { label: "检测周期", value: device.zeroCalibrationDetectedPeriodSeconds > 0 ? device.zeroCalibrationDetectedPeriodSeconds.toFixed(2) + " s" : "识别中" },
                            { label: "完整周期", value: device.zeroCalibrationCycleCount + " 个" }
                        ]
                        delegate: Rectangle {
                            width: 289; height: 72; radius: 15; color: "#F2F8FC"
                            Text { x: 13; y: 10; text: modelData.label; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                            Text { x: 13; y: 32; text: modelData.value; color: Theme.inkStrong; font.family: Theme.numberFont; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }
                    }
                }
            }

            Item {
                visible: step === 3
                anchors.left: parent.left; anchors.right: parent.right
                y: 82; height: 282

                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "统计校正结果已生效"; color: Theme.inkStrong; font.family: Theme.fontFamily; font.pixelSize: 23; font.weight: Font.Bold }
                Text { y: 43; width: parent.width; text: "本次偏移来自完整时间窗口，而不是单次读数；统计量和协议状态已写入审计记录。"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter; y: 88; width: 360; height: 72; radius: 19; color: Theme.greenSoft
                    Text { anchors.centerIn: parent; text: "零点偏移  " + (device.zeroOffsetKPa >= 0 ? "+" : "") + device.zeroOffsetKPa.toFixed(4) + " kPa"; color: Theme.green; font.family: Theme.numberFont; font.pixelSize: 20; font.weight: Font.DemiBold }
                }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter; y: 178; spacing: 12
                    Repeater {
                        model: [
                            device.zeroCalibrationSampleCount + " 个样本",
                            "uA " + device.zeroCalibrationStandardErrorKPa.toFixed(4) + " kPa",
                            device.zeroCalibrationCycleCount + " 周期 · " + device.zeroCalibrationDetectedPeriodSeconds.toFixed(2) + " s"
                        ]
                        delegate: Rectangle { width: 170; height: 44; radius: 14; color: "#F2F8FC"; Text { anchors.centerIn: parent; text: modelData; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall } }
                    }
                }
            }

            Row {
                anchors.right: parent.right; anchors.bottom: parent.bottom; spacing: 10
                PremiumButton {
                    visible: step === 2
                    width: 128; height: 48; compact: true
                    text: "取消采集"; variant: "secondary"
                    onClicked: { device.cancelZeroCalibration(); step = 1 }
                }
                PremiumButton {
                    width: step === 0 ? 184 : 158; height: 48; compact: true
                    text: step === 0 ? "继续" : (step === 1 ? "开始校正" : (step === 2 ? "采集中" : "完成"))
                    iconName: step === 3 ? "check" : "arrow"
                    variant: "primary"
                    enabled: step !== 1 || device.canZero
                    onClicked: {
                        if (step === 0) {
                            step = 1
                        } else if (step === 1) {
                            failureText = ""
                            if (device.startZeroCalibration())
                                step = 2
                        } else if (step === 3) {
                            root.opened = false
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: device
        function onZeroCalibrationChanged() {
            if (device.zeroCalibrationActive && root.opened)
                root.step = 2
        }
        function onZeroCalibrationCompleted(correctionKPa, cumulativeOffsetKPa) {
            root.failureText = ""
            if (root.opened)
                root.step = 3
        }
        function onZeroCalibrationFailed(reason) {
            root.failureText = reason
            if (root.opened)
                root.step = 1
        }
    }

    onOpenedChanged: {
        if (opened) {
            failureText = ""
            step = device.zeroCalibrationActive ? 2 : 0
        }
    }
}
