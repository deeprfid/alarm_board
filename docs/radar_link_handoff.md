# F4A0 雷达板链路（radar_link）交接文档

> 2026-09-19。写给下一个会话/下一个人的**断点续传**说明。
> 状态：**RX 已打通**（2026-09-19 晚第二次会话，提交 `4258f30`）：驱动三处断点已修，
> LED2/3/4 有人常亮；上板复核见 §9.1。

---

## 0. 新会话开场白（直接复制粘贴）

```
工作目录 J:\dsh\alarm_board（HC32F4A0 + HC32F460 报警板两个项目在同一仓库）。

先完整读一遍 docs/radar_link_handoff.md，然后按它的 §3.1 修驱动的三处 RX 断点：
  ① hc32f4a0_driver/projects/user/src/rs485_1.c / rs485_2.c / rs485_3.c
     的 RX 中断回调里 BUF_Write 被注释掉了，恢复它
  ② usart_driver.c 的 hc32f460_uart_get_bytes_cnt() 只认 uartid 0..3，
     要补 4/5/6（对应 RS485_1/2/3）
  ③ io_stream.c 的 read() switch 缺 COMMON_INTERFACE_RS485_1/2/3 三个 case
改完重编 hc32f4a0_driver 工程生成 hc32f4a_driver.lib，再链回 app。

要求：完全兼容项目 1（STM32F0_linux_v4.31）的协议，不要另起炉灶；
代码要能移植到其它平台（按 §4.3 拆 radar_proto / radar_plat）。
板子上接了 3 块雷达板，LED2/3/4 是状态灯（BOARD_LED1 被 app 占用）。
现在无论怎么改三个灯都不亮，TRACE 刷屏 read--invalid interface number。

不要先问我，直接做；做完编译验证并报告。
```

### 新会话必须知道的 5 个环境事实

1. **工作目录** `J:\dsh\alarm_board`；F4A0 工程在 `HC32F4A0_OTA/hc32f4a0_app`，
   驱动源码在 `HC32F4A0_OTA/hc32f4a0_driver`（**自己写的，可以改**）。
2. **Keil 无头编译**：`D:\Keil_v5\UV4\UV4.exe -r <uvprojx> -j0 -o <log>`，
   验收标准是 log 里 `0 Error(s)`。
3. **判断模块有没有真被链接**：看 map 里的 `Removing ... radar_link`，
   以及 `Program Size: Code=` 有没有增长。**没接线 = 被链接器回收 = 白做**。
4. **git 推送需要代理**：`git -c http.proxy=http://127.0.0.1:7890 -c http.sslBackend=openssl push origin main`。
   本地领先 origin 若干提交，**推送由用户自己做**。
5. **文件编码不统一**：`main.c` 是 GBK（fs 工具读不了，要用 pwsh 做 GBK 往返），
   `ota_*.c` / `radar_*.c` 是 UTF-8。改错编码会让中注释变乱码。

---

## 1. 背景与目标

这是**项目 2**：**HC32F4A0 直接驱动 HC32F460 报警板**（没有 STM32 中继板）。
项目 1 是 Linux -> STM32 -> HC32 雷达板，其中 STM32 只是 uart-hub（Linux 只剩 1 个串口）。

要求：
- **完全兼容项目 1 的协议**（新老两种帧都要认）；
- 只是串口数量从 5 个变 3 个，**不要另起炉灶**；
- 代码要**可移植到其它平台**（所以应拆成"协议纯逻辑" + "平台层"两层）。

### 端口 -> 天线映射（现场确认）

| F4A0 接口 | 宏值 | 底层 USART | 天线 |
| --- | --- | --- | --- |
| `COMMON_INTERFACE_RS485_1` | 104 | `CM_USART3` | 天线1 |
| `COMMON_INTERFACE_RS485_2` | 105 | `CM_USART8` | 天线2,3 |
| `COMMON_INTERFACE_RS485_3` | 106 | `CM_USART5` | 天线4 |

