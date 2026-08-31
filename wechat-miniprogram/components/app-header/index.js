Component({
  properties: {
    title: {type: String, value: "PressureOS"},
    subtitle: {type: String, value: ""},
    back: {type: Boolean, value: false},
    actionText: {type: String, value: ""},
    light: {type: Boolean, value: false}
  },
  data: {statusBarHeight: 24},
  lifetimes: {
    attached: function () {
      this.setData({statusBarHeight: getApp().globalData.statusBarHeight || 24})
    }
  },
  methods: {
    handleBack: function () {
      var pages = getCurrentPages()
      if (pages.length > 1) wx.navigateBack()
      else wx.switchTab({url: "/pages/home/index"})
    },
    handleAction: function () { this.triggerEvent("action") }
  }
})
