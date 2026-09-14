# 雷达口多口化 —— 第 3 步执行清单（radar.c 每口一份 + 按口循环）

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