**没有 `COMMON_INTERFACE_RS485_4`**（全树搜过，只有 1/2/3）。

---

## 2. 协议事实（已逐行核对，可直接用）

### 2.1 下行查询帧（板子认这个）

STM32F0 的轮询就一句（`STM32F0_linux_v4.31/BSP/app.c:423`）：

```c
(void)stmVarSend(sPortCom[i], 0x10u, (uint8_t)(i + 1u), 0, 0u);
//                              cmd=0x10  addr = 端口序号 + 1
```

即 **`0xAA` 变长帧 + cmd `0x10` + addr = 本口序号+1（1/2/3）**。
**addr 不能发广播 0x00** —— 板子按地址过滤，广播会全部不应答（这是我踩过的坑）。

变长帧格式（与 F460 `common.c` 的 `frame_var_send` 逐字节一致）：

```
AA | lenv | cmd | addr | payload[lenv-2] | crc16_lo | crc16_hi
lenv = plen + 2 ;  total = lenv + 4 ;  crc16 覆盖 total-2 字节
CRC16 = CCITT，poly 0x1021，init 0xFFFF
```

### 2.2 定长帧（32B，量产验证过的）

两个方向都用同一个 32 字节结构，定义在
`STM32F0_linux_v4.31/BSP/app.h:20-45` 与 `Radar_V4.2.../projects/source/bsp_rs485.h:121-133`：

```c
// 下行 alarm_pdu
FrameHead(0xFF) | Pdu_len(32) | DeviceID | AntID | Alarm_Duration[6]
| Radarcfg[5]×2 | time_stamp(4) | random_forest(4) | reserved(2) | crc(2)

// 上行 gpio_pdu
FrameHead(0xFF) | Pdu_len(32) | DeviceID | AntID
| Rad_Status[8] | Alarm_Done[8] | GPIO[10] | crc(2)
```

```c
// 板子回给主机的定长帧（F460 main.h:82-92）
alarm_confirm_package { framehead(0xFF), deviceID, alarm_done, unused[5],
                        stc_radar_scan_data_t radar /*16B*/, rngkey(4), uidkey(2), crc(2) }
```

**重要**：项目 1 里 STM32F0 是**纯透传 hub** —— 它把 Linux 下发的 32B 帧原样转发到雷达口
（`app.c:726-728`：`memcpy(&alarmboard, sIpcRx.buf, sizeof(alarm_pdu)); ipc_hpm_message(...)`），
**自己不产生查询**。所以项目 2 里 F4A0 要自己生成下行帧。

### 2.3 应答解析

`0x10` 查询的应答 payload 3B：`[gpioIn][workMode][alarmDone]`
- `gpioIn`：bit0..2 = 雷达1..3，bit3 = GPIO_IN1，bit4 = GPIO_IN2
- `radarVal = (gpioIn & 0x07) != 0`（有人）

---

## 3. 【核心阻塞】驱动层三处断点 —— RX 通路根本没接　【已修复，见 §3.2】

现场日志刷屏：

```
read--invalid interface number
read--invalid interface number
...
```

追下来是**三处叠加**，缺一不可，**任何一处单独修都无效**：

| # | 文件:位置 | 问题 |
| --- | --- | --- |
| ① | `hc32f4a0_driver/projects/user/src/rs485_1.c:34-39`（`rs485_2.c`/`rs485_3.c` 同构） | **RX 中断把字节读出来就扔了**：`// (void)BUF_Write(&Uart4RingBuf, &u8Data, 1UL);` 被注释掉 |
| ② | `usart_driver.c:182 hc32f460_uart_get_bytes_cnt()` | 只处理 `uartid` 0..3；RS485 映射到下标 **4/5/6**，落到 `else return 0` |
| ③ | `io_stream.c:797-800 read()` 的 switch | 只有 `UART0..3` + `USB*` + `SOCKET*`；**没有 `RS485_1/2/3`** → 掉 `default` |

