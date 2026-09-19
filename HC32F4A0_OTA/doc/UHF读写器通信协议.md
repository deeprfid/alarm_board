# UHF RFID 读写器通信协议（设备 ↔ 上位机 / App / 微信小程序）

> 版本：v1.0（2026-08-28 整理）
> 依据：`hc32f4a0_app/projects/app/src/reader_msg.c`、`HttpModuleAPI.cpp`、`driver_lib/hc32f46_driver.h`
> 用途：PC 上位机、手机 App、微信小程序**三端统一**的通信协议依据。
> 原则：**协议统一，传输分离**——标签格式、名单协议、moduleapi 接口在所有通道一致，只换传输层。

---

## 1. 通信通道总览

| 通道 | 方向 | 传输 | 典型场景 | 端侧 |
|---|---|---|---|---|
| WinUSB | 双向 | USB Bulk（FS 12Mbps / HS 480Mbps 需外 PHY） | 本地直连：高速收标签 + 下发名单 | PC 上位机 |
| CDC 串口 | 双向 | USB 虚拟串口 | 本地盘点 / OTA / 参数 | PC 上位机 |
| MSC | 设备→PC | USB 虚拟 U 盘 | 离线升级（FW.BIN）/ 名单文件 | PC |
| HID 键盘 | 设备→系统 | USB 键盘 | ERP 输入框扫码（旁路记录） | 任意聚焦程序 |
| MQTT | 双向 | 网络（TCP/WS） | 远程汇聚、移动端 | PC / App / 小程序 |
| HTTP | 双向 | 网络 | moduleapi 配置 / 主动推送 | PC / App / 小程序 |
| TCP | 双向 | 网络 Socket | 远程双向长连接 | PC / App / 小程序 |

**上传接口选择（`hw_inf`，设备运行参数 `upload.hw_inf`）**

| 值 | 含义 | 备注 |
|---|---|---|
| 0 | 未选择 | |
| 1 | 以太网 | 配置须含 tcp/http/mqtt 之一 |
| 2 | 串口（UART1） | |
| 3 | HID 键盘 | 标签经键盘输出 |
| 4 | 4G | 配置须含 tcp/http/mqtt 之一 |
| 5 | WIFI | 配置须含 tcp/http/mqtt 之一 |
| 6 | 韦根 | 配置须含 wiegand |
| 7 | 串口（UART2） | |

---

## 2. 标签上报协议

### 2.1 消息类型（MidMsgType）

来源：`hc32f46_driver.h` L629-660

| 值 | 枚举 | 含义 |
|---|---|---|
| 0 | MidMsgType_None | |
| 1 | MidMsgType_TagRead | 标签读取（批量/盘点） |
| 2 | MidMsgType_GpiTrigger | GPIO 触发 |
| 3 | MidMsgType_TagComing | **单标签实时上报**（主动模式） |
| 4 | MidMsgType_HeartBeat | 心跳 |
| 5 | MidMsgType_RdrError | 设备异常 |
| 6 | MidMsgType_SyncTimeReq | 时间同步请求 |
| 20-27 | GetConf/SetConf/GetGPI/SetGPO/Reboot/DetectBoardExt/GetBoardExt/UpdateFwByFtp | 配置/控制命令 |
| 30-41 | GetStaticConf/SetStaticConf/.../SetBluetoothConf | 静态配置 |

### 2.2 二进制帧格式（TCP / CDC / WinUSB 通用）

来源：`reader_msg.c` AddMsgHeader2SockBuffer + AddTag2SockBuffer + SetMsgDatalen + AddTagCnt2SockBuffer

**帧头（TagRead/TagComing 事件）**

| 偏移 | 长度 | 字段 | 说明 |
|---|---|---|---|
| 0 | 1 | 帧头标记 | 固定 0xFF |
| 1 | 1 | 设备名长度 | namelen（钳位 ≤ 0xFE） |
| 2-3 | 2 | 数据长度 | totallen - 10 - namelen，大端 |
| 4 | 1 | 消息类型 | MidMsgType |
| 5 | 1 | 保留 | 0x00 |
| 6-9 | 4 | 错误码 | 大端，err_ |
| 10.. | namelen | 设备名 | glob_params.name |
| ... | 2 | 标签数 | 大端（AddTagCnt2SockBuffer） |

**标签记录（每个标签，AddTag2SockBuffer 二进制分支）**

