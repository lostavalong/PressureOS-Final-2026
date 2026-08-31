import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root
    property string title: "标题"
    property string subtitle: ""
    implicitHeight: subtitle === "" ? 28 : 42
    Text {
        text: root.title
        color: Theme.ink
        font.family: Theme.fontFamily
        font.pixelSize: 18
        font.weight: Font.Bold
    }
    Text {
        visible: root.subtitle !== ""
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        text: root.subtitle
        color: Theme.inkMuted
        font.family: Theme.fontFamily
        font.pixelSize: 11
    }
}
