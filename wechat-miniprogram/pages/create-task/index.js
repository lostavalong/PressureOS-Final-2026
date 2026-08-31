var api = require("../../services/api")

Page({
  data: {
    loading: true,
    templates: [],
    selectedTemplate: null,
    selectedTemplateId: "",
    name: "",
    targetPoints: 6,
    submitting: false,
    nameFocused: false
  },

  onLoad: function (options) {
    this.preselectedId = options.templateId || ""
    this.loadTemplates()
  },

  loadTemplates: function () {
    var self = this
    api.templateApi.list().then(function (templates) {
      var selected = templates.find(function (item) { return item.id === self.preselectedId }) || templates[0]
      self.setData({
        loading: false,
        templates: templates,
        selectedTemplate: selected,
        selectedTemplateId: selected ? selected.id : "",
        name: selected ? selected.name + " · " + self.defaultSuffix() : "",
        targetPoints: selected ? selected.targetPoints : 6
      })
    }).catch(function (error) {
      self.setData({loading: false})
      wx.showToast({title: error.message || "模板加载失败", icon: "none"})
    })
  },

  defaultSuffix: function () {
    var date = new Date()
    return (date.getMonth() + 1) + "月" + date.getDate() + "日"
  },

  selectTemplate: function (event) {
    var id = event.currentTarget.dataset.id
    var selected = this.data.templates.find(function (item) { return item.id === id })
    if (!selected) return
    this.setData({
      selectedTemplate: selected,
      selectedTemplateId: selected.id,
      name: selected.name + " · " + this.defaultSuffix(),
      targetPoints: selected.targetPoints
    })
  },

  inputName: function (event) { this.setData({name: event.detail.value}) },
  focusName: function () { this.setData({nameFocused: true}) },
  blurName: function () { this.setData({nameFocused: false}) },
  decreasePoints: function () { this.setData({targetPoints: Math.max(3, this.data.targetPoints - 1)}) },
  increasePoints: function () { this.setData({targetPoints: Math.min(30, this.data.targetPoints + 1)}) },

  createTask: function () {
    var self = this
    var name = this.data.name.trim()
    if (!this.data.selectedTemplate) return
    if (name.length < 2) {
      wx.showToast({title: "请为本次任务命名", icon: "none"})
      return
    }
    this.setData({submitting: true})
    api.taskApi.create({
      templateId: this.data.selectedTemplate.id,
      name: name,
      targetPoints: this.data.targetPoints
    }).then(function (task) {
      self.setData({submitting: false})
      wx.showToast({title: "任务已创建", icon: "success"})
      setTimeout(function () {
        wx.redirectTo({url: "/pages/task/index?id=" + encodeURIComponent(task.id)})
      }, 450)
    }).catch(function (error) {
      self.setData({submitting: false})
      wx.showToast({title: error.message || "创建失败", icon: "none"})
    })
  }
})
