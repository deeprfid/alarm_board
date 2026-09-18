# ota/ —— OTA1 帧协议可移植核心（上位机端发送器 + 帧核心）

> 设计依据：`docs/ota_boot_design.md` v0.2（协议规格见其 §9）与 `docs/decisions_2026-09-18.md`。

## 文件

| 文件 | 角色 | 来源 |
| --- | --- | --- |
| `ota_frame.h/.c` | OTA1 帧核心：CRC16 / 组帧 / 解析 / 流式状态机。**平台无关纯逻辑** | 与 HC32F4A0 工程 `hc32f4a0_app/projects/app/{inc,src}/ota_frame.*` 逐字一致 |
| `ota_host.h/.c` | **上位机端发送器**：包头握手 / 分块 / ACK 跟随 / 超时探测 / 卡死保护 / 收尾。非阻塞状态机 | 移植自 C# `ReaderUI_v1_MCU/OtaUpdater.cs`（`Update()` 串口分支）+ `OtaProtocol.cs`，与 `tools/ota_send.py` 语义一致 |

## 去哪里

- **HC32F4A0 主机**：把 4 个文件加入 `hc32f4a0_app/projects/MDK/hc32f4a0_app.uvprojx`（`ota_frame.*` 该工程已有，可只加 `ota_host.*`）；
  集成层只需填 `ota_host_io_t` 的 4 个回调：
  ```c
  static int  io_write(void *ctx, const uint8_t *b, uint32_t n) { return (write(*(int*)ctx, b, n) < 0) ? -1 : 0; }
  static int  io_read (void *ctx, uint8_t *b, uint32_t n)       { return read(*(int*)ctx, b, n); }
  static uint32_t io_tick(void *ctx)                            { return (uint32_t)osKernelGetTickCount(); }
  ```
  然后 `ota_host_init()` → `ota_host_start(pkg, len)` → 周期 `ota_host_poll()`。
- **HC32F460 报警板**：只需 `ota_frame.*`（接收端解析），不需要 `ota_host.*`。

## 关键约束（改动前必读）

- **偏移语义 = 串口语义**：含 82B 包头的绝对偏移，`total = len(pkg)`。切勿与 USB-CDC/WinUSB 的「载荷相对偏移」混用（见设计文档 §9.2）；
- **首帧只发一次**：串口设备没有重复包头保护，burst 会把包头重复写进载荷（C# 侧曾因此卡在 41370）；
- **分块 256B**：267B 的帧正好落在报警板 512B DMA 窗口内，接收路径零改动；
- **不乐观推进偏移**：`off` 只在设备确认后跟随，因此 `on_progress` 报告的是已确认进度；
- **掉电不落盘进度**：会话内断点续传，掉电即整包重传。

## 编译验证

AC5（ARMCC 5.06u7）单文件编译，两个工程均为 `uC99=1`：

```
armcc --cpu=Cortex-M4.fp.sp --c99 -c -O1 -I ota ota_frame.c   # 0 error / 0 warning
armcc --cpu=Cortex-M4.fp.sp --c99 -c -O1 -I ota ota_host.c    # 0 error / 0 warning
```

注意：`ota_frame.c` 用了 C99 的 `for (uint32_t i = ...)`，AC5 必须带 `--c99`（两个工程默认已开）。

## 待做

- 报警板侧接收端（`ota_host` 的对端：写非活动槽 + 回 ACK/RESUME）；
- 报警板侧 A/B Boot；
- F4A0 侧集成层（通道选择、业务帧互斥、进度与结果上报）。