var env = require("../config/env")
var mockTargetKPa = 524.7

function createMockStream(callbacks) {
  var sequence = 128450
  var tick = 0
  var timer = null
  if (callbacks.onState) callbacks.onState("connected")
  function push() {
    tick += 1
    sequence += 1
    var slow = Math.sin(tick / 15) * .12
    var ripple = Math.sin(tick / 4.5) * .035
    var pressure = mockTargetKPa + slow + ripple
    var sample = {
      type: "measurement.sample",
      taskId: null,
      sequence: sequence,
      timestamp: new Date().toISOString(),
      pressure: {value: Number(pressure.toFixed(4)), unit: "kPa", source: "builtin.pressure"},
      rawPressureKPa: Number((pressure + Math.sin(tick * 1.7) * .018).toFixed(4)),
      temperature: {value: Number((24.6 + Math.sin(tick / 80) * .08).toFixed(2)), unit: "degC"},
      stable: Math.abs(ripple) < .045,
      stabilityP2PKPa: .0003,
      quality: "valid",
      statusFlags: 0
    }
    if (callbacks.onSample) callbacks.onSample(sample)
  }
  timer = setInterval(push, Math.round(1000 / Math.max(1, env.mobileStreamHz)))
  push()
  return {
    close: function () {
      if (timer) clearInterval(timer)
      timer = null
      if (callbacks.onState) callbacks.onState("closed")
    }
  }
}

function createLiveStream(callbacks) {
  var socket = null
  var closedByUser = false
  var retryCount = 0
  var retryTimer = null
  function connect() {
    var token = wx.getStorageSync("pressureos.access_token")
    if (callbacks.onState) callbacks.onState(retryCount ? "reconnecting" : "connecting")
    socket = wx.connectSocket({
      url: env.websocketUrl,
      header: {Authorization: token ? "Bearer " + token : ""},
      timeout: env.requestTimeoutMs
    })
    socket.onOpen(function () {
      retryCount = 0
      if (callbacks.onState) callbacks.onState("connected")
    })
    socket.onMessage(function (event) {
      try {
        var message = JSON.parse(event.data)
        if (message.type === "measurement.sample" && callbacks.onSample) callbacks.onSample(message)
      } catch (error) {
        if (callbacks.onError) callbacks.onError({message: "收到无法解析的实时消息"})
      }
    })
    socket.onError(function (error) {
      if (callbacks.onError) callbacks.onError(error)
    })
    socket.onClose(function () {
      if (closedByUser) return
      if (callbacks.onState) callbacks.onState("reconnecting")
      var delay = Math.min(5000, 500 * Math.pow(2, retryCount++))
      retryTimer = setTimeout(connect, delay)
    })
  }
  connect()
  return {
    close: function () {
      closedByUser = true
      if (retryTimer) clearTimeout(retryTimer)
      if (socket) socket.close({code: 1000, reason: "page hidden"})
      if (callbacks.onState) callbacks.onState("closed")
    }
  }
}

function subscribe(callbacks) {
  return env.mode === "mock" ? createMockStream(callbacks || {}) : createLiveStream(callbacks || {})
}

function setMockTarget(pressureKPa) {
  if (env.mode === "mock" && isFinite(Number(pressureKPa))) mockTargetKPa = Number(pressureKPa)
}

module.exports = {subscribe: subscribe, setMockTarget: setMockTarget}
