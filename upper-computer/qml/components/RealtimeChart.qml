import QtQuick 2.15
import PressureOS 1.0

Canvas {
    id: root

    property var series: []
    property var rawSeries: []
    property bool compact: false
    property bool showRaw: !compact
    property bool showTimeLabels: !compact
    property bool showSafetyAxis: !compact
    property bool interactive: false
    property int maxPoints: 0
    property real sampleRateHz: 0
    property real resolution: 0.1
    property real rangeMinimum: -100
    property real rangeMaximum: 600
    property string axisUnit: "kPa"
    property color lineColor: Theme.blue
    property color rawColor: "#8FBEEA"
    property real paddingLeft: compact ? 0 : 54
    property real paddingRight: compact ? 0 : 13
    property real paddingTop: compact ? 6 : 15
    property real paddingBottom: compact ? 0 : (showTimeLabels ? 30 : 16)

    readonly property bool hovered: chartHover.hovered
    property real displayedMinimum: 0
    property real displayedMaximum: 0
    property int visibleSampleCount: 0

    signal activated()

    antialiasing: true
    Accessible.role: interactive ? Accessible.Button : Accessible.StaticText
    Accessible.name: interactive ? "打开全屏实时波形" : "实时压力波形"

    onSeriesChanged: schedulePaint()
    onRawSeriesChanged: schedulePaint()
    onShowRawChanged: schedulePaint()
    onShowTimeLabelsChanged: schedulePaint()
    onShowSafetyAxisChanged: schedulePaint()
    onMaxPointsChanged: schedulePaint()
    onSampleRateHzChanged: schedulePaint()
    onResolutionChanged: schedulePaint()
    onRangeMinimumChanged: schedulePaint()
    onRangeMaximumChanged: schedulePaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: if (visible) requestPaint()

    function schedulePaint() {
        if (visible && !paintThrottle.running)
            paintThrottle.start()
    }

    function sampleStart(values) {
        if (!values || values.length <= 0)
            return 0
        return maxPoints > 0 ? Math.max(0, values.length - maxPoints) : 0
    }

    function sampleCount(values, start) {
        return values ? Math.max(0, values.length - start) : 0
    }

    function niceStep(roughStep) {
        if (!isFinite(roughStep) || roughStep <= 0)
            return 0.1
        const exponent = Math.floor(Math.log(roughStep) / Math.LN10)
        const magnitude = Math.pow(10, exponent)
        const fraction = roughStep / magnitude
        const choices = [1, 1.25, 1.5, 2, 2.5, 5, 10]
        for (let i = 0; i < choices.length; ++i) {
            if (fraction <= choices[i])
                return choices[i] * magnitude
        }
        return 10 * magnitude
    }

    function decimalsFor(step) {
        if (step >= 1)
            return 0
        if (step >= 0.1)
            return 1
        if (step >= 0.01)
            return 2
        return 3
    }

    function safetyRatio(value) {
        if (value >= 0)
            return rangeMaximum > 0 ? Math.abs(value / rangeMaximum) : 0
        return rangeMinimum < 0 ? Math.abs(value / rangeMinimum) : 0
    }

    function safetyColor(value, normalColor) {
        const ratio = safetyRatio(value)
        if (ratio >= 0.98)
            return Theme.red
        if (ratio >= 0.90)
            return "#E66F49"
        if (ratio >= 0.80)
            return "#D9952B"
        return normalColor
    }

    function yFor(value, minimum, span, plotHeight) {
        return paddingTop + plotHeight - (value - minimum) / span * plotHeight
    }

    function plot(ctx, values, start, count, minimum, span, color, widthValue, alpha) {
        if (!values || count < 2)
            return
        const plotWidth = width - paddingLeft - paddingRight
        const plotHeight = height - paddingTop - paddingBottom
        let hasPoint = false
        ctx.beginPath()
        for (let i = 0; i < count; ++i) {
            const value = Number(values[start + i])
            if (!isFinite(value)) {
                hasPoint = false
                continue
            }
            const x = paddingLeft + i / (count - 1) * plotWidth
            const y = yFor(value, minimum, span, plotHeight)
            if (!hasPoint) {
                ctx.moveTo(x, y)
                hasPoint = true
            } else {
                ctx.lineTo(x, y)
            }
        }
        ctx.strokeStyle = color
        ctx.globalAlpha = alpha
        ctx.lineWidth = widthValue
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.stroke()
        ctx.globalAlpha = 1
    }

    function fillFilteredArea(ctx, values, start, count, minimum, span) {
        if (!values || count < 2)
            return
        const plotWidth = width - paddingLeft - paddingRight
        const plotHeight = height - paddingTop - paddingBottom
        const gradient = ctx.createLinearGradient(0, paddingTop, 0, height - paddingBottom)
        gradient.addColorStop(0, "rgba(18,130,255,0.23)")
        gradient.addColorStop(1, "rgba(18,130,255,0.01)")
        ctx.beginPath()
        let firstX = paddingLeft
        let lastX = paddingLeft
        let started = false
        for (let i = 0; i < count; ++i) {
            const value = Number(values[start + i])
            if (!isFinite(value))
                continue
            const x = paddingLeft + i / (count - 1) * plotWidth
            const y = yFor(value, minimum, span, plotHeight)
            if (!started) {
                ctx.moveTo(x, y)
                firstX = x
                started = true
            } else {
                ctx.lineTo(x, y)
            }
            lastX = x
        }
        if (!started)
            return
        ctx.lineTo(lastX, height - paddingBottom)
        ctx.lineTo(firstX, height - paddingBottom)
        ctx.closePath()
        ctx.fillStyle = gradient
        ctx.fill()
    }

    function drawSafetyBand(ctx, lower, upper, axisMin, axisMax, span, plotHeight, color, alpha) {
        const clippedLow = Math.max(axisMin, Math.min(lower, upper))
        const clippedHigh = Math.min(axisMax, Math.max(lower, upper))
        if (clippedHigh <= clippedLow)
            return
        const yTop = yFor(clippedHigh, axisMin, span, plotHeight)
        const yBottom = yFor(clippedLow, axisMin, span, plotHeight)
        ctx.fillStyle = color
        ctx.globalAlpha = alpha
        ctx.fillRect(paddingLeft, yTop, width - paddingLeft - paddingRight, yBottom - yTop)
        ctx.globalAlpha = 1
    }

    function drawSafetyBackground(ctx, axisMin, axisMax, span, plotHeight) {
        if (!showSafetyAxis)
            return
        drawSafetyBand(ctx, axisMin, rangeMinimum * 0.98, axisMin, axisMax, span, plotHeight,
                       Theme.red, 0.075)
        drawSafetyBand(ctx, rangeMinimum * 0.98, rangeMinimum * 0.90, axisMin, axisMax, span,
                       plotHeight, "#E66F49", 0.050)
        drawSafetyBand(ctx, rangeMinimum * 0.90, rangeMinimum * 0.80, axisMin, axisMax, span,
                       plotHeight, "#D9952B", 0.040)
        drawSafetyBand(ctx, rangeMaximum * 0.80, rangeMaximum * 0.90, axisMin, axisMax, span,
                       plotHeight, "#D9952B", 0.040)
        drawSafetyBand(ctx, rangeMaximum * 0.90, rangeMaximum * 0.98, axisMin, axisMax, span,
                       plotHeight, "#E66F49", 0.050)
        drawSafetyBand(ctx, rangeMaximum * 0.98, axisMax, axisMin, axisMax, span, plotHeight,
                       Theme.red, 0.075)
    }

    Timer {
        id: paintThrottle
        interval: 50
        repeat: false
        onTriggered: root.requestPaint()
    }

    HoverHandler {
        id: chartHover
        enabled: root.interactive
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    }

    TapHandler {
        enabled: root.interactive
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.activated()
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        const filteredStart = sampleStart(series)
        const filteredCount = sampleCount(series, filteredStart)
        const rawStart = sampleStart(rawSeries)
        const rawCount = sampleCount(rawSeries, rawStart)
        visibleSampleCount = filteredCount
        if (!series || filteredCount < 2)
            return

        let dataMin = Number.POSITIVE_INFINITY
        let dataMax = Number.NEGATIVE_INFINITY
        for (let i = 0; i < filteredCount; ++i) {
            const value = Number(series[filteredStart + i])
            if (isFinite(value)) {
                dataMin = Math.min(dataMin, value)
                dataMax = Math.max(dataMax, value)
            }
        }
        if (showRaw && rawSeries) {
            for (let j = 0; j < rawCount; ++j) {
                const value = Number(rawSeries[rawStart + j])
                if (isFinite(value)) {
                    dataMin = Math.min(dataMin, value)
                    dataMax = Math.max(dataMax, value)
                }
            }
        }
        if (!isFinite(dataMin) || !isFinite(dataMax))
            return

        // Every genuine extreme remains inside the plot. The scale expands
        // immediately and is rounded only to human-readable tick values.
        const dataSpan = Math.max(0, dataMax - dataMin)
        const minimumSpan = Math.max(0.4, resolution * 4.0)
        const paddedSpan = Math.max(minimumSpan, dataSpan * 1.24 + resolution * 2.0)
        const middle = (dataMax + dataMin) / 2.0
        const targetMin = middle - paddedSpan / 2.0
        const targetMax = middle + paddedSpan / 2.0
        const desiredTicks = compact ? 4 : 6
        const step = niceStep((targetMax - targetMin) / Math.max(1, desiredTicks - 1))
        let axisMin = Math.floor(targetMin / step) * step
        let axisMax = Math.ceil(targetMax / step) * step
        if (axisMax <= axisMin)
            axisMax = axisMin + step
        const span = axisMax - axisMin
        displayedMinimum = axisMin
        displayedMaximum = axisMax

        const plotWidth = width - paddingLeft - paddingRight
        const plotHeight = height - paddingTop - paddingBottom
        drawSafetyBackground(ctx, axisMin, axisMax, span, plotHeight)

        if (!compact) {
            ctx.lineWidth = 1
            ctx.font = "10px '" + Theme.numberFont + "'"
            ctx.textBaseline = "middle"
            const tickTotal = Math.max(1, Math.round(span / step))
            for (let tick = 0; tick <= tickTotal; ++tick) {
                const value = axisMax - tick * step
                const y = yFor(value, axisMin, span, plotHeight)
                const warningColor = safetyColor(value, Theme.inkFaint)
                const markerColor = safetyColor(value, Theme.green)
                ctx.strokeStyle = Math.abs(value) < step * 0.001 ? "#BCD2E2" : Theme.lineSoft
                ctx.setLineDash(Math.abs(value) < step * 0.001 ? [] : [4, 6])
                ctx.beginPath()
                ctx.moveTo(paddingLeft, y)
                ctx.lineTo(width - paddingRight, y)
                ctx.stroke()
                if (showSafetyAxis) {
                    ctx.fillStyle = markerColor
                    ctx.fillRect(paddingLeft - 8, y - 1.5, 5, 3)
                }
                ctx.fillStyle = warningColor
                ctx.textAlign = "right"
                ctx.fillText(value.toFixed(decimalsFor(step)), paddingLeft - 12, y)
            }

            ctx.strokeStyle = Theme.lineSoft
            ctx.setLineDash([4, 6])
            const verticalLines = 5
            for (let xg = 0; xg <= verticalLines; ++xg) {
                const x = paddingLeft + xg / verticalLines * plotWidth
                ctx.beginPath()
                ctx.moveTo(x, paddingTop)
                ctx.lineTo(x, height - paddingBottom)
                ctx.stroke()
            }
            ctx.setLineDash([])

            if (showTimeLabels) {
                const effectiveRate = sampleRateHz > 0 ? sampleRateHz : 1
                const duration = Math.max(0, (filteredCount - 1) / effectiveRate)
                ctx.font = "10px '" + Theme.fontFamily + "'"
                ctx.fillStyle = Theme.inkFaint
                ctx.textBaseline = "alphabetic"
                for (let tg = 0; tg <= verticalLines; ++tg) {
                    const x = paddingLeft + tg / verticalLines * plotWidth
                    const remaining = duration * (1.0 - tg / verticalLines)
                    ctx.textAlign = tg === 0 ? "left" : (tg === verticalLines ? "right" : "center")
                    const label = tg === verticalLines ? "现在"
                                  : "−" + (duration < 10 ? remaining.toFixed(1)
                                                         : Math.round(remaining)) + " s"
                    ctx.fillText(label, x, height - 6)
                }
            }
        }

        if (showRaw)
            plot(ctx, rawSeries, rawStart, rawCount, axisMin, span, rawColor, 1.15, 0.62)

        fillFilteredArea(ctx, series, filteredStart, filteredCount, axisMin, span)
        plot(ctx, series, filteredStart, filteredCount, axisMin, span, lineColor,
             compact ? 2.2 : 2.6, 1)

        const lastValue = Number(series[filteredStart + filteredCount - 1])
        if (isFinite(lastValue)) {
            const lastY = yFor(lastValue, axisMin, span, plotHeight)
            const markerColor = safetyColor(lastValue, lineColor)
            ctx.fillStyle = "white"
            ctx.strokeStyle = markerColor
            ctx.lineWidth = 2.6
            ctx.beginPath()
            ctx.arc(width - paddingRight, lastY, compact ? 4 : 5, 0, Math.PI * 2)
            ctx.fill()
            ctx.stroke()
        }

        if (interactive && hovered) {
            ctx.strokeStyle = "#730A84FF"
            ctx.lineWidth = 1.5
            ctx.setLineDash([])
            ctx.strokeRect(paddingLeft + 0.75, paddingTop + 0.75,
                           plotWidth - 1.5, plotHeight - 1.5)
        }
    }
}
