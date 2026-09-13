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
### [stm32f0] BSP / app

- **docs**: 按要求**精简**为"上行包格式定义 + 数据包解析"：
  - `docs/stm32_uplink_interface.md` 只保留：上行包帧结构表(32B/帧头 `0xFF`/CRC 覆盖 0..29 小端)、**数据包结构体定义**（与固件 `BSP/app.h` 的 `gpio_pdu` 完全一致：`FrameHead/Pdu_len/DeviceID/AntID/Rad_Status[8]/Alarm_Done[8]/GPIO[10]/uint16 crc`，32B 无填充）、字段定义与示例帧；删除了下行协议、串口参数、时序/心跳、健壮性建议、天线映射推导等与"上传包格式"无关的内容；
  - 解析代码收敛为单文件 `docs/stm32_gpio_pdu.hpp`：`crc16_ccitt()` + `GpioPdu` + `parse_gpio_pdu()`（校验帧头/帧长/CRC 后取字段），**不含收发、缓存、状态管理、统计与示例程序**；删除 `docs/cpp/`（示例程序、README、旧解析器）。

- **docs**: 新增 **Linux 侧接口文档 + 解析库**（供 Linux 主机解析 STM32F0 中继板上行帧）：
  - `docs/stm32_uplink_interface.md` **v1.0**：串口参数（COM1 @115200 8N1）、0xFF/32B 定长帧壳与 CRC-16/CCITT-FALSE（poly 0x1021/init 0xFFFF，覆盖 0..29，小端，自检向量 `"123456789"`→`0x29B1`）、上行 `gpio_pdu` 字段表（`Rad_Status[8]`/`Alarm_Done[8]` 按 **8 支 RFID 天线**、`GPIO[10]` 预留）、5 路雷达板→8 天线映射、发送时机（变化即报 / 1s 心跳 / 命令回执）、下行 `alarm_pdu` 字段表与 `AntID`=天线号 1..8 语义、LED 颜色码、键壮性/超时建议、CRC 参考实现（C/Python）与待定字段清单；
  - `docs/cpp/stm32_gpio_pdu.hpp`：仅头文件 C++11 解析库 —— `crc16_ccitt()`、`GpioPdu`（按天线号 1..8 的 `present()/alarming()`）、流式 `GpioPduParser`（任意切分喂入、逐字节重同步、CRC 校验、帧/CRC 错/重同步统计、回调或 `pop()` 取帧）、`GpioPduStatus`（状态快照 + 变化判定 + 链路判活，默认 2.5s）、`build_frame()` 自测组帧；
  - `docs/cpp/example_gpio_pdu.cpp` + `docs/cpp/README.md`：示例程序（`--selftest` 无硬件自测 / `-d /dev/ttySx -b 115200` 读串口 / 从 stdin 读）与编译说明（`g++ -std=c++11 -I docs/cpp ...`）。
  - 解析算法已用等价 Python 镜像验证：CRC 自检向量、整帧/逐字节喂入、噪声+半帧重同步、坏 CRC 丢弃、心跳帧不判变化。

- `[stm32f0]` **refactor**: LED 并发策略改为**"单一所有者 + 请求标志"**（承接上一条）：`LED_T` 增加 `ucStopReq`；`LED_Pro()`（SysTick 中断里运行，是 `LED_T` 的唯一所有者）开头先处理停止请求并调用 `Led_pwr_init()` 收尾（清全部计数 + `bsp_LedOff`）；`Led_Stop()` 退化为**只投递请求** —— 写 `ucEnalbe=0` + `ucStopReq=1` + 立即 `bsp_LedOff()`，**不再调用 `Led_pwr_init()`、彻底不需要临界区**（`mutex_led_lock/unlock` 与其 PRIMASK 备份变量整体删除），可在任意上下文安全调用；`LED_Start()` 改为"先写完整参数、最后置 `ucEnalbe=1`（单字节写原子）并撤销未决停止请求"，消除中断读到半套参数的撕裂。代价：`Led_Stop()` 后状态机最迟在下一个 10ms tick 内彻底停止（物理熄灭是立即的）。`bsp_led.h` 同步更新 `LED_T` 定义与注释。

- `[stm32f0]` **refactor**: `bsp_led.c` 的 LED 互斥改为标准临界区。原来 `mutex_led_lock/unlock` 用 `HAL_SuspendTick()/HAL_ResumeTick()`（低功耗 API）来挡 SysTick，等于**每次 `Led_Stop()` 都停掉全局时基** —— 临界区内的 tick 被永久丢失、`HAL_GetTick()` 少走，而 `bsp_RunPer10ms`/`BEEP_Pro`/雷达判活(`radarStaleMs`)/触发保持窗口/上行心跳全部依赖该时基；同时 `mutex_led` 标志只写不读，是假互斥。现改为 `__get_PRIMASK()` + `__disable_irq()` / `__set_PRIMASK()`：屏蔽期间 SysTick 异常只是**挂起**、解锁后立即补执行，**时基不丢**，也不再动用 HAL 低功耗 API；临界区仅几十条指令（约 1~2µs），对 460800 串口中断无影响。删除只写不读的 `mutex_led`；`bsp_led.h` 注明 lock/unlock 需成对且不可嵌套。

- `[stm32f0]` **chore**: 调试期配置调整（现场调试用）—— `bsp.h` 关闭看门狗 `STM32F0_IWDG_ENABLE (1U) → (0U)`；`bsp.c` 把 `rd_idkey_fun()` 移出 `#if STM32F0_IWDG_ENABLE` 改为**上电必调**（UID 校验不再随看门狗开关失效），并把本板 UID 期望值由 `0x587B3B44` 更新为 `0x03852952`；`MDK-ARM/STM32F030.uvprojx` 编译优化由 `-O3/oTime` 改为 `-O0`（便于单步调试）。**注：量产烧录前需把 IWDG 打开、优化调回。**

- `[stm32f0]` **fix**: 修复"关闭某路雷达后 LED 常亮不更新 / 触发信号一直为 1"——根因:**STM32 侧的雷达状态只在收到 HC32 的 Cmd 0x10 应答时才写入**, 该口若不再应答(该路雷达被关闭、板子掉线、接线断开), `sPorts[i].radarVal` 会**冻结在最后一次的 1**, 保持窗口永不结束 → LED 常亮、`Host_IRQ` 恒为 1。修法(STM32 侧自己判活/实时刷新, 不依赖 HC32 侧):
  - `portRx_t` 增加 `lastRxMs`(该口最后一次**有效** Cmd 0x10 应答时刻), `radarPumpPort()` 收到有效应答时刷新;
  - 新增 `radarStaleMs = 200ms`(约 10 个轮询周期)与 `radarPortFresh()`/`radarPresence()`:**应答超时一律按"无人"处理**;
  - `radarTriggerOut()` 改用 `radarPresence()`, 保持窗口结束时**显式 `Led_Stop()` 熄灭该口 LED**(不再只靠 LED 驱动收尾), `Host_IRQ` 每拍都按当前状态重写;
  - 上行 `gpio_pdu` 组帧同样门控: 应答过期的口 `Rad_Status`/`Alarm_Done` 上报 0(新增 `sIpcReportVals` 存本拍实际值, 变化判定基于它), 避免 Linux 端看到冻结值。

