# Boot/OTA 设计 —— A/B 双槽即运行区、选择器标志、无搬运（设计稿 v0.2）

> 状态：设计稿，未改动任何固件代码。
> **v0.2 修订（2026-09-18）**：① 分发者由「Linux → STM32 中继」改为 **HC32F4A0 直连 RS485 逐板分发**；
> ② 升级期间 **关闭全部业务帧、链路独占、只跑固件下发**；③ 本地帧协议确定为 **OTA1 帧**（取代 v0.1 §9 建议的自建帧头族）；
> ④ 明确借鉴来源与 A/B 模型下的取舍；⑤ 载荷粒度定为 256B。
> 范围：HC32F4A0（主机）→ HC32F460 报警板（目标）。STM32F0 中继板与 Linux 主机的 OTA 本次不做（v0.1 的 Linux→STM32 中继路径暂缓，见 §12）。

## 0. 修订记录（v0.1 → v0.2）

| # | v0.1 | v0.2 | 依据 |
| --- | --- | --- | --- |
| 1 | Linux → STM32(中继) → HC32 | **HC32F4A0 直连 RS485 逐板分发** | 去掉 Linux/STM32 两跳；F4A0 物理上就在 RS485 一端，且有文件系统 + QSPI + USB-MSC 可自行存放固件 |
| 2 | §12 默认「升级期业务照常，仅切换瞬间复位」 | **升级期关闭全部业务帧、链路独占** | 半双工 RS485 上 34KB 下载会饿死业务；独占后两端状态机大幅简化 |
| 3 | §9 建议自建 0xA5 帧头族（后草案为 0xAA 的 0xA1~0xA6 OTA 命令族） | **复用既有 OTA1 帧**（魔数 `OTA1`） | C# 上位机 `OtaUpdater.cs` / Python `ota_send.py` / F4A0 与扫描板 `ota_transport_uart.c` 三端已实现且真机调优过；0x4F 与 0xFF/0x55/0xAA 全不冲突 |
| 4 | 载荷粒度未定 | **256B**（不是 4096B） | 雷达板 RS485 是 512B DMA 窗口 + 1KB 环形缓冲，267B 的帧正好落在一个窗口内，接收路径零改动；4096B 需 8 个窗口且环形缓冲 22ms 内被冲掉 |
| 5 | A/B 双槽、无搬运、按槽编译两份 | **不变**（本次确认维持） | 见 §2 |
| 6 | §7 未提启动改 VTOR 的具体位置 | **App 启动早期写 `SCB->VTOR = 本槽基址`** | M4 有 VTOR；M0（STM32）需向量表重映射，本次不涉及 |

## 1. 背景与目标

- 需求：由 HC32F4A0 主机把固件分发给各 HC32F460 报警板并完成升级，**不依赖 PC 与 Linux 主机**；
- 硬指标：升级过程任何时刻掉电/中断/新固件无法启动，设备都不变砖，且可自动回退；
- 现状固件体积（Release、去调试信息）：HC32 ≈25.46KB（512KB 的 5.0%）、RAM 7.5KB → 空间余量充足，容量不是问题，流程安全才是问题；
- 主机侧资源：HC32F4A0PITB（2MB Flash / 512KB RAM），承载固件仓库（QSPI 暂存 / 文件系统 / USB-MSC 导入）与分发逻辑，余量充足。

## 2. 核心模型（本设计的基本盘）

1. A/B 双槽即为 App 运行区：新固件写入「非活动槽」，写完只翻转一个选择器标志，Boot 按标志直接跳到 A 或 B 运行；
2. 不做任何「搬运/拷贝覆盖」（暂存槽→固定运行区的两段式单槽模型被否决：搬运中断即损坏运行区；引入备份区虽可重试恢复，但多一个脆弱窗口与一套恢复逻辑）；
3. 选择器标志是唯一真值：版本号不参与选槽（见 §6）；
4. RAM 只做页级缓冲（1KB），不做整包暂存；
5. 下载期页粒度擦写**非活动槽**，运行区永不触碰；
6. Boot 区永不自擦、永不参与槽切换以外的写操作；App 与升级流程无权写 Boot。

## 3. 内存布局（默认值，可调）

### HC32F460xE（512KB Flash，基址 0x00000000，EFM 扇区 8KB）

| 区 | 地址 | 大小 |
| --- | --- | --- |
| Boot | 0x00000000 | 32KB |
| 槽 A（App 运行区） | 0x00008000 | 128KB |
| 槽 B（App 运行区） | 0x00028000 | 128KB |
| 保留/扩容 | 0x00048000 | ~192KB |
| 参数/选择器标志区（双份） | 0x0007E000 | 8KB |

> 标志区取 0x7E000 是按 **8KB 扇区对齐**选的（0x7E000 是最后一个完整扇区）；注意不要用扫描板 `boot_iap` 里的 0x7F000 —— 它落在扇区中间，擦除会连带邻区。

