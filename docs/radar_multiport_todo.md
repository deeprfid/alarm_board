# 雷达口多口化 —— 第 3 步执行清单（radar.c 每口一份 + 按口循环）

> **状态：第 3 步已交付（三口现场同时锁定）。完成记录、现场验收读数、判读速查见文末 §8。**
>
> 背景：第 2 步已完成（提交 `7b3bd77`）：`radar_port.c` 三口参数化（端口描述符 + 每口上下文 + 6 个 ISR 跳板 +
> 轮询 TXE 发送，不用 DMA/中断），`radar.c` 暂以 port 0 调用。第 3 步 = 把 `radar.c` 的状态与流程也按口拆分。

## 0) 现状（已完成，勿重复）

- `radar_port.h/.c`：多口 API，首参为 `uint8_t port`（0..2）；`RADAR_PORT_CNT = 3`。
- 硬件资源：雷达1 = USART1/PA2/PA3，雷达2 = USART2/PA0/PA1，雷达3 = USART3/PB14/PB15；
  中断线 INT013~INT018（半径2/3 的 RI/EI），**不用定时器、不用 DMA**。
- 换档 = 整套重来：关收发 → `USART_DeInit` → 重新 Init（分频随波特率选）→ **BRR 回读校验** → 清状态/NVIC → 重开收发 → 清缓冲。

## 1) 常量

- `radar_cfg.h`：`RADAR_DEV_CNT` = 3（与 `RADAR_PORT_CNT` 对齐）。

## 2) 状态改数组（全部加 `[RADAR_PORT_CNT]`，所有引用点加 `[port]`）

`s_rx`（分帧器）、`s_ack`、`s_ack_ready`、`s_probe_st`、`s_probe_idx`、`s_probe_t0`、`s_baud_locked`、
`s_rep_frames`、`s_ack_frames`、`s_reports`、`s_presence_src`、`s_comm_map`、`s_probe_rx0`、`s_rep_last_ms`、
`s_fps_ms`、`s_fps_cnt`、`s_fps`、`s_link_ms`、`s_link_bytes0`、`s_win_ack_cnt`、`s_link_last_rx_ms`、`s_link_sweep_ms`。

## 3) 回调按口分发

- `radar_port_set_rx_handler()` 的回调无参 → 在 `radar.c` 加 3 个跳板 `radar_rx_cb0/1/2`，各自调用 `radar_on_bytes(port, data, len)`。
- `radar_on_bytes(port, …)`：分帧、解析、计数（`s_rep_frames[port]`、`s_reports[port]`、ACK 计数等）全部落到该口。

## 4) 函数加口号（全部带 `uint8_t port`）

`radar_pump`、`radar_probe_tick`、`radar_probe_next`、`radar_probe_accept`、`radar_switch_baud`、
链路监控（1 秒判据 + 失联兜底）、`radar_cfg_cmd`（使能配置→命令→结束配置的 ACK 事务按口）。

## 5) 主循环 / 初始化

```c
void radar_poll(void) {
    for (p = 0U; p < (uint8_t)RADAR_PORT_CNT; p++) {
        radar_pump(p);            /* radar_port_poll(p) + tx_watchdog(p) */
        radar_probe_tick(p);      /* 探测/重扫(含 3 秒零字节兜底) */
        radar_link_check(p);      /* 1 秒窗口: 字节增量>=32 且合法帧==0 -> 重扫 */
    }
}
```

- `radar_init()`：`for (p…) { radar_port_init(p); radar_port_set_rx_handler(p, rx_cb[p]); }`。

## 6) Watch 输出（仍是 3 个变量，只改数组）

```c
volatile uint32_t g_radar_lock[RADAR_PORT_CNT];   /* 1=已锁定 */
volatile uint32_t g_radar_baud[RADAR_PORT_CNT];   /* 硬件实际波特率(反推) */
volatile uint32_t g_radar_comm[RADAR_PORT_CNT];   /* 位图+标志+BRR指纹+帧率 */
```

编码不变：bit0..7 = 各候选档是否收到过字节（顺序同 `RADAR_BAUD_TABLE`）；`0x100` 曾解出合法帧；
`0x200` 最近 1 秒仍在收；`0x400` 换档回读校验通过；bit16..23 = BRR 整数分频指纹；bit24..31 = 帧率 Hz。

## 7) 编译与验证

- 目标 **0 Error / 0 Warning**；
- 逐口验证：每口单独接模块，确认三项读数各自正确；**改其中一档，另两口状态不受影响**；
- 在线改档（APP）后应自动重扫并跟上（约 1~6 秒）。

## 8) 完成记录（第 3 步已交付）

