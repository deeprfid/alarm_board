# OTA 侵入源工程分析报告

> 版本：v1.0（2026-08-29）
> 目的：梳理 OTA 功能对 hc32f4a0_app 核心文件的侵入，为架构收拢提供依据。
> 结论先行：**OTA 不是独立模块，而是侵入式改写了 4 个核心文件**——这是串口/启动/命令改动相互牵连的根源。

---

## 1. OTA 独立模块（12 个文件，正常）

| 文件 | 职责 |
|---|---|
| ota_agent.c | OTA 主流程（下载→校验→置标志→复位）|
| ota_download.c | 网络下载 |
| ota_flag.c | OTA 标志页 |
| ota_frame.c | OTA 帧解析 |
| ota_http.c | HTTP OTA 通道（8081）|
| ota_security.c | 验签（HMAC）|
| ota_state.c | OTA 状态/进度 |
| ota_storage.c | QSPI 暂存 + 通道锁 |
| ota_transport_uart.c | 串口 OTA 通道 |
| ota_usb_stream.c | USB-CDC OTA 通道 |
| ota_usb.c | USB OTA 启动 |
| usb_msc_ota.c | USB 虚拟 U 盘 OTA |

这些是**自包含**的，职责清晰，OK。

---

## 2. 🔴 侵入核心文件（4 个，问题源）

### 2.1 user_main.c（24 处）—— 侵入最重

| 位置 | 内容 | 问题 |
|---|---|---|
| L354-376 | **整个 `ota_uart_active_task` 函数** | OTA 串口线程内嵌在 user_main，不在独立文件 |
| L433 | `ota_agent_confirm()` | 业务初始化里插 OTA 确认（防回滚）|
| L819-826 | 暂存包版本检查 + `ota_set_progress(0)` | 启动流程被 OTA 插入 |
| L832-836 | `ota_agent_boot` / `usb_msc_ota_check` / `ota_usb_start` / `ota_http_start` | **OTA 启动链插入主启动** |
| L853 | `osThreadNew(ota_uart_active_task)` | OTA 线程创建 |

### 2.2 Lan2Uart.c（14 处）—— 侵入核心轮询

| 位置 | 内容 | 问题 |
|---|---|---|
| L219 | `ota_transport_uart_init(UART1)` | send_func 里绑 OTA 串口 |
| L542-554 | USB OTA 让路（`ota_channel_busy` / `ota_usb_stream_active`）| **send_func 主循环被 OTA 改写** |
| L564-569 | USB OTA 帧头转交（`ota_usb_stream_feed`）| send_func 抢 OTA 帧 |
| L577-606 | **UART1 OTA 帧拦截（整段）** | 命令口 OTA 帧处理内嵌 |

### 2.3 http_callback.c（11 处）—— 侵入 HTTP 接口

| 位置 | 内容 |
|---|---|
| L831-840 | `ota_update` 命令 → `ota_agent_run(url)` |
| L854-859 | `ota_switch` 命令 → `ota_switch_bank()` |

### 2.4 reader_msg.c（4 处）—— 侵入上报路径

| 位置 | 内容 |
|---|---|
| L1182-1185 | RecvCmd 让路（OTA 会话时跳过）|

---

## 3. 问题本质

OTA 的**启动、线程、串口拦截、命令处理**直接内联在核心业务函数中：

```
user_main（启动）    ← OTA 启动链 + 串口线程 + 版本检查
Lan2Uart（轮询）     ← OTA 帧拦截 + 让路 + 通道绑定
http_callback（HTTP） ← OTA 命令
reader_msg（上报）    ← OTA 让路
```

**任何串口/启动/命令的改动，都会被这些侵入点牵连**——串口互换失败即因此。

---

## 4. 收拢方案（把侵入式改为接口调用）

| 侵入点 | 现状 | 收拢后 |
|---|---|---|
| user_main OTA 启动链 | 内联 4+ 调用 | `ota_init_all()` 一行 |
| user_main OTA 串口线程 | 整个函数内嵌 | `ota_uart_start()`（ota_transport_uart.c）|
| Lan2Uart OTA 帧拦截 | send_func 内整段 | `ota_serial_pump(fd, buf, len)` 一行 |
| Lan2Uart OTA 让路 | 内联判断 | `ota_channel_pending(fd)` 一行 |
| reader_msg 让路 | 内联判断 | `ota_channel_pending(fd)`（复用）|
| http_callback OTA 命令 | 2 个分支 | 已较干净，可保留 |

**核心原则**：核心文件只留**一行接口调用**，OTA 内部逻辑全在 ota_*.c，互不干扰。

---

## 5. 待确认前提

1. **HEAD 基线（f858aab）能否正常开机 + 盘存**——重构前必须确认
2. 重构范围：只收拢侵入点（4 个文件），不动 OTA 功能逻辑
3. 每步可回退（git 提交）

---

## 5. 收拢实施计划（v1.0 待确认）

### 5.1 新增集成层：ota_integration.h / ota_integration.c（app 工程，GBK 编码）

| 接口 | 收拢内容 | 替代的侵入点 |
|---|---|---|
| `void ota_init_all(void)` | 暂存包版本检查 + ota_agent_boot + usb_msc_ota_check(reset) + ota_usb_start + ota_http_start | user_main.c L816-836（内联 5 处）|
| `void ota_confirm_after_init(void)` | ota_agent_confirm（防回滚）| user_main.c L433 |
| `void ota_uart_task_start(void)` | 创建 ota_uart_active_task 线程（线程体移入集成层）| user_main.c L354-380 内嵌函数 + L847-853 osThreadNew |
| `int ota_serial_pump(fd, head3)` | UART1 OTA 帧拦截（otabuf malloc + read_n + handle_frame），返回 1=已处理 | Lan2Uart.c L577-606 整段 |
| `int ota_usb_feed_pump(fd, head3)` | USB1 "OTA" 帧头转交 ota_usb_stream_feed | Lan2Uart.c L564-571 |
| `int ota_channel_pending(fd)` | (fd==USB1|UART1) && (ota_channel_busy() || ota_usb_stream_active()) | Lan2Uart.c L549-554 / reader_msg.c L1182-1188 |

