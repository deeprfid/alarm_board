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

- `[hc32f460]` **fix**: UART4 RX DMA 窗口由固定 32B 改为 **512B 整帧窗口**（`bsp_rs485.c` 新增 `RS485_RX_WIN=512`，`m_au8RxBuf` 随之放大，TC/空闲超时上抛均按窗口计算）——消除不定长帧 >32B 时每 32B 边界“DMA TC 停→AOS_SW_Trigger 重装”窗口丢字节，整帧一次落入 DMA 块、空闲 flush 才上抛；针对 0xAA 128B 负载（134B 帧）偶发整帧丢失（errcnt=0、rxcnt 短少）修复。legacy 32B 帧路径不受影响。

### Added

- **docs**：新增《RS485 雷达/摄像头有人状态实时上传链路设计》（`docs/rs485_io_upload_design.md`，设计稿 v0.1）。覆盖：单字节状态码语义（有人=0x55/无人=0xAA、无人静默、20ms 心跳、下沿 0xAA）、RS485 非对称 LBT 仲裁（G_m/G_s/G_edge、关键帧双发、主站让行窗）、STM32 中断内归属补全上报（[0x7E][串口号][通道号][state]）、延迟预算（雷达 100ms 量化为主导）与两端实施清单。**未改动任何固件代码**。
- **docs**：设计稿升级 **v0.2**（`docs/rs485_io_upload_design.md`）：STM32→Linux 上报由“逐口 4B 即时帧”改为 **5 通道聚合 [0x7E][mask]** + **窗口上报（W=20ms 默认，可配 10ms）** + **全无人静默不报**；任一路 0→1 立即首报（不经过窗口，保 RFID 启动实时）；STM32 侧改为“RX 中断维护 5 位掩码 → 窗口任务组帧非阻塞入队 COM1”。含口→通道位序映射与 Linux 超时判清建议。**未改动任何固件代码**。

- **docs**：新增/升级设计稿后，本 Unreleased 追加**固件 MVP 改动**（首次改动固件代码）：

- `[hc32f460]` **feat**: 恢复“主机查询-报警板上报有人状态”老链路（MVP 第一步，复用 Radar V3.2 2025-0430 实现）：
  - `bsp_rs485.c` `Uart4_int()` 使能 `USART_TX`（此前仅 RX，发送功能未启用）；
  - `common.c` 新增 `Send_RadarStatus_to_Master()`：应答 32B `GPIOHEAD(0x55)` 帧，`Radarcfg[0]`=1/0（雷达 GPIO(PC14/PC13/PH2) 或 摄像头 IN1(PB00) 有人），阻塞 `USART_UART_Trans` 发送；
  - `Get_pdu_data()` 的 GPIOHEAD 分支由空转改为调用上述应答（`common.c`），`bsp_rs485.h` 增加原型。
- `[stm32f0]` **feat**: `GET_RADAR_ENABLE` 0→1（`BSP/bsp.h`）：启用既有 50ms 周期雷达状态轮询（`Broadcast_Get_Radar_Status` 广播 GPIOHEAD 查询）、应答缓存（`Chaneel_ID[]`）与按 AntID 查询应答（`Send_RadarStatus_to_Master`/`radar_pdu`）——拉取式上传链路打通，用于验证；
- 备注：MVP 保留老 50ms 周期与“每问必答 0/1”语义；周期压缩、STM32→Linux 聚合帧 `[0x7E][mask]`、0x55/0xAA 推送等优化放下一迭代（设计文档 v0.2 暂缓执行）。
- `[stm32f0]` **fix**: 恢复 `Radar_thread` 原版 50ms 广播节奏（每 50ms 调 `Broadcast_Get_Radar_Status()`，与出厂一致）；删除逐口错峰 `send_legacy_query/s_query_next` 与 0xAA 自检 ping（`FRAME_TEST_PING=0`，代码 `#if` 保留开关）；保留逐口帧解析泵、RX_GUARD、诊断计数与 `refresh_chaneel`（150ms 新鲜度）。0xAA 接收能力保留、不再主动并发探活。

- `[stm32f0]` **fix**: 定位并缓解 4 号口(USART5)持续丢回显——根因 **RX 溢出(ORE)**：

- `[stm32f0]` **fix**: `UartSend` 不再用 `HAL_NVIC_DisableIRQ(uartirq)` 屏蔽整个共享中断来写 TX FIFO，改为**只清/置本口 `USART_CR1_TXEIE`**（TX 事件源门控）——消除“发一个口时把 USART3..6 四个口的 RXNE 一起关窗”造成的 ORE；TX FIFO 容量等待改为无锁读 `usTxCount`（ISR 只减不增，读旧值只会多等不会溢出）。
  - `bsp_uart_fifo.c`：`UartIRQ` RXNE 单字节处理改 **while drain**（一口气收完该口 pending 字节并刷新 ISR）；`USART3_6_IRQHandler` 改为**按 ISR 标志只服务有数据的 USART**（不再空轮询 4 口）；
  - `app.c`：50ms 老格式 GPIOHEAD 广播由“5 口同时发”改为**逐口错峰 5ms**（`s_query_next` 调度），拆开 5 口同时回显的峰值；
  - 保留诊断计数（`dbg_uart_ore/fe/full`、`rxByteCnt/varCrcFail/legCrcFail`）便于复测；预期 `dbg_uart_ore[4]` 大幅下降、`varCnt[4]` 追平。

