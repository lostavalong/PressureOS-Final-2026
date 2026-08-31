Component({
  data: {
    selected: 0,
    list: [
      {pagePath: "/pages/home/index", text: "仪表", icon: "/assets/icons/gauge.svg"},
      {pagePath: "/pages/tasks/index", text: "任务", icon: "/assets/icons/tasks.svg"},
      {pagePath: "/pages/templates/index", text: "模板", icon: "/assets/icons/templates.svg"},
      {pagePath: "/pages/device/index", text: "设备", icon: "/assets/icons/device.svg"}
    ]
  },
  methods: {
    switchTab: function (event) {
      var index = Number(event.currentTarget.dataset.index)
      var url = this.data.list[index].pagePath
      if (index === this.data.selected) return
      this.setData({selected: index})
      wx.switchTab({url: url})
    }
  }
})