### 5.2 4 个核心文件的改动（行为完全一致，git diff 可核对）

| 文件 | 改动 |
|---|---|
| user_main.c | include ota_integration.h；删 ota_uart_active_task 函数体；L433→ota_confirm_after_init()；L816-836→ota_init_all()；L847-853→ota_uart_task_start() |
| Lan2Uart.c | L549-554 让路→ota_channel_pending(gCurInfFd)；L564-571→ota_usb_feed_pump；L577-606→ota_serial_pump（custom 分支内先判）|
| reader_msg.c | L1182-1188→ota_channel_pending(commonfd) |
| http_callback.c | **不动**（ota_update/ota_switch 是命令处理，位置合理）|

### 5.3 编译接入

- uvprojx 的 app src 组新增 ota_integration.c（参照 ota_http.c 条目 L848-850）
- Keil 编译：app 工程 0 Error
- 不改 driver 工程（ota_integration 只调 app 内 OTA 头 + driverlib 接口）

### 5.4 验证步骤

1. 编译通过（0 Error / 0 Warning 新增）
2. 开机 + 盘存正常（前提：HEAD 基线 f858aab 确认可用）
3. 被动模式：串口 OTA 全流程（握手→下载→ACK→重启）
4. 主动模式：ota_uart_task_start 正常监听
5. USB OTA / HTTP OTA 各跑一次
6. TRACE 无新增异常（ota: 相关日志正常）

### 5.5 风险与回退

- 行为等价性：每个收拢点逻辑与原代码逐行对应，git diff 可核对
- 回退：git 单 commit 包裹，可整体 revert
- 不做的事：不动 OTA 功能逻辑（ota_*.c 本身）、不动 http_callback 命令、不动波特率/时序

### 5.6 当前状态（2026-xx）

- [x] 侵入清单（上文）
- [ ] ota_integration.h/c 已起草（未接入）
- [ ] user_main.c / Lan2Uart.c / reader_msg.c 收拢（**待确认后执行**）
- [ ] uvprojx 接入 + 编译验证

---




---

## 6. 单线程合并方案（OTA 分发线程，v1.0）

### 6.1 现状：3 个 OTA 线程，栈合计 32KB

| 线程 | 位置 | 栈 | 职责 | 运行模式 |
|---|---|---|---|---|
| ota_usb_task | ota_usb.c L17 | 1024x16 | 轮询 read(USB1) 到 ota_usb_stream_feed | CDC 模式 |
| ota_http_server | ota_http.c L181 | 1024x8 | select(SOCKET2) 到 http_handle_conn（阻塞下载）| 全部 |
| ota_uart_active_task | user_main.c L354 | 1024x8 | 轮询 read(UART1) 到 ota_transport_uart_feed | 仅主动模式 |

### 6.2 合并后：单 ota_dispatch_task（ota_integration.c）

ota_dispatch_task（栈 1024x16，省 16KB）
  - 初始化：USB1 ioctl（CDC 模式）/ SOCKET2 ioctl / UART1 open+init（主动模式）
  - for(;;) 轮询三通道：
      1) USB1 read（50ms 超时）到 ota_usb_stream_feed
      2) UART1 read（仅主动模式）到 ota_transport_uart_feed + idle_timeout
      3) SOCKET2 select（100ms 短超时）到 有连接则 http_handle_conn（阻塞下载）

### 6.3 关键设计点

| 点 | 决策 | 原因 |
|---|---|---|
| HTTP select 超时 | 10000ms 改为 100ms | 合并后必须快速轮询，否则 USB/串口被拖住 |
| 被动模式 UART1 | 分发线程不轮询（send_func 已读 UART1 + ota_serial_pump 拦截）| 双读竞争会抢 OTA 帧 |
| 主动模式 UART1 | 分发线程轮询（原 ota_uart_active_task 逻辑）| send_func 被动模式才启动 |
| USB stream finished / aborted | finished(升级重启)断出；aborted 继续（原独立线程 break 使 USB 通道失效，合并后应存活）| 行为微调需注明 |
| 通道互斥 | 沿用 ota_channel_busy（一次一个通道下载）| HTTP 阻塞期间串口/USB 本来就被锁拒绝 |
| 被动模式 send_func 帧拦截 | 不变（不经 OTA 线程）| 串口 OTA 被动模式仍走 send_func |

### 6.4 涉及文件

| 文件 | 改动 |
|---|---|
| ota_integration.c | 新增 ota_dispatch_task / ota_dispatch_start（替代 3 个线程创建）|
| ota_usb.c | ota_usb_start 删 osThreadNew（保留 init_usb + USB1 初始化）|
| ota_http.c | ota_http_start 删 osThreadNew（SOCKET2 配置移集成层）|
| user_main.c | 主动模式分支 ota_uart_task_start 到 集成层统一启动 |

### 6.5 风险

- HTTP 下载期间（阻塞）USB/串口通道轮询暂停——但通道锁已排他，行为等价
- 合并后 HTTP 阻塞拖住 idle_timeout 检查（串口/USB 会话中止检测延迟）——HTTP 下载本身持锁，无碍
- ota_usb_task 的 break 语义（finished/aborted 退出线程）在合并后需调整——升级完成走 system_reset，线程随之消亡，行为一致

