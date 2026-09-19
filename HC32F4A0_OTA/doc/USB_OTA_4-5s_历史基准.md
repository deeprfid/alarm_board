# USB OTA 速度历史基准（用户实测记录）

## ⚠️⚠️ 用户明确指令（2026-09，务必遵守，勿再违反）⚠️⚠️
- **不要再确认 QSPI 的读写速度**——用户已确认几十遍，再确认就是浪费时间（用户原话："别再确认QSPI的读写速度了，已经确认几十遍了，不要再浪费时间"）
- 涉及 QSPI 读写速度的讨论/实验/分析到此为止，一律不再做
- 排查 USB OTA 慢问题时，聚焦 USB 侧（驱动调用、URB、读策略），不要再回到 QSPI

## 关键事实
- **以前 USB 下载达到过 4-5 秒**（用户实测，与 HTTP 同水平）
- 当前（96677e6 + 本次改动）：20s，每批 R=281ms（ACK 传输延迟）
- HTTP 当前实测：t_erase=1201ms t_write=628ms total=4879ms（4.9s）

## 设备端实测（USB，每帧耗时 TRACE）
- 帧处理：7ms/帧（QSPI 写 4096B）
- 64KB 擦除：~170ms/次，6 次共 ~1s
- 设备处理 98 帧 ≈ 1.7s（不是瓶颈）

## C# 批计时（每批 2 帧）
- W=0ms（Write 快） R=281ms（等 ACK） U=0ms（UI 快）
- 设备处理 2 帧只要 14ms → 267ms 是 ACK 传输/读取延迟

## 回环测试（对照）
- 64B：500 iter/s（2ms/次）
- 4107B：93 iter/s（10.8ms/次，380KB/s）
- 结论：USB 传输本身快，连续传输无延迟

## 推论
- 4-5s 时代无 281ms 延迟 → 当前是回归
- 本次会话唯一非原版改动：usb_send XFRC 等待（g_usb_cdc_xfer_done）→ 首要嫌疑
- 对照实验：回滚 XFRC 等待 → 测 R 是否恢复

## 回环 vs OTA：usb_send 快慢的本质（用户问"回环为什么快"）
- 回环：read(128)→write(128) 连续 33 次 → USB IN 端点持续有数据 → 主机 usbser.sys 高频轮询 → 每段 XFRC ~0.1ms
- OTA：QSPI 写后孤立发 15B ACK → 之后无数据 → IN 端点持续 NAK → Windows usbser.sys NAK 降频 → ACK 等下次低频轮询 → 281ms
- 同一 usb_send 路径，差异 = 传输连续性

## 最终结论（2026-08-31 全部证据闭合）
- 串口 OTA 也慢（非 4-5s）；仅 HTTP 4.9s（t_erase=1201ms t_write=628ms total=4879ms）
- "以前 USB 4-5s" 疑为 HTTP 误记（HTTP 实测吻合 4.9s）
- **USB 20s 根因 = Windows usbser.sys 对稀疏批量 IN 的 ~265ms 取走周期（驱动固有）**
  - C# 三种读法（ReadByte/批量 Read/轮询）total 恒 265ms → 与 C# 无关
  - keep-alive（设备持续发 0x00）无效 → 不是"设备空闲→降频"
  - 设备 usb_send XFRC = 1ms → 设备发送瞬间完成
  - 回环连续流 10.8ms → usbser.sys 对连续流高频
- 已确认正确的修复（保留）：
  - usb_send XFRC 等待（连续 usb_deveptx 覆盖修复，回环 5000 次验证）
  - 重复 header 不回 ACK（残留 ACK 污染修复）
  - C# ReadAck 批量读（v1.7）
- 剩余选项：
  a) batch 8/16 减少 ACK 次数（适配驱动固有周期——13 次×265ms≈3.4s+设备 1.7s≈5-6s，唯一不改驱动的实用方案）
  b) 换驱动架构（libusb/WinUSB 绕过 usbser.sys）
  c) USB OTA 改用 HTTP 通道（已 4.9s）

---

# WinUSB 通道阶段（v1.10 → v1.14，2026-09）

## 背景与目标
- 用户要求"单独加一路 WinUSB（与串口/HTTP 并列），通过配置参数选择初始化；USB(CDC+MSC)/HID 不变"
- 动机：usbser.sys 稀疏 IN 265ms 取走周期无解 → 换 winusb.sys（声称无 265ms 限制）→ 实测 33s（R=530ms/批），比 CDC 还慢

## WinUSB 通道构成（已全部落地并验证）
- 设备：hw_inf=7 → init_usb(WinUsb) → 纯 WinUSB（PID 0x4608，EP4 OUT / EP5 IN 64B bulk）
- MS OS 描述符（MSFT100 + VendorCode 0xA0 + Extended Compat ID "WINUSB"）→ Windows 自动绑 winusb.inf
- 上传（EPC 标签）路由：sock_uart_upload → COMMON_INTERFACE_USB2 → winusb_send
- C#：WinUsbChannel.cs（SetupAPI + WinUsb API）+ OtaUpdater.OpenWinUsb（batch=2）
- 设备 winusb_send：等 XFRC（TX5_FIFO_HS_SIZE=64 修复 EP5 ACK FIFO）

