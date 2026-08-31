Component({
  properties: {
    points: {type: Array, value: [], observer: "scheduleDraw"},
    accent: {type: String, value: "#1683FF", observer: "scheduleDraw"},
    grid: {type: Boolean, value: true, observer: "scheduleDraw"}
  },
  data: {ready: false},
  lifetimes: {
    ready: function () {
      var self = this
      this.createSelectorQuery().select("#pressureChart").fields({node: true, size: true}).exec(function (res) {
        if (!res || !res[0] || !res[0].node) return
        var canvas = res[0].node
        var ctx = canvas.getContext("2d")
        var info = wx.getWindowInfo ? wx.getWindowInfo() : wx.getSystemInfoSync()
        var dpr = info.pixelRatio || 2
        canvas.width = res[0].width * dpr
        canvas.height = res[0].height * dpr
        ctx.scale(dpr, dpr)
        self.canvas = canvas
        self.ctx = ctx
        self.width = res[0].width
        self.height = res[0].height
        self.setData({ready: true})
        self.draw()
      })
    },
    detached: function () {
      if (this.drawTimer) clearTimeout(this.drawTimer)
    }
  },
  methods: {
    scheduleDraw: function () {
      var self = this
      if (this.drawTimer) clearTimeout(this.drawTimer)
      this.drawTimer = setTimeout(function () { self.draw() }, 30)
    },
    draw: function () {
      if (!this.ctx || !this.width || !this.height) return
      var ctx = this.ctx
      var width = this.width
      var height = this.height
      var values = (this.data.points || []).map(function (item) { return Number(item) }).filter(function (item) { return isFinite(item) })
      if (values.length < 2) values = [0, 0]
      ctx.clearRect(0, 0, width, height)
      var padX = 4
      var padY = 14
      if (this.data.grid) {
        ctx.strokeStyle = "rgba(128, 166, 196, .17)"
        ctx.lineWidth = 1
        ctx.setLineDash([3, 5])
        for (var i = 1; i < 4; i++) {
          var gy = Math.round(height * i / 4) + .5
          ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke()
        }
        ctx.setLineDash([])
      }
      var min = Math.min.apply(Math, values)
      var max = Math.max.apply(Math, values)
      var margin = Math.max((max - min) * .3, .08)
      min -= margin
      max += margin
      var coords = values.map(function (value, index) {
        return {
          x: padX + index * (width - padX * 2) / Math.max(1, values.length - 1),
          y: padY + (max - value) * (height - padY * 2) / Math.max(.0001, max - min)
        }
      })
      var gradient = ctx.createLinearGradient(0, 0, 0, height)
      gradient.addColorStop(0, "rgba(29, 143, 255, .26)")
      gradient.addColorStop(1, "rgba(29, 143, 255, 0)")
      ctx.beginPath(); ctx.moveTo(coords[0].x, height)
      coords.forEach(function (point) { ctx.lineTo(point.x, point.y) })
      ctx.lineTo(coords[coords.length - 1].x, height); ctx.closePath(); ctx.fillStyle = gradient; ctx.fill()
      ctx.beginPath()
      coords.forEach(function (point, index) { if (index === 0) ctx.moveTo(point.x, point.y); else ctx.lineTo(point.x, point.y) })
      ctx.strokeStyle = this.data.accent
      ctx.lineWidth = 2.5
      ctx.lineJoin = "round"
      ctx.lineCap = "round"
      ctx.shadowColor = "rgba(22, 131, 255, .22)"
      ctx.shadowBlur = 8
      ctx.stroke()
      ctx.shadowBlur = 0
      var last = coords[coords.length - 1]
      ctx.beginPath(); ctx.arc(last.x, last.y, 4, 0, Math.PI * 2); ctx.fillStyle = "#fff"; ctx.fill()
      ctx.beginPath(); ctx.arc(last.x, last.y, 2.5, 0, Math.PI * 2); ctx.fillStyle = this.data.accent; ctx.fill()
    }
  }
})
