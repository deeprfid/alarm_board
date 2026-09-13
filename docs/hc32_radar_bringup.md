# HC32 报警板 雷达(LD2410C) 上板验证步骤

> 目的: 确认新雷达驱动(**USART1 + DMA + 空闲超时**)能收到模块的 `0x02` 上报, 且目标状态/距离合理。
> 调试输出是**临时模块** `radar_dbg.c/.h`, 总开关在 `radar_cfg.h` 的 `RADAR_DBG_EN`, 验证完按第 5 节清理。
> 本步骤只动 HC32 报警板固件, 与 STM32 中继板无关。

---

## 1. 前置检查

| 项目 | 值 | 来源 |
| --- | --- | --- |
| 串口 | `CM_USART1` | `radar_cfg.h` |
| MCU TX → 模块 RX | **PA2 / FUNC32** | `RADAR_UART_TX_PORT/PIN/FUNC` |
| MCU RX ← 模块 TX | **PA3 / FUNC33** | `RADAR_UART_RX_PORT/PIN/FUNC` |
| 模块 OUT(第 1 路) | **PC14** | `RADAR_PORT0/PIN0` |
| 候选波特率 | 256000 → 460800 → 115200 | `RADAR_BAUD_TABLE` |
| 工程/目标 | `projects/MDK/alarm_board.uvprojx` / `usart_uart_dma_Debug` | — |

- 与旧 `bsp_radar.c` 的宏逐项一致(旧文件注释里的 PB9/PE6 是错的)。
- **共地**, 模块供电按模组说明书；TX/RX 交叉接(MCU-TX→模块-RX)。
- 若原理图与上表不同: 只改 `radar_cfg.h` 中那 8 行宏, 其它代码不依赖具体引脚。

---

## 2. 方式 A(推荐, 零额外硬件): Keil Watch 窗口

1. 编译烧写, 进入调试(Start/Stop Debug Session)。
2. Watch 窗口加 **`g_radar_dbg`**(结构体, 展开全是十进制整数)与 **`g_radar_dbg_evt`**(最近一次事件的字符串)。
3. 全速运行(F5) 1 秒以上, 观察 `g_radar_dbg`。

### `g_radar_dbg` 字段

| 字段 | 含义 | 正常值 |
| --- | --- | --- |
| `ms` | 当前 1ms 计数 | 持续增长 |
| `rdy` | 波特率自适应是否结束 | 1 |
| `lock` | 自适应是否命中 | 1 |
| `baud` | 当前波特率 | 实际模块波特率 |
| `rep` | 解析出的上报帧数 | 持续增长 |
| `fok` / `fer` | 分帧成功 / 失败帧数 | fok 增长, fer 不变 |
| `rx` / `drp` | 串口累计收字节数 / 缓冲丢弃数 | rx 增长, drp=0 |
| `st` | 目标状态: 0 无 / 1 运动 / 2 静止 / 3 动静 / 4~6 底噪 | 有人时 1/2/3 |
| `mv_dist` / `mv_eng` | 运动目标距离(cm) / 能量 | 与手实际距离接近 |
| `st_dist` / `st_eng` | 静止目标距离(cm) / 能量 | — |
| `dd` | 探测距离(cm) | — |
| `out` | 模块 OUT 脚电平 | 有人=1 |
| `online` | 串口数据是否新鲜(1s 内) | 1 |
| `pre` | `radar_presence(0)` 最终判定 | 默认源=OUT |

(`g_radar_dbg_line` 是同一份数据的文本行, 供方式 B/C 用。)

---

## 3. 判读表(现象 → 结论 → 下一步)

