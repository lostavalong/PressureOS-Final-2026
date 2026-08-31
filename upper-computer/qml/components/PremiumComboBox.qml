import QtQuick 2.15
import QtQuick.Controls 2.15
import PressureOS 1.0

Item {
    id: root

    property var model: []
    property string currentValue: ""
    property string label: ""
    property string iconName: ""
    property bool openUpward: false
    readonly property bool opened: popup.opened

    signal selected(string value)

    implicitWidth: 170
    implicitHeight: Theme.touchTarget
    activeFocusOnTab: true
    scale: controlTap.pressed ? 0.978 : 1.0
    transformOrigin: Item.Center

    Accessible.role: Accessible.ComboBox
    Accessible.name: label
    Accessible.description: currentValue

    GlassPanel {
        anchors.fill: parent
        radius: 15
        material: "clear"
        tint: popup.opened ? Theme.blue : Theme.cyan
        tintStrength: popup.opened ? 0.09 : 0.045
        elevated: false
        borderColor: popup.opened || root.activeFocus ? "#C5B8DEFF" : Theme.glassBorder
    }

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 13
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 9

        VectorIcon {
            visible: root.iconName !== ""
            width: 17
            height: 17
            name: root.iconName
            color: popup.opened ? Theme.blue : Theme.inkMuted
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - (root.iconName !== "" ? 48 : 22)
            spacing: 1

            Text {
                visible: root.label !== ""
                text: root.label
                color: Theme.inkFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMicro
            }

            Text {
                width: parent.width
                text: root.currentValue
                color: Theme.ink
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textBody
                font.weight: Font.DemiBold
            }
        }

        Canvas {
            width: 12
            height: 12
            anchors.verticalCenter: parent.verticalCenter
            rotation: popup.opened ? 180 : 0

            onPaint: {
                const context = getContext("2d")
                context.clearRect(0, 0, width, height)
                context.strokeStyle = Theme.inkMuted
                context.lineWidth = 1.7
                context.lineCap = "round"
                context.beginPath()
                context.moveTo(2, 4)
                context.lineTo(6, 8)
                context.lineTo(10, 4)
                context.stroke()
            }

            Behavior on rotation {
                RotationAnimator {
                    duration: Theme.motionNormal
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    TapHandler {
        id: controlTap
        margin: 2
        onTapped: popup.opened ? popup.close() : popup.open()
    }

    Keys.onEnterPressed: popup.opened ? popup.close() : popup.open()
    Keys.onSpacePressed: popup.opened ? popup.close() : popup.open()

    Popup {
        id: popup

        x: 0
        y: root.openUpward ? -height - 8 : root.height + 8
        width: root.width
        height: Math.min(list.contentHeight + 12, 290)
        padding: 6
        modal: true
        dim: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: GlassPanel {
            radius: 18
            material: "modal"
            tint: Theme.blue
            tintStrength: 0.012
            elevated: true
        }

        contentItem: ListView {
            id: list

            clip: true
            spacing: 3
            model: root.model
            boundsBehavior: Flickable.DragOverBounds
            boundsMovement: Flickable.FollowBoundsBehavior
            flickDeceleration: 2200
            maximumFlickVelocity: 4200
            pressDelay: 80

            delegate: Rectangle {
                id: option

                width: list.width
                height: Theme.touchTarget
                radius: 12
                color: modelData === root.currentValue ? "#541A8FFF"
                                                      : (optionHover.hovered ? "#38FFFFFF" : "#00FFFFFF")
                border.width: modelData === root.currentValue ? 1 : 0
                border.color: "#72FFFFFF"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 40
                    text: modelData
                    color: modelData === root.currentValue ? Theme.blueDeep : Theme.ink
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textBody
                    font.weight: modelData === root.currentValue ? Font.DemiBold : Font.Normal
                }

                VectorIcon {
                    visible: modelData === root.currentValue
                    anchors.right: parent.right
                    anchors.rightMargin: 11
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    name: "check"
                    color: Theme.blue
                }

                HoverHandler { id: optionHover }

                TapHandler {
                    onTapped: {
                        root.selected(String(modelData))
                        popup.close()
                    }
                }
            }
        }

        enter: Transition {
            ParallelAnimation {
                OpacityAnimator { from: 0; to: 1; duration: Theme.motionFast }
                ScaleAnimator {
                    from: 0.965
                    to: 1
                    duration: Theme.motionNormal
                    easing.type: Easing.OutCubic
                }
            }
        }

        exit: Transition {
            ParallelAnimation {
                OpacityAnimator { to: 0; duration: Theme.motionFast }
                ScaleAnimator { to: 0.985; duration: Theme.motionFast }
            }
        }
    }

    Behavior on scale {
        ScaleAnimator {
            duration: Theme.motionFast
            easing.type: Easing.OutCubic
        }
    }
}