| 偏移 | 长度 | 字段 |
|---|---|---|
| 0 | 1 | AntennaID |
| 1 | 1 | ReadCnt（读次数） |
| 2 | 1 | RSSI（有符号） |
| 3 | 1 | protocol |
| 4 | 1 | Epclen（EPC 长度） |
| 5.. | Epclen | EPC 原始字节 |
| ... | 1 | EmbededDatalen（bank 数据长度） |
| ... | EmbededDatalen | bank 数据 |

### 2.3 错误码编码

`err_ = ecode + httpAPIErrCodeBase`（ecode != 0 且 < 基础值时），大端 4 字节。

### 2.4 JSON 格式（HTTP / MQTT 推送）

来源：`reader_msg.c` AddMsgHeader2SockBuffer_j + AddTag2SockBuffer_j

**帧结构（默认分支）**

```json
{
  "reader_name": "<设备名>",
  "event_type": "<事件类型>",
  "event_data": [ <标签> ]
}
```

**事件类型（event_type）**

| 事件 | 值 |
|---|---|
| 标签读取 | tag_read |
| 单标签实时 | tag_coming |
| 心跳 | heart_beat |
| 设备异常 | reader_exception |
| GPIO 触发 | gpi_changed |
| 时间同步请求 | sync_time_req |

**标签 JSON（默认短格式，字段由 tag_json_format 位开关控制）**

| 字段 | 含义 | 位开关 |
|---|---|---|
| ep | EPC hex 字符串 | 必含 |
| bd | bank 数据 hex | 0x01 |
| at | 天线号 | 0x02 |
| rc | 读次数 | 0x04 |
| fq | 频率 | 0x08 |
| pt | 协议 | 0x10 |
| ri | RSSI（有符号） | 0x20 |
| rv | 保留值+CRC 组合 | 0x40 |
| ft | 首次时间戳（ms） | 0x80 |
| lt | 末次时间戳（ms） | 0x100 |

**完整格式开关（tag_json_format & 0x8000）**

```json
{"epc":"...","bank_data":"...","antenna":n,"read_count":n,"protocol":n,"rssi":n,
 "firstseen_timestamp":ms,"lastseen_timestamp":ms}
```

### 2.5 HID 键盘输出格式

来源：`usb_utility.c` hid_kbd_type_epc

- EPC 每字节拆 2 个 hex 字符（**大写**）逐个发送
- 标准 USB HID 扫描码：0-9 → 0x27,0x1E..0x26，A-F → 0x04-0x09
- 末尾 Enter（0x28）
- 12 字节 EPC ≈ 24 字符 + Enter，约 80ms

---

## 3. moduleapi HTTP 接口（设备 HTTP 服务器）

来源：`HttpModuleAPI.cpp` L272-291

**端口**：默认 **8080**（`networkParaConfig.listenPort`，`hc32f46_driver.h` L87），
**可通过网络配置修改**（设备搜索弹窗/网络配置下发）。基础 URL = `http://{device_ip}:{listenPort}/moduleapi/`。
注意：旧版文档（V2 变更记录附带）曾写 8090，为过期版本，以 8080 为准。

| URL | 用途 |
|---|---|
| /moduleapi/paramset | 运行参数设置 |
| /moduleapi/paramget | 运行参数读取 |
| /moduleapi/syncinventory | 同步盘点 |
| /moduleapi/startasyncinventory | 启动异步盘点 |
| /moduleapi/stopasyncinventory | 停止异步盘点 |
| /moduleapi/getasynctags | 取异步标签 |
| /moduleapi/readtagbank | 读标签 bank |
| /moduleapi/writetagbank | 写标签 bank |
| /moduleapi/writetagepc | 写标签 EPC |
| /moduleapi/locktag | 锁标签 |
| /moduleapi/killtag | 杀标签 |
| /moduleapi/getgpi | 读 GPI |
| /moduleapi/setgpo | 写 GPO |
| /moduleapi/psamtransceiver | PSAM 透传 |
| /moduleapi/reboot | 重启 |
| /moduleapi/resetrfidmodule | 复位 RFID 模块 |
| /moduleapi/eascfg | **EAS/名单配置（get/set）** |
| /moduleapi/tagdata | 标签方法保存（兼容） |

---

## 4. EAS / 名单配置协议（eascfg）

来源：`HttpModuleAPI.cpp` httpEascfg

### 4.1 读取（get=1）

响应 JSON 字段：