### STM32F030CC（256KB Flash，基址 0x08000000）——本次不实施，保留原设计

| 区 | 地址 | 大小 |
| --- | --- | --- |
| Boot | 0x08000000 | 16KB |
| 槽 A（App 运行区） | 0x08004000 | 64KB |
| 槽 B（App 运行区） | 0x08014000 | 64KB |
| 保留/扩容 | 0x08024000 | ~104KB |
| 参数/选择器标志区（双份） | 0x0803F000 | 4KB |

## 4. 槽镜像头（每个槽首部固定字段）

| 偏移 | 字段 | 长度 | 说明 |
| --- | --- | --- | --- |
| 0 | Magic | 4B | 固定魔数，区分空/错槽 |
| 4 | Version | 4B | 主.次.构建（仅展示/防呆，不参与选槽） |
| 8 | ImageLen | 4B | 镜像字节数 |
| 12 | CRC32 | 4B | 整包（含头）CRC32 |
| 16 | TargetSlot | 1B | A 或 B（防错槽运行） |
| 17+ | 保留 | - | 后续可加日期/描述 |

Boot 跳槽前校验：Magic 正确 → ImageLen 在合法区间 → 整镜像 CRC32 一致 → TargetSlot 与槽位一致。

> 与 OTA1 包的关系：OTA1 包 = 82B 包头（含 len/CRC32/SHA256/HMAC）+ payload。**写入槽时把槽镜像头与 payload 一起落盘**，槽头由设备端在写入过程中生成（Magic/Version/ImageLen/CRC32/TargetSlot），因此主机不需要预置槽头。

## 5. Boot 启动流程与选择器标志

选择器标志区独立放置，双份存储 + 各自 CRC；每槽状态字：EMPTY / RUNNABLE / TRIAL(试运行) / FAILED。

```
复位 → 读双份标志(各自CRC)
  ├─ 双份一致且有效: 选中槽 RUNNABLE/TRIAL → 校验该槽头+整CRC
  │      ├─ 通过 → 状态置 TRIAL, 跳转该槽, 开"新固件确认窗口"(默认3s)
  │      └─ 失败 → 该槽置 FAILED(计数+1), 切另一槽(回到流程头)
  ├─ 双份不一致/损坏 → 按 A→B 顺序取第一个 Magic+CRC 全过的槽运行
  ├─ 全部槽无效 → 停留 Boot(等待 OTA / 上报主机) —— 不跳任何槽=不砖
  └─ 跳转后新固件未在窗口内喂狗(起不来) → 该槽 FAILED+1, 复位切另一槽;
      同槽连续失败达上限(默认2次) → 不再自动试, 上报主机等待人工/重发
```

## 6. 为什么版本号不用于选槽

| 情形 | 若用版本号比较 | 用选择器标志 |
| --- | --- | --- |
| 两槽版本相同（重刷同版/改配置发布） | 无法区分 | 明确选刚下载的槽 |
| 新版本起不来 | 版本高→每回都跳坏槽→一直砖 | 试运行失败→自动回滚 |
| 人为降级发布 | 版本低→永不选中 | 下载流程明确写目标槽 |
| 某槽头损坏/半擦 | 读到假版本误判 | 先过 Magic+CRC，坏了不参与选择 |
| 双份标志半写 | — | CRC 判无效→顺序扫描兜底 |

版本号职责收敛为：主机读取/展示当前运行版本、下载前防呆（新版本≤当前版本时提示/中止）、日志记录。

## 7. 按槽分别编译两份（两槽链接不同偏移）

- 槽 A 与槽 B 是两个独立编译产物（同一源码，链接脚本偏移不同：A 起点 vs B 起点）；
- **HC32F460（M4F，有 VTOR）：App 启动早期写 `SCB->VTOR = 本槽基址` 即可**；
- STM32F030（M0，无 VTOR）：App 在非 0 偏移启动必须把向量表搬到 SRAM 底部并用 `SYSCFG_CFGR1.MEM_MODE` 重映射 0 地址（A/B 各自实现），Boot 跳转前也要正确设置（本次不实施）；
- 禁止「同一份镜像运行时自适应基址」：向量表为链接期写死，做不到——发布流程固定为两步构建（产 A 镜像 + 产 B 镜像），镜像头 TargetSlot 字段防止错槽烧录/运行。

## 8. 升级全流程（无搬运步骤）

链路：HC32F4A0 --RS485@460800--> HC32F460 报警板（点对点，按通道选择，无需寻址）

**进入升级模式（链路独占）**

1. 主机在目标通道上发出「进入升级」请求，并**停止该链路的全部业务帧**；
2. 报警板收到后：停止上行状态帧、停止对业务帧的应答、把 RS485 接收路径切到 OTA1 帧解析、回「就绪」；
3. 双方约定：升级期间该链路**只跑 OTA1 帧**，任一方在超时（默认 10s 无进展）后各自退出升级模式、恢复业务。

