import QtQuick 2.15
import PressureOS 1.0

GlassPanel {
    id: root

    property string currentPage: "home"
    signal navigate(string page)
    signal assistantRequested()

    width: 394
    height: 66
    radius: 29
    material: "clear"
    tint: Theme.blue
    tintStrength: 0.072
    elevated: true
    accentGlow: true

    readonly property var navItems: [
        { page: "home", icon: "home", label: "桌面" },
        { page: "measure", icon: "gauge", label: "测量" },
        { page: "tasks", icon: "task", label: "任务" },
        { page: "device", icon: "device", label: "设备" }
    ]

    Row {
        anchors.centerIn: parent
        spacing: 6

        Repeater {
            model: root.navItems

            delegate: Item {
                id: navItem

                width: 59
                height: 54
                scale: navTap.pressed ? 0.95 : 1.0

                readonly property bool selected: root.currentPage === modelData.page
                                                 || (modelData.page === "tasks"
                                                     && root.currentPage === "runner")

                Accessible.role: Accessible.Button
                Accessible.name: modelData.label

                Rectangle {
                    id: selectionGlass
                    anchors.centerIn: parent
                    width: 52
                    height: 48
                    radius: 16
                    color: navItem.selected ? "#780A84FF"
                                            : (navHover.hovered ? "#28FFFFFF" : "#00FFFFFF")
                    border.width: navItem.selected || navHover.hovered ? 1 : 0
                    border.color: navItem.selected ? "#82D7EEFF" : "#58FFFFFF"
                    gradient: navItem.selected ? selectedGradient : null

                    Rectangle {
                        visible: navItem.selected
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 13
                        anchors.rightMargin: 13
                        anchors.topMargin: 1
                        height: 1
                        radius: 1
                        color: "#86FFFFFF"
                    }
                }

                Gradient {
                    id: selectedGradient
                    GradientStop { position: 0.0; color: "#B832A6FF" }
                    GradientStop { position: 0.55; color: "#D70A84FF" }
                    GradientStop { position: 1.0; color: "#DE0869DD" }
                }

                VectorIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: navItem.selected ? 10 : 16
                    width: 21
                    height: 21
                    name: modelData.icon
                    color: navItem.selected ? "white" : Theme.inkMuted
                    lineWidth: 1.75

                    Behavior on y {
                        YAnimator {
                            duration: Theme.motionNormal
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Text {
                    visible: navItem.selected
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 34
                    text: modelData.label
                    color: "white"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textMicro
                    font.weight: Font.DemiBold
                }

                HoverHandler { id: navHover }

                TapHandler {
                    id: navTap
                    margin: 2
                    onTapped: root.navigate(modelData.page)
                }

                Behavior on scale {
                    ScaleAnimator {
                        duration: Theme.motionFast
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 30
            color: "#66FFFFFF"
        }

        Item {
            id: assistantItem

            width: 57
            height: 54
            scale: assistantTap.pressed ? 0.95 : 1.0

            Accessible.role: Accessible.Button
            Accessible.name: "智能助手"

            Rectangle {
                anchors.centerIn: parent
                width: 48
                height: 48
                radius: 16
                border.width: 1
                border.color: "#82D7EEFF"
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#B66CB9FF" }
                    GradientStop { position: 0.55; color: "#CD4F87E8" }
                    GradientStop { position: 1.0; color: "#D84B65D4" }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 1
                    height: 1
                    radius: 1
                    color: "#86FFFFFF"
                }

                VectorIcon {
                    anchors.centerIn: parent
                    width: 21
                    height: 21
                    name: "assistant"
                    color: "white"
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 3
                    width: assistant.hasAttention ? 10 : 8
                    height: width
                    radius: width / 2
                    color: assistant.statusLevel === "critical" ? Theme.red
                           : (assistant.statusLevel === "warning" ? Theme.orange
                              : (assistant.statusLevel === "success" ? Theme.green : Theme.blue))
                    border.width: 1.5
                    border.color: "white"

                    SequentialAnimation on scale {
                        running: assistant.statusLevel === "critical"
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.28; duration: 520; easing.type: Easing.OutCubic }
                        NumberAnimation { to: 1.0; duration: 520; easing.type: Easing.InCubic }
                    }
                }
            }

            TapHandler {
                id: assistantTap
                margin: 2
                onTapped: root.assistantRequested()
            }

            Behavior on scale {
                ScaleAnimator {
                    duration: Theme.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
