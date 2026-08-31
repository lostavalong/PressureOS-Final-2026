var env = require("../config/env")
var mockServer = require("../mock/server")

function request(options) {
  var method = options.method || "GET"
  var path = options.path || "/"
  var data = options.data || {}
  if (env.mode === "mock") return mockServer.handle(method, path, data)
  return new Promise(function (resolve, reject) {
    var token = wx.getStorageSync("pressureos.access_token")
    wx.request({
      url: env.apiBaseUrl + path,
      method: method,
      data: data,
      timeout: env.requestTimeoutMs,
      header: {
        "Content-Type": "application/json",
        "Accept": "application/json",
        "X-PressureOS-Protocol": env.protocolVersion,
        "Authorization": token ? "Bearer " + token : ""
      },
      success: function (response) {
        if (response.statusCode >= 200 && response.statusCode < 300) resolve(response.data)
        else reject({
          statusCode: response.statusCode,
          message: response.data && response.data.message ? response.data.message : "请求失败"
        })
      },
      fail: function (error) {
        reject({statusCode: 0, message: error.errMsg || "无法连接仪表"})
      }
    })
  })
}

module.exports = {request: request}