**下载与激活**

4. 主机读目标板当前状态（运行槽 / 版本 / 槽状态），选定**非活动槽**对应的镜像；
5. 主机发 OTA1 包头帧握手；报警板按 §9.3 回 ACK/RESUME；
6. 分块下载：每块 ≤256B，带序号 + CRC16；报警板**按页粒度擦写非活动槽**（8KB 扇区），写完即回 ACK（携带绝对偏移）；
7. 整包完成 → 报警板重算整镜像 CRC32（及 SHA256/验签，按安全等级）比对；
8. 校验通过 → **ACTIVATE：写「目标槽 RUNNABLE」标志（双份+CRC）** ← 唯一破坏性点，对另一槽零影响；回「升级完成」；
9. 复位 → Boot 读标志 → 校验 → 跳新槽（TRIAL）→ 新固件 3s 内喂狗确认 → 状态转 RUNNABLE；
10. 主机恢复该链路业务帧，并把升级结果（成功/回滚/版本）上报上层。

### 掉电矩阵（任何时刻掉电均不砖）

| 掉电时刻 | 结果 |
| --- | --- |
| 非活动槽擦/写到一半 | 该槽 CRC 不过 → 继续跑另一槽，可整包重传 |
| 标志写了一半 | 双份+CRC 判「未就绪」 → 跑原槽 |
| 新固件启动崩溃/超时 | TRIAL 失败计数 → 自动回原槽并上报主机 |
| 两槽都被写坏（出厂前/异常双写） | 停留 Boot 等待恢复（Boot 自身永不被写） |

## 9. 协议（本地帧 = OTA1 帧）

### 9.1 帧格式

```
[0:4] "OTA1"   [4] type   [5:7] seq(16LE)   [7:9] len(16LE)   [9:9+N] payload   [末2] CRC16(LE)
```

- CRC16：CCITT-FALSE，poly 0x1021，init 0xFFFF，MSB-first，覆盖 [0 : 9+len]；
- type：0x50=DATA（主机→设备）、0x51=ACK（设备→主机，载荷 4B LE 偏移）、0x52=RESUME（设备→主机，载荷 4B LE 偏移）；
- 魔数首字节 0x4F（`'O'`）与现有帧头 0xFF（报警/参数 alarm_pdu）、0x55（状态/GPIOHEAD）、0xAA（变长帧）**全不冲突**；
- 接收端按首字节分流，可在既有 `frame_rx_feed()` 状态机上加第三路分支，不动原两路。

### 9.2 OTA1 包与偏移语义

- 包 = 82B 包头（`OTA1` + ver + platform + app + len + CRC32 + SHA256 + HMAC）+ payload；
- **串口语义（本设计采用）：偏移是「含 82B 包头的绝对偏移」，total = len(pkg)**；
- 对照 USB-CDC/WinUSB 语义是「载荷相对偏移」，total = len(pkg)-82 —— **两者不可混用**（C# 侧曾因串口重复发包头导致包头被写进载荷、卡在 41370，故串口首帧**只发一次**）。

### 9.3 主机侧发送算法（借鉴 C# `OtaUpdater.Update()` / Python `ota_send.py`）

```
1. 首帧 DATA(seq=0, payload=82B 包头)，只发一次，等 4s
     ACK<0        → devOff = 0（从 0 起发，首数据帧含包头字节）
     ACK<82       → devOff = 82
     否则          → devOff = ACK
2. while devOff < total:
     chunk = pkg[devOff : +min(256, total-devOff)];  seq = devOff/256 + 1
     发 DATA 帧 → 等设备帧(3s)
       ACK/RESUME  → 跟随设备偏移 devOff（可进可退）
       超时        → 发探测帧 len=0 DATA(seq=0xFFFF) → 等 2.5s → 跟随设备偏移
     ackedMax 连续 10s 不推进 → 中止并退出升级模式
3. 进度满 → 再发一次探测帧兜底，等设备 finish+复位
读设备帧：逐字节同步 "OTA1"；非帧字节是设备 TRACE 文本，忽略；plen>64 或 CRC 错 → 重同步
版本查询：RESUME(seq=0xFFFF, payload="VER1") → ACK(4B 版本)
```

### 9.4 设备侧接收（借鉴扫描板 `ota_transport_uart.c` 设备端语义）

- 首帧 DATA 且已有进度 → 回 RESUME(已收偏移)；包头不一致（残留/换包）→ 清进度回 ACK(82)；
- 帧 CRC16 错/超尾 → 回 RESUME(当前偏移) 请求重发；
- 每写完一块 → 回 ACK(已写绝对偏移)；
- 整包校验通过 → 激活标志 → 回「完成」。

### 9.5 载荷粒度：256B

