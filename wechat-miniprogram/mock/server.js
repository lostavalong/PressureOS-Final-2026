var seed = require("./data")
var ids = require("../utils/id")
var analysis = require("../utils/analysis")

var TASKS_KEY = "pressureos.mock.tasks.v1"
var TEMPLATES_KEY = "pressureos.mock.templates.v1"
var DEVICE_KEY = "pressureos.mock.device.v1"

function clone(value) {
  return JSON.parse(JSON.stringify(value))
}

function storageGet(key) {
  if (typeof wx === "undefined" || !wx.getStorageSync) return null
  try { return wx.getStorageSync(key) || null } catch (error) { return null }
}

function storageSet(key, value) {
  if (typeof wx === "undefined" || !wx.setStorageSync) return
  try { wx.setStorageSync(key, value) } catch (error) {}
}

function loadTasks() {
  var stored = storageGet(TASKS_KEY)
  return Array.isArray(stored) ? stored : clone(seed.tasks)
}

function loadTemplates() {
  var stored = storageGet(TEMPLATES_KEY)
  return Array.isArray(stored) ? stored : clone(seed.templates)
}

function loadDevice() {
  var stored = storageGet(DEVICE_KEY)
  return stored && stored.id ? stored : clone(seed.device)
}

function saveTasks(tasks) {
  storageSet(TASKS_KEY, tasks)
}

function saveTemplates(templates) {
  storageSet(TEMPLATES_KEY, templates)
}

function wait(result, delay) {
  return new Promise(function (resolve) {
    setTimeout(function () { resolve(clone(result)) }, delay || 180)
  })
}

function fail(statusCode, message, details) {
  return Promise.reject({statusCode: statusCode, message: message, details: details || null})
}

function findTask(tasks, taskId) {
  return tasks.find(function (task) { return task.id === taskId })
}

function decorateTask(task, templates) {
  var result = clone(task)
  var template = templates.find(function (item) { return item.id === task.templateId })
  result.template = template || null
  result.completedPoints = (result.points || []).filter(function (point) { return point.valid !== false }).length
  result.progress = Math.min(100, Math.round(result.completedPoints * 100 / Math.max(1, result.targetPoints)))
  return result
}

function validateTemplateDocument(document) {
  var errors = []
  if (!document || typeof document !== "object") errors.push("文件内容不是有效JSON对象")
  if (!document.id && !document.templateId) errors.push("缺少模板ID")
  if (!document.name) errors.push("缺少模板名称")
  if (!document.version) errors.push("缺少模板版本")
  if (document.script || document.shell || document.sql) errors.push("模板包含禁止执行的代码字段")
  return {valid: errors.length === 0, errors: errors, warnings: []}
}

