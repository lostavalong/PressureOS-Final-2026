# PressureOS STM32 嵌入式固件 V1.0.5

本目录为最终标定版纯净 Keil 源码。已移除 EIDE/VS Code 工作区、个人 Keil GUI 配置、编译缓存、Listings、Objects、旧固件、Flash 备份和调试日志。

## 硬件与版本

- MCU：STM32F103RC
- ADC：AD7124-8
- Keil 工程：`Project.uvprojx`
- 固件版本：V1.0.5
- 标定参数版本：`CAL-Q2-UP-20260824-R1`
- 串口：USART1，115200 bit/s，8N1，无流控
- 协议：PressureOS Serial Protocol V1，结构化 ASCII + CRC-16/CCITT-FALSE
- 压力串口数据：上传 ADC 原始码，由上位机进行换算、滤波与零点校正
- 温度：保留采集、显示和留档，常温项目中不参与压力补偿

## 源码结构

```text
User/       主程序、中断与 STM32 配置
Hardware/   AD7124、LCD、按键、串口和 PressureOS 协议
System/     延时等系统工具
Library/    STM32F10x 标准外设库
Start/      CMSIS、启动文件和系统时钟文件
```

## 编译与烧录

1. 使用 Keil MDK-ARM 打开 `Project.uvprojx`。
2. 选择工程中的既有 Target，执行 Rebuild。
3. 确认编译结果为 `0 Error(s), 0 Warning(s)`。
4. 使用 ST-Link 下载；也可直接烧录交付包 `02_最终固件` 中的最终 HEX。
5. HEX 已包含地址信息，无需手动填写起始地址。

最终固件文件：

```text
PressureOS_STM32F103RC_V1.0.5_FinalCalibration_20260826.hex
```

## 上电验收

1. 下位机 LCD 能连续更新压力和温度。
2. 串口输出以 `@PS1,` 开头，以 `*XXXX\r\n` 结尾。
3. 相邻测量帧序号连续递增，运行时间单调增加。
4. 上位机识别为“V1 ASCII · CRC16”，CRC 错误计数保持为 0。
5. 连续运行并检查断线重连、数据超时、ADC/DRDY 状态和看门狗状态。

## 重要边界

- 不要用旧 `Project.bin` 或旧版 HEX 代替最终固件。
- 零点校正由上位机在卸压、连通大气并满足稳定性条件后完成。
- 当前软件报警不能替代独立硬件阀、继电器或物理泄压保护。

