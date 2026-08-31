pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property string fontFamily: Qt.platform.os === "windows" ? "Microsoft YaHei UI" : "Noto Sans CJK SC"
    readonly property string numberFont: Qt.platform.os === "windows" ? "Segoe UI Variable Display" : "DejaVu Sans"

    // PressureOS commercial visual system.  The palette deliberately keeps
    // content quieter than controls so measurement values remain dominant.
    readonly property color ink: "#12304F"
    readonly property color inkStrong: "#06284F"
    // Secondary text must remain readable on the high-density 7-inch panel.
    // These colors keep visual hierarchy without the low-contrast "tiny grey"
    // appearance of the first prototype.
    readonly property color inkMuted: "#45647F"
    readonly property color inkFaint: "#647E96"
    readonly property color blue: "#0A84FF"
    readonly property color blueDeep: "#0067E6"
    readonly property color blueSoft: "#E9F4FF"
    readonly property color cyan: "#15B8D2"
    readonly property color green: "#19B987"
    readonly property color greenSoft: "#E4F8F2"
    readonly property color orange: "#F29A43"
    readonly property color orangeSoft: "#FFF3E7"
    readonly property color violet: "#7964F4"
    readonly property color red: "#E95A6F"
    readonly property color surface: "#F6FBFF"
    readonly property color surfaceStrong: "#FFFFFF"
    // Modal surfaces deliberately stay opaque.  They sit above the glass
    // workspace and therefore need a stronger foreground/background split.
    readonly property color modalSurface: "#FCFEFF"
    readonly property color modalSurfaceBottom: "#F5FAFE"
    readonly property color modalBorder: "#BFD4E4"
    readonly property color modalShadow: "#38233A55"
    readonly property color modalScrim: "#74102A49"

    // Liquid-glass material hierarchy. Dense surfaces carry measurement
    // content, regular surfaces carry supporting information, and clear
    // surfaces are reserved for controls and navigation.
    readonly property color glassDenseTop: "#D4FFFFFF"
    readonly property color glassDenseBottom: "#B8EFF7FF"
    readonly property color glassRegularTop: "#A8FFFFFF"
    readonly property color glassRegularBottom: "#84E5F2FF"
    readonly property color glassClearTop: "#78FFFFFF"
    readonly property color glassClearBottom: "#4BD8ECFF"
    readonly property color glassBorder: "#88FFFFFF"
    readonly property color glassInnerLine: "#C4FFFFFF"
    readonly property color glassLowerRim: "#244A6688"
    readonly property color glassRefractionBlue: "#4E8EC8FF"
    readonly property color glassRefractionAqua: "#3FD9F0E8"
    readonly property color glass: glassDenseTop
    readonly property color glassBlue: glassRegularBottom

    // Compatibility aliases for existing pages. GlassPanel maps the legacy
    // material names to the new three-level hierarchy.
    readonly property color materialContent: glassRegularTop
    readonly property color materialContentBottom: glassRegularBottom
    readonly property color materialFloating: glassClearTop
    readonly property color materialFloatingBottom: glassClearBottom
    readonly property color materialThick: glassDenseTop
    readonly property color materialThickBottom: glassDenseBottom
    readonly property color line: "#D5E6F2"
    readonly property color lineSoft: "#E6F0F7"
    readonly property color shadow: "#19385A7A"
    readonly property color shadowAmbient: "#161E416B"
    readonly property color shadowFloating: "#201B3D69"

    readonly property color backgroundTop: "#EDF7FF"
    readonly property color backgroundMiddle: "#DCEAFF"
    readonly property color backgroundBottom: "#E8E3FF"
    readonly property color backdropCobalt: "#A8567EFF"
    readonly property color backdropPeriwinkle: "#8C806BFF"
    readonly property color backdropAqua: "#8A28D8DF"
    readonly property color backdropRose: "#72D596FF"

    readonly property int radiusSmall: 10
    readonly property int radiusMedium: 16
    readonly property int radiusLarge: 22
    readonly property int radiusXL: 30
    readonly property int touchTarget: 48
    readonly property int textMicro: 11
    readonly property int textSmall: 12
    readonly property int textBody: 13
    readonly property int textLabel: 14
    readonly property int textTitle: 23

    readonly property int spaceXS: 4
    readonly property int spaceS: 8
    readonly property int spaceM: 12
    readonly property int spaceL: 16
    readonly property int spaceXL: 24

    property bool reduceMotion: false
    readonly property int motionFast: reduceMotion ? 0 : 105
    readonly property int motionNormal: reduceMotion ? 0 : 175
    readonly property int motionSlow: reduceMotion ? 0 : 220
    readonly property int swipeCommitDistance: 86

    function pageTitle(page) {
        const titles = {
            "home": "桌面",
            "measure": "实时测量",
            "tasks": "任务中心",
            "runner": "任务运行",
            "templates": "模板库",
            "data": "数据工作室",
            "device": "设备与连接"
        }
        return titles[page] || "PressureOS"
    }
}