| 现象 | 结论 | 下一步 |
| --- | --- | --- |
| `ms` 不增长 | 主循环没跑 / 停在断点 | 确认 `SysTick_Init(1000U)` 与 `main` 的 `for(;;)` |
| `rdy` 长期为 0 | 自适应没推进 | 确认主循环调了 `radar_poll()` |
| `rdy=1` 且 `evt="probe FAIL"` | 3 种波特率都没收到 ACK | 查 TX/RX 是否接反、模块供电、引脚宏与实板是否一致 |
| `rx=0` | 一个字节都没收到 | 重点查 RX(PA3) 与模块 TX, 以及共地 |
| `rx` 增长但 `fok=0`、`fer` 增长 | 收到的是乱码 | 波特率不是三者之一(先读回模块实际波特率) / 模块固件上报格式不同 |
| `fok`/`rep` 增长 | **串口链路 OK, 0x02 上报正在收** | 看 `st` 与距离 |
| `rep` 增长但 `st` 恒为 0 | 模块判定"无人" | 手在模块前 1~2m 晃动; 或探测区/灵敏度需配置 |
| `st=1/3` 且 `mv_dist` 随动作变化 | **运动目标判定正常** | — |
| `drp>0` | 环形缓冲溢出(消费太慢) | 检查主循环里是否有阻塞操作 |
| `out=1` 但 `st=0` | 模块 OUT 有输出、串口无目标 | 模块可能只输出 OUT, 或探测区参数不同 |
| `evt="rx stalled(no byte)"` | 收字节突然停止 | 线松/模块重启; 结合 `rx` 是否停在某个值 |

---

## 4. 方式 B / C(可选, 便于长时间观察)

### B. RS485 文本行
`radar_cfg.h` 里置 `RADAR_DBG_SINK_RS485 (1U)`: 每 500ms 从 RS485 主机口(USART4, PB6/PB7, 460800 8N1)输出一行 ASCII:

```
ms=12345 rdy=1 lock=1 baud=256000 rep=120 fok=120 fer=0 rx=3120 drp=0 st=1 mv=120:45 stl=0:0 dd=120 out=1 on=1 pre=1
```

用 USB-RS485 / 串口助手 @460800 查看。
**注意**: 与 STM32 挂同一总线时会和 STM32 每 20ms 的查询抢总线, 建议断开 STM32(或改用方式 A/C)。

### C. ITM / SWO
`radar_cfg.h` 里置 `RADAR_DBG_SINK_ITM (1U)`; Keil: Options for Target → Debug → Settings → Trace 勾选 Enable(SWO 时钟=内核时钟, 接 SWO 线), 然后 View → Serial Windows → **Debug (printf) Viewer**。

---

## 5. 验证通过后的清理(必做其一)

1. **最小改动**: `radar_cfg.h` 里 `RADAR_DBG_EN (0U)` —— 只剩空实现, 不占 Flash, 不影响业务; 或
2. **彻底删除**: 删 `radar_dbg.c` / `radar_dbg.h` → 从 Keil 工程两个目标移除 `radar_dbg.c` → 删 `main.c` 的 `radar_dbg_poll()` → 删 `main.h` 的 `#include "radar_dbg.h"`。

`radar_rx_bytes()` / `radar_rx_drop()` 是常驻诊断接口(值很小, 可长期保留), 不属于要删的调试代码。

---

## 附: 波特率自适应(当前实现)

- 候选顺序 **256000 → 460800 → 115200**, 每个波特率超时 `RADAR_BAUD_PROBE_TIMEOUT_MS=300ms`(`radar_cfg.h`)。
- 判定依据: 该波特率下发出 **"使能配置 0x00FF"** 命令后收到 **ACK 且 status=0** → 锁定该波特率。
- 命中后自动发 **"结束配置 0x00FE"** 退出配置态; 三个都失败 → **回落 256000**(仍可继续收上报, 只是发命令无效)。
- 全程**非阻塞**: 由主循环的 `radar_poll()` 推进(状态机 `radar_probe_tick()`), 上电不阻塞。
- 状态查询: `radar_ready()` = 探测已结束; `radar_baud_locked()` = 是否命中; `radar_get_baud()` = 当前波特率。