| 粒度 | 帧长 | 460800 下线上耗时 | 与雷达板接收路径的关系 |
| --- | --- | --- | --- |
| **256B** | 267B | 5.8ms | **正好落在一个 512B DMA 窗口内**，接收路径零改动 |
| 4096B | 4107B | 89ms | 需 8 个窗口；1KB 环形缓冲在 22ms 内被冲掉，必须加大缓冲或改接收方式 |

34KB 固件 ≈ 136 帧 ≈ **1~1.5 秒**（含逐帧 ACK 往返）；即使按 128KB 满槽算也仅约 4 秒。

## 10. 关键工程约束清单（实现时必须满足）

- Boot 区永不自擦/自升级；App 与升级流程无 Boot 区写权限；
- **擦写代码必须 RAM 驻留**：片内 Flash 擦/写期间取指不能来自 Flash —— 参照 `boot_iap` 的 `RW_RAMCODE` 段（把 `hc32_ll_efm.o` / `flash.o` / OTA 相关 .o 放 RAM 执行），Boot 与 App 两侧都要；
- 下载期页擦写避开业务时序（升级期业务已停，只需保证喂狗）；
- 每页写后回读校验，失败即中止并上报（可重传该页）；
- 整包传输失败策略（默认）：整包重传；断点续传作为会话内能力（设备进度不回写 Flash，掉电即从 0 重来）；
- 升级期间看门狗：下载期与 Boot 搬运/跳转窗口都要喂狗，超时值按最长单次阻塞（8KB 扇区擦除）留足余量；
- App 侧 OTA 入口挂在既有 `Check_Uart_Pdu()` 的收帧循环上，新增 `'O'`(0x4F) 分支，不改动 0xFF/0xAA 两路既有行为。

## 11. 默认参数表

| 参数 | 默认值 |
| --- | --- |
| 分块大小 | 256B（帧长 267B） |
| 校验 | 帧 CRC16 + 整包 CRC32（安全等级 2/3 再加 SHA256/HMAC） |
| 新固件确认窗口 | 3s（Boot 看门狗） |
| 自动回退上限 | 同槽连续失败 2 次后不再自动试 |
| 会话断点续传 | 仅会话内；掉电整包重传 |
| 链路超时 | 单帧 ACK 3s；无推进 10s 退出升级模式 |
| Boot 大小 | HC32 32KB（默认） |
| 槽大小 | HC32 A/B 各 128KB（默认，余量可扩） |
| 波特率 | 460800（与现行 RS485 一致，不改） |

## 12. 开放问题

- **STM32F0 中继板与 Linux 主机的 OTA 路径**：v0.1 的中继分发设计暂缓（本次范围外）；STM32 自身升级的入口与时机需另行确定；
- 是否需要保留「上一版」能力（多版本回退，需 N+1 槽位或主机侧存历史包）；
- Boot 本身升级路径（建议只在产线用调试器更新，远程不做）；
- 每块报警板在 OTA 命令里的编址方式（与现有 AntID/通道映射衔接）——本次为点对点、按通道选路，无需寻址；
- 「进入升级」请求用哪种帧承载：业务帧命令（0xAA 变长帧新增 Cmd）还是直接以 OTA1 首帧触发，需在实现前定稿（见 §13 待定项）。

## 13. 借鉴来源与落地分工

### 13.1 HC32F460 报警板侧 —— 借鉴 `Scanner_20260901`

| 借鉴对象 | 用途 |
| --- | --- |
| `boot_iap/source/flash.c` | 片内 Flash 擦/写/回读（RAM 驻留） |
| `boot_iap/source/boot_ota.c` | OTA 状态记录、校验、跳转决策 |
| `boot_iap/source/fw_jump_helper.c` | 跳转前的时钟/PLL/FCG/wait cycle 恢复 |
| `boot_iap/MDK/config/linker/*.sct` | `RW_RAMCODE` RAM 执行段写法 |
| `hc32f46_app/.../ota_transport_uart.c` | 设备端 OTA1 帧处理（收帧/回 ACK/续传） |

> **注意模型差异**：`boot_iap` 是 **single_bak** 模型（QSPI 暂存 + 备份 + 覆盖写片内 + 搬运），本设计是 **A/B 双槽无搬运**。
> 因此**只借鉴帧处理、Flash 擦写、跳转/时钟恢复、链接脚本**等机制代码，**不照搬其 QSPI 依赖、备份区与搬运流程**；
> 报警板无 QSPI 外设（不是未贴片），一切落在片内 Flash。

### 13.2 HC32F4A0 主机侧 —— 借鉴 C# 上位机串口 OTA

| 借鉴对象 | 用途 |
| --- | --- |
| `ReaderUI_v1_MCU/OtaUpdater.cs` | 发送主循环：包头握手 / 分块 / ACK 跟随 / 探测 / 卡死保护 / 收尾 |
| `ReaderUI_v1_MCU/OtaProtocol.cs` | 帧常量与 CRC16（与固件端一致） |
| `tools/ota_send.py` | 同一算法的 Python 参考实现（含注释里的真机调优结论） |
| F4A0 既有 `ota_frame.c/h` | 帧组包/解析/流式状态机（平台无关纯逻辑，可直接复用） |