关键背景：`gUartParams` 按 **`id - COMMON_INTERFACE_UART_BASE(100)`** 索引
（`io_stream.c:263, 288`），所以 RS485_1/2/3 = **下标 4/5/6**。

对比：**发送是通的** —— `Uart_RS485_send()` 有自己独立的 switch（`usart_driver.c:115`），
认 104/105/106。所以"能发不能收"。

### 3.1 修法（驱动是自己写的，可以直接改）

```c
// ① rs485_1.c / rs485_2.c / rs485_3.c：各加一个环形缓冲，恢复写入
static stc_ring_buf_t s_rs485_1_rx;                   // rs485_2/3 各自一个
static void USART_RS485_RxFull_IrqCallback(void)      // rs485_2/3 里叫 USART8_ / USART5_
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART_RS485_1);
    (void)BUF_Write(&s_rs485_1_rx, &u8Data, 1UL);     // ← 取消注释并接各自的口
}

// ② usart_driver.c hc32f460_uart_get_bytes_cnt()：加 3 个分支
else if (uartid == 4) return (int)BUF_UsedSize(&s_rs485_1_rx);
else if (uartid == 5) return (int)BUF_UsedSize(&s_rs485_2_rx);
else if (uartid == 6) return (int)BUF_UsedSize(&s_rs485_3_rx);

// ③ io_stream.c read()：与 UART0..3 合并进同一分支
case COMMON_INTERFACE_RS485_1:
case COMMON_INTERFACE_RS485_2:
case COMMON_INTERFACE_RS485_3:
case COMMON_INTERFACE_UART0:
...
```

改完**重编 `hc32f46_driver.uvprojx` 生成 `hc32f4a_driver.lib`**，再链回 app。
（注意 `read()` 的 UART 分支里用的是 `uart_recv()`，它读 `gUartParams[s-100].recvbuf`，
所以 ①②里缓冲的接法要跟 `gUartParams[4/5/6].recvbuf` 对齐，别各写一套。）

### 3.2 实际修法（2026-09-19 晚，提交 `4258f30`）——比 §3.1 草图多 3 处配套

**① 没照 §3.1 的 `BUF_Write/BUF_UsedSize` 抄**：`read()` 走的是 `uart_recv()` →
`gUartParams[s-100].recvbuf`，而 `BUF_UsedSize()` 给的是"已用字节数"、不是写指针；
且 4/5/6 的 `recvbuf` 原本是 **NULL / recvbufsize=0**（照草图写会从 NULL 读）。
所以 ①②按 §3.1 括号那句要求，用与 UART0..3 **完全相同**的模型：

| # | 位置 | 做法 |
| --- | --- | --- |
| ① | `rs485_1/2/3.c` RX 中断 | 字节写入 `gRs485_NRecvBuf`，写指针 `rs485_Nreccount`（2K/口，静态数组不进堆） |
| ② | `usart_driver.c` | `hc32f460_uart_get_bytes_cnt()` 补 4/5/6 返回写指针；`uart_err_clear()` 按**实际串口**下标（RS485_1/2/3 = 4/5/6；`uart4.c` 原来误清下标 2） |
| ③ | `io_stream.c` | `read()` 补 `RS485_1/2/3` case，与 UART0..3 合并进同一分支 |
| **配套 1** | `usart_driver.c hc32f460_uart_init()` | 给 4/5/6 **先挂 `recvbuf/recvbufsize`、再 `uart_rs485_Init()`**（中断在里面才使能）；`clear_buf`/`init_uart_close` 一并补 4/5/6 |
| **配套 2** | `io_stream.c ioctl()` | 补 RS485 —— **不补这条 §9.3.3 必然发生**：`radar_link` 的 `SET_ISBLOCK(1)` 会静默失败，`read()` 一直走 `O_BLOCK` |
| **配套 3** | `io_stream.c write()` | 补 RS485 转 `Uart_RS485_send()`（`radar_ota.c`→`ota_dist.c` 的 `write(104)` 原来必定失败）；`uart_close()` 也补 |