- `[stm32f0]` **refactor**: 简化雷达触发输出实现——去掉自定义的 LED 指针表/刷新逻辑，**直接调用已有驱动**：`switch(口)` → `LED_Start(&Port_x_LED, PORTLED_x, 10, 10, 1)` 仅在"有人"上升沿调用一次（闪烁节拍交给 LED 驱动，由 `bsp_RunPer10ms()`→`LED_Pro()` 推进）；触发保持只用一个时刻数组 `sTrigMs[stmPortCnt]` + `radarTrigHoldMs = 1000ms`，窗口内为 1、全部口超时后 `HAL_GPIO_WritePin(..., GPIO_PIN_RESET)`。

- `[stm32f0]` **feat**: 新增**雷达有人触发输出** —— `app.c` 新增 `radarTriggerOut()`，在 `Radar_thread()` 每 `radarPollMs`(20ms) 一拍调用：任一口轮询到的雷达状态为"有人"（`sPorts[i].radarVal == 1`，来自 HC32 0xAA Cmd 0x10 应答 Byte0 的雷达位）时，①输出触发信号 `HAL_GPIO_WritePin(Host_IRQ_GPIO_Port, Host_IRQ_Pin, GPIO_PIN_SET)`；②点亮该口对应 LED `LED_Start(&Port_x_LED, PORTLED_x, 10, 10, 1)`；两者保持 `radarTrigHoldMs = 1000ms`（窗口内持续有人则每拍刷新续期，保持为 1/常亮；窗口结束后 `Host_IRQ` 拉低、LED 由 `LED_Pro` 收尾熄灭）。口→LED 映射：COM6→Port_1_LED/PORTLED_1、COM2→Port_2、COM3→Port_3、COM4→Port_4、COM5→Port_5。
- `[stm32f0]` **fix**: `bsp.c` 中 `Host_IRQ(PA12)` 上电初始化电平由 `GPIO_PIN_SET` 改为 `GPIO_PIN_RESET`（触发信号为 1，空闲必须为 0，避免上电即出现假触发）；并清掉 `radarPumpPort()` 里遗留的空 `if` 块。

- `[stm32f0]` **clean**: 清理 `BSP/bsp.c`、`BSP/bsp_beep.c` 中未使用的代码（`bsp.c` 353 → 313 行，`bsp_beep.c` 175 → 132 行，`bsp_beep.h` −4 行，`bsp.h` −1 行）：
  - `bsp.c`：删除 `STM32F030_delay()`（0 调用，`bsp.h` 中对应原型一并删除）与 HAL 断言钩子 `assert_failed()`（`stm32f0xx_hal_conf.h` 里 `USE_FULL_ASSERT` 处于注释状态，`assert_param` 展开为 `(void)0`，无人调用——**若今后启用 `USE_FULL_ASSERT` 需把它加回**），连同其 `#ifdef USE_FULL_ASSERT` 空壳与两段孤立注释；删除 `bsp_Init()` 中指向已删函数的注释调用 `// EXTI4_15_IRQHandler_Config();`。
  - `bsp_beep.c`：删除 `mutex_beep_lock()`/`mutex_beep_unlock()`（只被 `BEEP_Stop` 调用）、`BEEP_Stop()`（只被 `BEEP_Pause`/`BEEP_Resume` 调用）、`BEEP_Pause()`、`BEEP_Resume()`、`BEEP_KeyTone()`（三者 0 调用）及静态变量 `mutex_beep`、`bsp_beep.h` 中对应 5 条原型；另外删掉被注释掉的旧 `BEEP_ENABLE/BEEP_DISABLE`（引用已删的 `GPO_BZ_GPIO_Port/GPO_BZ_Pin`）与 `BEEP_InitHard()` 里注释掉的 HC32 风格 GPIO 初始化片段。保留 `BEEP_InitHard`/`BEEP_Start`/`BEEP_Pro`、`g_tBeep`、在用的一对 `BEEP_ENABLE/BEEP_DISABLE`（`GPO_BZ3V3_*`，被 `BEEP_Start`/`BEEP_Pro` 使用）。

- `[stm32f0]` **clean**: 仅清理 `BSP/bsp_uart_fifo.c`/`bsp_uart_fifo.h` 中未使用的接口（`bsp_uart_fifo.c` 1441 → 1133 行，−308 行；**其它文件未动**）：删除 `comSendChar()`（唯一调用者 `fputc()` 本次一并删除）、`fputc()`/`fgetc()`（stdio 重定向，工程内无 `printf`；且 `fgetc` 阻塞在 COM1 会破坏 IPC）、`uart_recv()`、`UartGetRxcnt()`、`comClearTxFifo()`、`comClearRxFifo()`、`comSetBaud()`、`bsp_SetUartParam()`（只被 `comSetBaud` 调用）、`ComToUSARTx()`（只被 `comSetBaud` 调用）、`UartSendBlocking()`（含文件顶部的前置声明），以及它们在 `bsp_uart_fifo.h` 的全部原型和各函数上方的 doc 注释块。保留全部在用接口：`bsp_InitUart`/`UartVarInit`/`InitHardUart`/`UartSend`/`UartGetChar`/`UartIRQ`+`uart_dma_rx_move`/`uart_dma_rx_cfg`/`UartIRQ_DmaIdle`/`uart4_dma_tx_start`/`uart6_dma_tx_start`/`UartTxEmpty`/`UartTxWait`/`comSendBuf`/`comGetChar`/`ComToUart`、6 个 `MX_USARTx_UART_Init`、5 个中断处理函数，以及 `UART_T` 全部成员、`UARTx_FIFO_EN`/`UARTx_BAUD`/`UARTx_*_BUF_SIZE`/`UARTx_DMA_RX`/`UART_DMA_LEN` 宏与 6 组收发缓冲。

- `[stm32f0]` **clean**: `BSP/bsp.h` 删除未使用的宏与声明（126 → 116 行）：`__STM32H7_BSP_VERSION`（含其上方注释行）、`EXTI9_5_ISR_MOVE_OUT`、`ERROR_HANDLER()`、`GPI_IN1_GPIO_Port`/`GPI_IN2_GPIO_Port`/`GPI_IN3_GPIO_Port`（`bsp.c` 的 `MX_GPIO_Init` 直接写 `GPIOC`，只用到 `GPI_INx_Pin`）、`GPO_BZ_GPIO_Port`（仅被 `bsp_beep.c` 里注释掉的旧 `BEEP_ENABLE/DISABLE` 行引用）以及既无定义也无引用的原型 `bsp_GetCpuID()`。保留全部在用项：`GPI_INx_Pin`/`GPI_IN4_GPIO_Port`、`GPO_BZ3V3_*`、`LED_B/R/G_*`、`MCULED_x_*`、`GPO1/2_*`、`CM4RESET_*`、`Host_IRQ_*`、`GPO_BZ_Pin`、`STM32F0_IWDG_ENABLE`(`1U`)/`GET_RADAR_ENABLE`、`ENABLE_INT`/`DISABLE_INT`、`BSP_Printf`、`TRUE`/`FALSE`、`STM32_V7`/`USE_RTX`、各 `#include` 与 `bsp_Init`/`bsp_Idle`/`System_Init`/`Error_Handler`/`STM32F030_delay`/`CM4_System_Reset` 原型。同时纳入工作区里 `STM32F0_IWDG_ENABLE` 由 `0U` 改 `1U` 的改动。

