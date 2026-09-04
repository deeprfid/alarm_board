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
| FrameHead | 1 | 0xFF=报警/配置包 PDUHEAD；0x55=GPIO 扩展/查询包 GPIOHEAD |
| Pdu_len | 1 | 帧长（=sizeof，32 字节） |
| DeviceID | 1 | 设备号 |
| AntID | 1 | 通道号（0 表示特殊命令：offline/LED test） |
| Alarm_Duration[6] | 6 | [0]蜂鸣占空 buzz_duty [1]雷达距离门 radar_range [2]LED颜色码 [3]报警时长(秒) [4]EAS开关 [5]离线标志 |
| Radarcfg[5] | 10 | 雷达配置/应答区（GPIOHEAD 查询应答用 bit0） |
| time_stamp / random_forest / reserved | 4/4/2 | 时间戳/随机数/保留(IO回显) |
| crc | 2 | CCITT CRC16（起始 0xFFFF，poly 0x1021） |

**颜色码语义：** `0xA5` 绿码放行、`0x5A` 红码报警、`0x55` 继电器报警、`0x90/0x64/0x32` 为历史/未用码。

## STM32F0 中继板 v4.31（BSP/app.c）

- 主循环：`Check_Uart_Pdu()`（[GET_RADAR_ENABLE=0 时 Radar_thread 不编译]）+ IWDG 喂狗。
- COM1 收到 GPIOHEAD：把 reserved 低4位写到 GPO1-4，再回读 GPI1-4 填入 reserved 原样回发（IO 扩展）。
- COM1 收到 PDUHEAD：按 AntID 分发到 COM2..COM6，并闪对应端口 LED（Port_1..5_LED）。
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