| 字段 | 说明 |
|---|---|
| result | "get" |
| tagstoragedays | 标签存储天数 |
| totaltags | 白名单总数（FlashDB 计数） |
| totalalarmcnt | 报警总数 |
| deviceID | 设备 ID |
| easflag | EAS 标志 |
| radar_range | 雷达范围 |
| peoplecount | 人数统计 |
| alarm_duration / alarm_volume / alarm_switch / tag_read_cnt | 报警参数 |
| filter_rule[] | 过滤规则（10 条）：ch_num/ch_status/start_addr/match_len/mask_code |

### 4.2 写入（set）

| 字段 | 说明 |
|---|---|
| set | 1 = 写入 |
| tagstoragedays | 存储天数 |
| easflag | EAS 标志 |
| filter_rule[] | 过滤规则 |
| radar_range / alarm_volume / tag_read_cnt / alarm_duration / alarm_switch | 报警参数 |
| accumulated_time / accumulated_count | 累计时间/次数 |
| opening_time / closing_time | 开/闭门时间 |
| system_time | 系统时间 |
| remark | 备注 |

### 4.3 白名单读写（HTTP readtag 分块）

- 读：遍历 whitelistDB（FlashDB TSDB）导出 EPC 列表
- 写：del_all_cb 软删全部 → tagtable_list_update(taglist, epcid, uflag) 逐条写入
- 存储：FlashDB whitelistDB TSDB + 内存链表 ADDlist/DELlist/taglist

---

## 5. MQTT 主题与载荷

来源：`mqtt_interface.c`（MQTTClient.h）

| 参数 | 字段（配置 upload.sw_potl_params.mqtt） |
|---|---|
| Broker | host / port |
| 认证 | user / pwd |
| Keepalive | kal_time（5-86400s） |
| 发布主题 | pub_topic（≤48 字符） |
| 发布 QoS | pub_qos |
| 广播订阅主题 | sub_b_topic |
| 单播订阅主题 | sub_u_topic |

- 设备 ClientID = glob_params.name，MQTTVersion 4（MQTT 3.1.1），cleansession=1
- 标签数据经 send_evt_tagcoming → JSON 帧 → pub_topic 发布
- 上位机/移动端订阅 pub_topic 即收实时标签；下发走 sub_b_topic/sub_u_topic

---

## 6. 双向数据流（WinUSB / 网络统一）

### 6.1 设备 → 上位机（上报）

```
RFID 模块 → tagGetNext → send_evt_tagcoming
  ├─ hw_inf==HidKb(3) ─→ HID 键盘输出（hex + Enter）
  ├─ HTTP/MQTT ───────→ JSON 帧（AddMsgHeader2SockBuffer_j）→ up_send
  └─ 其它 ────────────→ 二进制帧（AddMsgHeader2SockBuffer）→ up_send
```

### 6.2 上位机 → 设备（下发）

```
上位机 ── moduleapi HTTP / MQTT sub 主题 / WinUSB 命令 ──→ 设备
   ├─ eascfg（名单/报警参数）
   ├─ paramset/paramget（运行参数）
   └─ 其它控制命令（reboot/gpo/盘点控制...）
```

### 6.3 多端一致性

| 能力 | PC 上位机 | App / 小程序 |
|---|---|---|
| USB 直连（WinUSB/CDC/MSC/HID） | ✅ | ❌（无 USB） |
| MQTT 订阅标签 | ✅ | ✅（WS 桥接） |
| moduleapi HTTP | ✅ | ✅（wx.request / HTTP） |
| 名单下发 | ✅ | ✅（走网络协议） |
| OTA 升级 | ✅（串口/USB/HTTP） | ❌（或远程触发） |

---

## 7. 附录：字段枚举参考

### 7.1 标签 JSON 字段位开关（tag_json_format）

| 位 | 字段 |
|---|---|
| 0x0001 | bd（bank data） |
| 0x0002 | at（天线） |
| 0x0004 | rc（读次数） |
| 0x0008 | fq（频率） |
| 0x0010 | pt（协议） |
| 0x0020 | ri（RSSI） |
| 0x0040 | rv（保留+CRC） |
| 0x0080 | ft（首次时间戳） |
| 0x0100 | lt（末次时间戳） |
| 0x8000 | 完整格式开关（epc/bank_data/antenna/...） |

---

*文档结束。协议字段以固件源码为准（本仓库 hc32f4a0_app），如有冲突以代码为准。*
