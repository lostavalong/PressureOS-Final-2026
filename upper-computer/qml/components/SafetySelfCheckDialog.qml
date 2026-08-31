import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root
    property bool opened: false
    property var storageResult: ({})
    property var checks: []
    property string checkedAt: ""
    visible: opened
    z: 160

    readonly property bool criticalOk: checks.length > 0 && checks.every(function(item) {
        return item.level !== "fail"
    })
    readonly property int limitedCount: checks.filter(function(item) {
        return item.level === "limited"
    }).length

    function runChecks() {
        storageResult = database.runSelfCheck()
        const hardware = device.hardwareMode
        checks = [
            {
                title: "数据库完整性与写入回读",
                detail: storageResult.message || "数据库尚未就绪",
                level: storageResult.ok === true ? "pass" : "fail"
            },
            {
                title: "串口连接与采样时效",
                detail: hardware
                        ? (deviceLink.dataFresh ? deviceLink.portName + " · " + deviceLink.lastFrameAgeMs + " ms" : deviceLink.statusText)
                        : "Demo 数据源运行中；不代表真实硬件链路",
                level: hardware ? (deviceLink.dataFresh ? "pass" : "fail") : "limited"
            },
            {
                title: "压力通道独立超时",
                detail: hardware ? (deviceLink.pressureFresh ? "压力原码持续到达" : "压力通道超过 2.2 s 未更新") : "Demo 通道",
                level: hardware ? (deviceLink.pressureFresh ? "pass" : "fail") : "limited"
            },
            {
                title: "环境温度监测",
                detail: hardware
                        ? (!deviceLink.temperatureChannelEnabled
                           ? "温度通道已停用"
                           : (deviceLink.temperatureFresh
                           ? "温度原码持续到达"
                           : "温度通道超过 2.2 s 未更新"))
                        : "Demo 温度仅用于界面与记录演示",
                level: hardware ? (!deviceLink.temperatureChannelEnabled || deviceLink.temperatureFresh ? "pass" : "limited") : "limited"
            },
            {
                title: "原码范围与异常行筛查",
                detail: hardware ? (deviceLink.invalidFrames + " 行异常数据；ADC 原码按 24 位范围检查") : "模拟值范围检查正常",
                level: hardware && deviceLink.invalidFrames > 0 ? "limited" : "pass"
            },
            {
                title: "协议完整性校验",
                detail: hardware
                        ? (deviceLink.protocolIntegrityAvailable
                           ? "V1 CRC16 已启用；序号丢帧 " + deviceLink.droppedFrames + "，CRC 错误 " + deviceLink.crcErrors
                           : "尚未收到合法 V1 帧；请确认下位机输出 @PS1 帧、CRC16、序号和状态位")
                        : "模拟数据源不提供串口协议完整性证明",
                level: hardware && deviceLink.protocolIntegrityAvailable ? "pass" : "limited"
            },
            {
                title: "量值可信链",
                detail: hardware
                        ? (device.valueTrustedForSafety
                           ? "常温压力标定已验收并显式启用量值安全判断"
                           : "待完成：零压检查、多点升/降程、重复性、回程误差及参数版本/CRC 固化")
                        : "Demo 量值仅用于交互演示",
                level: hardware && device.valueTrustedForSafety ? "pass" : "limited"
            }
        ]
        checkedAt = Qt.formatDateTime(new Date(), "HH:mm:ss")
    }

    function open() {
        opened = true
        runChecks()
        delayedCheck.restart()
    }

    Timer {
        id: delayedCheck
        interval: 2500
        repeat: false
        onTriggered: {
            if (root.opened)
                root.runChecks()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.modalScrim
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
    }

    GlassPanel {
        anchors.centerIn: parent
        width: Math.min(780, parent.width-34)
        // Keep all data-chain checks readable on the 1024x600 panel.
        height: Math.min(500, parent.height-18)
        radius: 27
        material: "modal"
        tintStrength: 0.012

        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; preventStealing: true }

        Item {
            anchors.fill: parent
            anchors.margins: 19

            Rectangle {
                width: 43; height: 43; radius: 14
                color: root.criticalOk ? Theme.greenSoft : "#FFF0F2"
                VectorIcon { anchors.centerIn: parent; width: 21; height: 21; name: root.criticalOk ? "shield" : "warning"; color: root.criticalOk ? Theme.green : Theme.red }
            }
            Text {
                x: 55; y: 0
                text: root.criticalOk ? "基础数据链路可用" : "自检发现阻断项"
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: 18
                font.weight: Font.Bold
            }
            Text {
                x: 55; y: 25
                text: root.criticalOk
                      ? "已完成可自动执行的检查；另有 " + root.limitedCount + " 项需协议或量值确认。"
                      : "请先处理红色项目；橙色项目不能被软件自检替代。"
                color: Theme.inkMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textSmall
            }
            StatusChip {
                anchors.right: closeButton.left; anchors.rightMargin: 9; y: 6
                height: 27
                text: "检查于 " + root.checkedAt
                accent: Theme.blue
                iconName: "clock"
            }
            Rectangle {
                id: closeButton
                anchors.right: parent.right; y: 2
                width: 48; height: 48; radius: 16; color: "#EDF4F9"
                VectorIcon { anchors.centerIn: parent; width: 15; height: 15; name: "close"; color: Theme.inkMuted }
                TapHandler { onTapped: root.opened = false }
            }

            Rectangle {
                x: 0; y: 58; width: parent.width; height: 31; radius: 10; color: "#EDF4F9"
                Row {
                    anchors.fill: parent; anchors.leftMargin: 12
                    Text { width: 210; anchors.verticalCenter: parent.verticalCenter; text: "检查项目"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
                    Text { width: parent.width-300; anchors.verticalCenter: parent.verticalCenter; text: "结果与限制说明"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: "状态"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
                }
            }

            ListView {
                id: checkList
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top; anchors.topMargin: 94
                anchors.bottom: actionRow.top; anchors.bottomMargin: 9
                clip: true
                model: root.checks
                boundsBehavior: Flickable.DragOverBounds
                boundsMovement: Flickable.FollowBoundsBehavior
                flickDeceleration: 2200
                maximumFlickVelocity: 4200
                pressDelay: 80
                delegate: Rectangle {
                    width: checkList.width; height: 31
                    color: index % 2 === 0 ? "#36F4F8FC" : "transparent"
                    Row {
                        anchors.fill: parent; anchors.leftMargin: 10; spacing: 8
                        Rectangle {
                            width: 19; height: 19; radius: 6; anchors.verticalCenter: parent.verticalCenter
                            color: modelData.level === "pass" ? Theme.greenSoft
                                   : modelData.level === "fail" ? "#FFF0F2" : Theme.orangeSoft
                            VectorIcon {
                                anchors.centerIn: parent; width: 10; height: 10
                                name: modelData.level === "pass" ? "check" : (modelData.level === "fail" ? "warning" : "info")
                                color: modelData.level === "pass" ? Theme.green : (modelData.level === "fail" ? Theme.red : Theme.orange)
                            }
                        }
                        Text {
                            width: 183; anchors.verticalCenter: parent.verticalCenter
                            text: modelData.title; color: Theme.ink; font.family: Theme.fontFamily
                            font.pixelSize: Theme.textSmall; font.weight: Font.DemiBold; elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width-306; anchors.verticalCenter: parent.verticalCenter
                            text: modelData.detail; color: Theme.inkMuted; font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMicro; elide: Text.ElideRight
                        }
                        StatusChip {
                            width: 72; height: 23; anchors.verticalCenter: parent.verticalCenter
                            text: modelData.level === "pass" ? "已验证" : (modelData.level === "fail" ? "未通过" : "受限")
                            accent: modelData.level === "pass" ? Theme.green : (modelData.level === "fail" ? Theme.red : Theme.orange)
                            dot: false
                        }
                    }
                }
            }

            Row {
                id: actionRow
                anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 40; spacing: 9
                Text {
                    width: 380; anchors.verticalCenter: parent.verticalCenter
                    text: "结论只覆盖可观测的软件链路，不等同于设备安全认证。"
                    color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro
                    horizontalAlignment: Text.AlignRight; elide: Text.ElideRight
                }
                PremiumButton { width: 108; height: 48; compact: true; text: "重新检查"; iconName: "sync"; variant: "secondary"; onClicked: root.runChecks() }
                PremiumButton { width: 94; height: 48; compact: true; text: "完成"; iconName: "check"; variant: "primary"; onClicked: root.opened = false }
            }
        }
    }
    Component.onCompleted: {
        if (launchSelfCheckDialog)
            open()
    }
}
