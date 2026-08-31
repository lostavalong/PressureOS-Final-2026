var api = require("../../services/api")
var exampleTemplate = require("../../mock/import-example")

Page({
  data: {
    loading: true,
    templates: [],
    visibleTemplates: [],
    categories: ["全部", "物理实验", "计量检定", "工程测试", "设备维护", "用户模板"],
    activeCategory: "全部",
    keyword: "",
    showImportSheet: false,
    importing: false
  },

  onShow: function () {
    if (this.getTabBar) this.getTabBar().setData({selected: 2})
    this.loadTemplates()
  },

  loadTemplates: function () {
    var self = this
    return api.templateApi.list().then(function (templates) {
      self.setData({loading: false, templates: templates})
      self.applyFilter()
    }).catch(function (error) {
      self.setData({loading: false})
      wx.showToast({title: error.message || "模板加载失败", icon: "none"})
    })
  },

  applyFilter: function () {
    var category = this.data.activeCategory
    var keyword = this.data.keyword.trim().toLowerCase()
    var visible = this.data.templates.filter(function (item) {
      var matchesCategory = category === "全部" || item.category === category
      var text = (item.name + " " + item.category + " " + (item.tags || []).join(" ")).toLowerCase()
      return matchesCategory && (!keyword || text.indexOf(keyword) >= 0)
    })
    this.setData({visibleTemplates: visible})
  },

  inputSearch: function (event) {
    this.setData({keyword: event.detail.value})
    this.applyFilter()
  },

  chooseCategory: function (event) {
    this.setData({activeCategory: event.currentTarget.dataset.category})
    this.applyFilter()
  },

  createFromTemplate: function (event) {
    wx.navigateTo({url: "/pages/create-task/index?templateId=" + encodeURIComponent(event.currentTarget.dataset.id)})
  },

  showImport: function () { this.setData({showImportSheet: true}) },
  hideImport: function () { if (!this.data.importing) this.setData({showImportSheet: false}) },
  stopBubble: function () {},

  chooseJsonFile: function () {
    var self = this
    wx.chooseMessageFile({
      count: 1,
      type: "file",
      extension: ["json"],
      success: function (result) {
        var file = result.tempFiles && result.tempFiles[0]
        if (!file) return
        if (file.size > 1024 * 1024) {
          wx.showToast({title: "模板文件不能超过 1 MB", icon: "none"})
          return
        }
        wx.getFileSystemManager().readFile({
          filePath: file.path,
          encoding: "utf8",
          success: function (content) {
            try { self.validateAndInstall(JSON.parse(content.data), file.name) }
            catch (error) { wx.showToast({title: "JSON格式错误", icon: "none"}) }
          },
          fail: function () { wx.showToast({title: "文件读取失败", icon: "none"}) }
        })
      }
    })
  },

  installExample: function () {
    this.validateAndInstall(exampleTemplate, "示例模板_压力温度联合观察.json")
  },

  validateAndInstall: function (document, fileName) {
    var self = this
    this.setData({importing: true})
    wx.showLoading({title: "正在安全校验"})
    api.templateApi.validate(document).then(function (result) {
      wx.hideLoading()
      if (!result.valid) {
        self.setData({importing: false})
        wx.showModal({title: "模板未通过校验", content: (result.errors || []).join("\n"), showCancel: false})
        return
      }
      wx.showModal({
        title: "安装此模板？",
        content: fileName + "\n已通过格式、字段和危险能力检查。模板只能使用白名单计算能力。",
        confirmText: "安装",
        success: function (modal) {
          if (!modal.confirm) {
            self.setData({importing: false})
            return
          }
          wx.showLoading({title: "正在安装"})
          api.templateApi.install(document).then(function (installed) {
            wx.hideLoading()
            self.setData({importing: false, showImportSheet: false})
            wx.showToast({title: "模板已安装", icon: "success"})
            self.loadTemplates()
          }).catch(function (error) {
            wx.hideLoading()
            self.setData({importing: false})
            wx.showToast({title: error.message || "安装失败", icon: "none"})
          })
        }
      })
    }).catch(function (error) {
      wx.hideLoading()
      self.setData({importing: false})
      wx.showToast({title: error.message || "校验失败", icon: "none"})
    })
  }
})
