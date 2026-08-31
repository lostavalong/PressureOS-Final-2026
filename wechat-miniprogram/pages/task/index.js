var api = require("../../services/api")
var realtime = require("../../services/realtime")
var format = require("../../utils/format")
var ids = require("../../utils/id")

Page({
  data: {
    loading: true,
    task: null,
    template: null,
    selectedStage: 0,
    maxUnlockedStage: 0,
    stages: [
      {id: 0, title: "说明", caption: "了解任务"},
      {id: 1, title: "记录", caption: "采集数据"},
      {id: 2, title: "分析", caption: "审核结果"},
      {id: 3, title: "结果", caption: "导出分享"}
    ],
    prepChecks: [
      {id: "device", label: "已核对设备、量程与连接状态", checked: false},
      {id: "setup", label: "已按说明连接实验装置", checked: false},
      {id: "safety", label: "已阅读并理解安全注意事项", checked: false}
    ],
    prepReady: false,
    measurement: {
      pressureKPa: 524.7,
      pressureDisplay: "524.700",
      temperatureC: "24.6",
      stable: true,
      sequence: 0
    },
    chartPoints: [524.64, 524.68, 524.66, 524.71, 524.69, 524.73],
    externalValue: "",
    suggestions: [],
    savingPoint: false,
    analysis: null,
    analysisLoading: false,
    exporting: false,
    artifact: null,
    streamState: "connecting",
    autoSaveText: "所有更改自动保存",
    touchStartX: 0,
    touchStartY: 0
  },

  onLoad: function (options) {
    this.taskId = options.id
    this.loadTask(true)
  },

  onShow: function () { this.startRealtime() },
  onHide: function () { this.stopRealtime() },
  onUnload: function () { this.stopRealtime() },

  onPullDownRefresh: function () {
    this.loadTask(false).finally(function () { wx.stopPullDownRefresh() })
  },

  loadTask: function (initial) {
    var self = this
    if (!this.taskId) return Promise.reject({message: "缺少任务ID"})
    return api.taskApi.get(this.taskId).then(function (task) {
      self.applyTask(task, initial)
      if (task.currentStage >= 2) self.loadAnalysis()
    }).catch(function (error) {
      self.setData({loading: false})
      wx.showToast({title: error.message || "任务加载失败", icon: "none"})
    })
  },

  applyTask: function (task, initial) {
    var points = (task.points || []).map(function (point) {
      return Object.assign({}, point, {
        pressureText: Number(point.pressureKPa).toFixed(3),
        externalText: Number(point.externalValue).toFixed(Number(point.externalValue) % 1 ? 2 : 0),
        temperatureText: Number(point.temperatureC).toFixed(1),
        timeText: format.fullTime(point.capturedAt).slice(5)
      })
    })
    task.points = points
    task.completedPoints = points.filter(function (point) { return point.valid !== false }).length
    task.progress = Math.min(100, Math.round(task.completedPoints * 100 / Math.max(1, task.targetPoints)))
    task.missingPoints = Math.max(0, task.targetPoints - task.completedPoints)
    var template = task.template || {}
    var suggestions = this.makeSuggestions(task, template)
    var data = {
      loading: false,
      task: task,
      template: template,
      maxUnlockedStage: Math.max(0, task.currentStage || 0),
      suggestions: suggestions,
      autoSaveText: "已保存 · " + format.relativeTime(task.updatedAt)
    }
    if (initial) data.selectedStage = Math.max(0, task.currentStage || 0)
    this.setData(data)
  },

  makeSuggestions: function (task, template) {
    var unit = template.variableUnit || ""
    var start = (task.points || []).length + 1
    if (unit === "g") {
      var lastMass = task.points && task.points.length ? Number(task.points[task.points.length - 1].externalValue) : -60
      return [lastMass + 60, lastMass + 120, lastMass + 180]
    }
    if (unit === "kPa") return [(start - 1) * 100, start * 100, (start + 1) * 100]
    if (unit === "s") return [(start - 1) * 60, start * 60, (start + 1) * 60]
    return [start, start + 1, start + 2]
  },

  startRealtime: function () {
    var self = this
    if (this.stream) return
    this.stream = realtime.subscribe({
      onState: function (state) { self.setData({streamState: state}) },
      onSample: function (sample) {
        var pressure = Number(sample.pressure.value)
        self.setData({
          measurement: {
            pressureKPa: pressure,
            pressureDisplay: pressure.toFixed(3),
            temperatureC: Number(sample.temperature.value).toFixed(1),
            stable: !!sample.stable,
            sequence: sample.sequence
          },
          chartPoints: self.data.chartPoints.concat([pressure]).slice(-34)
        })
        getApp().globalData.latestMeasurement = sample
      }
    })
  },

  stopRealtime: function () {
    if (this.stream) this.stream.close()
    this.stream = null
  },

  togglePrep: function (event) {
    var id = event.currentTarget.dataset.id
    var checks = this.data.prepChecks.map(function (item) {
      return item.id === id ? Object.assign({}, item, {checked: !item.checked}) : item
    })
    this.setData({
      prepChecks: checks,
      prepReady: checks.every(function (item) { return item.checked })
    })
  },

  startTask: function () {
    var self = this
    if (!this.data.prepReady && this.data.maxUnlockedStage === 0) {
      wx.showToast({title: "请先完成准备确认", icon: "none"})
      return
    }
    wx.showLoading({title: "正在进入测量"})
    api.taskApi.setStage(this.taskId, 1).then(function (task) {
      wx.hideLoading()
      self.applyTask(task, false)
      self.setData({selectedStage: 1})
    }).catch(function (error) {
      wx.hideLoading()
      wx.showToast({title: error.message || "操作失败", icon: "none"})
    })
  },

  selectStage: function (event) {
    var stage = Number(event.currentTarget.dataset.stage)
    if (stage > this.data.maxUnlockedStage) {
      var names = ["任务说明", "数据记录", "数据分析", "结果导出"]
      wx.showToast({title: "请先完成" + names[this.data.maxUnlockedStage], icon: "none"})
      return
    }
    this.setData({selectedStage: stage})
  },

  inputExternal: function (event) {
    this.setData({externalValue: event.detail.value})
    this.updateMockTarget(Number(event.detail.value))
  },
  useSuggestion: function (event) {
    var value = Number(event.currentTarget.dataset.value)
    this.setData({externalValue: String(value)})
    this.updateMockTarget(value)
  },

  updateMockTarget: function (externalValue) {
    if (!isFinite(externalValue) || !this.data.task) return
    var target = null
    if (this.data.task.templateId === "tpl-pressure-mass") target = Math.min(599, .1 + 1.7487 * externalValue)
    if (this.data.task.templateId === "tpl-gauge-verification") target = externalValue
    if (target === null) return
    realtime.setMockTarget(target)
    this.setData({
      "measurement.pressureKPa": target,
      "measurement.pressureDisplay": Number(target).toFixed(3),
      chartPoints: this.data.chartPoints.concat([target]).slice(-34)
    })
  },

  addPoint: function () {
    var self = this
    var value = Number(this.data.externalValue)
    if (!isFinite(value) || this.data.externalValue === "") {
      wx.showToast({title: "请先填写" + (this.data.template.variableName || "外部变量"), icon: "none"})
      return
    }
    if (!this.data.measurement.stable) {
      wx.showToast({title: "读数尚未稳定，请稍后采集", icon: "none"})
      return
    }
    this.setData({savingPoint: true, autoSaveText: "正在保存…"})
    api.taskApi.addPoint(this.taskId, {
      externalValue: value,
      externalUnit: this.data.template.variableUnit || "",
      idempotencyKey: ids.createId("mobile"),
      clientTimestamp: new Date().toISOString(),
      measurement: {
        pressureKPa: this.data.measurement.pressureKPa,
        temperatureC: Number(this.data.measurement.temperatureC),
        sequence: this.data.measurement.sequence,
        stable: this.data.measurement.stable,
        source: "builtin.pressure"
      }
    }).then(function (result) {
      self.setData({savingPoint: false, externalValue: ""})
      self.applyTask(result.task, false)
      wx.showToast({title: "第 " + result.point.index + " 点已保存", icon: "success"})
    }).catch(function (error) {
      self.setData({savingPoint: false, autoSaveText: "保存失败，请重试"})
      wx.showToast({title: error.message || "保存失败", icon: "none"})
    })
  },

  removePoint: function (event) {
    var self = this
    var pointId = event.currentTarget.dataset.id
    var point = this.data.task.points.find(function (item) { return item.id === pointId })
    wx.showModal({
      title: "删除第 " + (point ? point.index : "") + " 个数据点？",
      content: "删除后拟合结果会自动重新计算，正式版本将保留审计记录。",
      confirmText: "删除",
      confirmColor: "#E24C64",
      success: function (result) {
        if (!result.confirm) return
        api.taskApi.removePoint(self.taskId, pointId).then(function (task) {
          self.applyTask(task, false)
          wx.showToast({title: "数据点已删除", icon: "success"})
        }).catch(function (error) {
          wx.showToast({title: error.message || "删除失败", icon: "none"})
        })
      }
    })
  },

  finishMeasurement: function () {
    var self = this
    if (this.data.task.completedPoints < this.data.task.targetPoints) {
      wx.showToast({title: "还差 " + this.data.task.missingPoints + " 个数据点", icon: "none"})
      return
    }
    wx.showModal({
      title: "测量数据已齐全",
      content: "系统将锁定当前数据快照并进入分析。之后仍可返回删除或补测，分析结果会自动更新。",
      confirmText: "开始分析",
      success: function (result) {
        if (!result.confirm) return
        wx.showLoading({title: "正在计算"})
        api.taskApi.setStage(self.taskId, 2).then(function (task) {
          self.applyTask(task, false)
          self.setData({selectedStage: 2})
          return self.loadAnalysis()
        }).then(function () { wx.hideLoading() }).catch(function (error) {
          wx.hideLoading()
          wx.showToast({title: error.message || "分析失败", icon: "none"})
        })
      }
    })
  },

  loadAnalysis: function () {
    var self = this
    this.setData({analysisLoading: true})
    return api.taskApi.getAnalysis(this.taskId).then(function (analysis) {
      var residuals = (analysis.residuals || []).map(function (row) {
        return Object.assign({}, row, {
          actualText: Number(row.actual).toFixed(3),
          predictedText: Number(row.predicted).toFixed(3),
          residualText: (row.residual >= 0 ? "+" : "") + Number(row.residual).toFixed(4)
        })
      })
      var display = Object.assign({}, analysis, {
        rText: Number(analysis.r || 0).toFixed(6),
        r2Text: Number(analysis.r2 || 0).toFixed(6),
        rmseText: Number(analysis.rmse || 0).toFixed(4),
        typeAText: Number(analysis.typeA || 0).toFixed(4),
        typeBText: Number(analysis.typeB || 0).toFixed(4),
        combinedText: Number(analysis.combined || 0).toFixed(4),
        expandedText: Number(analysis.expanded || 0).toFixed(4),
        residuals: residuals
      })
      self.setData({analysis: display, analysisLoading: false})
    }).catch(function (error) {
      self.setData({analysisLoading: false})
      wx.showToast({title: error.message || "分析加载失败", icon: "none"})
    })
  },

  confirmAnalysis: function () {
    var self = this
    if (!this.data.analysis || !this.data.analysis.ready) {
      wx.showToast({title: "当前数据不足以确认分析", icon: "none"})
      return
    }
    var warning = this.data.analysis.outlierCount > 0 ? "检测到 " + this.data.analysis.outlierCount + " 个粗大误差嫌疑点，请确认已经审核。" : "未发现显著粗大误差嫌疑。"
    wx.showModal({
      title: "确认分析结果",
      content: warning + "\n确认后将进入结果与导出页面。",
      confirmText: "确认结果",
      success: function (result) {
        if (!result.confirm) return
        api.taskApi.setStage(self.taskId, 3, false).then(function (task) {
          self.applyTask(task, false)
          self.setData({selectedStage: 3})
        }).catch(function (error) {
          wx.showToast({title: error.message || "确认失败", icon: "none"})
        })
      }
    })
  },

  chooseExport: function () {
    var self = this
    wx.showActionSheet({
      itemList: ["PDF实验报告", "CSV原始数据", "JSON分析证据包", "PNG结果图"],
      success: function (result) {
        var formats = ["pdf", "csv", "json", "png"]
        self.exportResult(formats[result.tapIndex])
      }
    })
  },

  exportResult: function (formatName) {
    var self = this
    this.setData({exporting: true})
    wx.showLoading({title: "正在生成"})
    api.taskApi.exportArtifact(this.taskId, formatName).then(function (artifact) {
      wx.hideLoading()
      self.setData({exporting: false, artifact: artifact, "task.status": "completed"})
      wx.showModal({
        title: "结果文件已生成",
        content: artifact.fileName + (artifact.mock ? "\n\n当前为交互原型，真实版本将从树莓派下载或调用微信分享。" : ""),
        showCancel: false
      })
    }).catch(function (error) {
      wx.hideLoading()
      self.setData({exporting: false})
      wx.showToast({title: error.message || "导出失败", icon: "none"})
    })
  },

  taskMenu: function () {
    var self = this
    wx.showActionSheet({
      itemList: ["刷新任务", "返回任务列表", "删除任务"],
      itemColor: "#294D69",
      success: function (result) {
        if (result.tapIndex === 0) self.loadTask(false)
        if (result.tapIndex === 1) wx.switchTab({url: "/pages/tasks/index"})
        if (result.tapIndex === 2) self.deleteTask()
      }
    })
  },

  deleteTask: function () {
    var self = this
    wx.showModal({
      title: "删除整个任务？",
      content: "任务名称、数据点和当前分析结果都将被删除。",
      confirmText: "删除",
      confirmColor: "#E24C64",
      success: function (result) {
        if (!result.confirm) return
        api.taskApi.remove(self.taskId).then(function () {
          wx.showToast({title: "任务已删除", icon: "success"})
          setTimeout(function () { wx.switchTab({url: "/pages/tasks/index"}) }, 400)
        })
      }
    })
  },

  touchStart: function (event) {
    var touch = event.changedTouches[0]
    this.setData({touchStartX: touch.clientX, touchStartY: touch.clientY})
  },

  touchEnd: function (event) {
    var touch = event.changedTouches[0]
    var dx = touch.clientX - this.data.touchStartX
    var dy = touch.clientY - this.data.touchStartY
    if (Math.abs(dx) < 75 || Math.abs(dx) < Math.abs(dy) * 1.4) return
    var next = this.data.selectedStage + (dx < 0 ? 1 : -1)
    if (next < 0 || next > this.data.maxUnlockedStage) return
    this.setData({selectedStage: next})
  },

  onShareAppMessage: function () {
    return {
      title: this.data.task ? this.data.task.name + " · PressureOS结果" : "PressureOS任务",
      path: "/pages/task/index?id=" + encodeURIComponent(this.taskId)
    }
  }
})
