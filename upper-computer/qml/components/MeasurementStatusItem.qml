import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root

    property string iconName: "pulse"
    property string label: "状态"
    property string value: "—"
    property color accent: Theme.blue

    implicitWidth: 220
    implicitHeight: 36

    Rectangle {
        id: iconPlate
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        radius: 10
        color: "#F0F6FC"

        // This item is intentionally persistent. Only its bound text/value is
        // updated at the sampling rate, so the Canvas icon is never recreated
        // and cannot flash on every incoming measurement frame.
        VectorIcon {
            anchors.centerIn: parent
            width: 16
            height: 16
            name: root.iconName
            color: root.accent
        }
    }

    Column {
        anchors.left: iconPlate.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Text {
            width: parent.width
            text: root.label
            color: Theme.inkFaint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textMicro
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.value
            color: Theme.ink
            font.family: Theme.numberFont
            font.pixelSize: Theme.textBody
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }
}
