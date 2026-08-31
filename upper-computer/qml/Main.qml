import QtQuick 2.15
import QtQuick.Controls 2.15
import PressureOS 1.0
import "components"
import "pages"

ApplicationWindow {
    id: window
    readonly property bool pageModalActive: pageLoader.item
                                                && pageLoader.item.modalActive === true
                                            || waveformFocus.opened
    readonly property var swipeRoutes: ["home", "measure", "tasks", "device"]
    property int navigationDirection: 1
    property bool waveformFocusOpen: launchWaveformFocus

    function routeIndex(route) {
        return swipeRoutes.indexOf(route)
    }

    function navigateWithMotion(route) {
        const currentIndex = routeIndex(app.currentPage)
        const targetIndex = routeIndex(route)
        if (currentIndex >= 0 && targetIndex >= 0)
            navigationDirection = targetIndex >= currentIndex ? 1 : -1
        app.navigate(route)
    }

    function finishPageSwipe(distance, velocity) {
        if (!pageLoader.item)
            return

        const currentIndex = routeIndex(app.currentPage)
        const committed = Math.abs(distance) >= Theme.swipeCommitDistance
                          || Math.abs(velocity) >= 920
        const targetIndex = distance > 0 ? currentIndex - 1 : currentIndex + 1

        if (committed && currentIndex >= 0
                && targetIndex >= 0 && targetIndex < swipeRoutes.length) {
            navigationDirection = distance < 0 ? 1 : -1
            app.navigate(swipeRoutes[targetIndex])
        } else {
            swipeReturn.restart()
        }
    }
    // The supplied documents identify a separate 7-inch HDMI touch display
    // but do not state its native resolution. Use 1024x600 as the conservative
    // review canvas; layouts remain responsive when EDID reports a larger mode.
    width: 1024
    height: 600
    minimumWidth: 960
    minimumHeight: 540
    visible: true
    visibility: launchFullscreen ? Window.FullScreen : Window.Windowed
    title: "PressureOS · 智能精密压力测量任务中心"
    color: "#EDF6FD"
    font.family: Theme.fontFamily

    AmbientBackdrop {
        anchors.fill: parent
    }

    TouchKeyboard { anchors.fill: parent }

    TopStatusBar {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        powerEnabled: !app.keyboardVisible && !window.pageModalActive && !systemPowerDialog.opened
        onPowerRequested: {
            app.assistantOpen = false
            systemPowerDialog.open()
        }
        z: 5
    }

    Loader {
        id: pageLoader
        // The touch keyboard uses Pointer Handlers for its keys. On some
        // Raspberry Pi touch stacks the same release can otherwise also be
        // observed by a handler on the loaded page underneath. Disable the
        // whole page while the keyboard owns the interaction so a key press
        // can never open a task, delete a row, or activate another control.
        enabled: !app.keyboardVisible
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: statusBar.bottom; anchors.bottom: parent.bottom
        anchors.bottomMargin: 75
        sourceComponent: {
            switch (app.currentPage) {
            case "measure": return measurementComponent
            case "tasks": return taskCenterComponent
            case "runner": return taskRunnerComponent
            case "templates": return templatesComponent
            case "data": return dataComponent
            case "device": return deviceComponent
            default: return desktopComponent
            }
        }
        onLoaded: {
            if (!item)
                return
            item.x = window.navigationDirection * 26
            item.opacity = 0
            // Defer one event turn so the newly loaded page has completed its
            // first polish/layout pass before the transition begins. This is
            // also deterministic for automated 1024x600 screenshot capture.
            Qt.callLater(enterAnimation.restart)
        }

        DragHandler {
            id: pageSwipe
            target: null
            enabled: !app.keyboardVisible && !window.pageModalActive
                     && window.routeIndex(app.currentPage) >= 0
            acceptedDevices: PointerDevice.TouchScreen | PointerDevice.TouchPad
            xAxis.enabled: true
            yAxis.enabled: false
            dragThreshold: 18

            onTranslationChanged: {
                if (!active || !pageLoader.item)
                    return
                const offset = Math.max(-72, Math.min(72, translation.x * 0.34))
                pageLoader.item.x = offset
                pageLoader.item.opacity = 1.0 - Math.min(0.11, Math.abs(offset) / 650)
            }

            onActiveChanged: {
                if (!active)
                    window.finishPageSwipe(translation.x, centroid.velocity.x)
            }
        }
    }

    Component { id: desktopComponent; DesktopPage {} }
    Component {
        id: measurementComponent
        MeasurementPage {
            onWaveformFocusRequested: {
                app.assistantOpen = false
                window.waveformFocusOpen = true
            }
        }
    }
    Component { id: taskCenterComponent; TaskCenterPage {} }
    Component { id: taskRunnerComponent; TaskRunnerPage {} }
    Component { id: templatesComponent; TemplateLibraryPage {} }
    Component { id: dataComponent; DataStudioPage {} }
    Component { id: deviceComponent; DeviceCenterPage {} }

    ParallelAnimation {
        id: enterAnimation
        NumberAnimation {
            target: pageLoader.item
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motionNormal
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: pageLoader.item
            property: "x"
            from: window.navigationDirection * 26
            to: 0
            duration: Theme.motionSlow
            easing.type: Easing.OutCubic
        }
    }

    ParallelAnimation {
        id: swipeReturn
        NumberAnimation {
            target: pageLoader.item
            property: "x"
            to: 0
            duration: Theme.motionNormal
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: pageLoader.item
            property: "opacity"
            to: 1
            duration: Theme.motionFast
        }
    }

    NavDock {
        id: dock
        enabled: !app.keyboardVisible && !window.pageModalActive
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        currentPage: app.currentPage
        z: 20
        onNavigate: function(page) { window.navigateWithMotion(page) }
        onAssistantRequested: app.toggleAssistant()
    }

    AssistantNudge {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: dock.top
        anchors.bottomMargin: 11
        recommendation: assistant.recommendation
        shown: assistant.nudgeVisible && !app.assistantOpen && !app.keyboardVisible
               && !window.pageModalActive && !systemPowerDialog.opened
        z: 24
        onOpenRequested: {
            assistant.dismissNudge()
            app.assistantOpen = true
        }
    }

    AssistantDrawer {
        enabled: !app.keyboardVisible && !window.pageModalActive
        opened: app.assistantOpen
        onCloseRequested: app.assistantOpen = false
    }

    SystemPowerDialog {
        id: systemPowerDialog
        Component.onCompleted: {
            if (launchPowerDialog)
                open()
        }
        onExitRequested: Qt.callLater(Qt.quit)
    }

    WaveformFocusOverlay {
        id: waveformFocus
        anchors.fill: parent
        opened: window.waveformFocusOpen
        z: 180
        onCloseRequested: window.waveformFocusOpen = false
    }

    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: dock.top
        anchors.bottomMargin: 12
        width: Math.min(toastText.implicitWidth + 58, 650)
        height: 46
        radius: 16
        color: "#ED0E2F52"
        border.width: 1
        border.color: "#375A7A"
        opacity: app.toastVisible ? 1 : 0
        scale: app.toastVisible ? 1 : .96
        visible: opacity > .01
        z: 90
        Row {
            anchors.centerIn: parent; spacing: 9
            Rectangle { width: 22; height: 22; radius: 7; color: "#244F73"; VectorIcon { anchors.centerIn: parent; width: 13; height: 13; name: "check"; color: "#79DDBE" } }
            Text { id: toastText; text: app.toastText; color: "white"; font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideMiddle; width: Math.min(implicitWidth, 570) }
        }
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        anchors.leftMargin: 14; anchors.bottomMargin: 12
        width: 92; height: 24; radius: 12
        color: "#70FFFFFF"; border.width: 1; border.color: "#A0FFFFFF"
        Row {
            anchors.centerIn: parent; spacing: 5
            Rectangle { width: 6;height:6;radius:3;color:app.demoMode?Theme.orange:(deviceLink.dataFresh?Theme.green:Theme.orange) }
            Text { text:app.demoMode?"DEMO 模式":"接口 1.0";color:Theme.inkMuted;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.DemiBold }
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: waveformFocus.opened ? window.waveformFocusOpen = false
                                          : (app.assistantOpen ? app.assistantOpen=false : app.goBack())
    }
    Shortcut { sequence: "Ctrl+F"; onActivated: window.visibility = window.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }

}