落地形态：新增 **host 端发送器**（纯逻辑，只依赖「写字节 / 读字节」两个回调），底层调 `Uart_RS485_send()`；
**不改动** F4A0 既有的 `ota_transport_uart.c`（那是设备端，F4A0 自身升级在用）。

### 13.3 待定项（实现前定稿）

- 「进入升级」请求的承载帧（§12）；
- 升级期间是否**只停该通道**还是停全部通道的业务：本次按「只停该通道」实现，全局停作为可配项（全局停会让读写器主功能整体停顿数秒）；
- F4A0 自身 OTA 与「分发他人 OTA」的互斥策略（两者不得并发）。

## 14. 实现约定（v0.2 补充 · 2026-09-18 实现时定稿）

> **一处修订**：§4 附注原写「槽头由设备端在写入过程中生成」，实现改为 **包 payload 即槽镜像（含槽头），由打包侧生成、设备原样落盘**。
> 理由：免去设备侧按偏移搬移，且 TargetSlot 天然随镜像走；设备侧收满后校验 magic/长度/TargetSlot/CRC32，任一不过即拒绝。
> 代价：**主机必须先查目标槽再选对应 A/B 镜像下发**（见下方 SLOT 查询）。

### 14.1 槽镜像头（`ota_layout.h`）

- magic = `0x534C4F54`（'SLOT' LE）；头长 17B；
- 偏移：Version@4 / ImageLen@8 / CRC32@12 / TargetSlot@16。

### 14.2 CRC32

- **IEEE**：poly `0xEDB88320`，init/xorout `0xFFFFFFFF`（即 `zlib.crc32`），与 `tools/ota_pack.py` 一致；
- 用于：整镜像校验、标志记录 CRC。

### 14.3 选择器标志记录（40B）

| 顺序 | 字段 | 说明 |
| --- | --- | --- |
| 0 | magic | `0x4F544131`（"OTA1" LE） |
| 1 | seq | 写入序号，双份择新 |
| 2 | active | 当前运行槽 |
| 3,4 | state_a / state_b | EMPTY / RUNNABLE / TRIAL / FAILED |
| 5,6 | fail_a / fail_b | 连续启动失败计数 |
| 7 | boot_count | TRIAL 启动计数 |
| 8 | flags | NEED_CONFIRM（A/B 无搬运模式下 **不使用 NEED_COMMIT**） |
| 9 | crc32 | 覆盖 [0,36) |

双份各 4KB，同处 8KB 扇区 `0x7E000`；读时取「CRC 有效且 seq 更大」的一份；写时**先擦整扇区再写两份**（同扇区擦除会同时清掉两份）。

### 14.4 「激活」= 一次标志写入

`active = 目标槽`、该槽 `TRIAL`、`fail=0`、`boot_count=0`、`flags=NEED_CONFIRM`；另一槽若仍停在 TRIAL 则回落 RUNNABLE 作为回退。
**v0.1 的 `boot_state_t`（NEED_COMMIT + 搬运）语义未采用** —— A/B 无搬运下没有搬运阶段。

### 14.5 RAM 驻留：**不需要改 scatter**

- 本工程两套 scatter 的 `RW_IRAM2` **均已含** `.ANY (RAMCODE)`；
- DDL `hc32_ll_def.h` 已定义 `__RAM_FUNC = __attribute__((section("RAMCODE")))`；
- 直接把擦/写函数标 `__RAM_FUNC` 即可 —— 扫描板 `boot_iap` 另加 `RW_RAMCODE` 执行段的做法在本工程**不需要**。

### 14.6 EFM 调用序列（本工程 DDL 实测）

```
EFM_REG_Unlock();
  每个操作前: EFM_FWMC_Cmd(ENABLE);      <- 置 FWMC.PEMODE=1
              EFM_SectorErase() / EFM_Program();
EFM_FWMC_Cmd(DISABLE);
EFM_REG_Lock();
```

**每个操作前都要重新 ENABLE**：`EFM_Program`/`EFM_SectorErase` 退出时会把 PEMOD 复位成只读态（`hc32_ll_efm.c` 实测）。

### 14.7 查询扩展（附加式，不影响旧语义）

| 请求 | 响应 |
| --- | --- |
| `RESUME(payload="VER1")` | ACK(固件版本) —— 既有 |
| `RESUME(payload="SLOT")` | ACK(当前运行槽 0/1) —— **新增**，供主机选择 A/B 镜像 |

### 14.8 落地清单

