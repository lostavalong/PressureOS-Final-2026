var api = require("../../services/api")
var realtime = require("../../services/realtime")
var format = require("../../utils/format")

Page({
  data: {
    loading: true,
    device: null,
    activeTask: null,
    streamState: "connecting",
    unitOptions: ["kPa", "MPa", "bar", "Pa"],
    unitIndex: 1,
    filterOptions: ["设备补偿值", "设备滤波值", "原始值"],
    filterIndex: 0,
    measurement: {
      pressureKPa: 524.7,
      displayValue: "0.5247",
      temperatureC: "24.6",
      stable: true,
      stability: "±0.0003",
      sequence: 0
    },
    chartPoints: [524.64, 524.68, 524.65, 524.71, 524.69, 524.74, 524.70, 524.73],
    rangePercent: 89,
    refreshedAt: "刚刚"
  },

  onShow: function () {
    if (this.getTabBar) this.getTabBar().setData({selected: 0})
    this.loadOverview()
    this.startRealtime()
  },

  onHide: function () { this.stopRealtime() },
  onUnload: function () { this.stopRealtime() },

  onPullDownRefresh: function () {
    var self = this
    this.loadOverview().finally(function () { wx.stopPullDownRefresh() })
  },

  loadOverview: function () {
    var self = this
    return Promise.all([api.deviceApi.getDevice(), api.taskApi.list()]).then(function (results) {
      var active = results[1].find(function (task) { return task.status !== "completed" })
      self.setData({
        loading: false,
        device: results[0],
        activeTask: active || null,
        refreshedAt: "刚刚"
      })
      getApp().globalData.pairedDevice = results[0]
    }).catch(function (error) {
      self.setData({loading: false})
      wx.showToast({title: error.message || "加载失败", icon: "none"})
    })
  },

  startRealtime: function () {
    var self = this
    if (this.stream) return
    this.stream = realtime.subscribe({
      onState: function (state) { self.setData({streamState: state}) },
      onSample: function (sample) {
        var pressureKPa = Number(sample.pressure.value)
        var points = self.data.chartPoints.concat([pressureKPa]).slice(-42)
        var unit = self.data.unitOptions[self.data.unitIndex]
        var range = self.data.device ? self.data.device.rangeMaxKPa : 600
        self.setData({
          measurement: {
            pressureKPa: pressureKPa,
            displayValue: format.pressure(pressureKPa, unit),
            temperatureC: Number(sample.temperature.value).toFixed(1),
            stable: !!sample.stable,
            stability: "±" + Number(sample.stabilityP2PKPa || .0003).toFixed(4),
            sequence: sample.sequence
          },
          chartPoints: points,
          rangePercent: Math.max(0, Math.min(100, Math.round(pressureKPa * 100 / range)))
        })
        getApp().globalData.latestMeasurement = sample
      }
    })
  },

  stopRealtime: function () {
    if (this.stream) this.stream.close()
    this.stream = null
  },

  changeUnit: function (event) {
    var index = Number(event.detail.value)
    var unit = this.data.unitOptions[index]
    this.setData({
      unitIndex: index,
      "measurement.displayValue": format.pressure(this.data.measurement.pressureKPa, unit)
    })
  },

  changeFilter: function (event) {
    this.setData({filterIndex: Number(event.detail.value)})
    wx.showToast({title: "仅调整手机显示来源", icon: "none"})
  },

  openActiveTask: function () {
    if (!this.data.activeTask) return
    wx.navigateTo({url: "/pages/task/index?id=" + encodeURIComponent(this.data.activeTask.id)})
  },

  openTasks: function () { wx.switchTab({url: "/pages/tasks/index"}) },
  openDevice: function () { wx.switchTab({url: "/pages/device/index"}) },
  openPairing: function () { wx.navigateTo({url: "/pages/pairing/index"}) }
})
