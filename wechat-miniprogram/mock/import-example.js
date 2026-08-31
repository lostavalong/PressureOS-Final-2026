module.exports = {
  schemaVersion: "1.0",
  templateId: "user.pressure-temperature.observe",
  name: "压力—温度联合观察",
  version: "1.0.0",
  author: {name: "示例用户"},
  category: "physics",
  tags: ["温度", "压力", "相关性"],
  description: {plainText: "同步记录环境温度和稳定压力，观察温度变化对压力示值的影响。"},
  principle: {plainText: "在压力输入尽量保持不变的条件下，分时记录温度和压力，用相关性与趋势模型评估温度影响。"},
  safety: ["不得超过传感器工作量程", "改变环境温度时避免产生冷凝"],
  taskConfiguration: [
    {id: "pointCount", name: "观察点数", dataType: "integer", default: 8, minimum: 3, maximum: 20}
  ],
  variables: [
    {id: "referenceTemperature", name: "参考温度", unit: "℃", role: "input", required: true},
    {id: "pressure", name: "稳定压力", unit: "kPa", role: "observation", required: true}
  ],
  workflow: [
    {id: "intro", type: "instruction", title: "确认恒压条件"},
    {id: "temperatureInput", type: "input", title: "填写参考温度"},
    {id: "pressureAcquire", type: "acquire", title: "等待稳定并采集压力"},
    {id: "calculate", type: "calculate", title: "分析温度相关趋势"},
    {id: "export", type: "export", title: "生成图表和数据包"}
  ],
  calculations: [
    {id: "fit", operator: "linearRegression", inputs: ["referenceTemperature", "pressure"]},
    {id: "r", operator: "pearsonR", inputs: ["referenceTemperature", "pressure"]}
  ],
  permissions: ["pressure.read", "manual.input", "artifact.generate"]
}
