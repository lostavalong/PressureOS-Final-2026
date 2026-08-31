var now = Date.now()

var templates = [
  {
    id: "tpl-pressure-mass",
    name: "压力—质量关系实验",
    shortName: "压力—质量",
    category: "物理实验",
    icon: "PM",
    accent: "#1683FF",
    version: "1.0.0",
    duration: "约 20 分钟",
    difficulty: "入门",
    installed: true,
    description: "逐次增加质量并采集稳定压力，自动完成线性拟合、残差和不确定度分析。",
    tags: ["线性拟合", "不确定度", "自动出图"],
    targetPoints: 6,
    variableName: "质量",
    variableId: "mass",
    variableUnit: "g",
    variablePlaceholder: "请输入本次加载质量",
    intro: "本任务用于研究加载质量与压力示值之间的关系。系统会在每次压力稳定后读取仪表数据，你只需补充外部测得的质量。",
    safety: ["确认压力接口紧固且无泄漏", "不得超过传感器与实验装置允许量程", "读数稳定后再采集，卸载过程应缓慢"],
    workflow: [
      {title: "检查与准备", detail: "核对设备、量程、零点状态和实验装置。"},
      {title: "逐级加载", detail: "输入本级质量，等待压力读数稳定。"},
      {title: "采集数据", detail: "采用当前稳定压力，可随时删除并重测。"},
      {title: "分析与导出", detail: "完成线性拟合、残差和不确定度计算。"}
    ],
    analysisMethods: ["最小二乘线性拟合", "相关系数 R 与 R²", "逐点残差", "A/B类不确定度", "3σ粗大误差预筛"]
  },
  {
    id: "tpl-gauge-verification",
    name: "数字压力计示值检定",
    shortName: "示值检定",
    category: "计量检定",
    icon: "JJG",
    accent: "#7964F4",
    version: "1.1.0",
    duration: "约 35 分钟",
    difficulty: "专业",
    installed: true,
    description: "按JJG 875思路组织升压、降压检定点，计算示值误差、回程误差和判定结果。",
    tags: ["JJG 875", "正反行程", "自动判定"],
    targetPoints: 12,
    variableName: "标准压力",
    variableId: "referencePressure",
    variableUnit: "kPa",
    variablePlaceholder: "请输入标准器示值",
    intro: "按预设检定点依次完成升压和降压采集。正式判定前请确认标准器、环境和检定规程版本。",
    safety: ["标准器和被检表均不得超量程", "切换行程前缓慢泄压", "异常点不得无记录删除"],
    workflow: [
      {title: "确认配置", detail: "核对量程、准确度等级、标准器和检定点。"},
      {title: "升压行程", detail: "依次到达各标准点并等待稳定。"},
      {title: "降压行程", detail: "从上限点按相反顺序采集。"},
      {title: "审核结果", detail: "检查示值误差、回程误差和异常点。"}
    ],
    analysisMethods: ["示值误差", "回程误差", "最大允许误差判定", "重复性分析"]
  },
  {
    id: "tpl-leak-test",
    name: "压力系统保压与泄漏测试",
    shortName: "保压测试",
    category: "工程测试",
    icon: "ΔP",
    accent: "#19B987",
    version: "1.0.0",
    duration: "约 15 分钟",
    difficulty: "入门",
    installed: true,
    description: "稳定加压后自动计时，计算压降速率、总压降与是否满足模板阈值。",
    tags: ["自动计时", "压降速率", "趋势图"],
    targetPoints: 8,
    variableName: "观察时刻",
    variableId: "elapsed",
    variableUnit: "s",
    variablePlaceholder: "系统自动填写",
    intro: "达到目标压力并隔离压力源后，系统按固定间隔记录压力，用于评估管路或被测对象的密封性。",
    safety: ["检查接头额定压力", "隔离压力源后再开始保压", "结束后受控泄压"],
    workflow: [
      {title: "密封检查", detail: "检查接头、阀和软管状态。"},
      {title: "加压稳定", detail: "到达目标值并等待读数稳定。"},
      {title: "保压记录", detail: "系统定时记录压力和温度。"},
      {title: "趋势判定", detail: "计算压降速率并生成曲线。"}
    ],
    analysisMethods: ["总压降", "平均泄漏速率", "温度相关性", "阈值判定"]
  },
  {
    id: "tpl-zero-stability",
    name: "零点稳定性与漂移观察",
    shortName: "零点漂移",
    category: "设备维护",
    icon: "0",
    accent: "#20BED5",
    version: "1.0.0",
    duration: "约 10 分钟",
    difficulty: "入门",
    installed: true,
    description: "在安全卸压条件下观察零点变化，输出峰峰值、标准差和漂移趋势。",
    tags: ["零点", "稳定性", "趋势诊断"],
    targetPoints: 10,
    variableName: "观察序号",
    variableId: "index",
    variableUnit: "",
    variablePlaceholder: "自动生成",
    intro: "本任务只观察零点稳定性，不会从手机执行归零。需要写入零点修正时，必须回到仪表端确认。",
    safety: ["确认压力口与大气连通", "读数未稳定时不得执行零点修正", "手机端不具备直接归零权限"],
    workflow: [
      {title: "安全卸压", detail: "确认压力口与大气连通。"},
      {title: "等待稳定", detail: "观察温度和压力波动。"},
      {title: "定时采样", detail: "按固定间隔保存零点读数。"},
      {title: "诊断建议", detail: "评估漂移并提示是否需要仪表端校正。"}
    ],
    analysisMethods: ["均值与标准差", "峰峰值", "线性漂移率", "稳定性判定"]
  }
]

