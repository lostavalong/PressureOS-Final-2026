function pad(value) {
  return value < 10 ? "0" + value : String(value)
}

function relativeTime(timestamp) {
  var delta = Math.max(0, Date.now() - Number(timestamp))
  if (delta < 60000) return "刚刚"
  if (delta < 3600000) return Math.floor(delta / 60000) + " 分钟前"
  if (delta < 86400000) return Math.floor(delta / 3600000) + " 小时前"
  var date = new Date(Number(timestamp))
  return pad(date.getMonth() + 1) + "-" + pad(date.getDate()) + " " + pad(date.getHours()) + ":" + pad(date.getMinutes())
}

function fullTime(timestamp) {
  var date = new Date(Number(timestamp))
  return date.getFullYear() + "-" + pad(date.getMonth() + 1) + "-" + pad(date.getDate()) + " " + pad(date.getHours()) + ":" + pad(date.getMinutes())
}

function pressure(value, unit) {
  var kpa = Number(value) || 0
  if (unit === "MPa") return (kpa / 1000).toFixed(4)
  if (unit === "bar") return (kpa / 100).toFixed(4)
  if (unit === "Pa") return String(Math.round(kpa * 1000))
  return kpa.toFixed(1)
}

function statusMeta(status) {
  var map = {
    measuring: {label: "待测数据", tone: "blue"},
    analyzing: {label: "待数据分析", tone: "orange"},
    exporting: {label: "待导出结果", tone: "violet"},
    completed: {label: "已完成", tone: "green"}
  }
  return map[status] || map.measuring
}

module.exports = {
  relativeTime: relativeTime,
  fullTime: fullTime,
  pressure: pressure,
  statusMeta: statusMeta
}
