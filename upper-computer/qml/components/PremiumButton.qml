import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root

    property string text: "按钮"
    property string iconName: ""
    property string variant: "secondary"
    property color accent: Theme.blue
    property bool compact: false
    property bool busy: false
    property bool checked: false
    property int horizontalPadding: compact ? 15 : 19

    readonly property bool primary: variant === "primary"
    readonly property bool ghost: variant === "ghost"
    readonly property bool danger: variant === "danger"
    readonly property bool hovered: hover.hovered
    readonly property bool pressed: press.pressed
    readonly property color foreground: danger ? Theme.red
                                               : (primary ? "white"
                                                          : (checked ? Theme.blueDeep : Theme.ink))

    signal clicked()

    implicitWidth: Math.max(compact ? 108 : 152,
                            buttonLabel.implicitWidth + horizontalPadding * 2
                            + (iconName !== "" ? 27 : 0))
    implicitHeight: compact ? Theme.touchTarget : 52
    activeFocusOnTab: true
    opacity: enabled ? 1.0 : 0.46
    scale: pressed ? 0.972 : 1.0
    transformOrigin: Item.Center

    Accessible.role: Accessible.Button
    Accessible.name: text

    Rectangle {
        visible: !root.ghost
        anchors.fill: buttonBody
        anchors.topMargin: root.primary ? 5 : 4
        anchors.bottomMargin: root.primary ? -5 : -4
        radius: buttonBody.radius
        color: root.danger ? "#28B73751"
                           : (root.primary
                              ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.22)
                              : Theme.shadowAmbient)
    }

    Rectangle {
        id: buttonBody
        anchors.fill: parent
        radius: Math.min(16, height / 2)
        border.width: 1
        border.color: root.activeFocus ? Theme.blue
                                      : (root.primary ? "#86D7FFFF"
                                                      : (root.danger ? "#88FFD1D9"
                                                                     : (root.checked ? "#80A5D4FF" : Theme.glassBorder)))
        gradient: root.primary ? stainedGradient : secondaryGradient
    }

    Gradient {
        id: stainedGradient
        GradientStop {
            position: 0.0
            color: root.danger ? "#DCE95A6F"
                               : Qt.rgba(root.accent.r, root.accent.g, root.accent.b,
                                         root.hovered ? 0.90 : 0.84)
        }
        GradientStop {
            position: 0.55
            color: root.danger ? "#E8D64860"
                               : Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.91)
        }
        GradientStop {
            position: 1.0
            color: root.danger ? "#E0C83A52"
                               : Qt.rgba(Theme.blueDeep.r, Theme.blueDeep.g, Theme.blueDeep.b, 0.94)
        }
    }

    Gradient {
        id: secondaryGradient
        GradientStop {
            position: 0.0
            color: root.ghost ? "#30FFFFFF"
                              : (root.danger ? "#B8FFF0F2" : Theme.glassClearTop)
        }
        GradientStop {
            position: 1.0
            color: root.ghost ? "#18DCEFFF"
                              : (root.danger ? "#8CFFE2E8" : Theme.glassClearBottom)
        }
    }

    Rectangle {
        anchors.fill: buttonBody
        radius: buttonBody.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.primary ? "#48FFFFFF" : "#3EFFFFFF" }
            GradientStop { position: 0.30; color: "#12FFFFFF" }
            GradientStop { position: 0.68; color: "#00FFFFFF" }
            GradientStop { position: 1.0; color: root.primary ? "#16003F86" : "#0C1474B8" }
        }
    }

    Rectangle {
        anchors.fill: buttonBody
        radius: buttonBody.radius
        color: root.primary ? "white" : root.accent
        opacity: root.pressed ? 0.11 : 0.0
    }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 9

        VectorIcon {
            id: buttonIcon
            visible: root.iconName !== ""
            width: root.compact ? 17 : 19
            height: width
            name: root.busy ? "sync" : root.iconName
            color: root.foreground
            lineWidth: 1.8
        }

        Text {
            id: buttonLabel
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth,
                            Math.max(0, root.width - root.horizontalPadding * 2
                                     - (root.iconName !== "" ? 27 : 0)))
            text: root.text
            color: root.foreground
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.fontFamily
            font.pixelSize: root.compact ? Theme.textLabel : 15
            font.weight: Font.DemiBold
        }
    }

    RotationAnimator {
        target: buttonIcon
        from: 0
        to: 360
        duration: 900
        loops: Animation.Infinite
        running: root.busy && root.visible
        onRunningChanged: if (!running) buttonIcon.rotation = 0
    }

    HoverHandler { id: hover }

    TapHandler {
        id: press
        enabled: root.enabled
        margin: 3
        onTapped: root.clicked()
    }

    Keys.onEnterPressed: if (enabled) clicked()
    Keys.onSpacePressed: if (enabled) clicked()

    Behavior on scale {
        ScaleAnimator {
            duration: Theme.motionFast
            easing.type: Easing.OutCubic
        }
    }

    Behavior on opacity {
        OpacityAnimator { duration: Theme.motionFast }
    }
}