- `[stm32f0]` **clean**: 仅清理 `BSP/bsp_gpio.c`/`bsp_gpio.h` 中未使用的代码（**其它文件未动**）：删除 `beep_on()`、`beep_off()`、`gpo_set()`、`PIO_GpioRead()`、`PIO_GpioSet()`、`gpi_get()`、`gpi_get_all()`、`EXTI4_15_IRQHandler_Config()` 及其在 `bsp_gpio.h` 的全部原型（`gpo_set` 只被自身原型引用，`beep_on/off` 只被 `gpo_set` 调用，`PIO_GpioRead` 只被 `gpi_get/gpi_get_all` 调用，`EXTI4_15_IRQHandler_Config` 在 `bsp.c:95` 只有一行注释引用）。保留 `PIO_GPIOInit()`（`bsp.c:99` 调用）、`EXTI4_15_IRQHandler()`（向量表）、`HAL_GPIO_EXTI_Callback()`（HAL 回调）及文件内的 `/* END OF FILE */` 注释；`bsp_gpio.c` 135 行 → 44 行，大括号 4/4 配平。备注：`bsp.h` 里的 `GPI_IN1..4_Pin/GPIO_Port` 随 `PIO_GpioRead` 一起失去引用，但按要求未动 `bsp.h`。

- `[stm32f0]` **clean**: 仅清理 `BSP/bsp_led.c`/`bsp_led.h` 中未使用的函数、变量与原型（**其它文件未动**）：删除 `LED_GPIO_Init()`、`Alarm_Off()`、`Check_alarm_status()`、`Green_pass_Tag()`、`Reguler_Tag()`、`Relay_AM_EAS()`、`Alarm_SilenceCmd()`、`Alarm_On()`（后 4 个仅被前 4 个死函数调用，构成调用闭环，全工程引用数 0 已逐项核对）、变量 `alarm_duration`/`rgb_led_status`、`extern BEEP_T g_tBeep;`，以及 `bsp_led.h` 中无定义也无引用的原型 `bsp_LedToggle()`。保留全部在用接口：`bsp_InitLed`/`bsp_LedOn`/`bsp_LedOff`/`LED_Start`/`Led_Stop`/`LED_Pro`/`Led_status_update`/`Led_pwr_init`/`mutex_led_lock`/`mutex_led_unlock`/`bsp_RunPer10ms` 及 `PORTLED_x`/`LED_xLED` 宏。

- `[stm32f0]` **clean**: `BSP/app.c`/`app.h` 删除未使用的应用层代码（脚本全工程符号引用统计确认 0 引用，**库文件 `Drivers/`、armfly BSP 驱动、HAL 回调/中断向量/stdio 重定向一律未动**）：未用函数 `rfid_app()`、`Alarm_CMD()`（其唯一调用者）、`CalcCRC()`（`ipcCrc()` 保留且仍用 `CRC_calcCrc8()`）；未用宏 `frameTestPing`（连同 `#if frameTestPing` 的 `pingOne()` 自检块）、`ipcReportPort3b`、`NONE_EAS_CODE`/`AUX_EAS_CODE`（各重复定义两份）、`ALARM_NONE_CODE`、`ACT_TAGDATA`/`ACT_GPICHANGE`/`ACT_TAGCOMING`、`MAX_EPCLEN`、`MAX_BOARD_CNT`；未用变量 `uart3Tx..uart6Rx` 及只写不读的 `radarCntTx()`/`radarCntRx()` 计数链、`portRx_t.lastRcvMs`（成员与其赋值）；未用原型 `rfid_app`/`Alarm_CMD`。**保留**：`ipcReportVar20En=0` 包住的 0xAA Cmd 0x20 变长上行（按约定后续要用）、`frameEvVarBad` 返回值、`frameAaEn`/`GET_RADAR_ENABLE` 等特性开关。

- `[stm32f0]` **clean**: Linux 链路帧头统一为 **PDUHEAD(0xFF)**，**GPIOHEAD(0x55) 废弃并删除**（响应帧头 = 下行帧头 = PDUHEAD）：上行 `gpio_pdu` 的 `FrameHead` 改为 `PDUHEAD`；`app.h` 删除 `GPIOHEAD`/`RADAR_PDU_LEN`/`radar_pdu` 结构体/`Send_RadarStatus_to_Master()` 原型；`app.c` 删除 `Send_RadarStatus_to_Master()`(8B 应答)、`Broadcast_Get_Radar_Status()`、`Check_RadarStatus()`、`stmHandleLegacy()`(通道口 0x55 解析)、旧计数式 `Check_Uart_Pdu()`(`#if !frameAaEn` 分支)、`Chaneel_ID[]`/`refresh_chaneel()`（随 8B 应答一起失去作用）及 `txcnt/rxcnt` 诊断计数；帧核心去掉 `frameHdrLegGpio`，定长 32B 分支只由 `0xFF` 触发；`ipcHandleLegacy()` 只认 PDUHEAD。最终：COM1 下行 = PDUHEAD 32B（0xAA 变长预留），上行 = PDUHEAD `gpio_pdu` 32B（变化即报 + 1s 心跳 + 查询立即应答）。**STM32↔HC32 侧仍为 0xAA Cmd 0x10 查询/3B 应答，未改**。

- `[stm32f0]` **change**: STM32→Linux(COM1) 上行**统一为 `gpio_pdu` 单一格式**，老的 8B `radar_pdu` 应答停用（`ipcRadarPduRptEn=0`，函数与代码保留）：Linux 下发 `0xFF` PDUHEAD(AntID≠0) 时不再回 8B 帧，改为 **立即应答一帧 32B gpio_pdu**（`ipcReportOnQuery=1` → `ipcReportForce()`）。gpio_pdu 组帧/发送拆为 `ipcReportBuild()`/`ipcReportSend()`，周期路径 `ipcReportStatus()` = 变化即报 + 1s 心跳，查询路径 `ipcReportForce()` = 立即发送。**0xAA 变长上行（旧 Cmd 0x20）后续要用，以 `ipcReportVar20En=0` 保留**；STM32↔HC32 侧 0xAA Cmd 0x10 链路不变。

- `[stm32f0]` **chore**: 明确 COM1 上行**两路并存**并加开关——①主动：32B `gpio_pdu`（变化即报 + 1s 心跳）；②应答：Linux 下发 `0xFF` PDUHEAD 且 AntID≠0 时回 8B `radar_pdu`（`GPIOHEAD`，`alarm_done=Chaneel_ID[AntID]`）。新增 `ipcRadarPduRptEn`（默认 1=保留老应答，置 0 即只走 32B 主动上报）；`Send_RadarStatus_to_Master()` 增加通道号 1..8 越界保护（Linux 可下发任意 AntID，原实现 `Chaneel_ID[antid]` 会越界读）并在发送前 `UartTxWait(COM1,5ms)`，避免与 32B 心跳帧撞车。

