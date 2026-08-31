# PressureOS V1.0.10 软件交付说明

## 1. 交付内容

- `PressureOS_Setup_1.0.10_x64.exe`：Windows 10/11 64 位安装程序，支持开始菜单、桌面快捷方式和标准卸载。
- `PressureOS_Portable_1.0.10_x64.zip`：免安装便携版，完整解压后运行 `PressureOS.exe`。
- `PressureOS_Source_1.0.10.zip`：与交付程序对应的完整源码、算法、接口、滤波、测试、文档及树莓派部署配置。
- `SHA256SUMS.txt`：交付文件的 SHA-256 完整性校验值。

## 2. 产品能力

软件包括桌面首页、自由测量、实时波形、量程安全提示、多周期零点校正、任务模板、多任务自动保存、任务采点、统计拟合、残差与不确定度展示、CSV/JSON/SVG 导出、SQLite 数据持久化、离线中文拼音输入及本地任务助手。

Windows 版默认启用 50 Hz 模拟数据源，主要用于软件交互、任务流程、算法和数据管理功能的展示验收。真实 STM32/AD7124/CP2102N 串口接入在树莓派 Linux 构建中启用，协议实现和兼容解析源码均包含在源码包中。

## 3. 安装与运行

安装版直接双击安装程序，默认安装到当前用户目录，不要求管理员权限。便携版必须先完整解压，不能只复制主程序或直接在压缩包预览窗口运行。

首次运行会自动建立 SQLite 数据库。Windows 数据通常位于：

```text
%LOCALAPPDATA%\PressureOS Lab\PressureOS\pressureos_demo.sqlite
```

卸载软件不会主动删除用户测量数据。

## 4. 验证情况

- Release x64 构建成功，产品版本和文件版本均为 `1.0.10`。
- `pressure_filter_regression` 滤波回归测试通过。
- `pressureos_backend_smoke` 数据库、任务、模板、助手及安全逻辑冒烟测试通过。
- 在清除外部 Qt 路径的环境下，便携版可独立启动并完成首页渲染截图。
- 部署包包含 Qt Quick/QML、SQLite 驱动、图形后端和 MSVC 运行库。

## 5. 交付边界

- Windows 安装包未使用商业代码签名证书，Windows SmartScreen 可能首次给出未知发布者提示；应先核对 `SHA256SUMS.txt`。
- Windows 默认模拟数据不能作为正式计量结论。
- 正式实机测量仍需正确连接下位机、使用树莓派版本并遵守压力源、量程和泄压安全规程。
- 软件安全提示不能替代独立硬件保护和现场人员操作规范。

## 6. 源码构建

构建要求为 Qt 6.4+、CMake 3.21+、Ninja、C++17 和 MSVC 2022。项目根目录提供 `scripts/build_windows.ps1`、树莓派部署脚本、自动化测试以及完整接口文档。
