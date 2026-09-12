# STM32F0 中继板 -- Linux 上行/下行接口说明

> 版本 v1.0 · 对应固件 `STM32F0_linux_v4.31`（提交 `9cd2366` 之后的链路形态）
> 适用范围：Linux 主机 <-> STM32F0 中继板（COM1/UART1）之间的 IPC 协议。
> 相关实现：`BSP/app.c`（`ipcReportBuild`/`ipcReportStatus`/`ipcHandleLegacy`/`ipc_hpm_message`）、`BSP/app.h`（`alarm_pdu`/`gpio_pdu`）。
> 解析库：`docs/cpp/stm32_gpio_pdu.hpp`（仅头文件），示例：`docs/cpp/example_gpio_pdu.cpp`。

---

## 1. 链路与串口参数

| 项 | 值 |
| --- | --- |
| 物理口（STM32 侧） | COM1 = UART1 = `IPC_huart1` |
| 波特率 / 帧格式 | 115200，8 数据位，无校验，1 停止位（8N1） |
| Linux 侧 | 对应 /dev/ttySx（或 USB 转串口 /dev/ttyUSBx），需 raw 模式 |
| 方向 | 全双工；**两个方向的帧头都是 0xFF（PDUHEAD）**，靠方向区分 |
| 定长 | 32 字节（`APP_FRAME_LEN_MAX`） |

一个串口上只有一种帧长（32B），没有变长帧混跑；`0xAA` 变长帧当前**仅保留**（STM32 不会主动发送，收到也会忽略）。

---

## 2. 帧总览

两个方向共用同一套“外壳”，只是负载结构不同：

    +--------+---------+--------+-------+----------------+--------+--------+
    | 0      | 1       | 2      | 3     | 4 .. 29        | 30     | 31     |
    | 0xFF   | 32      | DevID  | AntID | Payload (26B)  | CRC_L  | CRC_H  |
    +--------+---------+--------+-------+----------------+--------+--------+
      帧头    帧长      设备号   通道/天线  见下             CRC16 小端

- **CRC**：CRC-16/CCITT-FALSE —— poly `0x1021`，初值 `0xFFFF`，逐位 MSB 优先，**不反射、无终值异或**；覆盖第 0..29 字节；小端存放（低字节在偏移 30，高字节在偏移 31）。
- 自检向量：`"123456789"` -> `0x29B1`（两端与解析库都用同一条算法，见第 7 节）。
- CRC 或帧头/帧长不合法 -> **直接丢弃，不回错**。解析端应逐字节重同步（见第 6 节）。

| 方向 | 负载结构 | 说明 |
| --- | --- | --- |
| 上行（STM32 -> Linux） | `gpio_pdu`：Rad_Status[8] + Alarm_Done[8] + GPIO[10] | 见第 3 节 |
| 下行（Linux -> STM32） | `alarm_pdu`：Alarm_Duration[6] + Radarcfg[5] + 时间戳/随机数/保留 | 见第 4 节 |

---

## 3. 上行帧：`gpio_pdu`（STM32 -> Linux）

STM32 每拍（20ms）轮询 5 路雷达板，把结果汇总成这一帧推给 Linux。

| 偏移 | 长度 | 字段 | 取值 / 语义 |
| --- | --- | --- | --- |
| 0 | 1 | FrameHead | `0xFF`（PDUHEAD） |
| 1 | 1 | Pdu_len | `32`（与 `sizeof(gpio_pdu)` 一致，STM32 侧有编译期断言） |
| 2 | 1 | DeviceID | 设备号；当前 **0**（语义待定） |
| 3 | 1 | AntID | 当前 **0**（整机聚合上报，不分通道） |
| 4..11 | 8 | Rad_Status[8] | **按 RFID 天线 1..8**：`1` = 有人 / `0` = 无人（下标 0 = 天线1） |
| 12..19 | 8 | Alarm_Done[8] | 按天线 1..8：`1` = 该天线所属雷达板**正在声光报警**（红灯在闪或蜂鸣器在响） |
| 20..29 | 10 | GPIO[10] | 预留，当前全 0（STM32 自身 IO 状态，待定口径） |
| 30..31 | 2 | crc | CRC-16/CCITT-FALSE 小端，覆盖 0..29 |

### 3.1 8 支天线怎么来的（5 路雷达板 -> 8 支天线）

STM32 只有 5 路雷达口，RFID 模块是 8 支天线，映射固定为：

    COM6 -> 天线1 ; COM2 -> 天线2,3 ; COM3 -> 天线4,5 ; COM4 -> 天线6,7 ; COM5 -> 天线8

一块雷达板覆盖 1~2 支天线，因此该板的“有人 / 报警中”状态会**复制到它对应的所有天线**上。

### 3.2 数据来源与超时