- `[stm32f0]` **change**: STM32→Linux(COM1) 上行格式改为 **Linux 给定的 `gpio_pdu` 定长 32B 帧**（`app.h` 新增该结构体）：`[0x55][Pdu_len=32][DeviceID][AntID][Rad_Status[8]][Alarm_Done[8]][GPIO[10]][CRC16_L][CRC16_H]`（CRC 覆盖前 30B，与老帧一致）——**变化即报 + 1s 心跳**。`Rad_Status[n]`/`Alarm_Done[n]` 由 5 个通道口状态按“通道号 1..8 → 下标 0..7”填入（一个口可覆盖 2 个通道）；顺带把 `Chaneel_ID[8]` 扩为 `Chaneel_ID[9]`，修掉通道 8 的越界写。`GPIO[10]`/DeviceID/AntID 语义待 Linux 侧确认（暂填 0）。旧 0xAA Cmd 0x20（5×3B）上报以 `ipcReportVar20En=0` **代码保留**（`ipcReportStatusVar20()`）。**STM32↔HC32 侧 0xAA Cmd 0x10 查询/3B 应答完全不变。**


- **fix**: COM1(Linux IPC) 接收由“`UartGetRxcnt>=32` 定长取包”改为**帧头分流状态机**（`app.c` 新帧核心 `frameRxInit/frameRxGuard/frameRxFeed` + `frCrc16`）：`0x55`/`0xFF` 按 32B 定长、`0xAA` 按 `AA+Len+Cmd+Addr+Payload+CRC16` 变长；`PDUHEAD` 帧分发 `ipc_hpm_message()`（GPIOHEAD 查询应答暂 `#if 0`）。修复两处接收失效：泵内未刷新 `lastByteMs` 导致 `frameRxGuard` 每拍误复位状态机（Linux 包收不到）；CRC 失败时误调 `comClearRxFifo` 破坏与中断共享的 FIFO 索引（改为直接丢弃、下一个帧头重同步）。
- **feat**: 上行新增 **0xAA Cmd 0x20 五口状态聚合帧**：`[AA][Len=17][0x20][00][5×(gpioIn,workMode,alarmDone)][CRC16]`，数据源为 0xAA Cmd 0x10 应答（雷达位图 / 安装模式位图 / 报警完成），**变化即报 + 1s 无变化心跳**；`Radar_thread` 周期（`radarPollMs=2000`）向 5 口发 Cmd 0x10 查询（COM6/COM2/COM3/COM4/COM5 ↔ 通道 1..9）。
- **fix**: 发送前增加 **TX 空闲等待** `UartTxWait(port, 5ms)`（`bsp_uart_fifo.c/.h` 新增，基于 `UartTxEmpty`）：消除 UART4/UART6 DMA 忙时 `uart4/6_dma_tx_start` **静默丢包**（雷达查询与 Linux 报警包并发时丢报警）。
- **perf**: `UartGetChar` 增加“FIFO 空”快速路径（无临界区直接返回），仅在有数据时进一次临界区出队——主循环逐字节泵的关中断开销显著下降。
- **refactor**: `app.c` 本轮新增标识符统一改为 **lowerCamelCase**（函数/变量/宏共 51 个，如 `frameRxFeed`/`stmVarSend`/`radarQueryAll`/`sPortCom`/`frameRxGuardMs`/`frameAaEn`）；`portRx_t` 删除未使用成员（`port/rxByteCnt/varCnt/varCrcFail/legCrcFail/varAddr`）。
- **chore**: `bsp.c` 中 `CM4_System_Reset()` 移入 `#if STM32F0_IWDG_ENABLE`（与看门狗使能一致，不再无条件拉复位脉冲）。

### [hc32f460] projects/source

- `[hc32f460]` **fix**: 0xAA Cmd 0x10 应答 Byte2 `alarm_done` 原来是**写死的 1**（占位: "answering 期间恒 1"），导致 STM32 上行 `gpio_pdu.Alarm_Done[8]` 8 个通道全是 1，即使 Linux 从未下发报警包。改为**实时报警状态**：`out[2] = (R_tLED.ucEnalbe || g_tBeep.ucEnalbe) ? 1 : 0`（红灯在闪或蜂鸣器在响 = 1，报警结束 `Alarm_Off/Check_alarm_status` 后自动回 0）；`bsp_report.c` 增加 `extern LED_T R_tLED; extern BEEP_T g_tBeep;`，`bsp_report.h` 注释同步。STM32 侧无需改动（Byte2 透传到 `Alarm_Done[ch]`，并已按应答新鲜度门控）。注：绿放行 `G_tLED` 不计入报警，如需计入再加一个判断。
- `[stm32f0]` **docs**: 明确 `gpio_pdu` 的 `Rad_Status[8]`/`Alarm_Done[8]` 是**按 RFID 模块 8 支天线**（下标 = 天线号 1..8），而 STM32 侧只有 **5 路雷达板**（COM6/COM2/COM3/COM4/COM5）：一块雷达板覆盖 1~2 支天线，板级状态复制到其全部天线 —— `COM6→天线1、COM2→2,3、COM3→4,5、COM4→6,7、COM5→8`（`sPortCh` 加中文注释说明，组帧处措辞由"通道"改为"天线"）。`Alarm_Done` 语义确认为 **A：该天线所属雷达板正在声光报警**（HC32 红灯/蜂鸣器在动作，停即回 0）。README 的 `gpio_pdu` 字段表与映射说明同步更新。
- `[hc32f460]` **feat**: **重写雷达驱动**（原 `bsp_radar.c` 753 行整段删除）—— LD2410C 从"仅读模块 OUT 引脚"扩展到"串口精细判定 + 参数读写 + 底噪自检"，分层、全非阻塞：
  - 新增 `radar_cfg.h`（配置集中：UART 实例/引脚/波特率候选/DMA/TMR0/缓冲/超时/设备数）、`radar_port.c/.h`（USART1 + DMA2 CH1/RX + TMR0 空闲超时 + 软件环形缓冲，按**实收字节数**上抛；DMA2 CH0/TX 非阻塞发送 + busy 标志）、`radar_frame.c/.h`（纯逻辑分帧：4 字节魔术字滑窗 → 按 `datalen` 变长收帧 → 帧尾校验 → 半包超时复位 + 统计，可 PC 单测）、`radar_proto.c/.h`（命令组帧、ACK 按命令字匹配、上报帧解析 0x02 普通/0x01 工程、参数解析）、`radar.c/.h`（设备状态 + `radar_poll()` 非阻塞主循环 + 命令事务 + 参数/底噪 API + 有人判定策略）；
  - **波特率自适应**：由 `radar_poll()` 非阻塞推进，依次试 256000/460800/115200，谁能回"使能配置(0x00FF)"ACK 就锁定谁，全失败回落 256000；`radar_ready()` 表示探测结束；
  - **解析**：目标状态(0 无/1 运动/2 静止/3 动静/4 底噪检测中/5 成功/6 失败)、运动与静止距离(cm)与能量、探测距离；工程模式额外 9 个距离门能量 + 光感值 + OUT 脚状态；
  - **命令**：`radar_read_params()`(0x61)、`radar_set_sensitivity()`(0x64)、`radar_set_max_gate()`(0x60)、`radar_set_resolution()`(0xAA)、`radar_set_uart_baud_index()`(0xA1)、`radar_eng_mode()`(0x62/0x63)、`radar_noise_start()`(0x0B)/`radar_noise_status()`(0x1B)，内部自动"使能配置→命令→结束配置"；
  - **有人判定可选源** `radar_set_presence_src()`：OUT(默认，保持现行为) / UART / 二者取或 / UART 优先掉线回落 OUT；串口数据 1s 无更新视为离线；
  - **集成**：`main.c` 在 `SysTick_Init()` 之后 `radar_init()`、主循环 `radar_poll()`（替换原 `Check_Radar_state()`）；`main.h` 改 include `radar.h`；`common.c` 老 32B 确认帧的雷达字段改取 `radar_report(0)`；`bsp_report.c` 注释同步；`alarm_board.uvprojx` 增删源文件；
  - **删除**：`bsp_radar.c`/`bsp_radar.h` —— 含从未被调用的 `Rardar_init()`、阻塞收发、固定 45B 匹配、`get_pdu_len`/`config_frame_init` 调试代码，以及 3 个只有声明没有定义的 `Radar_singal_input/output`/`Alarm_Out_Enable`；
  - **待硬件确认**：雷达串口按旧宏沿用 **USART1 / PA2(TX,FUNC32) / PA3(RX,FUNC33)**（旧文件注释里的 PB9/PE6 是错的）；若 PCB 实际不同，只需改 `radar_cfg.h` 中的 8 行。
