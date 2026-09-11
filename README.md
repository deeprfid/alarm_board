# alarm_board —— 通道报警系统固件（两套）

防盗/防损(EAS类)通道系统：一套“Linux 主机 → STM32F0 通道中继板 → HC32F460 报警板”的报警链路。

## 目录

| 目录 | MCU / 工具链 | 角色 |
| --- | --- | --- |
| `STM32F0_linux_v4.31` | STM32F030CCTx / MDK-ARM (keil) | 通道中继板 v4.31（桥接 Linux 主机 IPC → 各通道报警板；IO 扩展） |
| `Radar_V4.2_2026_0425_MOS` | HC32F460 / MDK(`alarm_board.uvprojx`)、EWARM、GCC | 单通道报警板 V4.2（确认触发并输出声光报警） |

## 报警链路（已确认的现状）

```
Linux 主机（上位机）
   │  IPC: USART1 @115200, alarm_pdu(32B, 帧头 0xFF, CCITT CRC16)
   ▼
STM32F0 中继板 (BSP/app.c Check_Uart_Pdu)
   │  CRC 校验通过 → ipc_hpm_message() 按 AntID 通道号分发
   │   (COM6→通道1, COM2→通道2/3, COM3→4/5, COM4→6/7, COM5→8) @460800
   ▼
HC32F460 报警板 (Radar V4.2, Check_Uart_Pdu → Get_pdu_data)
   │  解析 Alarm_Duration[0..4]：buzz_duty/radar_range/颜色码/时长/EAS开关
   │  触发条件（本地确认）：
   │    · 雷达 GPIO(PC14/PC13/PH2) 有人（bsp_get_radar_singal）
   │    · 摄像头报警输入 IN1(PB00, AI_CAMERA)
   │    · 继电器输入 IN2(PB01, INput_RELAY, 由 10ms 轮询 Relay_status_check 上报)
   │    · EAS 模式/距离门(radar_range)为 0 时无条件触发
   ▼
输出声光报警（Alarm_On 按颜色码分发）
   · 0x5A 红警 Reguler_Tag   → 蜂鸣器(PWM PA08, buzz_duty) + 红灯(PB5) + 板载灯2 + OPA灯(GPO2/3) + 继电器GPO(PA15)
   · 0xA5 绿放行 Green_pass  → 绿灯(PB8)常亮，静音
   · 0x55 继电器联动报警     → 同红警
   · 主机再下发(非法/0值色码等) → Alarm_Off 全停
```

## 协议要点（alarm_pdu，两套固件一致）

| 字段 | 长度 | 含义 |
| --- | --- | --- |
| FrameHead | 1 | 0xFF=PDUHEAD（Linux 链路唯一下行帧头；**GPIOHEAD 0x55 已废弃并删除**） |
| Pdu_len | 1 | 帧长（=sizeof，32 字节） |
| DeviceID | 1 | 设备号 |
| AntID | 1 | 通道号（0 表示特殊命令：offline/LED test） |
| Alarm_Duration[6] | 6 | [0]蜂鸣占空 buzz_duty [1]雷达距离门 radar_range [2]LED颜色码 [3]报警时长(秒) [4]EAS开关 [5]离线标志 |
| Radarcfg[5] | 10 | 雷达配置区（历史 GPIOHEAD 应答已废弃，当前未用） |
| time_stamp / random_forest / reserved | 4/4/2 | 时间戳/随机数/保留(IO回显) |
| crc | 2 | CCITT CRC16（起始 0xFFFF，poly 0x1021） |

**颜色码语义：** `0xA5` 绿码放行、`0x5A` 红码报警、`0x55` 继电器报警、`0x90/0x64/0x32` 为历史/未用码。

## STM32F0 中继板 v4.31（BSP/app.c）

- 主循环：`Check_Uart_Pdu()`（[GET_RADAR_ENABLE=0 时 Radar_thread 不编译]）+ IWDG 喂狗。
- COM1 下行只有 `PDUHEAD(0xFF)` 定长 32B 一种：CRC 校验通过后按 `AntID` 分发到 COM2..COM6 并闪对应端口 LED（Port_1..5_LED）；`0xAA` 变长下行已支持但业务预留。
- COM1 上行只有 `PDUHEAD(0xFF)` 定长 32B **`gpio_pdu`** 一种：状态变化即报 + 1s 无变化心跳，Linux 下发查询时立即应答一帧（旧 8B `radar_pdu` 应答已删除）。

### COM1 上行帧 `gpio_pdu`（32B，帧头 = PDUHEAD）