- `[hc32f460]` `[stm32f0]` **feat**: 不定长帧接收增加**超时复位（RX_GUARD=50ms）**：解析器新增 `lastByteMs` 时间戳与 `frame_rx_guard()`，用系统 tick 差值判定半包悬挂（不新增定时器/不阻塞），超时即回 IDLE 防死锁；HC32 在 `Check_Uart_Pdu` 开头调用（`m_u32Tickms`），STM32 在 `Radar_thread` 逐口 pump 前调用（`HAL_GetTick`）。

- **验证记录（定长/不定长广播收发，驱动未改）**：交替广播模型（一拍只广播一类包：定长 0x55 ↔ AA 0xAA，负载档 0/8/32/N 轮换，收端 Check/aa5 分别计 rxcnt）：
  - 50ms 拍 + 50B：txcnt=rxcnt、errcnt=0（0 丢包，~4min）；
  - 50ms 拍 + 128B(134B帧)：24600/24598、errcnt=0（丢2包 0.008%，~4min）；
  - 20ms 拍 + 128B：55160/55153、errcnt=0（丢7包 0.013%）；
  - 结论：链路无 CRC 错；偶发整帧丢失率随 广播频率×帧长 轻微上升，机理为收端共享中断(USART3_6)在 5 口同时回显高峰偶发溢出整帧丢弃（非主循环/FIFO 满）。业务 ≤50B@50ms 实测 0 丢；>50B/高并发如需 0 丢 → 大包逐口发或 RX-DMA（未实施，备选）。

- `[hc32f460]` **refactor**: 重构 `Radar_Led_update`（`bsp_exint.c`）：新增统一助手 `led_blink_update(有效则Start/无效则Stop)`，按“报警中 / 安装模式(C1 AICAM+Radar / C2 AICAM / C3 Radar / C4 EAS / C5 Light)”分组，逻辑清晰化；**修复 C3(仅雷达)缺失的停止分支**——`bsp_get_radar_singal()==false`(无人)时停止 Radar_LED/B 灯（节拍 300）；C4/C5 原“B 灯选中即闪”行为按遗留保持。

- `[hc32f460]` **feat**: STM32↔HC32 链路支持不定长(0xAA)帧收发（老格式冻结不变）：
  - `bsp_rs485.c`：UART4 RX DMA 满 32B 的 TC 立即上抛并 `AOS_SW_Trigger()` 重挂（连续字节流可跨 32B 块），空闲超时只补推实际尾部字节（`got = 32 - count`，>0 才写）——支持 >32B/不定长连续接收；
  - `common.c`：新增帧核心（0xAA：AA+Len+Cmd+Addr+Payload+CRC16，Len=Addr+Cmd+Payload；老 0xFF/0x55 按 32B 帧扫描），`Check_Uart_Pdu` 改为逐字节泵；老帧解析/GPIOHEAD 应答/报警判定逻辑不变；0xAA cmd 0x01 回 0x81 回显（变长验证用）。
- `[stm32f0]` **feat**: 通道口(COM2..6) 不定长(0xAA)收发（IPC/COM1 Linux 段未动）：
  - `app.c` 新增同款帧核心与每口解析泵（仅 GET_RADAR_ENABLE=1 时编译）；legacy GPIOHEAD 应答更新 `Chaneel_ID[]`，改为“150ms 无新帧才清”（带迟滞，防误判无人）；
  - 每 1s 向 5 口发 cmd 0x01 变长 ping（负载 0/8/32/80B 轮换），HC32 回 0x81 验证双向变长收发；
  - 50ms 老 GPIOHEAD 轮询保留。

- **docs**：新增《Boot/OTA 设计稿 v0.1》（`docs/ota_boot_design.md`）：**A/B 双槽即运行区、无搬运**；选择器标志（双份+CRC）为选槽唯一真值，版本号不参与选槽；Boot 只做“读标志→校验(Magic/ImageLen/整包CRC32/TargetSlot)→跳槽”，TRIAL 试运行窗口(3s)+失败计数自动回退；按槽分别编译两份镜像（STM32 M0 无 VTOR 需向量表重映射，HC32 用 VTOR）；下载期页粒度擦写非活动槽，掉电矩阵任意时刻不砖；含 Linux→STM32→HC32 中继与内存布局（STM32 Boot16K/A64/B64/标志4K；HC32 Boot32K/A128/B128/标志8K）。**未改动任何固件代码**。

- **docs**：新增《设计定稿备忘 2026-09-04》（`docs/decisions_2026-09-04.md`）：汇总当日决策——老格式冻结(0xFF/0x55)+0xAA 变长新帧(Len+Cmd+Addr+Payload+CRC16)、按帧头分流的状态机接收引擎与三判决点/滑窗重同步、实时性=事件+周期+迟滞、流水线轮询、OTA A/B 槽/代理缓存、已确认产品口径与明天开工顺序。未改动任何固件代码。
## [0.1.0] - 2026-09-04

### Added

- **建立 git 单仓库**：在 `J:\dsh\alarm_board` 根目录 `git init`，纳入 `STM32F0_linux_v4.31` 与 `Radar_V4.2_2026_0425_MOS` 全部源码；新增 `.gitignore` 排除 IDE 生成物/编译输出，新增本文件与 `README.md`（架构、报警链路、协议说明）。
- **基线代码快照**：两套固件保持源码原状入库（未做任何逻辑改动），打基线 tag `v0.1.0`。
- 说明：仓库内源码含 GBK 编码注释（中文），提交/检出不改变字节内容（`core.autocrlf=false`）。

### [stm32f0] BSP

- （基线，无修改）
### [hc32f460] projects/source

- （基线，无修改）