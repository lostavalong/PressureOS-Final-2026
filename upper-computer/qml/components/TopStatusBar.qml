import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root
    height: 46
    property bool powerEnabled: true
    signal powerRequested()

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 32
        anchors.verticalCenter: parent.verticalCenter
        text: app.clockText
        color: Theme.ink
        font.family: Theme.numberFont
        font.pixelSize: 15
        font.weight: Font.Bold
    }
    GlassPanel {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: connectionRow.implicitWidth + 22
        width: implicitWidth
        height: 34
        radius: 17
        material: "clear"
        elevated: false
        tintStrength: 0.06
        tint: device.hardwareMode && !deviceLink.dataFresh ? Theme.orange : Theme.blue
        Row {
            id: connectionRow
            anchors.centerIn: parent
            spacing: 7
            Rectangle {
                width: 7; height: 7; radius: 4; anchors.verticalCenter: parent.verticalCenter
                color: device.hardwareMode
                       ? (deviceLink.dataFresh ? Theme.green : (deviceLink.connected ? Theme.orange : Theme.red))
                       : Theme.green
                Rectangle { anchors.fill: parent; anchors.margins: -3; radius: width/2; color: parent.color; opacity: 0.12 }
            }
            Text {
                text: device.hardwareMode
                      ? (deviceLink.dataFresh ? "下位机实时" : (deviceLink.connected ? "等待数据" : "下位机离线"))
                      : "PX-01 模拟"
                color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; font.weight: Font.DemiBold
            }
            Rectangle { width: 1; height: 12; color: Theme.line; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: device.hardwareMode
                      ? ("USB · " + (device.sampleRate > 0 ? device.sampleRate + " Hz" : deviceLink.protocolName))
                      : "模拟 · 50 Hz"
                color: Theme.inkFaint; font.family: Theme.numberFont; font.pixelSize: Theme.textSmall
            }
        }
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 9
        Item {
            width: 19; height: 19
            VectorIcon {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -1
                width: 16; height: 16; inset: 1.1
                name: "bluetooth"
                color: connectivity.bluetoothEnabled ? Theme.violet : Theme.inkFaint
                opacity: connectivity.bluetoothAvailable ? 1.0 : 0.48
            }
        }
        Item {
            width: 19; height: 19
            VectorIcon {
                anchors.centerIn: parent
                width: 18; height: 18; inset: 0.9
                name: "wifi"
                color: connectivity.wifiConnected ? Theme.blue : Theme.inkFaint
                opacity: connectivity.wifiAvailable ? 1.0 : 0.48
            }
        }
        GlassPanel {
            id: powerButton
            width: 44
            height: 44
            radius: 22
            anchors.verticalCenter: parent.verticalCenter
            material: "clear"
            tint: Theme.blue
            tintStrength: 0.055
            elevated: true
            interactive: true
            accessibleName: "电源"
            opacity: root.powerEnabled ? 1.0 : 0.42

            VectorIcon {
                anchors.centerIn: parent
                width: 18
                height: 18
                inset: 0.8
                name: "power"
                color: powerButton.pressed ? Theme.blueDeep : Theme.inkMuted
                lineWidth: 1.9
            }
            enabled: root.powerEnabled
            onClicked: root.powerRequested()
        }
    }
}
