# 设计定稿备忘（2026-09-04）

> 本文记录当日讨论收敛的设计决策，作为后续实现的依据。仓库已纳入 git 版本管理（单仓库，见 CHANGELOG）。

## 1. 通信帧（老格式冻结 + 0xAA 变长新帧）

- 老格式冻结（不得改动）：0xFF 报警/参数 alarm_pdu（32B）、0x55 状态/GPIOHEAD 问答/radar_pdu；字段偏移、长度、CCITT CRC16 为对外协议红线；
- 线上老设备未做 OTA：新固件必须永远能收老帧；对老设备的交互继续用老帧发出；
- 新帧 0xAA：AA + Len(1B) + Cmd(1B) + Addr(1B) + Payload + CRC16(2B)；Len = Addr+Cmd+Payload 字节数；变长通吃 32B~1KB+；
- 接收引擎按首字节分流（0xFF/0x55/0xAA 互斥），Modbus 式状态机 IDLE-HDR-BODY-CRC-DONE；
- 三判决点：Len 越界 / CRC 失败 / 超时(RX_GUARD 默认 50ms) → 回 IDLE，滑窗找头重同步，不再整段清空；
- 半包中遇疑似新头：按 payload 吞入，由判决点定生死；断线/主机重启：短帧靠帧头自愈，OTA 靠块号/会话；
- 0xAA Cmd 草案：0x01 报警、0x02 有人状态上传、0x03 GPIO 扩展、0x10 查询、0xA1~0xA6 OTA 族、0xF0 回执；第一阶段只实现接收，业务/发送按灰度启用。

## 2. 实时性与可靠性

- 实时性：事件驱动（雷达/摄像头有人上沿即报）为主 + 周期心跳/轮询兜底；
- 可靠性：周期重发自愈 + 状态迟滞（有人丢 N=2~3 拍才清，防 RFID 误停）+ 帧序号/事务重传（OTA）；
- 高并发轮询：流水线（查询连发 5 口 → 应答统一收割），单轮 3~5ms，周期目标 10ms；
- 驱动：保留 armfly 环 / HC32 DMA 主体，在其上加帧解析层；F0 不指望 DMA 全覆盖。

## 3. OTA（设计稿 docs/ota_boot_design.md v0.1）

- A/B 双槽即运行区、无搬运；选择器标志（双份+CRC）选槽，版本号不参与选槽；
- Boot 只读标志 → 校验(Magic+ImageLen+整包CRC32+TargetSlot) → 跳槽；TRIAL 试运行 3s + 失败计数 2 次自动回退；
- 按槽编译两份镜像（STM32 M0 无 VTOR 需向量表重映射；HC32 用 VTOR）；
- STM32 作为升级代理：IPC 下载缓存到自身 Flash 中转区（921600 可选），再逐板分发 HC32；
- 内存布局默认：STM32 Boot16K@0x08000000 / A64K / B64K / 标志 4K；HC32 Boot32K / A128K / B128K / 标志 8K。

## 4. 已确认的产品口径

- 5 条 RS485 点对点（STM32 6 串口：1 接 Linux、5 接 HC32），无需寻址；
- 雷达 OUT 100ms/10Hz 硬量化；摄像头输入已去抖；HC32@200MHz 冗余大、STM32 hub@40MHz 仅中继；
- MVP 已落地：GET_RADAR_ENABLE=1 老轮询（50ms、32B GPIOHEAD 一问一答、Radarcfg[0]=雷达或摄像头有人）；Release 体积 STM32 9.98KB / HC32 25.46KB；
- 已知待办：轮询提速/流水线、有人状态迟滞、0xAA 收帧引擎、HC32 DMA 收侧可变块改造、IPC 921600（可选）。

## 5. 明天开工顺序

1. 落 docs/frame_protocol_design.md 完整协议文档（含兼容基线表）+ CHANGELOG + commit；
2. P1：frame_parser 通用接收引擎（双端共用新文件），先挂 STM32 单口，老帧回归；
3. 铺开 STM32 各口 → HC32（DMA 收侧改造）→ 0xAA 接收 → 业务灰度。