另：`hc32f46_driver.h` 补 `wait_init_ok()` 声明（消掉 `radar_link.c` 的隐式声明 warning）。

**`radar_link.c` 侧同步改了 3 处**：
1. `radar_link_init()` 改**先 `uart_open()` 再 `ioctl`**（`io_stream.c:291` 的 `uart_open()` 对已打开的口是
   no-op，所以两个打开时序都收敛到"非阻塞"，§9.3.3 的洞消掉）；
2. 删掉 §4.2 的三级临时诊断闪灯；
3. 状态灯口径 = **有人常亮 / 无人释放**，走共用 `Alarm_Output(LED,1,0,0)` + `Alarm_Disable()`
   （不用 `(5,5,1)`：那是 50ms 亮/50ms 灭**连续闪**；`OFF` 传 0 时 `GPIO_Pro()` 直接 return，
   通道保持占用且引脚不翻转 —— `bsp_led.c:223`）。

---

## 4. 已落地的代码

### 4.1 `radar_link.c/h`（F4A0 侧，已注册进 Keil 工程并接线程）

- 帧核 `fr_crc16`（CCITT，与 F460 `common.c` 逐位一致）
- `0xAA` 变长解析（cmd `0x10` 应答 / `0x81` 上报）+ `0xFF` 定长 32B 解析，**两者都校验 CRC**
- 三口轮询：每 50ms 发一次查询；逐口 200ms 新鲜度超时（等价 STM32F0 的 `radarPortFresh`）
- 丢帧计数 `bad_frames`、发送失败计数 `tx_err`
- 状态灯 `BOARD_LED2/3/4`（**`BOARD_LED1` 被 app 占用**）
- **不自己写 ISR**：RX 走驱动；TX 用 `Uart_RS485_send()` 并**自加按口互斥**（那个函数没有锁，
  对比 `uart_send()` 有 `uart_tx_lock`）
- **不做 `uart_open`**：三个口由 `ipc.c:271 Tag_update_thread -> Usart_RS485_init()` 打开，
  且那里设的是 `O_BLOCK` —— 重复打开/被覆盖会让 `read()` 永久阻塞、线程卡死。
  本模块只做 `ioctl(SET_ISBLOCK/SET_TIMEOUT/CLEAR_REVBUF)`，并照 `send_tags` 先 `wait_init_ok()`。

### 4.2 ⚠️ 代码里有【临时诊断】必须删　【已删，2026-09-19 晚 / 4258f30】

`radar_link.c` 中一段三级 LED 定位（搜 `【临时诊断】`）：

| 灯 | 含义 |
| --- | --- |
| LED2 | 轮询线程活着（无条件每秒闪） |
| LED3 | 查询帧已发出（每次闪） |
| LED4 | `read()` 收到任何字节（不管 CRC） |

**验证通过后请删掉这一段**（含 `radar_link_poll()` 开头的 `s_hb_ms` 心跳块、
`read()` 后的 LED4 调用、查询前的 LED3 调用）。

### 4.3 建议的重构方向（用户明确要求"将来要移植到其它平台"）

现在 `radar_link.c` 是**逻辑与平台混在一起**的，应拆为：

| 层 | 内容 | 平台相关 |
| --- | --- | --- |
| `radar_proto.c/h` | `frCrc16`、两种帧的解析/组帧、每口状态、新鲜度超时 | **否**，逐行照 `STM32F0/BSP/app.c:264-560` 搬 |
| `radar_plat.h` + 各平台实现 | `send(port,buf,len)` / `recv(port,buf,len)` / `now_ms()` / 灯 | **是**，换平台只换这层 |

---

## 5. 排查历程（避免重走弯路）

