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