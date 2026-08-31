import QtQuick 2.15
import QtQuick.Controls 2.15
import PressureOS 1.0

Popup {
    id: root
    signal exitRequested()

    width: Math.min(440, parent ? parent.width - 56 : 440)
    height: 326
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle {
        color: Theme.modalScrim
    }

    background: GlassPanel {
        radius: 28
        material: "modal"
        tint: Theme.blue
        tintStrength: 0.012
    }

    contentItem: Item {
        Column {
            anchors.fill: parent
            anchors.leftMargin: 30
            anchors.rightMargin: 30
            anchors.topMargin: 25
            anchors.bottomMargin: 24
            spacing: 12

            Rectangle {
                width: 58
                height: 58
                radius: 20
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#EAF4FF"
                border.width: 1
                border.color: "#C8E3FA"

                Rectangle {
                    anchors.centerIn: parent
                    width: 42
                    height: 42
                    radius: 15
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#2193FF" }
                        GradientStop { position: 1.0; color: "#0869E8" }
                    }
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 23
                        height: 23
                        inset: 0.8
                        name: "power"
                        color: "white"
                        lineWidth: 1.9
                    }
                }
            }

            Text {
                width: parent.width
                text: "系统控制"
                horizontalAlignment: Text.AlignHCenter
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: 21
                font.weight: Font.Bold
            }

            Text {
                width: parent.width
                text: "退出 PressureOS 并返回树莓派桌面？"
                horizontalAlignment: Text.AlignHCenter
                color: Theme.inkMuted
                font.family: Theme.fontFamily
                font.pixelSize: 14
                font.weight: Font.Medium
            }

            Rectangle {
                width: parent.width
                height: 40
                radius: 13
                color: "#F1F8FE"
                border.width: 1
                border.color: "#DCECF7"
                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    VectorIcon {
                        width: 16
                        height: 16
                        name: "shield"
                        color: Theme.green
                        lineWidth: 1.8
                    }
                    Text {
                        text: "任务数据会保留，树莓派不会关机"
                        color: Theme.inkMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                }
            }

            Item { width: 1; height: 1 }

            Row {
                width: parent.width
                height: 50
                spacing: 12

                PremiumButton {
                    width: (parent.width - parent.spacing) / 2
                    height: parent.height
                    text: "继续使用"
                    variant: "secondary"
                    onClicked: root.close()
                }
                PremiumButton {
                    width: (parent.width - parent.spacing) / 2
                    height: parent.height
                    text: "退出到桌面"
                    iconName: "power"
                    variant: "danger"
                    onClicked: {
                        root.close()
                        root.exitRequested()
                    }
                }
            }
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 170; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 130; easing.type: Easing.InCubic }
    }
}
