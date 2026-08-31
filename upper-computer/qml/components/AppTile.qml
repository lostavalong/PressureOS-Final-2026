import QtQuick 2.15
import PressureOS 1.0

GlassPanel {
    id: root
    property string title: "应用"
    property string subtitle: ""
    property string iconName: "grid"
    property color accent: Theme.blue
    property bool newBadge: false
    readonly property bool compactTile: height < 118
    signal activated()

    implicitWidth: 180
    implicitHeight: 125
    radius: 19
    interactive: true
    elevated: true
    material: "content"
    tint: root.accent
    accentGlow: root.hovered
    onClicked: activated()

    Rectangle {
        x: 16
        y: root.compactTile ? 12 : 15
        width: root.compactTile ? 40 : 43
        height: width
        radius: root.compactTile ? 13 : 14
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(root.accent, 1.22) }
            GradientStop { position: 1; color: root.accent }
        }
        Rectangle { anchors.fill: parent; anchors.topMargin: 5; z: -1; radius: parent.radius; color: root.accent; opacity: 0.18 }
        VectorIcon { anchors.centerIn: parent; width: root.compactTile ? 20 : 22; height: width; name: root.iconName; color: "white"; lineWidth: 1.8 }
    }
    Rectangle {
        visible: root.newBadge
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 13
        width: 7; height: 7; radius: 4; color: Theme.orange
    }
    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottom: subtitleText.top
        anchors.bottomMargin: 2
        height: 20
        text: root.title
        color: Theme.ink
        font.family: Theme.fontFamily
        font.pixelSize: 14
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    Text {
        id: subtitleText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: root.compactTile ? 8 : 11
        height: 18
        text: root.subtitle
        color: Theme.inkMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textSmall
        verticalAlignment: Text.AlignVCenter
        maximumLineCount: 1
        elide: Text.ElideRight
    }
    VectorIcon {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
        width: 15; height: 15; name: "arrow"; color: root.hovered ? root.accent : Theme.inkFaint
        opacity: root.hovered ? 1 : 0
        x: root.hovered ? parent.width - 29 : parent.width - 36
        Behavior on opacity { NumberAnimation { duration: 160 } }
        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }
}
