var client = require("./client")

var deviceApi = {
  getDevice: function () { return client.request({path: "/device"}) },
  getCurrentMeasurement: function () { return client.request({path: "/measurement/current"}) }
}

var taskApi = {
  list: function () { return client.request({path: "/tasks"}) },
  get: function (taskId) { return client.request({path: "/tasks/" + encodeURIComponent(taskId)}) },
  create: function (data) { return client.request({method: "POST", path: "/tasks", data: data}) },
  remove: function (taskId) { return client.request({method: "DELETE", path: "/tasks/" + encodeURIComponent(taskId)}) },
  addPoint: function (taskId, data) { return client.request({method: "POST", path: "/tasks/" + encodeURIComponent(taskId) + "/points", data: data}) },
  removePoint: function (taskId, pointId) { return client.request({method: "DELETE", path: "/tasks/" + encodeURIComponent(taskId) + "/points/" + encodeURIComponent(pointId)}) },
  setStage: function (taskId, stage, completed) { return client.request({method: "POST", path: "/tasks/" + encodeURIComponent(taskId) + "/stage", data: {stage: stage, completed: !!completed}}) },
  getAnalysis: function (taskId) { return client.request({path: "/tasks/" + encodeURIComponent(taskId) + "/analysis"}) },
  exportArtifact: function (taskId, format) { return client.request({method: "POST", path: "/tasks/" + encodeURIComponent(taskId) + "/artifacts", data: {format: format || "pdf"}}) }
}

var templateApi = {
  list: function () { return client.request({path: "/templates"}) },
  get: function (templateId) { return client.request({path: "/templates/" + encodeURIComponent(templateId)}) },
  validate: function (document) { return client.request({method: "POST", path: "/templates/validate", data: {document: document}}) },
  install: function (document) { return client.request({method: "POST", path: "/templates/install", data: {document: document}}) }
}

var pairingApi = {
  confirm: function (payload) {
    return client.request({method: "POST", path: "/pairing/confirm", data: payload}).then(function (result) {
      if (result.accessToken) wx.setStorageSync("pressureos.access_token", result.accessToken)
      return result
    })
  }
}

module.exports = {
  deviceApi: deviceApi,
  taskApi: taskApi,
  templateApi: templateApi,
  pairingApi: pairingApi
}