## 实测数据（多次重复，稳定）
- 每批（2 帧 8KB）：W=0-16ms（写快） R=515-547ms（等 ACK，稳定 ~531ms）
- 擦除批：R=1093ms（≈2×531ms）
- 总时长：33s（402668B）
- 设备 TRACE："ws: len=15 0ms"——ACK XFRC 0ms 发出（发送瞬间完成），530ms 全在 C# ReadPipe
- 升级成功（ss: done + crc ok + swap 重启）

## RAW_IO 实验（关键结论）
- 尝试 1：RAW_IO policy，ULONG(4B) value=1 → **err=87 ERROR_INVALID_PARAMETER**
- 尝试 2：RAW_IO policy，UCHAR(1B) value=1 → **err=87 依旧**
- 对照：PIPE_TRANSFER_TIMEOUT（ULONG,4B）→ **成功（True err=0）**
- **结论：SetPipePolicy 本身、句柄、PipeID(0x85/0x04) 全对；RAW_IO 被本机 winusb.sys 明确拒绝（非调用方式问题）**
- 附：EP5 确认是 bulk（bmAttributes 0x02），RAW_IO 应支持 → 拒绝原因未知（驱动版本/设备组合），放弃 RAW_IO 路线

## 判定：530ms 与 CDC 265ms 同源
- 回环（连续流，无设备处理间隙）：10.8ms/4107B → USB 传输本身快
- OTA（设备处理期无数据 → 恢复发 ACK 的稀疏场景）：usbser.sys 265ms / winusb.sys 530ms
- 同一设备同一固件，两个 host 驱动取走周期不同 → **稀疏批量 IN 的 host 取走周期是共同根因**

## 当前实验（v1.14，进行中）
- **PIPE_TRANSFER_TIMEOUT 50ms → 0（无限）**：验证"50ms 超时取消 URB 再重提"的内核滞后是否是 530ms 来源
  - 设 0 后 URB 一直挂着，设备 ACK 到达即完成；升级完成设备重启断开时 ReadPipe 返回失败，上层 IsOpen 兜底
- 判定：R 掉到 ~30-50ms → 超时竞态是根因（收工）；R 仍 ~530ms → 上 OVERLAPPED 双 URB 并发读（libusb Windows 后端同款，始终 2 个 in-flight 读）

## 备选方案（按优先级）
1. OVERLAPPED 并发双读 —— 已试无效（R 仍 531ms）
2. 设备侧 IN 保活 —— 已试无效（R 仍 531ms），已回退
3. batch 8/16 减少 ACK 次数 —— 已试 batch4 回退（治标不治本）
---

# 深度排查记录（v1.14 → v1.25，用户全程实测）

## 决定性证据（cs/ws XFRC TRACE）
- CDC cs: len=15 0ms —— 设备 ACK 发送 XFRC 0ms（主机持续轮询取走）
- WinUSB ws: len=15 0ms —— 同样 0ms
- C# R=531ms（两个通道完全相同）——数据被取走后 Windows USB 栈 531ms 才上报应用
- 结论：531ms 在 Windows USB 栈"取走→应用可见"上报侧；不在设备/轮询/API 调用方式
## CDC 8.2s → 33s 回归（关键新线索）
- 75189f3（tick 500）：CDC 8.2s，R≈110ms
- WinUSB 全套加入后：CDC 33s，R=531ms（取走周期 110ms→531ms——不是固定值，是被改动放大的）
- 最大嫌疑：MS OS 字符串描述符（0xEE）无条件注册——user_desc 第 4 项 NULL→&usb_dev_winusbosstr 后，
  所有模式（含 CDC）都响应 MSFT100 → Windows 当 MS OS 设备处理 → usbser 枚举行为改变
- v1.25 修复：usb_dev_winusbosstr 仅 g_usb_winusb_mode 时响应（CDC/HID 返回空）——待测
- 次要嫌疑：TX5_FIFO_HS_SIZE 0→64（发送侧，cs=0ms 说明发送无碍，可能性低）

## 当前状态（v1.25，A/B 中）
- 固件：MS OS 字符串按模式门控 + cs/ws XFRC TRACE（每 5 次）+ notify bInterval 回 255
- C#：OVERLAPPED 回退同步读；COMMTIMEOUTS 禁用（A/B）；batch 2；CDC 批计时已加
- 待测判定：CDC R 回 ~110ms → MS OS 字符串是回归元凶（CDC 恢复 8.2s，WinUSB 模式不受影响）；
  仍 531ms → 继续查 TX5 FIFO / 其他 USB 库参数

## 用户明确指令（已置顶）
- 不要再确认 QSPI 的读写速度（见文档顶部警告）

## 推翻的旧论
- 旧论"usbser 265ms vs winusb 531ms" → 实测 CDC/WinUSB 都是 531ms
- PIPE_TRANSFER_TIMEOUT 0/50 均 531ms（竞态论排除）
- notify bInterval 255→1ms 无效（v1.19），已回滚（v1.22）
- COMMTIMEOUTS MAXDWORD+0 无效，当前禁用（A/B）
- OVERLAPPED 异步读无效（R 531ms）+ err=31 pending bug，v1.23 回退同步读