function handle(method, path, data) {
  var cleanPath = String(path || "").replace(/^https?:\/\/[^/]+/, "").replace(/^\/api\/v1/, "")
  var tasks = loadTasks()
  var templates = loadTemplates()
  var device = loadDevice()
  method = String(method || "GET").toUpperCase()

  if (method === "GET" && cleanPath === "/device") return wait(device)
  if (method === "GET" && cleanPath === "/measurement/current") {
    return wait({
      sequence: Date.now() % 1000000,
      timestamp: new Date().toISOString(),
      pressure: {value: 524.7, unit: "kPa", source: "builtin.pressure"},
      rawPressureKPa: 524.9,
      temperature: {value: 24.6, unit: "degC"},
      stable: true,
      stabilityP2PKPa: .0003,
      quality: "valid",
      statusFlags: 0
    }, 80)
  }
  if (method === "GET" && cleanPath === "/tasks") {
    var taskList = tasks.map(function (task) { return decorateTask(task, templates) })
    taskList.sort(function (a, b) { return b.updatedAt - a.updatedAt })
    return wait(taskList)
  }
  if (method === "POST" && cleanPath === "/tasks") {
    var template = templates.find(function (item) { return item.id === data.templateId })
    if (!template) return fail(404, "未找到所选模板")
    var now = Date.now()
    var task = {
      id: ids.createId("task"),
      templateId: template.id,
      templateName: template.name,
      name: String(data.name || template.name).trim(),
      status: "measuring",
      currentStage: 0,
      targetPoints: Number(data.targetPoints) || template.targetPoints || 6,
      createdAt: now,
      updatedAt: now,
      accent: template.accent,
      points: []
    }
    tasks.unshift(task)
    saveTasks(tasks)
    return wait(decorateTask(task, templates))
  }
  if (method === "GET" && cleanPath === "/templates") return wait(templates)
  if (method === "POST" && cleanPath === "/templates/validate") return wait(validateTemplateDocument(data.document))
  if (method === "POST" && cleanPath === "/templates/install") {
    var validation = validateTemplateDocument(data.document)
    if (!validation.valid) return fail(422, "模板校验失败", validation.errors)
    var document = clone(data.document)
    document.id = document.id || document.templateId
    document.installed = true
    document.accent = document.accent || "#1683FF"
    document.icon = document.icon || "JSON"
    var categoryMap = {physics: "物理实验", metrology: "计量检定", engineering: "工程测试"}
    document.category = categoryMap[document.category] || document.category || "用户模板"
    document.duration = document.duration || "按模板流程"
    document.difficulty = document.difficulty || "自定义"
    if (document.description && typeof document.description === "object") document.description = document.description.plainText || ""
    document.description = document.description || "通过JSON导入的用户任务模板。"
    document.shortName = document.shortName || document.name
    document.tags = document.tags || ["用户导入"]
    var pointConfig = (document.taskConfiguration || []).find(function (item) { return item.id === "pointCount" })
    document.targetPoints = document.targetPoints || (pointConfig && pointConfig.default) || 6
    var inputVariable = (document.variables || []).find(function (item) { return item.role === "input" })
    document.variableName = document.variableName || (inputVariable && inputVariable.name) || "外部变量"
    document.variableId = document.variableId || (inputVariable && inputVariable.id) || "external"
    document.variableUnit = document.variableUnit || (inputVariable && inputVariable.unit) || ""
    document.variablePlaceholder = document.variablePlaceholder || "请输入" + document.variableName
    document.intro = document.intro || (document.principle && document.principle.plainText) || document.description
    document.workflow = (document.workflow || []).map(function (step) {
      return {title: step.title || step.id || "任务步骤", detail: step.detail || step.type || "按模板说明完成本步骤。"}
    })
    document.safety = document.safety || []
    document.analysisMethods = document.analysisMethods || (document.calculations || []).map(function (item) { return item.operator || item.id })
    var existingIndex = templates.findIndex(function (item) { return item.id === document.id })
    if (existingIndex >= 0) templates.splice(existingIndex, 1, document)
    else templates.unshift(document)
    saveTemplates(templates)
    return wait(document)
  }
  if (method === "POST" && cleanPath === "/pairing/confirm") {
    device.connected = true
    device.lastSeenAt = Date.now()
    storageSet(DEVICE_KEY, device)
    return wait({
      device: device,
      clientId: data.clientId || ids.createId("phone"),
      accessToken: "mock-token-" + Date.now().toString(36),
      expiresIn: 900,
      permissions: ["measurement.read", "task.read", "task.manual_input", "template.install"]
    }, 400)
  }

  var templateMatch = cleanPath.match(/^\/templates\/([^/]+)$/)
  if (method === "GET" && templateMatch) {
    var selectedTemplate = templates.find(function (item) { return item.id === templateMatch[1] })
    return selectedTemplate ? wait(selectedTemplate) : fail(404, "模板不存在")
  }

  var taskMatch = cleanPath.match(/^\/tasks\/([^/]+)$/)
  if (taskMatch) {
    var taskId = taskMatch[1]
    var selectedTask = findTask(tasks, taskId)
    if (!selectedTask) return fail(404, "任务不存在")
    if (method === "GET") return wait(decorateTask(selectedTask, templates))
    if (method === "DELETE") {
      tasks = tasks.filter(function (task) { return task.id !== taskId })
      saveTasks(tasks)
      return wait({deleted: true, taskId: taskId})
    }
  }

  var pointsMatch = cleanPath.match(/^\/tasks\/([^/]+)\/points$/)
  if (pointsMatch && method === "POST") {
    var pointTask = findTask(tasks, pointsMatch[1])
    if (!pointTask) return fail(404, "任务不存在")
    if (!data || !isFinite(Number(data.externalValue))) return fail(422, "请输入有效的外部变量")
    if (!data.measurement || data.measurement.stable !== true) return fail(409, "当前压力尚未稳定，请稍后再采集")
    var point = {
      id: ids.createId("point"),
      index: pointTask.points.length + 1,
      externalValue: Number(data.externalValue),
      externalUnit: data.externalUnit || "",
      pressureKPa: Number(data.measurement.pressureKPa),
      temperatureC: Number(data.measurement.temperatureC),
      source: "manual.mobile+builtin.pressure",
      capturedAt: Date.now(),
      idempotencyKey: data.idempotencyKey || ids.createId("idem"),
      valid: true
    }
    pointTask.points.push(point)
    pointTask.currentStage = Math.max(1, pointTask.currentStage)
    pointTask.updatedAt = Date.now()
    saveTasks(tasks)
    return wait({point: point, task: decorateTask(pointTask, templates)})
  }

  var pointDeleteMatch = cleanPath.match(/^\/tasks\/([^/]+)\/points\/([^/]+)$/)
  if (pointDeleteMatch && method === "DELETE") {
    var deleteTask = findTask(tasks, pointDeleteMatch[1])
    if (!deleteTask) return fail(404, "任务不存在")
    deleteTask.points = deleteTask.points.filter(function (point) { return point.id !== pointDeleteMatch[2] })
    deleteTask.points.forEach(function (point, index) { point.index = index + 1 })
    deleteTask.updatedAt = Date.now()
    saveTasks(tasks)
    return wait(decorateTask(deleteTask, templates))
  }

  var stageMatch = cleanPath.match(/^\/tasks\/([^/]+)\/stage$/)
  if (stageMatch && method === "POST") {
    var stageTask = findTask(tasks, stageMatch[1])
    if (!stageTask) return fail(404, "任务不存在")
    stageTask.currentStage = Math.max(0, Math.min(3, Number(data.stage) || 0))
    if (stageTask.currentStage === 1) stageTask.status = "measuring"
    if (stageTask.currentStage === 2) stageTask.status = "analyzing"
    if (stageTask.currentStage === 3) stageTask.status = data.completed ? "completed" : "exporting"
    stageTask.updatedAt = Date.now()
    saveTasks(tasks)
    return wait(decorateTask(stageTask, templates))
  }

  var analysisMatch = cleanPath.match(/^\/tasks\/([^/]+)\/analysis$/)
  if (analysisMatch && method === "GET") {
    var analysisTask = findTask(tasks, analysisMatch[1])
    if (!analysisTask) return fail(404, "任务不存在")
    var fit = analysis.linearRegression(analysisTask.points, device.resolutionKPa)
    fit.algorithmId = "ols.v1"
    fit.calibrationVersion = device.calibrationVersion
    fit.generatedAt = Date.now()
    return wait(fit, 350)
  }

  var exportMatch = cleanPath.match(/^\/tasks\/([^/]+)\/artifacts$/)
  if (exportMatch && method === "POST") {
    var exportTask = findTask(tasks, exportMatch[1])
    if (!exportTask) return fail(404, "任务不存在")
    exportTask.currentStage = 3
    exportTask.status = "completed"
    exportTask.updatedAt = Date.now()
    saveTasks(tasks)
    return wait({
      artifactId: ids.createId("artifact"),
      taskId: exportTask.id,
      fileName: exportTask.name + "_结果报告.pdf",
      format: data.format || "pdf",
      mock: true,
      createdAt: Date.now()
    }, 600)
  }

  return fail(404, "Mock接口不存在：" + method + " " + cleanPath)
}

function reset() {
  storageSet(TASKS_KEY, clone(seed.tasks))
  storageSet(TEMPLATES_KEY, clone(seed.templates))
  storageSet(DEVICE_KEY, clone(seed.device))
}

module.exports = {handle: handle, reset: reset}