- `[hc32f460]` **fix**: 雷达驱动重构后的**编译修复**（Keil MDK ARMCC V5.06 update 7，目标 `usart_uart_dma_Debug`）：
  - `main.h`：补回 `stc_radar_scan_data_t` 结构体（老 32B 确认帧的雷达字段，字段名与 `common.c` 中 `radar_report_t` 的赋值一一对应）与 `HashConfig()` 原型；
  - `bsp_led.h` / `bsp_led.c` / `bsp_gpio.h`：补回 `BOARD_LED_1/2_PORT/PIN`、`RADAR_BOARD_LED_G_PORT/PIN` 宏与 `Board_LED_Init()`（板载指示灯初始化，`main.c` 上电自检调用；`Board_LED_On/Off/Toggle` 无调用者，未恢复）；
  - `radar.c`：补 `#include "radar_port.h"` —— 原先缺失导致 9 个 `radar_port_*` **隐式声明**告警（C 中隐式声明按 int 返回，`radar_port_get_baud()` 的返回值会被截断，属实质缺陷而非纯告警）；
  - **构建验证**：以 Keil 命令行无头构建复核（`UV4.exe -b alarm_board.uvprojx -j0 -o <log>`）—— `Code=29636 RO-data=760 RW-data=80 ZI-data=8232`，**0 Error / 0 Warning**（Debug 目标；Release 目标共用同一份源文件清单）；
  - **硬件配置复核**（与 git 历史中的旧 `bsp_radar.c/.h` 逐项比对，非推测）：串口 `CM_USART1`、TX=**PA2/FUNC32**、RX=**PA3/FUNC33**、OUT0/1/2=**PC14/PC13/PH02** —— 新 `radar_cfg.h` 与旧工程完全一致（旧文件里的 `PB9`/`PE6` 是写错的残留注释）。

- `[hc32f460]` **feat(调试)**: 新增雷达**上板验证**调试段（声明在 `radar_dbg.h`，实现在 `radar.c` 末尾的 `#if (RADAR_DBG_EN != 0U)` 段；总开关 `radar_cfg.h` 的 `RADAR_DBG_EN`，验证通过后可整体删除）：
  - 输出：结构体 `g_radar_dbg`（Keil Watch 一眼看全：rdy/lock/baud/rep/fok/fer/rx/drp/st/运动与静止距离能量/dd/out/online/pre）＋文本行 `g_radar_dbg_line`＋事件行 `g_radar_dbg_evt`（boot / 自适应探测结果 / 目标状态跳变 / 帧错误 / rx 停滞）；另可选 ITM(SWO) 与 RS485 主机口 ASCII 输出（`RADAR_DBG_SINK_ITM` / `RADAR_DBG_SINK_RS485`，默认关，后者会与 STM32 的 20ms 查询抢总线故默认关闭）；
  - 只调用驱动公开接口读状态，不碰驱动内部；不引入 printf（自带极简整数转 ASCII），`RADAR_DBG_EN=0` 时为空实现、不占 Flash；
  - 新增常驻诊断接口 `radar_rx_bytes()` / `radar_rx_drop()`（串口累计收字节数 / 环形缓冲丢弃数）——区分“没收到字节(接线/波特率)”与“收到但分帧失败(格式)”，现场排查用；
  - `main.c` 主循环增加 `radar_dbg_poll()`（内部 500ms 节流）；验证步骤 / 字段速查 / 现象判读表见 `docs/hc32_radar_bringup.md`；
  - **刻意不新建 .c 文件**：Keil GUI 打开工程时会用内存中的工程覆盖 `.uvprojx` 的改动（实测把已加入工程的 `radar_dbg.c` 覆盖掉，链接报 `L6218E: Undefined symbol radar_dbg_poll`），故调试段并入 `radar.c`，只需重新编译、不动工程文件；该工程目标另改为便于调试的设置（DebugInformation=1、Optim/oTime 由 -O3 改 0）。
- `[hc32f460]` **feat/fix(雷达波特率)**: 上板发现 `lock=0, baud=256000`（自适应三个候选都没收到 ACK，回落默认），针对性改动：
  - 新增 **固定波特率** `RADAR_BAUD_FORCE`（`radar_cfg.h`，当前 **460800UL**）：非 0 时跳过自适应、直接用该波特率（`radar_init()` 里生效，`radar_baud_locked()` 置 1）；填 `0` 恢复自适应。现场怀疑模块不在候选波特率里时逐个试最省事；
  - **放宽自适应命中判据**：原来要求 ACK 且 `status==0` 才算命中，现在 ① 只要收到 `0x00FF` 的 ACK（无论状态）即判定波特率正确（状态非 0 只表示命令没被接受）；② 没等到 ACK 但已收到 `RADAR_BAUD_LOCK_FRAMES(3)` 个**合法帧**也算命中（典型场景：模块 TX→MCU RX 通、MCU TX→模块 RX 断，此前会一路试到 115200 再回落）；
  - 调试段新增 **原始字节抓包** `g_radar_dbg_hex`（换波特率即清空，抓前 24 字节十六进制）：开头 `F4 F3 F2 F1`/`FD FC FB FA` 说明波特率正确，乱码说明波特率不对，空说明 RX 方向不通——这是区分“接线问题”和“波特率问题”的关键证据；
  - 调试段新增 **一次性改模块波特率** `RADAR_DBG_SET_BAUD_IDX`（默认 0=关；填 8 = 发 `0x00A1` 让模块切到 `RADAR_DBG_SET_BAUD_VALUE(460800)`，成功后驱动同步切波特率并重建分帧，结果记在 `g_radar_dbg_evt`）；
  - **候选波特率扩到协议表 6 全部 8 档**（256000/460800/115200/9600/19200/38400/57600/230400，无 921600）：
    模块波特率是**掉电保存**的配置项（协议 §2.2.9，出厂默认 0x0007=256000，改过就一直是改过的值），不能假设 256000；
    8 档全扫约 8×305ms≈2.5s（非阻塞），`RADAR_BAUD_FORCE` 复位为 `0`(自适应)，需要钉死时再填具体值。
  - `RADAR_DBG_SET_BAUD_IDX` 一次性改模块波特率改为**完整时序**：0x00A1 设置 → 0x00A3 重启模块（协议规定配置"重启后生效"，模块未切前驱动必须留在旧波特率）→ 800ms 后驱动再切到 `RADAR_DBG_SET_BAUD_VALUE` 并重建分帧，每步都记事件。
  - 构建验证：4 种组合（FORCE=0 自适应8档 / FORCE=460800 / FORCE=0+SET_BAUD_IDX=8 / FORCE=256000）均 0 Error 0 Warning。
