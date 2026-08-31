var api = require("../../services/api")
var format = require("../../utils/format")

Page({
  data: {
    loading: true,
    tasks: [],
    visibleTasks: [],
    filters: [
      {id: "all", label: "全部"},
      {id: "active", label: "进行中"},
      {id: "completed", label: "已完成"}
    ],
    activeFilter: "all",
    keyword: ""
  },

  onShow: function () {
    if (this.getTabBar) this.getTabBar().setData({selected: 1})
    this.loadTasks()
  },

  onPullDownRefresh: function () {
    this.loadTasks().finally(function () { wx.stopPullDownRefresh() })
  },

  loadTasks: function () {
    var self = this
    return api.taskApi.list().then(function (tasks) {
      var decorated = tasks.map(function (task) {
        var meta = format.statusMeta(task.status)
        var nextAction = "继续采集数据"
        if (task.status === "analyzing") nextAction = "审核分析结果"
        if (task.status === "exporting") nextAction = "生成并导出结果"
        if (task.status === "completed") nextAction = "查看任务报告"
        return Object.assign({}, task, {
          statusLabel: meta.label,
          statusTone: meta.tone,
          relativeTime: format.relativeTime(task.updatedAt),
          nextAction: nextAction
        })
      })
      self.setData({loading: false, tasks: decorated})
      self.applyFilter()
    }).catch(function (error) {
      self.setData({loading: false})
      wx.showToast({title: error.message || "任务加载失败", icon: "none"})
    })
  },

  applyFilter: function () {
    var filter = this.data.activeFilter
    var keyword = this.data.keyword.trim().toLowerCase()
    var visible = this.data.tasks.filter(function (task) {
      var matchesFilter = filter === "all" || (filter === "active" && task.status !== "completed") || (filter === "completed" && task.status === "completed")
      var text = (task.name + " " + task.templateName).toLowerCase()
      return matchesFilter && (!keyword || text.indexOf(keyword) >= 0)
    })
    this.setData({visibleTasks: visible})
  },

  chooseFilter: function (event) {
    this.setData({activeFilter: event.currentTarget.dataset.id})
    this.applyFilter()
  },

  inputSearch: function (event) {
    this.setData({keyword: event.detail.value})
    this.applyFilter()
  },

  clearSearch: function () {
    this.setData({keyword: ""})
    this.applyFilter()
  },

  openTask: function (event) {
    wx.navigateTo({url: "/pages/task/index?id=" + encodeURIComponent(event.currentTarget.dataset.id)})
  },

  createTask: function () { wx.navigateTo({url: "/pages/create-task/index"}) },

  taskMenu: function (event) {
    var self = this
    var taskId = event.currentTarget.dataset.id
    var task = this.data.tasks.find(function (item) { return item.id === taskId })
    wx.showActionSheet({
      itemList: ["打开任务", "复制任务名称", "删除任务"],
      itemColor: "#294D69",
      success: function (result) {
        if (result.tapIndex === 0) self.openTask({currentTarget: {dataset: {id: taskId}}})
        if (result.tapIndex === 1 && task) wx.setClipboardData({data: task.name})
        if (result.tapIndex === 2) self.confirmDelete(task)
      }
    })
  },

  confirmDelete: function (task) {
    var self = this
    if (!task) return
    wx.showModal({
      title: "删除任务？",
      content: "“" + task.name + "”及其模拟数据将被删除。真实版本会同时写入审计记录。",
      confirmText: "删除",
      confirmColor: "#E24C64",
      success: function (result) {
        if (!result.confirm) return
        wx.showLoading({title: "正在删除"})
        api.taskApi.remove(task.id).then(function () {
          wx.hideLoading()
          wx.showToast({title: "任务已删除", icon: "success"})
          self.loadTasks()
        }).catch(function (error) {
          wx.hideLoading()
          wx.showToast({title: error.message || "删除失败", icon: "none"})
        })
      }
    })
  }
})
