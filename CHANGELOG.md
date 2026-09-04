# CHANGELOG

本仓库（alarm_board）以**单仓库**方式管理两套固件，对应同一套防盗报警链路的上下游：

| 目录 | MCU | 角色 | 入口 |
| --- | --- | --- | --- |
| `STM32F0_linux_v4.31` | STM32F030CCTx | 通道中继板（桥接 Linux 主机 → 各报警板） | `BSP/app.c` |
| `Radar_V4.2_2026_0425_MOS` | HC32F460 | 报警板（确认触发源并输出声光报警） | `projects/source/main.c` |

**报警链路（基线已确认）：**
Linux 主机 --IPC(UART1@115200)--> STM32F0 中继板 --CRC 校验、按 AntID/通道分发(COM2..COM6@460800)--> HC32F460 报警板 --读 雷达GPIO/摄像头输入/继电器输入 确认--> 输出声光报警（蜂鸣器 PWM/红绿蓝 LED/OPA/继电器 GPO）。

修改记录约定：每次变更在对应版本条目下按
`### [工程] 模块名` 加 `### Added / ### Changed / ### Fixed / ### Removed` 小节记录，随后提交并在必要时打 tag。

## [Unreleased]

- （无）

## [0.1.0] - 2026-09-04

### Added

- **建立 git 单仓库**：在 `J:\dsh\alarm_board` 根目录 `git init`，纳入 `STM32F0_linux_v4.31` 与 `Radar_V4.2_2026_0425_MOS` 全部源码；新增 `.gitignore` 排除 IDE 生成物/编译输出，新增本文件与 `README.md`（架构、报警链路、协议说明）。
- **基线代码快照**：两套固件保持源码原状入库（未做任何逻辑改动），打基线 tag `v0.1.0`。
- 说明：仓库内源码含 GBK 编码注释（中文），提交/检出不改变字节内容（`core.autocrlf=false`）。

### [stm32f0] BSP

- （基线，无修改）
### [hc32f460] projects/source

- （基线，无修改）