| 位置 | 文件 | 角色 |
| --- | --- | --- |
| 本仓库 `Radar_V4.2_2026_0425_MOS/projects/source/` | `ota_layout.h` | 布局 / 标志记录 / 槽镜像头 |
| 同上 | `ota_frame.c/h` | 帧核心（与 F4A0 工程**逐字节一致**） |
| 同上 | `ota_flash.c/h` | EFM 擦写（RAM 驻留）+ CRC32 + 标志读写 + 槽校验 |
| 同上 | `ota_recv.c/h` | 接收端（写非活动槽 + ACK/RESUME） |
| F4A0 工程 `hc32f4a0_app/projects/app/{inc,src}/` | `ota_host.h/.c` | 上位机端发送器 |
| 本仓库 `ota/` | 同上一组 + `ota_frame.*` | 可移植副本（主机侧） |


## 15. 实现决定记录（2026-09-18 · 上板联调阶段）

> 本节记录在真机联调中**定下来、且不宜再反复**的决定，避免以后重复讨论。

### 15.1 Flash 擦写：统一走共享 DDL 的新版 EFM 驱动

**决定**：Boot 与 App 的片内 Flash 擦写**都经 `ota_flash.c` 封装，底层直接调用共享 DDL（Rev3.3.0）的 `hc32_ll_efm.c`**。
**不**恢复扫描板量产 bootloader（`boot_iap`）里那份 `flash.c`。

理由：
- `boot_iap` 的 `flash.c` 是按**它自带的旧 DDL** 写的（用 `EFM_Unlock()`/`EFM_Lock()` 命名），
  而共享 DDL 是 `EFM_REG_Unlock()`，且**每个擦写操作前都要 `EFM_FWMC_Cmd(ENABLE)`**
  （`hc32_ll_efm.c` 实测：`EFM_Program`/`EFM_SectorErase` 退出时会把 `PEMOD` 复位成只读）。
  → 那份 `flash.c` **无法原样编译**，恢复它等于既要改 API 又要拆契约。
- 全工程**一套 OTA 契约**（`ota_layout.h` 定义布局/标志/槽尾，`ota_flash.c` 实现读写）比两份独立维护更不容易出错 ——
  App 与 Boot 一旦对标志记录或槽尾结构的理解不一致，后果是升级静默失败。

**必须同时满足的硬约束（否则 CPU 会在擦写期间取指失败而卡死）**：
两个工程的 scatter **都要有 `RW_RAMCODE` 执行区，并把 `hc32_ll_efm.o` 与 `ota_flash.o` 放进去**。
- Boot：`projects/boot/MDK/config/linker/HC32F460xE.sct` → `RW_RAMCODE 0x20018000`
- App：`projects/MDK/config/linker/HC32F460xE_slot{A,B}.sct` → `RW_RAMCODE 0x20018000`

> 教训：只把标了 `__RAM_FUNC` 的函数放进 `.ANY (RAMCODE)` **不够** —— 收不到它们的**调用者**。
> App 曾因此缺 `RW_RAMCODE`，启动时擦标志扇区直接把 CPU 卡死（现象是「蜂鸣器长鸣 + 三灯常亮」）。
> 复核方法：在 `.map` 里确认 `ota_flag_write` / `EFM_Program` / `EFM_SectorErase` 的地址是 `0x20018xxx` 而**不是** `0x0000xxxx`。

### 15.2 Boot 的时钟配置：尝试过量产配置，但本板上电不运行 —— 已回退

**现状（现场实测可用）**：Boot **不配置时钟**，跑 ICG 决定的复位默认时钟（HRC，约 20MHz）。
App 自己 `BSP_CLK_Init()` 配到 PLL 200MHz；跳转前 `boot_jump()` 把时钟退回默认态再交接。

**试过什么**：按扫描板量产 `boot_iap` 的 `SystemClockConfig()`（XTAL 8MHz → MPLL 200MHz）逐行恢复，
唯一改动 `EFM_CacheRamReset` → `EFM_DataCacheResetCmd`。
→ **本板上电 Boot 完全不运行（绿灯都不亮）**，已回退（见 commit `f1d3d0c`）。

**已排查、未找到差异的三项**：

| 查什么 | 结果 |
| --- | --- |
| XTAL 引脚 | BSP 头文件：`GPIO_PORT_H` / `BSP_XTAL_IN_PIN=PIN_01` / `BSP_XTAL_OUT_PIN=PIN_00` —— 与 boot_iap 写法一致 |
| 配置顺序 | 与 App 的 `BSP_CLK_Init()`（**本板实测能跑**）逐行同序 |
| 参数 | `CLK_XTAL_MD_OSC/DRV_ULOW/ON/STB_2MS`、`PLLM=1/PLLN=50/PLLP=2/PLLQ=2/PLLR=2/PLLSRC=XTAL` —— 完全相同 |

**同一套配置 App 能跑、Boot 不能** —— 至今无法从代码解释。