按时间顺序，每一步都是**被现场现象或日志否掉**的：

1. 以为要自己写 USART ISR → 否：驱动已占用中断向量（`hc32f4a0_ll_interrupts_share.c`）
2. 以为 `Uart_RS485_send()` TX 忙会丢帧 → 否：它是逐字节阻塞自旋，**不丢帧**（但无锁、无超时）
3. 以为查询帧要用 32B 定长帧 → **部分对**：定长帧确实量产在用，但**轮询用的是 `0xAA`+`0x10`**
4. 以为地址要广播 `0x00` → 否：**必须用本口序号+1**（`app.c:423`）
5. 以为 LED 逻辑写错导致全灭 → 是错了一处（`Alarm_Output` 传 0 会被 `GPIO_Start` 静默丢弃），
   **但那不是根因**
6. 以为 `read()` 阻塞导致线程卡死 → 否（已修，但线程本来就活着，LED2 在闪）
7. **真正根因**：`read--invalid interface number` → §3 的三处驱动断点

**教训**：在拿到"LED2 在闪 + `read()` 报错"之前，每一步都是在猜。**先做可观测性，再改逻辑。**

---

## 6. 下一步（建议顺序）

1. ~~按 §3.1 修驱动三处 + 重编 lib（这是唯一阻塞项）~~ → ✅ **已完成**（`4258f30`，见 §3.2）
2. ~~上板验证：`LED4` 开始闪~~ → 诊断灯已删；改为**看 LED2/3/4 有人常亮**（现场已反馈正常）
3. 上板复核：人走开后灯是否**灭**、多久灭（见下方"灭灯延迟"这条待确认）
4. ~~删掉 §4.2 的临时诊断~~ → ✅ 已删
5. 按 §4.3 拆成 `radar_proto` + `radar_plat` —— **未做**
6. 数据上报去处（MQTT / HTTP / 现有 RFID 数据模型）→ 用户 2026-09-19 明确：**暂时不上传**

**待确认（下次上板第一件事）**：灭灯延迟到底是 50ms 还是 200ms —— 这能区分 F460 的两种行为：
- 现象 A：无人时板子**照常应答**且 `gpioIn=0` → `handle_var_frame`（`radar_link.c:148`）当场清零
  `radar_val` → 灭灯 ≈ **一个查询周期（`QUERY_PERIOD_MS`=50ms）+ 轮询粒度 10ms**；
- 现象 B：无人时板子**静默不答** → 只能等新鲜度超时（`RADAR_LINK_FRESH_MS`=200ms，`radar_link.c:280`）
  才灭，且此时 `online` 会被打成 0。

分辨方法：无人时看 `rx_frames` 还在不在涨（涨 = A，不涨 = B）。两个旋钮都在明面上，
确认现象后再决定调哪个。

---

## 9. 验收标准 / 开放项 / 改动风险

### 9.1 明确的完成标准（Definition of Done，按顺序）

| # | 验收点 | 怎么确认 |
| --- | --- | --- |
| 1 | 驱动改完能编过 | 驱动工程 log `0 Error(s)`；app 重链后也 `0 Error(s)` |
| 2 | **`read()` 能取到字节** | **LED4 开始闪**（`read()` 返回 >0 就闪）；TRACE 不再刷 `invalid interface number` |
| 3 | 帧能解析通过 | `bad_frames` 不再增长；`rx_frames` 开始涨（可加 TRACE 打印） |
| 4 | 雷达"有人"能上报 | 人走过雷达前，对应 LED 按 `Alarm_Output(LED,5,5,1)` 闪 |
| 5 | 临时诊断可删 | 删掉 §4.2 那段后，功能仍正常 |

**第 2 步是分水岭**：在 LED4 闪起来之前，**不要再去调帧格式、地址、周期** ——
链路根本收不到任何字节，调上层全是白费（本次就是这么浪费了四轮）。

**2026-09-19 晚复核（提交 `4258f30`）**：

