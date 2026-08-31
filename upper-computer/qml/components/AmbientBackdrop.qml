import QtQuick
import PressureOS 1.0

Canvas {
    id: root

    renderTarget: Canvas.Image
    Accessible.ignored: true

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const context = getContext("2d")
        context.clearRect(0, 0, width, height)

        const base = context.createLinearGradient(0, 0, width, height)
        base.addColorStop(0.0, Theme.backgroundTop)
        base.addColorStop(0.48, Theme.backgroundMiddle)
        base.addColorStop(1.0, Theme.backgroundBottom)
        context.fillStyle = base
        context.fillRect(0, 0, width, height)

        function glow(x, y, radius, color, middleAlpha) {
            const radial = context.createRadialGradient(x, y, 0, x, y, radius)
            radial.addColorStop(0.0, Qt.rgba(color.r, color.g, color.b, color.a))
            radial.addColorStop(0.48, Qt.rgba(color.r, color.g, color.b, middleAlpha))
            radial.addColorStop(1.0, Qt.rgba(color.r, color.g, color.b, 0.0))
            context.fillStyle = radial
            context.fillRect(0, 0, width, height)
        }

        glow(width * 0.93, height * 0.05, width * 0.62,
             Theme.backdropCobalt, 0.24)
        glow(width * 0.64, height * 0.34, width * 0.48,
             Theme.backdropAqua, 0.20)
        glow(width * 0.08, height * 0.92, width * 0.58,
             Theme.backdropPeriwinkle, 0.22)
        glow(width * 0.44, height * 1.04, width * 0.48,
             Theme.backdropRose, 0.14)

        const light = context.createLinearGradient(0, 0, width, height * 0.72)
        light.addColorStop(0.0, "rgba(255,255,255,0.74)")
        light.addColorStop(0.45, "rgba(255,255,255,0.16)")
        light.addColorStop(1.0, "rgba(255,255,255,0.00)")
        context.fillStyle = light
        context.fillRect(0, 0, width, height)
    }
}