**2026-09-19 补充：上面的三项比对【漏了一处真实差异】。** 重新逐行读量产 `boot_iap` 的 `SystemClockConfig()`
（`HC32F4A0_OTA/Scanner_20260901/boot_iap/source/main.c:43-96`）发现，它里面有 **3 处 `SWDT_FeedDog()`**：

| 位置 | boot_iap 原文 | 说明 |
| --- | --- | --- |
| 等 PLL 稳定【之前】 | `SWDT_FeedDog(); while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL)) {;}` | 进死循环前先喂一次 |
| 等 PLL 稳定【之后】 | `SWDT_FeedDog();` | 出来再喂一次 |
| `main()` 里配置完立刻 | `SWDT_FeedDog();` 注释「立即喂狗（对照 App/driver main）」 | 在 `LL_PERIPH_WP` 前后 |

原比对表只比了**引脚 / 顺序 / 参数**三类，**没比喂狗** —— 而 ICG（`ICG_REG_CFG0_CONST`）在复位后是**带 SWDT 预装**的。
**但注意：这一条暂不能定为根因** —— 按 ICG 取值反推（`SWDT_CNT_PERIOD65536` + `CLK/2048`），SWDT 周期是**秒级**，
比 XTAL 起振（`CLK_XTAL_STB_2MS`）+ PLL 锁定的耗时长了几个数量级，未必够得着。**要坐实只能上板测**：
按下面的第 1 步加时钟后，若现象从「不运行」变成「周期性重启」，就是它。

**另一处结构性风险（这一条比喂狗更值得警惕）**：`BSP_CLK_Init()` / `SystemClockConfig()` 等 PLL 稳定用的是
**没有超时的死循环**：

```c
while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL)) { ; }
```

PLL 一旦不锁，CPU 就**永远停在这里** —— 没有任何逃生路径，**也不会亮灯**（LED 初始化在其后），
现场表现就是「上电不运行、绿灯都不亮」，**并且连"是复位还是挂死"都分不出来**。
当前这版 Boot（跑 HRC）没有这个单点故障。**所以再试时必须先给这个等待加超时 + 失败点亮红灯**，
否则又会回到「只能靠猜」的状态。

**再试必须分步，每步上板验证**：

0. **（新增·前置条件）** 先给「等 PLL 稳定」加超时 + 失败走 LED 报错；并按 boot_iap 补上 3 处 `SWDT_FeedDog()`。
   这一步不改时钟，只保证**以后每一步失败都看得见**，否则又是黑盒。
1. 只加 `GPIO_AnalogCmd(PH0/PH1)` + `CLK_XtalInit`（**不开 PLL**）
2. 再加 `CLK_PLLInit` + 等 PLL 稳定
3. 再加 `SRAM/EFM/GPIO` 等待周期 + `PWC_HighSpeedToHighPerformance` + 切 PLL 源
4. 最后加 cache 复位/开启

### 15.2.1 （原 15.2 内容，已作废）

`main.c` 的 `SystemClockConfig()` 与 `boot_iap` **逐行一致**（XTAL 8MHz → MPLL 200MHz），
唯一改动是 `EFM_CacheRamReset` → `EFM_DataCacheResetCmd`（新版 DDL 命名）。
本板 8MHz 晶振（PH0/PH1）确认已焊接，App 一直靠它跑 PLL —— Boot 没有理由偏离量产路径。

### 15.3 诊断手段：只有 LED

本板**没有可接 printf 的调试口**（唯一串口是 RS485 业务口，日志发出去无人接收且占用业务线）。
故一切诊断走 LED，且编码必须**不用数数**：

| 现象 | 含义 |
| --- | --- |
| 跳转前**绿灯常亮 2 秒** | Boot 判定跳**槽 A** |
| 跳转前**蓝灯常亮 2 秒** | Boot 判定跳**槽 B** |
| **红灯** 1 秒亮 / 1 秒灭，一直闪 | 两槽都不可用，卡在 Boot |
| 三灯都不动 | Boot 没跑到 `main` |

灯节拍用 `DDL_DelayMS()`（按 `SystemCoreClock` 实测值计时），**不要用按频率估算的忙等循环** ——
曾因按 200MHz 拍而实际跑在 HRC 20MHz，闪得又快又糊、现场数不清。

### 15.4 OTA 总开关

`ota_flash.h` 的 `OTA_APP_ENABLE`（默认 **0 = 关闭**）。
关闭时 App 启动不写任何 Flash，Boot 恒走兜底路径（按 A→B 扫）→ **恒跳槽 A**。
用于把「Boot 能否跳到 App」与「OTA 整条链」这两件事分开验证。

### 15.5 已验证基线（2026-09-18 现场实测）

**可用的组合**：

