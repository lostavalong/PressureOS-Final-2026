App({
  globalData: {
    statusBarHeight: 24,
    capsuleHeight: 32,
    pairedDevice: null,
    latestMeasurement: null,
    apiMode: "mock"
  },

  onLaunch: function () {
    var info = wx.getWindowInfo ? wx.getWindowInfo() : wx.getSystemInfoSync()
    var capsule = wx.getMenuButtonBoundingClientRect ? wx.getMenuButtonBoundingClientRect() : null
    this.globalData.statusBarHeight = info.statusBarHeight || 24
    this.globalData.capsuleHeight = capsule ? capsule.height : 32
  }
})