| # | 结果 | 证据 |
| --- | --- | --- |
| 1 | ✅ 已达成 | 驱动 log `0 Error(s)`；app log `0 Error(s)`，`Code=383164 RO=24336 RW=14296 ZI=166440` |
| 2 | ⏳ **待上板** | —— |
| 3 | ⏳ 待上板 | —— |
| 4 | ⏳ 待上板（现场反馈 LED 已按"有人"点亮） | —— |
| 5 | ✅ 已删（§4.2 三段全删） | `grep Alarm_Output radar_link.c` 只剩状态灯那一处，且走的是 `Alarm_Output(LED,1,0,0)` |
| 附 | `radar_link` 未被链接器回收 | `firmware.map` 只 `Removing` 了 `radar_link_get/of_antenna/antenna_alarm` 三个没人调的访问器；`radar_link_poll` refers to `read`/`Alarm_Output`/`Alarm_Disable` |

### 9.2 开放项（还没定，别自行假设）

| 项 | 状态 |
| --- | --- |
| **数据上报去处** | **未定**：MQTT / HTTP / 现有 RFID 数据模型（`g_Uploadtag`）？现在只存本地状态 |
| **`0x81` 报警上报的处理** | 现在只记新鲜度，内容未往上送 |
| **查询周期 `QUERY_PERIOD_MS = 50`** | 我估的（STM32F0 是 200ms 判超时，取 1/4）。**现场合适值未实测** |
| **RS485 方向控制** | `uart_rs485_Init()` 里**没有任何 DE/RE 切换**（`RS485_set_send/rec` 只用在 USART4 那条路）。**推测 CM_USART3/8/5 是自动方向收发器 —— 未验证**。若 LED3 在闪（发了）但 LED4 不闪（收不到），这一条要优先怀疑 |
| **`DeviceID` / `Alarm_Duration` / `Radarcfg` 字段取值** | 现阶段只做"在线探测"，相关字段留 0；真要下发报警命令时需按项目 1 的定义填 |
| **驱动 `.lib` 的构建产物路径** | ✅ **已确认**（2026-09-19 晚）：工程 `hc32f4a0_driver.uvprojx`（**不是** `hc32f46_driver.uvprojx`，那是 F460 的）自带 AfterMake `xcopy .\output\hc32f4a_driver.lib ..\..\..\driver_lib /Y`，编完自动覆盖 app 链的那份；只编 driver 就够，**不用手工拷贝**。⚠️ 但 `.lib` 当时被 `HC32F4A0_OTA/.gitignore` 的 `**/*.lib` 忽略、**不入库** → clone/换机后必须先重编 driver。
   > ✅ **已修（2026-09-19）**：该规则已删除，链接必需的库全部入库（见 §8.5），clone 下来不再需要先重编；不过 `driver_lib/hc32f4a_driver.lib` 现在会随每次驱动改动产生二进制 diff，属预期 |

### 9.3 改动风险（改驱动前必须知道）

1. **`read()` 和 `hc32f460_uart_get_bytes_cnt()` 是共用函数** —— `UART0..3` 分支正被
   UART1/UART2/UART3 等既有功能使用（见 `Lan2Uart.c` / `reader_init.c` / `ota_integration.c`）。
   **只能"新增分支/新增 case"，绝不能改动既有 0..3 的行为**，否则会连带弄坏 RFID 主链路
   （那条链路是量产在跑的，用户刚验证过"检测到标签都有报警"）。
2. **RX 中断改回写缓冲后，注意缓冲溢出** —— 三个口都在 460800bps，若上层长时间不取，
   环形缓冲会绕圈覆盖。建议按 `gUartParams[4/5/6].recvbufsize` 的既有尺寸给，不要随意缩小。