| 项 | 状态 |
| --- | --- |
| Boot | 不配时钟（HRC）＋ 跳转路径完整 `SystemClock_DeInit` 对齐 ＋ `flash.c` 原语层；Code 5812 |
| App | 槽 A：`usart_uart_dma.hex` @0x8000；槽 B：`usart_uart_dma_b.hex` @0x28000；`RW_RAMCODE` 已在位 |
| OTA | `OTA_APP_ENABLE = 0`（关闭）→ Boot 恒走兜底路径按 A→B 扫 → **恒跳槽 A** |
| 现象 | 上电 **绿灯常亮 2 秒 → 跳槽 A → App 正常起来** ✓ |

**已确认修掉的真 bug**（仅 1 条有硬证据，其余为真 bug 但未逐一实测对应现象）：

| # | 问题 | 证据强度 |
| --- | --- | --- |
| 1 | **App 缺 `RW_RAMCODE`** —— 擦标志扇区时擦写代码在 Flash 里执行，CPU 取指失败卡死（现象：蜂鸣器长鸣 + 三灯常亮） | **硬证据**：map 地址 `0x0000f2f0` → `0x200189e4` |
| 2 | VTOR 用对齐掩码推导（槽 A=0x8000 / 槽 B=0x28000 都不是 128KB 倍数） | 真 bug，现象未逐一实测 |
| 3 | Boot 强制要求槽尾元数据，而它只在 OTA 下载时写入 → 烧录器烧的板子必卡 | 同上 |
| 4 | LED 极性搞反（红/蓝低有效，我却拉低当"灭"） | 同上 |
| 5 | LED 分组搞错（`LED_R/G/B`=PB5/PB8/PC15 与板载三灯 PA12/PA11/PB3 是两组） | 同上 |
| 6 | `main.c` 里一个静默 `for(;;)` 防呆块 | 已删 |
| 7 | `main.c` UTF-8/GBK 混编导致注释乱码 | 已统一 GBK |
| 8 | LED 节拍按 200MHz 拍（实际跑 HRC 20MHz）→ 改用 `DDL_DelayMS()` | 同上 |

**尚未做**：OTA 整链在真机上一次都没跑过（下载 / 激活 / 回退 / 槽 B）。

### 15.6 亮灯等待的喂狗策略：原先不对称，已统一（2026-09-19）

**问题**：Boot 的两个 LED 函数喂狗策略不一致 ——

| 函数 | 原实现 | 是否喂狗 |
| --- | --- | --- |
| `boot_led_error()` | `for(;;){ SWDT_FeedDog(); 亮/灭各 1s }` | 喂 |
| `boot_led_slot()` | `亮 → DDL_DelayMS(2000) → 灭` | **不喂** |

而 `boot_led_slot()` 正好占着**跳转前的 2 秒窗口**。ICG 里同时配了 `ICG_REG_WDT_CONFIG | ICG_REG_SWDT_CONFIG`（见 `hc32_ll_icg.h: ICG_REG_CFG0_CONST`），复位后可能已按 ICG 使能 —— 只要 SWDT 溢出周期短于 2s，就会**在亮灯中途被咬复位、循环重启**，现场表现与 §15.2 里那个「上电不运行」**几乎一样**，极难定位。

**为什么至今没暴露**：§15.5 的现场实测「绿灯亮 2 秒 → 跳槽 A」只能证明**当前这一颗 ICG 配置**下 SWDT 周期 > 2s，是「恰好没踩到」，不是「设计上安全」。

**改法**：新增 `delay_fed_ms(u32Ms)`，把任意长延时切成 `BOOT_DELAY_SLICE_MS = 100` 的分片，**每片喂一次狗**；`boot_led_slot()` 与 `boot_led_error()` **统一走它**（后者去掉裸 `DDL_DelayMS`），从根上消除不对称。约束变成一句话：**狗的溢出周期只要 > 100ms 就都安全**。

**验证**：

- **编译/map**：双 target 重编 UV4 exit 0 / 0 Error / 0 Warning，Code 5812 → 5892；RAMCODE 布局复核见 §15.5 那条规则（Debug 逐符号、Release 按执行域，两 target 均无擦写函数落在 0x0000xxxx）。
- **上板实测（2026-09-19，烧 `output\debug\iap_boot.hex`）**：行为与改动前**完全一致** —— 绿灯亮 2 秒 → 跳槽 A → App 正常起来，无复位重启、无异常灯态。**未引入回归**；同时把「SWDT 溢出周期 > 2s 才安全」这个隐式依赖，换成了「> 100ms 即可」的显式约束。
- **未验证**：Release 产物（只烧了 Debug）。

### 14.9 尚未接线

- `ota_recv` 目前是**独立模块**，尚未挂到业务收帧入口 `Check_Uart_Pdu()`；「进入升级」请求的承载帧仍待定（§12）；
- 因此当前**无调用点**，链接器会整体回收：两个 target 体积与改动前**一字不差**（Debug Code=33864 / Release Code=25364，均 0 Error / 0 Warning）；
- 即：本步只验证了**编译**，链接与运行需在接线后验证。