- `Rad_Status` 源：HC32 报警板 0xAA Cmd 0x10 应答 Byte0 的雷达位（bit0..2 = 雷达1/2/3，取或）。
- `Alarm_Done` 源：同应答 Byte2（HC32 侧 = 红灯/蜂鸣器正在动作）。
- **某口 200ms 没有有效应答**（板子断电/掉线/该路关闭）-> 该口对应的天线一律按 `Rad_Status=0`、`Alarm_Done=0` 上报，并在变化时主动推一帧（Linux 不会看到“冻结”的旧值）。
- HC32 Byte1（安装模式位图）与 Byte0 的 bit3/bit4（摄像头/继电器）当前**没有**进上行帧，只在 STM32 内部参与“变化即报”判定。

### 3.3 发送时机（Linux 侧不用轮询）

1. **变化即报**：任一被比较的端口数据变化 -> 立即发一帧；
2. **心跳**：连续 1000ms 无变化 -> 发一帧（所以“有帧”= 链路在线）；
3. **命令回执**：收到 Linux 的任意合法下行帧且 `AntID != 0` -> 立即多发一帧状态。

> 注意：STM32 的“变化”判定包含内部 `workMode`，所以**可能收到内容与前帧完全相同的帧**，这属于正常现象（不要用它推断“有变化”，要比较内容）。

### 3.4 示例帧

“天线1 有人、天线3 有人且报警中”，其余天线无人：

    偏移 0..3   : FF 20 00 00          帧头=0xFF, 帧长=32, DevID=0, AntID=0
    偏移 4..11  : 01 00 01 00 00 00 00 00   天线1=1, 天线3=1
    偏移 12..19 : 00 00 01 00 00 00 00 00   天线3 报警中
    偏移 20..29 : 00 00 00 00 00 00 00 00 00 00
    偏移 30..31 : 2E 8B                 CRC = 0x8B2E (小端)

整帧：`FF 20 00 00 01 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 2E 8B`

---

## 4. 下行帧：`alarm_pdu`（Linux -> STM32）

| 偏移 | 长度 | 字段 | 取值 / 语义 |
| --- | --- | --- | --- |
| 0 | 1 | FrameHead | `0xFF`（PDUHEAD） |
| 1 | 1 | Pdu_len | `32` |
| 2 | 1 | DeviceID | 设备号（HC32 侧会原样回填） |
| 3 | 1 | AntID | **天线号 1..8**；`0` = 特殊命令（见下） |
| 4..9 | 6 | Alarm_Duration[6] | [0]蜂鸣占空 [1]雷达距离门 [2]LED 颜色码 [3]报警时长(s) [4]EAS 开关 [5]离线标志 |
| 10..19 | 10 | Radarcfg[5] | 5 个 uint16 雷达配置（当前未使用） |
| 20..23 | 4 | time_stamp | 时间戳（当前未使用） |
| 24..27 | 4 | random_forest | 随机数（当前未使用） |
| 28..29 | 2 | reserved | 保留 |
| 30..31 | 2 | crc | CRC-16/CCITT-FALSE 小端，覆盖 0..29 |

### 4.1 AntID 语义（= 天线号，STM32 转发到对应雷达板）

| AntID | 转发到 | 说明 |
| --- | --- | --- |
| `1` | COM6 | 天线1 所在雷达板 |
| `2`,`3` | COM2 | 天线2,3 |
| `4`,`5` | COM3 | 天线4,5 |
| `6`,`7` | COM4 | 天线6,7 |
| `8` | COM5 | 天线8 |
| `0` | 5 口广播 | 特殊命令：网络离线 / LED 自检（此时**不会**触发状态回执） |

STM32 收到合法帧后：CRC 校验通过 -> 按 `AntID` 转发给对应雷达板（HC32）+ 闪对应端口 LED + 立即回一帧 `gpio_pdu`。

### 4.2 LED 颜色码（`Alarm_Duration[2]`，HC32 侧判据）

| 值 | 宏 | 行为 |
| --- | --- | --- |
| `0x5A` | ALARM_R_CODE | 红警：红灯闪 + 蜂鸣 + 继电器 |
| `0xA5` | ALARM_G_CODE | 绿放行：绿灯 + 不鸣叫 |
| `0x55` | ALARM_RELAY_CODE | 继电器联动报警（同红警） |
| `0x90` | ALARM_NONE_CODE | 无警（当前未使用） |
| 其它 | — | HC32 走 `Alarm_Off()`（全停） |

`Alarm_Duration[3]` = 报警时长（秒），HC32 收到 0 会强制为 5。`Alarm_Duration[5]` 仅在 `AntID == 0` 时作为“网络离线标志”。

### 4.3 注意：目前没有“纯查询”命令

下发 `AntID != 0` 的帧**本身就是一条真实命令**（会被转发给对应雷达板执行），只是顺带触发一帧状态回执。所以：