3. **`Usart_RS485_init()` 里设的是 `O_BLOCK`** —— 若 `read()` 补上 case 后仍按
   `isBlock == O_BLOCK` 走阻塞分支，而 `radar_link` 又没有把端口设成非阻塞，
   **轮询线程会被 `read()` 卡死**（本次已踩过一次）。平台层务必确保 `SET_ISBLOCK(1)` 生效。
   > **已消除（`4258f30`）**：一是 `ioctl()` 补了 RS485，`SET_ISBLOCK(1)` 才真正生效（此前静默失败）；
   > 二是顺序洞也补了 —— `radar_link_init()` 改为**先 `uart_open()` 再 `ioctl`**，
   > 而 `uart_open()` 对已打开的口是 no-op（`io_stream.c:291`），所以"它先开/我们先开"两种时序
   > 都收敛到非阻塞，不再依赖谁先跑。
   > 另：即使真落到 `O_BLOCK`，`Usart_RS485_init()` 给的 `timeout=20` 也只会 ~25ms 后返回 `-2`，
   > **不会死等**（会拖慢轮询节奏、可能踩到 200ms 新鲜度，但不会卡死线程）。
4. **驱动 `.lib` 是共享的** —— 它不只服务本次功能，改错了影响面是整个 F4A0 产品。
   改完**至少回归一次 RFID 主链路**（检测标签 + 报警）。

---

## 8. 必须交代给新会话的【非技术】事项

### 8.1 两个项目的区分（最容易搞混，务必先说清）

| | 项目 1 | 项目 2（**本次在做**） |
| --- | --- | --- |
| 链路 | Linux → STM32 → HC32 报警板 | **HC32F4A0 → HC32 报警板** |
| 仓库目录 | `STM32F0_linux_v4.31` + `Radar_V4.2_2026_0425_MOS` | `HC32F4A0_OTA/hc32f4a0_app` |
| STM32 的角色 | uart-hub（Linux 只剩 1 个串口，用来扩口） | **不存在** |
| 串口数 | 5 条雷达链路（COM6/2/3/4/5） | 3 个 RS485（485_1/2/3） |
| 天线数 | 8 | 4 |

**一切"和 STM32 一样"的要求，指的都是 `STM32F0_linux_v4.31/BSP/app.c` 那份雷达轮询实现。**

### 8.2 用户的工作方式（务必遵守）

- **不要问来问去，直接做。** 用户多次明确表达过对反复确认的不耐烦。该查就查，查完直接改。
- **用户手动改的代码优先级最高**，不要覆盖、不要"顺手清理"。
- **不要过早宣布"找到根因"。**（本次连续 6 次误判，见 §5）没拿到硬证据（日志、map、反汇编）就不要下结论。
- **先做可观测性，再改逻辑。** 本次最大的教训：F4A0 有 `TRACE` 可用于打印，
  **应该一开始就要求用户把 TRACE 打出来**，而不是连改四轮靠猜。
- **参考现成实现，不要另起炉灶。** 用户明确要求"完全兼容项目 1"、"将来要移植到其它平台"。
- **不要改动无关代码**，不要顺手重构。
- 汇报用**简洁中文**；提交信息单行、`type(scope): 中文标题 - 要点`。
- 涉及硬件的行为**必须标清"已验证/未验证"**——用户会真的拿板子去烧。

### 8.3 可观测性现状（决定排查手段）

| 平台 | 有什么 |
| --- | --- |
| **F4A0** | **`TRACE()` 可打印**（用户能贴出来）；LED1 被 app 占用，LED2/3/4 可用；3 块雷达板已接 |
| **F460 报警板** | **没有调试串口**（唯一串口是 RS485 业务口），诊断只能靠 LED 灯语 |

### 8.4 同一仓库里【其它在飞的工作】（别当成本次任务的一部分）

