import QtQuick 2.15

Item {
    id: root
    property color color: "#6685A4"
    property real lineWidth: Math.max(1.2, width / 12)

    Rectangle {
        x: root.width * 0.29
        y: root.height * 0.34
        width: root.width * 0.42
        height: root.height * 0.50
        radius: root.width * 0.06
        color: "transparent"
        border.width: root.lineWidth
        border.color: root.color
    }
    Rectangle {
        x: root.width * 0.20
        y: root.height * 0.25
        width: root.width * 0.60
        height: root.lineWidth
        radius: height / 2
        color: root.color
    }
    Rectangle {
        x: root.width * 0.39
        y: root.height * 0.13
        width: root.width * 0.22
        height: root.lineWidth
        radius: height / 2
        color: root.color
    }
    Rectangle {
        x: root.width * 0.42
        y: root.height * 0.45
        width: root.lineWidth
        height: root.height * 0.27
        radius: width / 2
        color: root.color
    }
    Rectangle {
        x: root.width * 0.57
        y: root.height * 0.45
        width: root.lineWidth
        height: root.height * 0.27
        radius: width / 2
        color: root.color
    }
}