- `[hc32f460]` **feat(把模块波特率改成 460800)**: `RADAR_DBG_SET_BAUD_IDX=8` 的一次性流程补齐**自检与回退**——使能配置 → `0x00A1(0x0008)` → `0x00A3` 重启模块（协议规定该配置"重启后生效"，模块未切前驱动必须留在旧波特率）→ 800ms 后驱动切到 `RADAR_DBG_SET_BAUD_VALUE` 并重建分帧 → 自检 2.5s 看有无上报帧：成功记 `baud verify OK 460800`；失败自动回退旧波特率再看 2.5s，分别记 `old baud still OK` / `no data on either baud: check wiring`。注：厂家固件里的"出厂默认 256000"无法更改（`0x00A2` 恢复出厂即回到 256000），本流程是把 460800 写进**模块自己的 flash**，从此这块模块上电就是 460800（每块需各做一次）。
- `[hc32f460]` **fix(雷达 TX 卡死 -> 命令全部 LL_ERR_BUSY)**: 上板 `s_dump.ok=0`、`last_ret=0xFFFFFFFA`（= -6 `LL_ERR_BUSY`）—— 说明 `radar_port_tx_busy()` 一直为 1：发送完成链（DMA2_CH0 TC -> 使能 USART1 TCI -> 清 busy）任何一环没来，`s_tx_busy` 就永远不清，之后所有命令都发不出去（探测阶段的 `radar_send_raw` 也会静默跳过 -> 只能靠"上报帧"锁定波特率、产线配置必然失败）。改动：
  - **兜底自愈**：新增 `radar_port_tx_watchdog()`（由 `radar_pump()` 每拍调用），超过 `RADAR_TX_TIMEOUT_MS(50ms)` 仍未收到完成中断 -> 关 TX DMA/关 USART TX/清标志/重新使能，然后**放行后续发送**（否则一条卡死会永久废掉命令通道）；
  - **定位用计数**（Keil Watch 直接看名字）：`g_radar_tx_dma_tc_cnt`（TX DMA 完成次数）、`g_radar_tx_tci_cnt`（USART1 发送完成中断次数）、`g_radar_tx_timeout_cnt`（兜底复位次数）。判读：DMA TC=0 → DMA 没跑/没触发；TC>0 而 TCI=0 → 中断映射/使能问题；timeout>0 且命令能通 → 只是完成通知没来，已被兜底放行；
  - 构建验证: 0 Error 0 Warning（Code 28560）。
- `[hc32f460]` **feat(雷达参数: A 探测行为参数 + C 只读/维护)**:
  - **A 组新增写接口**: `radar_set_aux_control(mode, threshold, out_level)`（0x00AD 光感辅助/OUT 默认电平）—— 补齐 A 组最后一条；其余（`radar_set_max_gate` 0x0060 / `radar_set_sensitivity` 0x0064 / `radar_set_resolution` 0x00AA / `radar_eng_mode` 0x0062·0x0063 / `radar_noise_start·status` 0x000B·0x001B）此前已有；
  - **A 组自动配置（幂等，默认关闭）**: `radar_cfg.h` 的 `RADAR_PARAM_EN` + 一组目标值宏（最大运动/静止距离门、无人持续时间、9+9 门灵敏度、光感辅助、OUT 默认电平）。上电在自适应锁定波特率后：先 `0x0061`/`0x00AE` 读回当前配置与目标逐项比对 → **只写不一致的项**（每拍只发一条命令，不长时间占住主循环）→ 写后读回复检；全一致则一条命令都不发。状态 `radar_param_state()`（4 成功/本来就一致, 5 失败）。门 0/1 的静止灵敏度按协议不可设置，比对与写入均跳过；距离分辨率（需重启生效）不纳入自动配置；
  - **C 组新增只读/维护接口**: `radar_read_resolution()`（0x00AB）、`radar_read_aux_control()`（0x00AE，配 `radar_aux_t`）、`radar_read_fw_version()`（0x00A0，配 `radar_fw_t`）、`radar_read_mac()`（0x00A5）、`radar_factory_reset()`（0x00A2）—— 加上已有的 `radar_read_params()`（0x0061）与 `radar_restart()`（0x00A3），C 组全部补齐；
  - 新增 `radar_read_all()` + 汇总结构 `radar_dump_t`（`s_dump`）：一次把 参数/分辨率/辅助控制/固件版本/MAC 全读回，`RADAR_DUMP_ONCE(默认 1)` 时上电自动读一次，Keil Watch 直接看 `s_dump`（`ok=5` 表示 5 项全读回成功）—— 无串口条件下唯一的"看当前配置"手段；
  - `radar_proto` 层新增 `radar_proto_parse_aux/parse_fw/parse_bytes` 与命令字 `0x00AD/0x00AE`；`radar_link_ready()`（探测结束且产线配置跑完才允许发命令）；
  - 构建验证: 参数配置=0/1 × 读回=0/1 × 调试开/关 共 6 种组合均 0 Error 0 Warning（Code 27712 ~ 29056）。
- `[hc32f460]` **clean(删除 `out_pin`)**: 删掉 `radar_report_t.out_pin` —— 它是"**工程模式**上报帧里的 OUT 脚状态"字节，正常工作模式帧里根本没有该字段，我们工程模式默认关闭 → 该字段恒 0，纯死数据；OUT 的真实电平用 `radar_dev_t.out_present`（直接读 PC14，与模块配置无关）即可，两者信息重复。改动：
  - `radar_proto.h` 删除字段；`radar_proto.c` 删除普通帧的清 0 与工程模式帧的解析；`common.c` 删除 `HC32_RS485_corfirm_PDU.radar.pinout = rr->out_pin;`（该确认帧目前**只填不发**，其 32B 布局里的 `pinout` 占位保留、恒 0，已加注释说明）；
  - 构建验证: 0 Error 0 Warning（Code 27712，比改动前少 20 字节）。
- `[hc32f460]` **clean(调试快照瘦身)**: `g_radar_dbg` 删掉全部冗余成员（~200B → 72B），只留真正要看的：
  - 保留: `ms`(主循环活着) / `probe_st`(探测阶段) / `lock`(是否找到波特率) / `baud` / `prov_st`(目标波特率配置结果) / `rep`(上报数) / `fer`(分帧错) / `rx`(收字节数) / `rx_head[8]` / 目标状态与距离: `st/mv_dist/mv_eng/st_dist/st_eng/dd` / `out/online/pre`；
  - 删除: `repf/ackf/fok`（与 `rep/fer` 重复）、`drp`、`probe_idx`（`baud` 已表示正在试哪一档）、`ack_cmd/ack_status/ack_len/ack_data[8]`（ACK 布局排查用, 已完成使命）、4 条**事件环** `evt_cnt/evt_code[]/evt_val[]/evt_ms[]`；
  - 随之删除 `radar_dbg_note_u32()` 与全部 13 处调用、`radar_dbg_ack_dump()` 及其调用 —— 驱动代码更干净（`radar.c` 从 798 → 710 行）；
  - 构建验证: 目标460800+调试开 / 目标=0 / 调试关 / 两者都关 四种组合均 0 Error 0 Warning（Code 28108 / 27480 / 27732 / 27092）。
