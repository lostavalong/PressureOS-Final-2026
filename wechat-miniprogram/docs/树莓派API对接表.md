# PressureOS小程序—树莓派API对接表

## 1. 总则

- 根地址：https://设备地址:8443/api/v1
- 实时流：wss://设备地址:8443/api/v1/stream
- JSON编码：UTF-8
- 认证：Authorization: Bearer 短期令牌
- 协议头：X-PressureOS-Protocol: 1.0
- 正式时间：ISO 8601并带时区；设备单调时间和树莓派接收时间均应保留。
- 写接口携带幂等键；重试不能产生重复数据点或重复任务。

标准错误：

    {
      "code": "MEASUREMENT_NOT_STABLE",
      "message": "当前压力尚未稳定，请稍后再采集",
      "requestId": "req-...",
      "details": {}
    }

## 2. 已在页面中预留的接口

| 方法 | 路径 | 用途 | 页面 |
|---|---|---|---|
| GET | /device | 设备、量程、版本、安全与连接状态 | 仪表、设备 |
| GET | /measurement/current | 当前测量快照 | 仪表 |
| GET | /tasks | 按最后编辑时间返回任务 | 仪表、任务 |
| POST | /tasks | 从模板创建并命名任务 | 新建任务 |
| GET | /tasks/{taskId} | 任务、模板快照、点数据和阶段 | 任务运行 |
| DELETE | /tasks/{taskId} | 删除任务并写审计 | 任务 |
| POST | /tasks/{taskId}/points | 提交人工变量并绑定稳定压力 | 数据记录 |
| DELETE | /tasks/{taskId}/points/{pointId} | 删除点并重算 | 数据记录 |
| POST | /tasks/{taskId}/stage | 冻结阶段快照/推进流程 | 任务运行 |
| GET | /tasks/{taskId}/analysis | 读取树莓派正式分析 | 数据分析 |
| POST | /tasks/{taskId}/artifacts | 生成PDF/CSV/JSON/PNG | 结果 |
| GET | /templates | 模板列表 | 模板库 |
| GET | /templates/{templateId} | 模板详情 | 新建任务 |
| POST | /templates/validate | 仅校验，不安装 | 模板导入 |
| POST | /templates/install | 事务安装校验后的模板 | 模板导入 |
| POST | /pairing/confirm | 仪表端确认后签发令牌 | 配对 |

原API草案中的 manual-values 可保留；本Demo增加 points 聚合接口，使“人工变量＋当前稳定压力＋数据来源＋幂等键”在树莓派中原子落库，避免先后两个请求形成半条记录。

## 3. 关键响应结构

设备：

    {
      "id": "PX-01",
      "name": "PX-01 精密压力模块",
      "connected": true,
      "endpoint": "pressureos-px01.local",
      "firmwareVersion": "1.0.0",
      "hardwareVersion": "PCB-A2",
      "protocolVersion": "1.0",
      "calibrationVersion": "CAL-2026-08-A",
      "rangeMinKPa": -100,
      "rangeMaxKPa": 600,
      "resolutionKPa": 0.1,
      "sampleRateHz": 50,
      "mobileRateHz": 10,
      "linkQuality": 99.8,
      "safetyState": "normal"
    }

实时消息：

    {
      "type": "measurement.sample",
      "sequence": 128450,
      "timestamp": "2026-08-11T14:30:31.420+08:00",
      "pressure": {"value": 524.8, "unit": "kPa", "source": "builtin.pressure"},
      "rawPressureKPa": 524.9,
      "temperature": {"value": 24.6, "unit": "degC"},
      "stable": true,
      "stabilityP2PKPa": 0.0003,
      "quality": "valid",
      "statusFlags": 0
    }

采集数据点请求：

    {
      "externalValue": 240,
      "externalUnit": "g",
      "idempotencyKey": "mobile-...",
      "clientTimestamp": "2026-08-11T14:31:03+08:00",
      "measurement": {
        "pressureKPa": 419.8,
        "temperatureC": 24.6,
        "sequence": 128470,
        "stable": true,
        "source": "builtin.pressure"
      }
    }

树莓派不能直接信任请求里的压力数值。应根据 sequence 在自己的采样缓冲区中取回正式样本，验证任务、权限、单位、范围和稳定性后原子落库。

分析：

    {
      "ready": true,
      "algorithmId": "ols.v1",
      "calibrationVersion": "CAL-2026-08-A",
      "equation": "P = 1.7487x + 0.1000",
      "slope": 1.7487,
      "intercept": 0.1,
      "r": 0.999999,
      "r2": 0.999998,
      "rmse": 0.012,
      "typeA": 0.005,
      "typeB": 0.029,
      "combined": 0.0294,
      "expanded": 0.0588,
      "outlierCount": 0,
      "residuals": []
    }

## 4. 配对二维码

JSON：

    {
      "deviceId": "PX-01",
      "endpoint": "pressureos-px01.local:8443",
      "pairingCode": "826419",
      "fingerprint": "D4:7A:24:8E:19:AC"
    }

也支持：

    pressureos://pair?deviceId=PX-01&endpoint=pressureos-px01.local%3A8443&code=826419&fingerprint=...

配对码必须短时有效、一次使用，并由仪表本机确认手机名称和权限。二维码不能包含长期令牌或Wi-Fi永久密码。

## 5. 状态和冲突

- 401：令牌失效，重新配对。
- 403：缺少具体能力，不自动升级权限。
- 404：任务或模板不存在。
- 409：任务版本冲突、数据不稳定或状态不允许。
- 413：模板或请求体过大。
- 422：字段、单位、范围或Schema校验失败。
- 429：请求过频。
- 503：设备链路暂不可用，页面保留本地输入并明确提示。

任务响应应包含 version 或 ETag。写操作提交上一版本，冲突时由用户决定刷新还是保留草稿，不能静默覆盖。