| 偏移 | 字段 | 含义 |
| --- | --- | --- |
| 0 | FrameHead | 0xFF PDUHEAD |
| 1 | Pdu_len | 32 |
| 2 | DeviceID | 设备号（暂 0，语义待 Linux 侧确认） |
| 3 | AntID | 通道号（暂 0 = 整机聚合） |
| 4..11 | Rad_Status[8] | **RFID 模块 8 支天线**：天线 1..8，有人=1/无人=0（源：HC32 应答 Byte0 的雷达位 bit0..2 取或） |
| 12..19 | Alarm_Done[8] | 同上按 **8 支天线**：1 = 该天线所属雷达板**正在声光报警**（源：HC32 应答 Byte2 = 红灯在闪或蜂鸣器在响），报警停即回 0 |
| 20..29 | GPIO[10] | 保留（暂 0，语义待确认） |
| 30..31 | crc | CCITT CRC16（起始 0xFFFF，poly 0x1021），覆盖前 30B |

**5 路雷达板 → 8 支天线的映射**（5 路雷达口 COM6/COM2/COM3/COM4/COM5，下标 = 天线号 1..8，0 = 该口不带天线）：

```
COM6 -> 天线1 ; COM2 -> 天线2,3 ; COM3 -> 天线4,5 ; COM4 -> 天线6,7 ; COM5 -> 天线8
```

一块雷达板覆盖 1~2 支天线，故该板的雷达有人/报警状态复制到它对应的所有天线；某口 200ms 无有效应答（掉线/该路关闭）时按无人、`Alarm_Done=0` 上报。

### STM32↔HC32 通道链路（COM2..COM6 @460800，0xAA 变长）

- 查询：`[AA][Len=2][Cmd=0x10][Addr=通道号][CRC16]`，STM32 每 `radarPollMs` 轮询 5 口。
- 应答：同格式，负载 3B = `[GPIO_IN位图][工作模式位图][alarm_done]`（HC32 `bsp_report_build()`）：
  - Byte0：bit0..2 雷达1/2/3（PC14/PC13/PH2，高有效）；bit3 GPIO_IN1=摄像头(PB0，低有效)；bit4 GPIO_IN2=继电器(PB1，低有效)
  - Byte1：bit0 LIGHT_ON / bit1 SYNC / bit2 RADAR / bit3 AICAM / bit4 EAS
  - Byte2：alarm_done
- 定长 32B 命令帧（`PDUHEAD`）仍由 STM32 转发给 HC32 触发声光报警；HC32 侧的 `GPIOHEAD` 应答路径已删除。
- 上电 UID 校验 `rd_idkey_fun()`（idcode==0xE882F340 正常；否则死循环）＋开机自检蜂鸣；IWDG 使能。
- Host_IRQ(PA12) 下降沿 → 蜂鸣提示（外部主机拉低通知）。
- beep = 有源蜂鸣器供电开关 GPO_BZ3V3(PB2) 电平节拍（BEEP_T 状态机，10ms 驱动）。

## HC32F460 报警板 V4.2（Radar_V4.2_2026_0425_MOS）

- 主循环 `main.c`：Check_Uart_Pdu（收 PDU）→ Check_alarm_state（alarm_thread 消费 g_AlarmRing 消息）→ Check_Radar_state（雷达帧，当前仅清零）→ Check_UidKey（1s 例行 + 喂狗）。
- UART4 @460800 收主机 alarm_pdu（DMA + 超时收包，32B/包，入 m_stcRingBuf）。
- 本地触发消息（g_AlarmRing）：MSG_485_TAG_RTU(1)/MSG_Relay_RX(5)→Alarm_On；MSG_NETWORK_OFFLINE(3)→蓝灯闪12次；MSG_LEDTEST(7)→RGB自检。
- 合法包后回填 `HC32_RS485_corfirm_PDU`（帧头+DeviceID+alarm_done+雷达扫描数据+rng/uid key+CRC），当前版本未接发送。
- 防拷贝：`Ucode_read()` 用 EFM 唯一ID+哈希+CRC 算 magic（Custom_By_SZBMA=1 时须等于 0xC1A53979，否则一切报警被拒）。
- 雷达(LD2410 帧协议 USART1) 解析当前为占位清零；触发依赖雷达 OUT GPIO 与摄像头/继电器输入。
- 硬件拨码开关（switch_decoder）选择工作模式：SYNC/RADAR/AICAM/EAS/LIGHT_ON 等影响蓝灯/雷达灯显示策略（bsp_exint.c Radar_Led_update）。

## 维护约定

- 每次代码修改前先更新本文件相关章节或 `CHANGELOG.md`；提交信息建议 Conventional Commits（feat/fix/docs/chore…）。
- 不要提交：IDE 用户文件(uvguix/uvoptx/settings)、MDK 编译输出(STM32F030/)、EventRecorderStub.scvd。
- 源码注释为 GBK 编码，请以“无 BOM”方式编辑或保持原编码，避免中文注释乱码。