- `[hc32f460]` **refactor(波特率配置收敛为一个开关)**: 按使用方口径收敛 —— **上电自适应(不管模块当前是多少) → 用当前波特率把模块改成 RADAR_BAUD_TARGET(默认 460800, 写模块 flash) → 0x00A3 重启 → 重跑自适应复检 → 之后每次上电模块自己就是 460800**：
  - 配置项只留一个 `RADAR_BAUD_TARGET (460800UL)`（0 = 不自动配置, 只跟随模块）；原 `RADAR_PROVISION_BAUD` 并入它, `RADAR_PROVISION_RESTART_MS` → `RADAR_PROV_RESTART_MS(500)`（内部）；
  - **删除 `RADAR_BAUD_FORCE`**（强制固定波特率）与 `radar_init()` 里的对应分支：与"自动配置"重复, 且设错会让链路直接不通 —— 一律走"自适应 + 自动配置"；
  - 候选表把 `460800` 提到第一档（配置好之后一档命中, 约 1.1s 出数据）, 其余 7 档仅兼容未配置的模块；
  - 构建验证: `RADAR_BAUD_TARGET=460800` / `=0` / `=460800+调试关` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(产线配置复检方式)**: 上板"运行 30 秒后仍是 `baud=256000 lock=1`、模块没切成 460800" —— 原实现是"固定等 2.5s 听一个波特率", 模块重启耗时不定, 且若模块其实**已经**切到 460800 而这两秒半里没听到, 会**错误地回退**到 256000（此时模块在 460800、驱动在 256000 → 链路反而断了）。改为:
  - 写完 `0x00A1` + `0x00A3` 重启后, 等 `RADAR_PROVISION_RESTART_MS(500ms)` 就**重新武装自适应探测**（`s_probe_st=0`, 内部自带 `RADAR_PROBE_BOOT_MS(1s)` 启动延时）, 按同样的"上报帧"判据重新确定模块当前波特率；
  - 复检结果: 又锁定在目标波特率 → 成功（`prov_st=4`）; 锁定在别的波特率 → 保持那个波特率继续工作（`prov_st=5`）, 不再"猜一个波特率硬听再回退"；
  - 删除 `RADAR_PROVISION_VERIFY_MS` 与 `s_prov_rep`/`s_prov_baud`（不再需要固定时间窗与回退路径）；
  - 构建验证: 产线配置=460800 / =0 两种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(复位循环)**: 上板出现"一直在重启", 根因是上一版引入的**无限递归** —— `radar_provision_tick()`（由 `radar_poll()` 调用）里调用了阻塞式的 `radar_set_uart_baud_index()`/`radar_restart()`, 而它们内部走 `radar_cmd()` 等 ACK 时会调 `radar_poll()` → 又回到产线配置状态机 → 无限递归 → 栈溢出/主循环饿死 → 看门狗复位（周期约 10.7s = 65536×8192/PCLK3(50MHz)）。修法（结构性，不靠喂狗）：
  - 拆出**底层泵** `radar_pump()`（只做 分帧超时 + 搬运字节, 不跑探测/产线配置状态机）, `radar_poll()` = `radar_pump()` + 探测 + 产线配置 + OUT 脚采样；
  - `radar_cmd()` 等 ACK / 等 TX 的循环改用 `radar_pump()` —— 阻塞命令从此**不可能**再进状态机, 递归在结构上被消除；
  - 另加 `s_prov_busy` 忙标志作为兜底（阻塞命令执行期间 `radar_provision_tick()` 直接返回）；
  - 构建验证: 产线配置=460800 / =0 两种组合均 0 Error 0 Warning。
  - 注: 看门狗本身没问题 —— `WDT_FeedDog()` 由主循环里的 `Check_UidKey()` 每圈投喂, 超时按 `WDT_CNT_PERIOD65536`+`WDT_CLK_DIV8192`(PCLK3=50MHz) 约 10.7s, 平时远够。
- `[hc32f460]` **feat(产线配置模块波特率)**: 把"让模块波特率固定为 460800"做成**常驻功能**（在此之前只有调试段里那个临时开关，默认关、且会随调试代码一起删除）：
  - 新增 `RADAR_PROVISION_BAUD`（`radar_cfg.h`，**当前默认 460800UL**；0 = 关闭）、`RADAR_PROVISION_RESTART_MS(800)`/`VERIFY_MS(2500)`、`RADAR_BAUD_IDX_TABLE`（波特率 → 协议表 6 索引）；
  - 新增 `radar_provision_tick()`（`radar.c`，由 `radar_poll()` 驱动, 非阻塞、**幂等**）：自适应找到模块当前波特率后 —— 已是目标值则**什么都不发**；否则 `0x00A1` 写入 → `0x00A3` 重启模块（协议规定该配置"重启后生效", 故模块重启前驱动留在原波特率）→ 800ms 后驱动切到目标波特率 → 自检 2.5s：收到上报即成功, 收不到则**自动回退原波特率**继续工作；
  - 新增查询接口 `radar_provision_state()`（0 待做 / 1 写入中 / 2 等重启 / 3 自检中 / 4 成功或本就是目标值 / 5 失败已回退 / 6 无法配置），调试快照增加 `g_radar_dbg.prov_st`；
  - **删除**调试段里的一次性开关 `RADAR_DBG_SET_BAUD_IDX`/`RADAR_DBG_SET_BAUD_VALUE`（已被上面这条正式流程取代）；事件码 12~18 归入"产线配置流程"（去掉原 19/20）；
  - 产线用法（详见 `docs/hc32_radar_bringup.md`）：直接烧本固件, 每块板上电约 4.5s 完成配置（`prov_st=4` 即成功）, 模块 flash 里从此是 460800；出正式版本时可把 `RADAR_PROVISION_BAUD` 改回 0, 并**建议保留自适应**以防有人误做"恢复出厂"（模块会回到 256000）；
  - 构建验证：产线配置=460800 / =0 / =256000 / 产线开+调试关 / `FORCE=460800` 五种组合均 0 Error 0 Warning。
