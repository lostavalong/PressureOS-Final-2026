Component({
  properties: {
    points: {type: Array, value: [], observer: "scheduleDraw"},
    analysis: {type: Object, value: null, observer: "scheduleDraw"},
    accent: {type: String, value: "#1683FF", observer: "scheduleDraw"}
  },
  lifetimes: {
    ready: function () {
      var self = this
      this.createSelectorQuery().select("#fitChart").fields({node: true, size: true}).exec(function (res) {
        if (!res || !res[0] || !res[0].node) return
        var info = wx.getWindowInfo ? wx.getWindowInfo() : wx.getSystemInfoSync()
        self.canvas = res[0].node
        self.ctx = self.canvas.getContext("2d")
        self.width = res[0].width
        self.height = res[0].height
        var dpr = info.pixelRatio || 2
        self.canvas.width = self.width * dpr
        self.canvas.height = self.height * dpr
        self.ctx.scale(dpr, dpr)
        self.draw()
      })
    },
    detached: function () { if (this.drawTimer) clearTimeout(this.drawTimer) }
  },
  methods: {
    scheduleDraw: function () {
      var self = this
      if (this.drawTimer) clearTimeout(this.drawTimer)
      this.drawTimer = setTimeout(function () { self.draw() }, 30)
    },
    draw: function () {
      var ctx = this.ctx
      var width = this.width
      var height = this.height
      var points = (this.data.points || []).filter(function (point) {
        return isFinite(Number(point.externalValue)) && isFinite(Number(point.pressureKPa))
      })
      var fit = this.data.analysis
      if (!ctx || !width || !height || !fit || points.length < 2) return
      ctx.clearRect(0, 0, width, height)
      var left = 43
      var right = 14
      var top = 18
      var bottom = 34
      var xs = points.map(function (point) { return Number(point.externalValue) })
      var ys = points.map(function (point) { return Number(point.pressureKPa) })
      var minX = Math.min.apply(Math, xs)
      var maxX = Math.max.apply(Math, xs)
      var minY = Math.min.apply(Math, ys)
      var maxY = Math.max.apply(Math, ys)
      var xMargin = Math.max((maxX - minX) * .08, 1)
      var yMargin = Math.max((maxY - minY) * .12, .1)
      minX -= xMargin
      maxX += xMargin
      minY -= yMargin
      maxY += yMargin
      function px(x) { return left + (x - minX) * (width - left - right) / Math.max(.0001, maxX - minX) }
      function py(y) { return top + (maxY - y) * (height - top - bottom) / Math.max(.0001, maxY - minY) }
      ctx.font = "10px sans-serif"
      ctx.fillStyle = "#7890A5"
      ctx.strokeStyle = "rgba(122, 157, 184, .18)"
      ctx.lineWidth = 1
      ctx.setLineDash([3, 5])
      for (var i = 0; i <= 4; i++) {
        var y = top + i * (height - top - bottom) / 4
        ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(width - right, y); ctx.stroke()
        var labelValue = maxY - i * (maxY - minY) / 4
        ctx.fillText(labelValue.toFixed(1), 2, y + 3)
      }
      ctx.setLineDash([])
      ctx.strokeStyle = "#BFD4E4"
      ctx.beginPath(); ctx.moveTo(left, top); ctx.lineTo(left, height - bottom); ctx.lineTo(width - right, height - bottom); ctx.stroke()
      ctx.fillText(minX.toFixed(0), left - 5, height - 10)
      ctx.fillText(maxX.toFixed(0), width - right - 20, height - 10)
      var startY = fit.slope * minX + fit.intercept
      var endY = fit.slope * maxX + fit.intercept
      ctx.strokeStyle = this.data.accent
      ctx.lineWidth = 2.5
      ctx.shadowColor = "rgba(22, 131, 255, .22)"
      ctx.shadowBlur = 7
      ctx.beginPath(); ctx.moveTo(px(minX), py(startY)); ctx.lineTo(px(maxX), py(endY)); ctx.stroke()
      ctx.shadowBlur = 0
      points.forEach(function (point) {
        var x = px(Number(point.externalValue))
        var y = py(Number(point.pressureKPa))
        ctx.beginPath(); ctx.arc(x, y, 5, 0, Math.PI * 2); ctx.fillStyle = "#fff"; ctx.fill()
        ctx.lineWidth = 2.5; ctx.strokeStyle = "#1683FF"; ctx.stroke()
      })
    }
  }
})
