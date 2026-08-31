import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root
    signal openRequested()

    property bool shown: false
    property var recommendation: ({})
    readonly property color accent: recommendation.level === "critical" ? Theme.red
                                      : (recommendation.level === "warning" ? Theme.orange
                                         : (recommendation.level === "success" ? Theme.green
                                            : Theme.blue))

    width: 318
    height: 70
    visible: opacity > 0.01
    opacity: shown ? 1 : 0
    scale: shown ? 1 : 0.96

    GlassPanel {
        anchors.fill: parent
        radius: 20
        material: "modal"
        tint: root.accent
        tintStrength: 0.025

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 5
            radius: 3
            color: root.accent
        }

        Rectangle {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 36
            height: 36
            radius: 12
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.13)
            VectorIcon {
                anchors.centerIn: parent
                width: 18
                height: 18
                name: root.recommendation.icon || "assistant"
                color: root.accent
            }
        }

        Column {
            x: 63
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 104
            spacing: 3
            Text {
                width: parent.width
                text: root.recommendation.title || "Pressure 助手"
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textBody
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: root.recommendation.summary || "点击查看当前建议"
                color: Theme.inkMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMicro
                elide: Text.ElideRight
            }
        }

        Item {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: closeButton.left
            TapHandler { onTapped: root.openRequested() }
        }

        Item {
            id: closeButton
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            VectorIcon {
                anchors.centerIn: parent
                width: 14
                height: 14
                name: "close"
                color: Theme.inkFaint
            }
            TapHandler { onTapped: assistant.dismissNudge() }
        }
    }

    Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
    Behavior on scale { ScaleAnimator { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }
}
