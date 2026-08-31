# PressureOS 上位机软件 V1.0.10

本目录是最终 Windows 安装包对应的纯净源码副本，已移除 Git 仓库、IDE 工作区、构建目录、运行数据库、截图、日志、备份和中间产物。

## 技术栈

- C++17、Qt 6、QML/Qt Quick
- SQLite 本地持久化
- CMake 构建
- PressureOS Serial Protocol V1（115200/8N1、CRC16）

## 目录

```text
src/                  C++ 后端、接口、算法、数据库和任务管理
qml/                  前端页面与触控组件
data/                 助手知识库、拼音词库和任务模板
tests/                后端冒烟测试与滤波回归测试
artifacts/            滤波回归测试所需的两份真实波形夹具
scripts/              Windows/树莓派构建、运行和联调工具
deploy/               树莓派及 Windows 发布配置
CMakeLists.txt         CMake 工程入口
THIRD_PARTY_NOTICES.md 第三方软件声明
```

## 已实现模块

- STM32/CP2102N 串口自动发现、断线重连、CRC16/序号/状态位校验及旧 ASCII 兼容解析
- 实时压力曲线、单位切换、滤波算法、零点校正和分层安全提示
- 任务创建、自动保存、多任务切换、采点、删点、线性拟合、残差和不确定度分析
- CSV、JSON、SVG 结果导出
- SQLite WAL 数据库存储与 JSON 实验模板导入
- 7 英寸 1024×600 触控界面、离线中文拼音键盘和本地任务助手
- 树莓派 Wi-Fi、蓝牙和串口状态读取

## Windows 源码构建

需要 Qt 6.4+、CMake 3.21+、Ninja、Visual Studio 2022 C++ 工具链。

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_windows.ps1 `
  -QtRoot "<Qt的MSVC套件目录>" `
  -BuildDir "<构建输出目录>"
```

运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows.ps1 `
  -QtRoot "<Qt的MSVC套件目录>" `
  -BuildDir "<构建输出目录>"
```

Windows 成品默认使用模拟数据，便于在没有硬件时验收界面与任务流程。树莓派版本可使用：

```text
--device-source auto
--device-source serial --serial-port /dev/ttyUSB0
--device-source simulation
```

## 数据位置

运行数据写入系统应用数据目录，不会写入源码目录：

- Windows：`%LOCALAPPDATA%\PressureOS Lab\PressureOS\`
- Linux：`~/.local/share/PressureOS Lab/PressureOS/`

## 最终版本说明

- 上位机版本：V1.0.10
- 对接固件：STM32 PressureOS V1.0.5
- 标定参数：`CAL-Q2-UP-20260824-R1`
- 温度保留显示与留档，按项目要求不参与常温压力补偿
- 软件只提供报警和操作引导，不等同于独立硬件安全切断装置