- `[hc32f460]` **refactor(调试输出)**: **删除雷达调试的全部串口/printf/文本输出** —— 本板没有连电脑的串口, 这些通道无用且占空间。具体:
  - 删除 ITM(SWO) 与 RS485 两个输出通道(`RADAR_DBG_SINK_ITM`/`RADAR_DBG_SINK_RS485`、`dbg_send_itm/dbg_send_rs485/dbg_sink`、`bsp_rs485.h` 依赖)；
  - 删除状态文本行与事件字符串(`g_radar_dbg_line`/`g_radar_dbg_evt`/`g_radar_dbg_hex`/`g_radar_dbg_ack` 及全部整数转 ASCII 助手)；
  - 调试信息改为**纯数值快照** `g_radar_dbg`（Keil Watch 展开看）: 在原有字段上补充 `probe_st`/`probe_idx`（探测状态机）、`repf`/`ackf`（上报帧/ACK 帧计数）、`rx_head[8]`（本档收到的最前面 8 字节, 判波特率用）、`ack_cmd/ack_status/ack_len/ack_data[8]`（最近一帧 ACK），以及 4 条**数值事件环** `evt_cnt/evt_code[]/evt_val[]/evt_ms[]`（码表见 `radar_dbg.h`）；
  - `radar_dbg_note_u32(code,val)` 取代原字符串版本, 探测每一步都记码；
  - 代码量下降（Code 28520 → 27916）, `RADAR_DBG_EN=0` 时 27120；
  - 构建验证: 自适应 / `SET_BAUD_IDX=8` / `RADAR_DBG_EN=0` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(波特率自适应判据)**: 上板出现"第一次 `lock=1 baud=9600`、第二次又回到 `256000`"的抖动 —— 说明判据不可靠。改动：
  - **判据从"任意 ACK"改为"上报帧"为准**：某档收到 ≥`RADAR_BAUD_LOCK_FRAMES(3)` 个上报帧直接认定；只收到 ACK 时先发 `0x00FE` 退出配置态, 再等 `RADAR_PROBE_VERIFY_MS(1000ms)` 用上报帧验证, 收不到就继续试下一档（ACK 帧仅 10 字节, 错波特率下的乱码/残留字节可能凑出魔术字+帧尾被误判；上报帧 13 字节且内容自校验）；
  - **上电先等 `RADAR_PROBE_BOOT_MS(1000ms)` 再探测**：模块没启动完成时命令不会被应答, 会让第一档(256000)误判失败, 一路试到尾后回落 256000；
  - **换波特率时丢弃接收缓冲残留字节**：新增 `radar_port_rx_flush()`（`radar_port.c`）并在 `radar_port_set_baud()` 内部调用 —— DMA 窗口/环形缓冲里上一档的字节不再参与本档判定；
  - **补 `radar_restart()`（0x00A3）语义化接口**：改波特率(0x00A1)/分辨率(0x00AA)/蓝牙(0x00A4)/密码(0x00A9)/恢复出厂(0x00A2) 需重启才生效, 灵敏度(0x0064)/最大距离门(0x0060) 立即生效 —— 已写进注释；上板调试的一次性改波特率流程改用它；
  - 调试信息加强：`g_radar_dbg.repf`/`g_radar_dbg.ackf`（上报帧/ACK 帧计数）与 `radar_dbg_note_u32()`；每个探测步骤都记事件, `g_radar_dbg_evt` 可直接读出扫描过程(try baud × / ack, verify baud × / lock × / no report at × / probe FAIL, fallback 256000)；
  - 构建验证：自适应 / `SET_BAUD_IDX=8` / `FORCE=9600` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(雷达 ACK 匹配)**: 上板 `lock=0` 的**另一个重要嫌疑**——原来要求 `ACK.cmd == 0x00FF && status == 0` 才算命中, 但协议 V1.09 的 ACK 示例里命令字**高字节写作 01**(如使能配置 ACK `FD FC FB FA 08 00 FF 01 00 00 01 00 40 00 ...`), 按 cmd(2)+status(2) 解析会得到 `cmd=0x01FF`, 即使接线完全正常也**永远匹配不上**。改动：
  - 新增 `RADAR_ACK_CMD_MATCH(ack_cmd, cmd)`（`radar_proto.h`）——只比命令字**低字节**（LD2410 命令全是 `0x00xx`, 低字节唯一, 可同时兼容文档的两种写法）, `radar_cmd()` 改用该宏；探测阶段改为**收到任意合法 ACK 帧即锁定波特率**（探测期只发过 0x00FF, 任何 ACK 都是它回的）；
  - 调试段新增 `g_radar_dbg_ack`：最近一帧 ACK 的 `c=xx / st=xx / d=N` + 原始字节十六进制, 用于现场核对 ACK 真实字段布局（文档示例自身不一致）；
  - 构建验证：自适应8档 / `FORCE=460800` / `SET_BAUD_IDX=8` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix**: `bsp_rs485.h` 补 **include guard** —— 该头文件原先没有 guard，同一编译单元被包含两次即报 `#256: invalid redeclaration of type name "alarm_pdu"`（本次由 `radar_dbg.c` 显式包含时暴露）。同目录 `bsp_alarm.h` / `bsp_exint.h` / `bsp_pwm.h` 同样缺 guard，当前无二次包含，未改动。
- **构建验证**：Keil 命令行无头构建 4 种组合全部 **0 Error / 0 Warning** —— 默认（DBG_EN=1、两个 SINK=0）`Code=28180`；SINK_ITM=1 `Code=28260`；SINK_RS485=1 `Code=28296`；恢复默认后全量重建 `Code=28180`。

- **clean**: 与 STM32 侧同步清除 GPIOHEAD —— 删除 `common.c` 的 `Send_RadarStatus_to_Master()`（32B `GPIOHEAD(0x55)` 应答，已无触发来源）与 `Get_pdu_data()` 里的 `GPIOHEAD` 分支（该分支只做应答，删除后落入原有 `else` 复位路径）、帧核心 `FRAME_HDR_LEG_GPIO`（定长 32B 分支只由 `0xFF` 触发）、`bsp_rs485.h` 原型、`main.h` 的 `GPIOHEAD` 宏。HC32↔STM32 仍为 0xAA Cmd 0x10 查询 / 3B 应答 + 0xFF 32B 报警命令转发，协议未改。README 协议章节同步更新（下行 PDUHEAD 唯一、上行 `gpio_pdu` 字段表、通道链路 0xAA 说明）。
- **feat**: 新增 `bsp_report.c/.h`：`bsp_report_build()` 组 3B 上报负载——Byte0 GPIO_IN 位图（bit0..2 雷达 PC14/PC13/PH2 高有效、bit3 GPIO_IN1=摄像头 PB0 低有效、bit4 GPIO_IN2=继电器 PB1 低有效）、Byte1 安装模式位图（LIGHT/SYNC/RADAR/AICAM/EAS）、Byte2 alarm_done；`common.c` 0xAA **Cmd 0x10** 查询回该 3B，`CMD 0x01` 仍回 0x81 回显。
- **refactor**: `Radar_Led_update()`（`bsp_exint.c`/`main.h` 原型）改为 **void 返回 + 直接 IO 控制**（旧 `LED_Start/Led_Stop` 定时闪烁逻辑 `#if 0` 保留）：报警(R/G) 优先；蓝灯 = presence 且**保持 1s**（`RADAR_PRESENCE_HOLD_MS`，消除雷达 ~100ms 脉冲间隙导致的闪烁）、`LIGHT_ON` 常亮；板载 Radar_LED 跟随实时 presence（不保持）。

## [0.1.0] - 2026-09-04

### Added

- **建立 git 单仓库**：在 `J:\dsh\alarm_board` 根目录 `git init`，纳入 `STM32F0_linux_v4.31` 与 `Radar_V4.2_2026_0425_MOS` 全部源码；新增 `.gitignore` 排除 IDE 生成物/编译输出，新增本文件与 `README.md`（架构、报警链路、协议说明）。
- **基线代码快照**：两套固件保持源码原状入库（未做任何逻辑改动），打基线 tag `v0.1.0`。
- 说明：仓库内源码含 GBK 编码注释（中文），提交/检出不改变字节内容（`core.autocrlf=false`）。

### [stm32f0] BSP

- （基线，无修改）
### [hc32f460] projects/source

- （基线，无修改）