| 工作流 | 状态 |
| --- | --- |
| **F460 OTA（A/B 槽 + Boot）** | **已打通并上板验证**：整片 `dist/radar_full_boot_A_B_release.bin` 烧录正常（红灯 2 秒→跳 B 槽→App 起来→通信正常）；`OTA_APP_ENABLE=1`。**但 OTA 整链真机联调一次都没做过**（下载/激活/回退/换槽） |
| **F4A0 OTA 分发（MSC 放 `RADAR.BIN`）** | 代码已实现并编译进镜像，**未上板** |
| **项目 1（STM32F0）** | **本次一行未动** |

### 8.5 构建产物与工具（`dist/` 被 gitignore，不入库）

| 工具 | 用途 |
| --- | --- |
| `tools/merge_f460_image.py` | Boot + 槽A/B 合成整片 512KB bin（烧录用） |
| `tools/ota_pack_f460.py` | 生成 `radar_slot{A,B}_v*.otapkg`（OTA 用，改名 `RADAR.BIN` 丢进 MSC） |

**入库策略（2026-09-19 修订）**：`HC32F4A0_OTA/.gitignore` 里的 `**/*.lib` 已删除 ——
原来那条把**链接必需的库**全挡在库外，clone 下来根本链不上。现已入库：

| 文件 | 谁链它 |
| --- | --- |
| `HC32F4A0_OTA/driver_lib/hc32f4a_driver.lib` | app |
| `HC32F4A0_OTA/driver_lib/ModuleAPI_C_ARM.lib` | app + driver |
| `HC32F4A0_OTA/driver_lib/RTX_CM4F.lib` | app |
| `HC32F4A0_OTA/driver_lib/json_parser.lib` | 当前工程没引用（留档） |
| `HC32F4A0_OTA/hc32f4a0_driver/projects/rtos2_help/RTX_CM4F.lib` | driver 工程 |

同时 `doc/`（18 篇工程/OTA 文档）与 `_archive_deadcode/`（14 个源文件存档）取消忽略并入库。
**仍然忽略**（都是产物或体量待定，注意 `**/output/` 里那份 `hc32f4a_driver.lib` 是构建产物，不入库）：
`release/`、`dist/`、`stm32f300cct6/`、`_microboot_ref/`、`ReaderUI_v1_MCU/`、`Scanner_20260901/`（独立仓库，自带远端）。

### 8.6 一个反复踩的坑：模块被链接器回收

`radar_link.c` / `ota_dist.c` 这类新模块，**如果没有任何调用点，会被 arm linker 整个 GC 掉**——
编译报 0 Error，但功能完全不在镜像里，表现为"改了等于没改"。

**判断方法（每次改完必做）**：
```powershell
# 1) 看有没有被回收
Select-String -Path '...\MDK\output\firmware.map' -Pattern 'Removing.*<模块名>'
# 2) 看 Code 有没有增长
Select-String -Path '<build log>' -Pattern 'Program Size'
```

---

## 7. 常用命令

```powershell
# 编 F4A0 driver（生成 hc32f4a_driver.lib，AfterMake 自动 xcopy 到 driver_lib）
Start-Process 'D:\Keil_v5\UV4\UV4.exe' -ArgumentList @('-r','J:\dsh\alarm_board\HC32F4A0_OTA\hc32f4a0_driver\projects\MDK\hc32f4a0_driver.uvprojx','-j0','-o',"$env:TEMP\drv.log") -Wait

# 编 F4A0 app
Start-Process 'D:\Keil_v5\UV4\UV4.exe' -ArgumentList @('-r','J:\dsh\alarm_board\HC32F4A0_OTA\hc32f4a0_app\projects\MDK\hc32f4a0_app.uvprojx','-j0','-o',"$env:TEMP\f4a0.log") -Wait

# 看 Code 是否变化（判断模块有没有被链接器回收）
Select-String -Path '...\MDK\output\firmware.map' -Pattern 'Removing.*radar_link'

# git（本地领先 origin 若干提交；推送需代理）
git -c http.proxy=http://127.0.0.1:7890 -c http.sslBackend=openssl push origin main
```
