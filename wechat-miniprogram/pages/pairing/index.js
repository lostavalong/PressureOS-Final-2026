var api = require("../../services/api")
var ids = require("../../utils/id")

Page({
  data: {
    state: "ready",
    scanned: null,
    pairing: false
  },

  scanCode: function () {
    var self = this
    wx.scanCode({
      scanType: ["qrCode"],
      success: function (result) {
        var payload = self.parsePayload(result.result)
        if (!payload) {
          wx.showToast({title: "不是有效的PressureOS配对码", icon: "none"})
          return
        }
        self.setData({state: "scanned", scanned: payload})
      }
    })
  },

  parsePayload: function (text) {
    try {
      var parsed = JSON.parse(text)
      if (parsed.deviceId && parsed.endpoint && parsed.pairingCode) return parsed
    } catch (error) {}
    if (String(text).indexOf("pressureos://pair") === 0) {
      var query = String(text).split("?")[1] || ""
      var values = {}
      query.split("&").forEach(function (item) {
        var parts = item.split("=")
        values[decodeURIComponent(parts[0] || "")] = decodeURIComponent(parts[1] || "")
      })
      if (values.deviceId && values.endpoint && values.code) {
        return {deviceId: values.deviceId, endpoint: values.endpoint, pairingCode: values.code, fingerprint: values.fingerprint || "未提供"}
      }
    }
    return null
  },

  useMockDevice: function () {
    this.setData({
      state: "scanned",
      scanned: {
        deviceId: "PX-01",
        endpoint: "pressureos-px01.local:8443",
        pairingCode: "826 419",
        fingerprint: "D4:7A:24:8E:19:AC"
      }
    })
  },

  confirmPairing: function () {
    var self = this
    if (!this.data.scanned) return
    this.setData({pairing: true, state: "confirming"})
    api.pairingApi.confirm({
      deviceId: this.data.scanned.deviceId,
      pairingCode: this.data.scanned.pairingCode,
      clientId: ids.createId("wechat"),
      clientName: "微信小程序"
    }).then(function (result) {
      getApp().globalData.pairedDevice = result.device
      self.setData({pairing: false, state: "success"})
    }).catch(function (error) {
      self.setData({pairing: false, state: "scanned"})
      wx.showToast({title: error.message || "配对失败", icon: "none"})
    })
  },

  finish: function () { wx.switchTab({url: "/pages/home/index"}) },
  rescan: function () { this.setData({state: "ready", scanned: null}) }
})
