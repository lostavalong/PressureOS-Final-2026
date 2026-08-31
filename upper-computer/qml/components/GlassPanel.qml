import QtQuick
import PressureOS 1.0

Item {
    id: root

    default property alias contentData: contentItem.data

    property string material: "regular"
    property color tint: Theme.blue
    property color borderColor: material === "modal" ? Theme.modalBorder : Theme.glassBorder
    property real radius: Theme.radiusLarge
    property bool elevated: true
    property bool interactive: false
    property bool accentGlow: false
    property real tintStrength: accentGlow ? 0.065 : 0.028
    property real contentPadding: 0
    property string accessibleName: ""

    readonly property string resolvedMaterial: material === "thick" ? "dense"
                                                       : (material === "content" ? "regular"
                                                                                 : (material === "floating" ? "clear" : material))
    property color panelColor: resolvedMaterial === "modal" ? Theme.modalSurface
                                                             : (resolvedMaterial === "clear" ? Theme.glassClearTop
                                                             : (resolvedMaterial === "dense" ? Theme.glassDenseTop
                                                                                               : Theme.glassRegularTop))
    property color panelColorBottom: resolvedMaterial === "modal" ? Theme.modalSurfaceBottom
                                                                   : (resolvedMaterial === "clear" ? Theme.glassClearBottom
                                                                   : (resolvedMaterial === "dense" ? Theme.glassDenseBottom
                                                                                                     : Theme.glassRegularBottom))
    readonly property real topAlphaCap: resolvedMaterial === "modal" ? 1.0
                                                                      : (resolvedMaterial === "dense" ? 0.83
                                                                      : (resolvedMaterial === "regular" ? 0.66 : 0.47))
    readonly property real bottomAlphaCap: resolvedMaterial === "modal" ? 1.0
                                                                         : (resolvedMaterial === "dense" ? 0.72
                                                                         : (resolvedMaterial === "regular" ? 0.52 : 0.30))
    readonly property color resolvedTopColor: Qt.rgba(panelColor.r, panelColor.g, panelColor.b,
                                                       Math.min(panelColor.a, topAlphaCap))
    readonly property color resolvedBottomColor: Qt.rgba(panelColorBottom.r, panelColorBottom.g,
                                                          panelColorBottom.b,
                                                          Math.min(panelColorBottom.a, bottomAlphaCap))
    readonly property bool hovered: hover.hovered
    readonly property bool pressed: press.pressed

    signal clicked()

    implicitWidth: 260
    implicitHeight: 180
    activeFocusOnTab: interactive
    transformOrigin: Item.Center
    scale: interactive && pressed ? 0.976 : 1.0

    Accessible.role: interactive ? Accessible.Button : Accessible.Pane
    Accessible.name: accessibleName

    Rectangle {
        visible: root.elevated
        anchors.fill: glassBody
        anchors.topMargin: root.resolvedMaterial === "modal" ? 8 : (root.resolvedMaterial === "clear" ? 6 : 4)
        anchors.bottomMargin: root.resolvedMaterial === "modal" ? -8 : (root.resolvedMaterial === "clear" ? -6 : -4)
        radius: root.radius
        color: root.resolvedMaterial === "modal" ? Theme.modalShadow
                                                   : (root.resolvedMaterial === "clear" ? Theme.shadowFloating : Theme.shadowAmbient)
    }

    Rectangle {
        id: glassBody
        anchors.fill: parent
        radius: root.radius
        border.width: 1
        border.color: root.activeFocus ? Theme.blue
                                       : (root.interactive && root.hovered ? "#B8FFFFFF" : root.borderColor)
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.resolvedTopColor }
            GradientStop {
                position: 0.30
                color: Qt.rgba(root.resolvedTopColor.r, root.resolvedTopColor.g,
                               root.resolvedTopColor.b, root.resolvedTopColor.a * 0.92)
            }
            GradientStop { position: 1.0; color: root.resolvedBottomColor }
        }
    }

    Rectangle {
        anchors.fill: glassBody
        radius: root.radius
        color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, root.tintStrength)
    }

    Rectangle {
        anchors.fill: glassBody
        radius: root.radius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: root.resolvedMaterial === "modal" ? "#16FFFFFF"
                                                           : (root.resolvedMaterial === "clear" ? "#4AFFFFFF" : "#38FFFFFF")
            }
            GradientStop { position: 0.34; color: "#12FFFFFF" }
            GradientStop { position: 0.62; color: "#00FFFFFF" }
            GradientStop { position: 1.0; color: root.resolvedMaterial === "modal" ? "#061A73B8" : "#0A8BCFFF" }
        }
    }

    Rectangle {
        anchors.fill: glassBody
        anchors.margins: 1
        radius: Math.max(0, root.radius - 1)
        color: "transparent"
        border.width: 1
        border.color: root.resolvedMaterial === "modal" ? "#9AFFFFFF"
                                                         : (root.resolvedMaterial === "clear" ? "#64FFFFFF" : "#3CFFFFFF")
    }

    Rectangle {
        anchors.left: glassBody.left
        anchors.right: glassBody.right
        anchors.top: glassBody.top
        anchors.leftMargin: root.radius
        anchors.rightMargin: root.radius
        anchors.topMargin: 1
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#00FFFFFF" }
            GradientStop { position: 0.24; color: "#A0FFFFFF" }
            GradientStop { position: 0.70; color: Theme.glassInnerLine }
            GradientStop { position: 1.0; color: "#00FFFFFF" }
        }
    }

    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: root.contentPadding
    }

    HoverHandler {
        id: hover
        enabled: root.interactive
    }

    TapHandler {
        id: press
        enabled: root.interactive && root.enabled
        margin: 4
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.clicked()
    }

    Keys.onEnterPressed: if (interactive && enabled) clicked()
    Keys.onSpacePressed: if (interactive && enabled) clicked()

    Behavior on scale {
        ScaleAnimator {
            duration: Theme.motionFast
            easing.type: Easing.OutCubic
        }
    }
}