- 只看状态：依赖“变化即报 + 1s 心跳”即可，**不要靠轮询**；
- 需要立刻要一帧：可以发一条合法命令（会真的执行）；如果以后需要纯查询，需要在固件里加一个专用 Cmd（例如 `AntID=0` + 特定 `Pdu_len`/保留字节），再补文档。

---

## 5. 解析实现（C++，仅头文件）

| 文件 | 说明 |
| --- | --- |
| `docs/cpp/stm32_gpio_pdu.hpp` | 解析库：`crc16_ccitt()`、`GpioPdu`、`GpioPduParser`（流式、自动重同步、统计）、`GpioPduStatus`（状态缓存 + 链路判活）、`build_frame()`（自测组帧） |
| `docs/cpp/example_gpio_pdu.cpp` | 示例程序：`--selftest` 无硬件自测；`-d /dev/ttySx -b 115200` 读串口；不给设备则读 stdin |

### 5.1 最小用法

```cpp
#include "stm32_gpio_pdu.hpp"
using namespace alarm_board;

GpioPduParser parser;       // 流式解析器
GpioPduStatus status;       // 最新状态 + 链路判活

void on_serial_data(const uint8_t* data, size_t n, uint64_t now_ms)
{
    parser.feed(data, n);   // 串口读到多少喂多少, 允许任意切分

    GpioPdu f;
    while (parser.pop(f))   // 取出完整且 CRC 正确的帧
    {
        const bool changed = status.update(f, now_ms);
        for (int ant = 1; ant <= 8; ++ant)
        {
            if (f.present(ant))  { /* 天线 ant 有人 */ }
            if (f.alarming(ant)) { /* 天线 ant 正在报警 */ }
        }
    }

    if (!status.link_alive(now_ms))   // 默认 2.5s 超时
    {
        /* 链路断线: 心跳(1s)都没来 */
    }
}
```

也可以用回调风格：

```cpp
static void cb(const GpioPdu& f, void* user) { /* ... */ }
parser.set_callback(cb, nullptr);
```

### 5.2 编译与运行（无需交叉编译）

    g++ -std=c++11 -O2 -Wall -Wextra -I docs/cpp docs/cpp/example_gpio_pdu.cpp -o gpio_pdu
    ./gpio_pdu --selftest                  # 自测: CRC 向量 + 组帧/切分/噪声/坏 CRC/变化判定
    ./gpio_pdu -d /dev/ttyS0 -b 115200     # 读串口
    cat /dev/ttyS0 | ./gpio_pdu            # 或从 stdin 读

自测项（`--selftest`）会验证：CRC 向量 `0x29B1`、整帧解析、逐字节喂入、噪声+半帧重同步、坏 CRC 丢弃、心跳帧不算变化。

---

## 6. 健壮性建议（Linux 侧）

| 场景 | 建议 |
| --- | --- |
| 上电/换线期间的杂字节 | 逐字节找 `0xFF` 重同步；不合法就丢 1 字节继续（解析库已内置） |
| 链路判活 | 心跳 1s，建议 >2.5s 无帧判离线 |
| 单帧丢失 | 不要依赖单帧；下一帧（≤1s 心跳）会补齐 |
| 断线期间的旧状态 | 判离线后应把 8 支天线的状态按“未知/无人”处理，不要继续沿用旧值 |
| 串口配置 | 必须 raw 模式（`cfmakeraw`），否则 0xFF/0x0A 会被行规程吞掉 |
| 大缓冲 | 一次 read 可能含多帧或多帧的片段；解析库对任意切分都成立 |

---

## 7. CRC 参考实现

两端固件用的都是逐位 CCITT-FALSE（`0x1021` / `0xFFFF`，无反射、无终值异或）。

C：

```c
uint16_t crc16_ccitt(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < n; i++)
    {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
```

Python：

```python
def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

assert crc16_ccitt(b"123456789") == 0x29B1     # 自检向量
```

---

## 8. 变更记录

| 版本 | 日期 | 说明 |
| --- | --- | --- |
| v1.0 | — | 首版：确定上行 `gpio_pdu`（0xFF 32B，8 支天线）、下行 `alarm_pdu`（AntID=天线号 1..8）、CRC、时序、超时与解析库 |

### 尚未定稿的字段（等待确认后更新本文档）

| 字段 | 现状 | 待定 |
| --- | --- | --- |
| `DeviceID` | 上行恒 0 | 多板/多机时的设备号编址方式 |
| `AntID` | 上行恒 0 | 是否按天线分包上报 |
| `GPIO[10]` | 恒 0 | 是否上报 STM32 自身输出脚（Host_IRQ / 5 个口 LED / 蜂鸣器 / GPO1,2 / CM4RESET） |
| 下行 `Radarcfg[5]`/`time_stamp`/`random_forest` | 未使用 | 是否启用 |
