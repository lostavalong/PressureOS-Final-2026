import QtQuick 2.15
import PressureOS 1.0

GlassPanel {
    id: root

    property string text: "状态"
    property color accent: Theme.green
    property bool dot: true
    property string iconName: ""

    implicitWidth: row.implicitWidth + 24
    implicitHeight: 34
    radius: 17
    material: "clear"
    tint: accent
    tintStrength: 0.065
    elevated: false

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.accent
        opacity: 0.045
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 7

        Rectangle {
            visible: root.dot && root.iconName === ""
            anchors.verticalCenter: parent.verticalCenter
            width: 7
            height: 7
            radius: 4
            color: root.accent

            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: width / 2
                color: root.accent
                opacity: 0.14
            }
        }

        VectorIcon {
            visible: root.iconName !== ""
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            name: root.iconName
            color: root.accent
            lineWidth: 1.8
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: Qt.darker(root.accent, 1.24)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textSmall
            font.weight: Font.DemiBold
        }
    }
}