- 编译：Keil 无头全量重建，5 种配置（默认 / `TARGET=460800` / `DUMP_ONCE=1` / `PARAM_EN=1` / 三者全开）**均 0 Error / 0 Warning**；默认配置 `Code=27416 RO-data=920 RW-data=80 ZI-data=9704`。
  其中 **`DUMP_ONCE=1` 单独**这一组抓出一处真 bug：`radar_link_ready()` 被误挪进 `#if (RADAR_PARAM_EN)`，该组合报 `#223-D: declared implicitly` + `L6218E: Undefined symbol radar_link_ready`，已改回 `#if ((RADAR_PARAM_EN != 0U) || (RADAR_DUMP_ONCE != 0U))`。

### 8.1 现场验收（三口同时上电，各自独立锁定）

| 口 | USART / 引脚 | `g_radar_lock` | `g_radar_baud` | `g_radar_comm` | 分频 | BRR 整数 | 帧率 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | USART1 / PA2,PA3 | 1 | 460800 | `0x0A1A0703` | DIV1 | 26 (0x1A) | 10 Hz |
| 1 | USART2 / PA0,PA1 | 1 | 115200 | `0x0A6B0707` | DIV1 | 107 (0x6B) | 10 Hz |
| 2 | USART3 / PB14,PB15 | 1 | 9600 | `0x0A130708` | DIV64 | 19 (0x13) | 10 Hz |

三个 BRR 整数与分频公式预测逐一吻合，`0x100/0x200/0x400` 三标志全置位 ⇒
**逐口独立探测、逐口独立换分频（9600 走 DIV64、另两口走 DIV1）、逐口 Watch 刷新全部成立**。

引脚功能号经数据手册核对无误：PA0/PA1 属 `Func_Grp1`（36/37 = USART2_TX/RX）、PB14/PB15 属 `Func_Grp2`（32/33 = USART3_TX/RX）；
PB15 的 Func7 列直接标注 `USART3_CK`，可交叉验证。（`bsp_rs485.h` 的 PB6/PB7 用 37/36 反解为 USART4，与实跑的 RS485 一致，佐证这套表理解无误。）

### 8.2 踩到的坑：雷达3 口 38400 以上收不到（**硬件，非固件**）

现象：口2 只有 9600 / 19200 能锁，38400 及以上失败；`g_radar_comm[2]` 低位位图全亮（八档都有字节）但解不出帧。

**根因：RX 线（PB15）上一颗对地滤波电容偏大。拆掉后三路 460800 全部自适应通过。**

判据（把「波特率选错」和「信号带宽不够」分开）：

- `RADAR_BAUD_LOCK_FRAMES = 1`，模块恒定 10 Hz 上报，每档 300ms 窗口内约 3 帧
  ⇒ **正确那一档只要信号干净，一轮扫描（2.4s）内必然锁定**；
- **正确档扫到了却不锁 ⇒ 不是波特率没选对，而是该波特率下信号本身不合格**。

量化窗口（UART 在 bit 中点采样，需信号基本建立，取 2.3τ ≈ 90%）：

| | bit 时间 | 采样点 | 要求 |
| --- | --- | --- | --- |
| 19200 能过 | 52.1 µs | 26.0 µs | 2.3τ < 26 µs ⇒ τ < 11.3 µs |
| 38400 过不去 | 26.0 µs | 13.0 µs | 2.3τ > 13 µs ⇒ τ > 5.7 µs |

⇒ 该路 **RC 时间常数约 6~11 µs**：1kΩ×6~11nF、4.7kΩ×1.3~2.3nF、10kΩ×0.6~1.1nF。
正常 ESD 滤波应在 **100 pF 量级**（τ ≈ 100 ns~1 µs），对 460800 也毫无影响。

**为什么能确定不是固件**：19200 与 38400 走的是**同一个 DIV64 分频、同一个 0.136% 误差**，
固件里不存在能切在两者之间的判据；且口0/口1 用同一份 `radar_port.c` 分别跑通 460800 / 115200。

### 8.3 判读速查（`g_radar_comm`）

| 现象 | 结论 |
| --- | --- |
| 位图全 0、帧率 0 | 线上没有任何东西 → 没接 / 没供电 / 没共地 / TX-RX 接反 / 引脚不对 |
| 位图有 bit、`lock=1`、帧率 > 0 | 正常 |
| 位图有 bit、**从不解出帧** | 信号在但质量不合格（带宽 / 接触电阻 / 畸变）—— §8.2 就是这一类 |
| 位图全亮、从不解出帧 | 严重畸变（八档只能收到乱码） |

两个**不要用**的判据：

- `0x100`（曾解出合法帧）**只置位不清零**，是「上电以来」而非「当前」；且**解出过帧 ≠ 锁定过**
  （它不是探测的判定条件；一帧合法帧出现在 `state 9` 那 5 秒空闲期也会把它点亮）；
- **bit0..7 位图是累积的**，不代表「当前这一轮」，只有整板冷启动后它才等于本轮结果。
  若现场需要「每轮」语义，可改为每轮扫描开始时清零 —— 属改编码语义，需另行确认。