var tasks = [
  {
    id: "task-pressure-mass-02",
    templateId: "tpl-pressure-mass",
    templateName: "压力—质量关系实验",
    name: "压力—质量关系实验 · 第二组",
    status: "measuring",
    currentStage: 1,
    targetPoints: 6,
    createdAt: now - 54 * 60000,
    updatedAt: now - 4 * 60000,
    accent: "#1683FF",
    points: [
      {id: "point-01", index: 1, externalValue: 0, externalUnit: "g", pressureKPa: 0.12, temperatureC: 24.4, source: "mobile+device", capturedAt: now - 38 * 60000, valid: true},
      {id: "point-02", index: 2, externalValue: 60, externalUnit: "g", pressureKPa: 105.03, temperatureC: 24.5, source: "mobile+device", capturedAt: now - 30 * 60000, valid: true},
      {id: "point-03", index: 3, externalValue: 120, externalUnit: "g", pressureKPa: 209.94, temperatureC: 24.6, source: "mobile+device", capturedAt: now - 4 * 60000, valid: true}
    ]
  },
  {
    id: "task-gauge-01",
    templateId: "tpl-gauge-verification",
    templateName: "数字压力计示值检定",
    name: "PX-01 预检 · 8月11日",
    status: "analyzing",
    currentStage: 2,
    targetPoints: 12,
    createdAt: now - 3 * 3600000,
    updatedAt: now - 42 * 60000,
    accent: "#7964F4",
    points: [
      {id: "g-01", index: 1, externalValue: 0, externalUnit: "kPa", pressureKPa: 0.1, temperatureC: 24.1, source: "device", capturedAt: now - 150 * 60000, valid: true},
      {id: "g-02", index: 2, externalValue: 100, externalUnit: "kPa", pressureKPa: 100.1, temperatureC: 24.2, source: "device", capturedAt: now - 142 * 60000, valid: true},
      {id: "g-03", index: 3, externalValue: 200, externalUnit: "kPa", pressureKPa: 200.2, temperatureC: 24.2, source: "device", capturedAt: now - 135 * 60000, valid: true},
      {id: "g-04", index: 4, externalValue: 300, externalUnit: "kPa", pressureKPa: 300.1, temperatureC: 24.3, source: "device", capturedAt: now - 128 * 60000, valid: true},
      {id: "g-05", index: 5, externalValue: 400, externalUnit: "kPa", pressureKPa: 400.2, temperatureC: 24.4, source: "device", capturedAt: now - 120 * 60000, valid: true}
    ]
  },
  {
    id: "task-completed-01",
    templateId: "tpl-leak-test",
    templateName: "压力系统保压与泄漏测试",
    name: "气路密封性初测",
    status: "completed",
    currentStage: 3,
    targetPoints: 8,
    createdAt: now - 26 * 3600000,
    updatedAt: now - 21 * 3600000,
    accent: "#19B987",
    points: [
      {id: "l-01", index: 1, externalValue: 0, externalUnit: "s", pressureKPa: 500.1, temperatureC: 24.0, source: "device", capturedAt: now - 22 * 3600000, valid: true},
      {id: "l-02", index: 2, externalValue: 60, externalUnit: "s", pressureKPa: 499.9, temperatureC: 24.0, source: "device", capturedAt: now - 22 * 3600000 + 60000, valid: true},
      {id: "l-03", index: 3, externalValue: 120, externalUnit: "s", pressureKPa: 499.8, temperatureC: 24.1, source: "device", capturedAt: now - 22 * 3600000 + 120000, valid: true}
    ]
  }
]

module.exports = {
  device: {
    id: "PX-01",
    name: "PX-01 精密压力模块",
    model: "ConST-EDU",
    connected: true,
    transport: "Wi-Fi · 局域网",
    endpoint: "pressureos-px01.local",
    firmwareVersion: "0.8.2-demo",
    hardwareVersion: "PCB-A2",
    protocolVersion: "1.0",
    calibrationVersion: "CAL-2026-08-A",
    rangeMinKPa: -100,
    rangeMaxKPa: 600,
    resolutionKPa: 0.1,
    sampleRateHz: 50,
    mobileRateHz: 10,
    batteryPercent: 86,
    linkQuality: 99.8,
    safetyState: "normal",
    safetyTitle: "测量链路正常",
    lastSeenAt: now
  },
  templates: templates,
  tasks: tasks
}
