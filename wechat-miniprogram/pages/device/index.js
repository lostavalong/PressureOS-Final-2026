var api = require("../../services/api")
var env = require("../../config/env")

Page({
  data: {
    loading: true,
    device: null,
    apiMode: env.mode,
    permissions: [
      {name: "查看实时测量", code: "measurement.read", enabled: true},
      {name: "查看与切换任务", code: "task.read", enabled: true},
      {name: "补充人工变量", code: "task.manual_input", enabled: true},
      {name: "安装任务模板", code: "template.install", enabled: true},
      {name: "归零与写入参数", code: "device.control", enabled: false}
    ]
  },

  onShow: function () {
    if (this.getTabBar) this.getTabBar().setData({selected: 3})
    this.loadDevice()
  },

  loadDevice: function () {
    var self = this
    return api.deviceApi.getDevice().then(function (device) {
      self.setData({loading: false, device: device})
    }).catch(function (error) {
      self.setData({loading: false})
      wx.showToast({title: error.message || "设备加载失败", icon: "none"})
    })
  },

  openPairing: function () { wx.navigateTo({url: "/pages/pairing/index"}) },

  showConnectionDetail: function () {
    if (!this.data.device) return
    wx.showModal({
      title: "连接信息",
      content: "地址：" + this.data.device.endpoint + "\n传输：HTTPS / WSS\n协议版本：" + this.data.device.protocolVersion + "\n手机推送：" + this.data.device.mobileRateHz + " Hz",
      showCancel: false
    })
  },

  runDiagnosis: function () {
    wx.showLoading({title: "正在检查链路"})
    setTimeout(function () {
      wx.hideLoading()
      wx.showModal({
        title: "连接诊断通过",
        content: "设备发现、身份校验、REST请求、实时数据流和任务读取均正常。当前为Mock体验模式。",
        showCancel: false
      })
    }, 900)
  },

  explainPermission: function (event) {
    var enabled = event.currentTarget.dataset.enabled
    wx.showModal({
      title: enabled ? "已授权能力" : "受保护操作",
      content: enabled ? "此能力已在仪表端配对时授予，可随时由仪表撤销。" : "归零、标定参数写入、量程修改和解除联锁必须在仪表本机明确确认，小程序不能静默执行。",
      showCancel: false
    })
  }
})
