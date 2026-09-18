# 修改记录 - hc32f4a0_app

> 最后更新: 2026-08-31
> 备份: `hc32f4a0_app_bak_20260628_190809/`（编译通过后的完整备份）

## 2026-08-31 F460 OTA 移植决策：以"今后好维护"为第一原则（与 F4A0 同构）

### 决策原则
F460 扫描盘 OTA 移植**不追求"F460 怎么省事怎么来"，而是与 F4A0 保持同构**——
同一套架构、同一套文件、同一套接口，F4A0 的修复/优化可直接同步到 F460（反之亦然），
两项目共一份 OTA 代码心智，长期维护成本最低。

### 关键决策（按同构原则定）
1. **boot 直接移植 F4A0 结构，不用 F460 原有 FTP boot**：
   - 原 boot（update_by_ftp.c + FWlib）升级链路已确认"原有不能用"，且与 F4A0 架构不同（FTP 主动拉取 vs App 收包+QSPI 暂存+boot commit）
   - 移植 F4A0 boot：boot_cfg / ota_boot / boot_qspi / boot_main 同构文件
2. **驱动风格与 F4A0 统一**：
   - F4A0 boot 用 LL 库（hc32_ll_*，华大官方轻量裸寄存器风格）
   - F460 应同样用 LL 库（HC32F460 官方有 LL 驱动）——避免 FWlib 双轨，两项目 boot 代码可逐文件对比
   - 待确认：华大 F460 LL 库获取
3. **F460 确定不做片内 AB 互换；QSPI 存双镜像，片内始终单份**（single-bak 落地形态）：
   - QSPI 16MB 存两份固件：**暂存区（新固件）+ 备份区（旧固件）**
   - 升级流程：App 收包→写 QSPI 暂存→置 NEED_COMMIT→复位→boot 校验暂存 CRC→
     把片内当前固件备份到 QSPI 备份区→暂存写片内（覆盖）→清标志→跳 App 自检→
     自检失败 boot_count 超限→从 QSPI 备份区恢复旧固件
   - QSPI 驱动已具备：App 有 w25qxx.c/.h；boot 有 hc32f46x_qspi（bak 目录）；driver_lib 有 hc32f460_qspi.h
   - 不引入新机制，仅把 F4A0 的 dual-bank swap 换成 single-bak 备份恢复（commit/回滚逻辑改，框架不变）
4. **App 侧 OTA 代码直接拷 F4A0**（ota_http/ota_storage/ota_frame/ota_security/ota_state/ota_flag/ota_agent）：
   - 平台差异仅收敛到：QSPI 驱动（w25qxx）+ Flash 布局常量 + 版本号
   - 换 DDL 时只改驱动适配层，OTA 业务逻辑零改动
5. **升级状态存储：用 QSPI 扇区，不引入 FlashDB**（F460 更轻，少一个依赖）：
   - F4A0 的 ota_state.c 依赖 FlashDB KV（进度/标志/boot_count）；F460 项目无 FlashDB
   - F460 改用 QSPI 固定扇区直存状态（magic+crc 校验，掉电安全），放"备用区"或独立扇区
   - QSPI 驱动用 w25qxx.c（App 已有）：Read/Write/Erase_Sector/Wait_Busy 齐全；
     芯片实际 W25Q128 16MB（ReadID 运行时探测，初始化自动识别）
   - boot 侧 QSPI 用 hc32f46x_qspi（FWlib，bak 目录已有）

### QSPI 16MB 分区（已定稿，连续无缝隙）
| 区域 | 起始地址 | 大小 | 用途 |
|---|---|---|---|
| 暂存区 | 0x000000 | 512KB | OTA 新固件暂存（F460 片内 512KB 对齐） |
| 备份区 | 0x080000 | 512KB | 升级前旧固件备份（回滚用） |
| FlashDB | 0x100000 | 8MB | 参数/配置 KV + TSDB |
| 网页/文件系统 | 0x900000 | 6MB | index.html 等网页文件 + 标签 CSV |
| 备用 | 0xF00000 | 1MB | 预留 |

> 依据：F460 片内 Flash 512KB → 暂存/备份各 512KB 与固件等大；FlashDB 8MB 富余；网页 6MB 足够放多套页面/CSV；1MB 备用。

### 维护收益
- 两项目 OTA 共一份代码心智：改 F4A0 的 bug/优化，diff 到 F460 即可
- boot 同为 LL 库同构文件：commit/回滚/自检逻辑可逐行对比
- DDL 升级只碰驱动适配层，不碰 OTA 框架
- 将来第三个华大项目（RK/其它）可复用同一套 OTA 接口（ota_interfaces 文档本按多平台设计）

### 待确认（按此原则推进）
1. 华大 F460 LL 库获取（与 F4A0 LL 同源风格）
2. ~~F460 QSPI 分区~~ → **已定稿**（见上表）；片内布局仍待核对（App 基址 0x16000、标志区位置）
3. F460 boot 的 QSPI 驱动（LL 或 w25qxx 抽象）

### 状态：决策已记录，移植未开始

## 2026-08-31 F460 扫描盘读写器 HTTP-only OTA 移植评估（W5100S 同款 + 外设接口几乎一致）

### 需求
- 另一项目（扫描盘读写器，芯片 HC32F460）增加 OTA，**只要 HTTP（设备做 HTTP 服务器收 POST）**；
- 网络条件与 F4A0 相同（W5100S），外设接口（SPI 引脚等）与 F4A0 几乎完全一致。

### 结论：最快路径，约 2-3 人·天，无需写新驱动

### 移植文件清单（全部从 F4A0 拷贝）

#### A. 平台无关 OTA 核心（直接拷，0 改动）
| 文件 | 说明 |
|---|---|
| ota_frame.c/.h | OTA1 帧格式 + CRC16 查表 |
| ota_security.c/.h | HMAC-SHA256 验签 + 版本检查 |
| ota_state.c/.h | 状态机 + FlashDB KV（F460 已移植 FlashDB） |
| ota_storage.c/.h | QSPI 暂存/校验/commit + 通道互斥 |
| ota_flag.c/.h | 升级标志 |
| ota_agent.c/.h | 下载→校验→置标志→复位 主流程 |
| ota_http.c/.h | 设备做 HTTP 服务器，收 POST 整包 |

#### B. 网络栈（W5100S 标准代码）
| 文件 | 改动 |
|---|---|
| wizchip/W5100S/w5100s.c/.h | 0（标准 WIZnet） |
| wizchip/wizchip_conf.h | 0（已 _WIZCHIP_=W5100S） |
| wizchip/socket.c/.h | 0（标准 WIZnet socket API） |
| wizchip/port.c/.h | **几乎 0**——外设接口一致，SPI 引脚/时钟宏大概率直接用（仅核对 F460 板子实际映射） |
| wizchip/dhcp.c/.h | 0（若用 DHCP；静态 IP 可不拷） |

#### C. 上位机（0 改动）
- tools/ota_http_send.py（POST 整包）；tools/ota_pack.py（同 OTA1 格式，密钥同源包通用）

### F460 侧需写的（极少）
1. **OTA 监听任务 ~30 行**（参考 ota_integration.c 裁剪，仅 HTTP 单通道）：
   - loop: apt_single_select_nob(SOCKET_x, OTA_PORT) → http_handle_conn(fd) → close → sleep_ms(1)
   - 无 USB 轮询、无 UART 拦截、无三通道合并，比 F4A0 dispatch 简单得多
2. **Flash 布局常量**：OTA_QSPI_STAGE_BASE / OTA_QSPI_STAGE_SIZE——**F460 片外 16MB QSPI 与 F4A0 同构**（F4A0 本身即 16MB 布局：kvdb1 0-2M + tsdb2 2-4M + tsdb1 4-16M + 暂存区 0xE00000 1MB），常量可直接复用同一布局；仅改 OTA_FW_VERSION（F460 版本号）
3. **启动链 2 行**：ota_init_http_only()（起监听 + ota_agent_boot 启动验暂存）

### 附加需求：简易网页配置页（纯 HTML 单文件）——确认可行且极简

**形态**：单文件 index.html（内嵌 CSS/JS，几 KB~十几 KB），放 QSPI FAT 卷，浏览器直接访问设备 IP 打开。

- 设备侧改动极小：
  1. HTTP 路由加 "/" → 从 QSPI FAT 流式发 index.html（1KB 缓冲 chunked，复用 http_serve_tagcsv 模式）
  2. 参数设置：页面 fetch POST /api 传 command_type=set_xxx → 复用现有 json_remote_cmd 命令集，0 新代码
  3. 标签上传：页面 fetch GET /tags?page=&size= → 新增流式路由（复制 tagcsv 改），或调现有 readtag API
- 无前端工程、无多文件、无 JS 框架依赖；RAM 占用 < 5KB（全程流式）
- 网页文件进 QSPI：推荐上位机 POST /web 推送一次（同推 TAG.CSV 流程）；或产线预置

**网页端直接 OTA（确认可行，闭环完成）**：
- 设备 ota_http.c 的 POST 处理原生兼容浏览器上传（解析 Content-Length -> 收整包写 QSPI -> 200 OK -> 验签 -> 复位）
- 网页实现：file input + fetch POST /ota（body=File）——浏览器自动加 Content-Length，与 PC ota_http_send.py 等效
- 响应处理：设备 200 后立即复位，页面提示"升级中，设备将重启"即可
- 浏览器 = 完整工具：参数设置 + 标签查看/上传 + OTA 一键升级，连上位机都可不装

### 收益（对比 WinUSB+HTTP）
| | WinUSB+HTTP | HTTP-only |
|---|---|---|
| USB 设备栈/MS OS/保活/共享缓冲 | 要移植 | 0 |
| 网络栈 | 要 | 拷（外设一致近乎零改） |
| 监听任务 | 3 通道 | 1 通道 30 行 |
| 工时 | 3-6 天 | **2-3 天** |

### RAM 差异（关键约束，两平台差 2.7 倍）
| 平台 | 片内 SRAM | 实测账目 |
|---|---|---|
| F4A0 | **512KB**（0x1FFE0000-0x20060000） | 静态 RW+ZI 127KB + 堆 384KB；堆内 tagbuf 64-128KB、m_postjson 32KB、线程栈 82KB |
| F460 | **192KB** | 待编译 map 确认静态区；若照搬 F4A0 布局（静态 127KB）→ 堆仅 ~65KB 装不下 → **必须按 F460 业务瘦身** |

**F460 内存策略**（网页配置 + HTTP OTA 均适用）：
- 全程流式：网页文件/标签上传走 1KB 缓冲 chunked（http_serve_tagcsv 模式），**禁止整文件进 RAM**
- HTTP POST 缓冲 32KB→**4-8KB**（参数单条 JSON 很小）
- 标签上传分页/流式（GET /tags?page=&size=），不做全量列表
- 先编译 F460 看 map 的 RW+ZI：≤60KB → 堆 ~130KB 方案随便做；>100KB → 先砍静态缓冲

### 待现场确认（风险点）
1. F460 的 W5100S SPI 引脚核对（虽外设接口一致，仍须确认板级映射）
2. F460 是否已有 RTOS（CMSIS-RTOS2/RTX5）——OTA 任务/互斥依赖；无 RTOS 则简化为 main 循环轮询
3. ~~F460 QSPI 型号/容量~~ → **已确认片外 16MB，与 F4A0 同构，暂存区常量可直接复用**
4. **F460 编译后静态区（RW+ZI）实测大小** → 决定堆余量（关键，优先做）

### 状态：评估完成，移植未开始（待 F460 工程就绪）

## 2026-08-31 OTA 侵入源工程分析与停机式升级（stop-the-world OTA）方案记录

### 背景问题：OTA 代码侵入源工程，哪些无法独立线程

已确认的 OTA 侵入点按侵入深度分三类：

#### 1. 侵入最深（驱动层，无法线程化）
| 位置 | 内容 | 为何不能独立线程 |
|---|---|---|
| usb_dev_cdc_class.c / usb_dev_winusb_class.c | SOF 1ms 中断保活（usb_dev_*_sof → keepalive_pump）+ tx_arm 协作发送 + s_*_ka_tick 状态 | 挂在 USB 硬件中断上下文，必须在 ISR 内贴近驱动完成；保活防主机 usbser/winusb 稀疏轮询（265/531ms），删了 OTA 反而变慢 |
| usb_utility.c | usb_compo_rx_buf 共享环形缓冲（2×16KB→1×16KB）+ usb_send/winusb_send 改走 tx_arm | USB 设备栈属驱动层，环形缓冲是中断与读线程共享数据，发送路径是类 API 一部分 |

#### 2. 侵入较深（业务层，已刻意合并为单线程）
| 位置 | 内容 | 现状 |
|---|---|---|
| ota_integration.c ota_dispatch_task | 原 3 个任务（ota_usb_task/ota_http_server/ota_uart_active_task）合并为 1 个 High 优先级线程，轮询 USB+HTTP+UART | 多线程抢 USB 读拆碎帧（CRC storm）、共享暂存区需互斥、HTTP select 阻塞饿死 USB——合并后更稳 |

#### 3. 侵入较浅（业务入口，胶水层）
| 位置 | 内容 | 能否消除 |
|---|---|---|
| Lan2Uart.c | L548 USB1 别碰 / L563 ota_usb_feed_pump / L571 ota_serial_pump / L618 firmware_upgrade | USB1/UART1 是业务与 OTA 共用物理口，业务读循环必须先判归属——通道复用决定，无法拆线程 |
| reader_msg.c | L1186 USB1 别碰 / L1192 ota_channel_pending | 同上，消息分发须避开 OTA 数据 |
| user_main.c | L404 ota_confirm_after_init / L787 ota_init_all | 启动串行依赖（FlashDB KV→验签→引导），非线程问题 |
| http_callback.c | "ota_update"/"ota_switch" 命令 | 客户 API 入口（HTTP POST 触发 OTA），接口须留，仅胶水 |

### 停机式升级方案（stop-the-world OTA）：OTA 开始冻结业务线程，完成即重启

**核心思路**：OTA 会话开始 → suspend 全部业务线程 → ota_dispatch_task 独占 → 成功 mark_ready+reset / 失败 resume 恢复。

**能消除的侵入**：
- Lan2Uart.c / reader_msg.c 的 5 处 OTA 判断（ota_usb_feed_pump / ota_serial_pump / ota_channel_pending / 两处 USB1 别碰）全删，业务代码净化
- USB1/UART1 无需让路逻辑（无竞争）
- 共享缓冲可缩回独立（业务冻结后 16KB 仅 OTA 用）
- CRC storm 根治（无多线程抢读）

**仍不能靠停线程解决的**：
1. SOF 保活——硬件中断上下文，且是驱动正常职责（端点保活），保留
2. http_callback 客户 API 入口——产品功能，只留入口即可（已是 ota_agent_run 胶水）
3. HTTP OTA 依赖 W5100S 网络栈——需确认栈是独立中断驱动还是依赖业务线程；若是后者，HTTP OTA 时不能全停（只冻串口/消息/标签线程，保留网络）

**实施要点**：
- 用 osThreadSuspend（可恢复）而非 osThreadTerminate（销毁）——OTA 失败可恢复业务免重启
- 冻结范围：send_func / sendthread / send_tags / msg_bus / FlashDB_Task / Task_Monitor / Tag_update_thread / HttpModuleAPI::AsyncRead / mqtt 线程
- 已有 ota_usb_stream_idle_timeout（8s）作超时保护：客户拔线/掉电后自动 resume
- OTA 已是 osPriorityHigh，冻结后独占 CPU 无需再调
- HTTP OTA 场景需评估网络栈依赖后决定冻结范围

**落地路径**：
1. ota_integration.c 加 ota_freeze_business() / ota_resume_business()（业务线程句柄表 suspend/resume）
2. ota_usb_stream 握手成功后 freeze；CRC 错/超时 resume；mark_ready 后直接 reset
3. 删 Lan2Uart.c / reader_msg.c 的 5 处 OTA 判断
4. 评估 HTTP OTA 网络栈依赖，决定冻结范围

**状态**：设计记录，未实现（待定）

## 2026-08-31 C# 死代码与交付风险点审计（ReaderUI_v1_MCU）

### C# 侧死代码（7 项，均零引用，v1 与 v2_linux 两套工程同步存在）

| 类型/文件 | 位置 | 说明 |
|---|---|---|
| UdpBroadcastSearch | UdpBroadcastSearch.cs:12 | 早期设备搜索遗留，**端口写成 11111（设备实际 15000）**；全工程无任何调用（无按钮/无 new）；ToolsDevSearchView（15000）才是真实搜索入口 |
| DigitsConvert | Models/BindConverts.cs:15 | LED 数码管 IValueConverter，从未在 XAML 注册 |
| EnumModel | Models/EnumModel.cs:9 | ProfileType 枚举 + 扩展方法，零引用 |
| app_initModel | Models/JsonModel.cs:442 | 嵌套模型类，未被任何 JSON 反序列化使用 |
| EpcList | ParamEasView.xaml.cs:101 | 告警 EPC 列表模型，零引用（兄弟类 AlarmData 被 ParamLogView 正常使用） |
| TaskExtensions | ToolsDevSearchView.xaml.cs:342 | WithCancellation 扩展方法，零引用 |
| ViewModelBase.cs | 根目录（孤儿文件） | 磁盘存在但 **csproj 未编译**；namespace MetroDemo.Core 系复制示例，引用 JetBrains.Annotations 但项目无此包，**不可加入 csproj（会编译失败）** |

### 交付风险点（排查结论）

1. **FTP 明文凭据写死**（FtpHelper.cs:16-17：RfidDemo/Silion123 + App.config ftpUrl=43.138.155.217:21）——程序发给客户 = FTP 账号密码泄露给客户，可登录服务器下载/篡改固件软件。**P0，交付版必须处理**（客户专用账号或去掉自动更新）。
2. **自动更新依赖内网 FTP**（ConnectView.xaml.cs:43-47 + TextBlock_MouseLeftButtonDown）——客户机连不上 FTP 时启动可能卡住/报错，无降级。
3. **FTP 版本检查无 try/catch 降级**（GetFtpFileInfos Timeout=1000ms）——FTP 不可达时异常冒泡。
4. **UDP 端口双轨**：实际设备发现链路 ToolsDevSearchView=15000 与设备固件 BRDCST_PORT=15000 一致（功能正常）；11111 仅在死代码 UdpBroadcastSearch 中，不影响功能但有误导性。
5. **位数兼容已排除**：ReaderManager.exe 与 ModuleAPI.dll 均为 AnyCPU 纯托管（32BITREQUIRED=0），64 位系统正常。
6. **WinUSB 免驱已排除**：固件 MSFT100+WINUSB CompatID 完整，Windows 8.1+ 自动绑定 winusb.sys，客户机零驱动干预；C# 按 VID 2E88/PID 4608 过滤枚举（与固件一致，GUID 不匹配不影响）。
7. **.NET Framework 4.8**：Win10 1903+/Win11 内置免装；Win7/8.1/旧 Win10 需安装（当前绿色免安装交付无检测引导，老系统客户直接双击会报错）。
8. **依赖库**：39 个第三方 DLL 全托管随目录拷贝，无 VC++ 运行库依赖。

### 建议动作（未执行，待定）

- 删除 7 项死代码（v1+v2 同步）
- FTP 凭据移出代码 / 交付版砍自动更新 / 加 try-catch 降级
- 可选：Inno Setup 安装包 + .NET 4.8 前置检测

## 2026-08-31 USB OTA 提速根因修复（R 516ms→16ms，全流程 20-33s → ~6s）

### 根本原因（USB OTA 慢的真凶——不是主机稀疏 IN）

设备端 OTA 分发线程（ota_dispatch_task）用**阻塞读**消费 USB 环形缓冲：
read(USB2/USB1, buf, 128)（timeout=-1/50ms），环形缓冲瞬间排空时 **sleep_ms(5) 一次、每 128B 一循环**
→ 排空速度仅 **~25KB/s** → 每批 8.2KB 需 350-500ms 才进入设备 → ACK 晚发 → 每批 R=516ms。
该阻塞读还反向拖住主机写入（W=15ms，环形缓冲满 → OUT NAK 重试）。

**回环快的原因**：回环数据连续、环形缓冲不空、5ms 睡眠不触发 → 误导排查方向。
历史文档 USB_OTA_4-5s_历史基准.md 的"usbser/winusb 稀疏 IN 265/531ms"结论为**误判**（该延迟实为设备端排空慢）。

### 设备端（固件）改动

| 文件 | 改动 |
|---|---|
| ota_integration.c | **根因修复**：缓冲 128B→512B（R=16ms 的真实来源——读得大、环形缓冲少空、5ms 睡眠少触发）+ USB1/USB2 真非阻塞（**O_NONBLOCK=1**，注意 0 是 O_BLOCK=阻塞！v9.82e 才修正——原 SET_ISBLOCK=0 实际是阻塞，WinUSB 模式 timeout=-1 永久卡死分发线程 → HTTP 永不轮询，此即"升级后 HTTP 连不上"根因）+ 内层循环排空 + 全空闲 sleep_ms(1) |
| ota_usb_stream.c | 通道锁泄漏修复（4 处擦/写失败路径补 ota_channel_release()）；完成前补发最终 ACK（免发送端 3s 超时探测）；热路径 TRACE 门控 SS_HOT_TRACE_EN；KV 进度同步 64KB→256KB（fdb 写 6 次→2 次）；会话开始全量预擦暂存区（64KB 块擦停顿移出流式，流式 R 恒定 16ms） |
| ota_frame.c | CRC16 逐位→查表（Python 验证结果逐字节一致，快 ~8 倍） |
| usb_dev_cdc_class.c/.h、usb_dev_winusb_class.c/.h | 保活 TX：SOF 1ms 点射 1B 0x00（防 IN 端点 NAK 降频轮询）；usb_cdc_tx_arm/usb_winusb_tx_arm 协作发送 + **直接装载兜底**（保活链死活都不影响 ACK 发出） |
| usb_utility.c | usb_send(uid=1)/winusb_send 改走 tx_arm（消除直接 usb_deveptx+XFRC 忙等竞争）；ws:/cs: TRACE 每 10 条一打 |

### 主机端（C#）改动

| 文件 | 改动 |
|---|---|
| WinUsbChannel.cs | **RAW_IO 常量 0x00→0x07**（0x00 是错的——历史 err=87 根因，用户发现）；PIPE_TRANSFER_TIMEOUT 显式=0（防 URB 取消/重提抖动）；后台常驻同步读（无限超时，总有 pending URB）；RX 队列 + 兼容 Read()；**先入队后 Status**（ACK 送达不被 UI 阻塞）；保活 1B 0x00 静默丢弃；错误退避（设备重启后 err=22 刷屏抑制）；日志 _verboseLog 门控（打开诊断/逐 ACK 收敛为 ~8 行）；CancelIoEx(NULL) 关闭 |
| OtaUpdater.cs | 批诊断每 10 批一打；WriteProbe 返回写结果 + 完成阶段设备断开立即判定（尾段 10s→1s） |
| OtaUpdateView.xaml.cs | 回环测试注释（省 3s） |
| 7 个文件 | 清理 17 处未用 catch 变量（CS0168） |

### 效果（真机实测）

- 每批 ACK 等待 R：516ms → **16ms**；写批 W：15ms → 0-16ms
- 流式 403KB：~25s → **~2.9s**（139KB/s，等效提升 ~9 倍）
- 全流程：20-33s → **~6s**（握手含预擦 ~1.5s + 流式 ~2.9s + 校验换区 ~1s + 完成判定 ~1s）
- 日志：主机端 ~8 行关键信息；设备端 ~6 行（ws/cs 节流、ss 热路径门控、关键流程保留）
- 三通道共用暂存区/通道互斥未动，串口/HTTP OTA 建议回归验证
- **CDC 与 WinUSB 双通道同时受益**（排空/保活/tx_arm/ota_usb_stream 全为共用代码；Python CDC 工具零改动即提速）

### 遗留/可选

- 64KB 块擦 ~200ms×7 为 flash 物理地板（已预擦到会话开始，流式内无停顿）
- 标签上传路径受益于排空修复 + 保活，建议真机验证上传延迟
- 固件版本仍 0x01150500（本次未升版本号）

## 2026-08-27 提交点状态（版本 0x01150500）

**HID 键盘模式完整打通 + OTA 恢复 + 统一上传框架**，真机验证通过：

**已解决（本次）**：
- **HID 键盘模式可用**（运行参数 upload.hw_inf==3）：标签 EPC 经键盘敲入电脑（记事本验证，12 字节 EPC 约 80ms 输出，扫码枪行为）；设备管理器识别为键盘（HID_DEVICE_SYSTEM_KEYBOARD，Usage 0x0001/0x0006）；PID 0x4606（4604/4605 曾被 Windows 缓存为失败枚举，逐次换新 PID 绕过）；
- 根因修复链（枚举失败→识别为USB输入设备→无输出→乱码→速度）：
  ① report 描述符路由：usb_standarditfreq 允许 0x21/0x22 在非 CONFIGURED 状态转发（枚举早期请求不再 STALL）；
  ② report 描述符换标准 6KRO 键盘描述符（63 字节，modifier usage 0xE0-0xE7；SDK 原版 0x00-0x00 非标致 Windows 无法绑定键盘类驱动），顺带规避 EP0 分包；
  ③ HID 描述符 bCountryCode 0x21→0x00、bcdHID 1.10→1.11（对齐 SDK 例程）；
  ④ send_key 改用静态报告缓冲 + 等 XFER_COMPL（g_kbd_tx_done，datain 回调置位）——非 DMA 模式下数据由中断从 xfer_buff 读 FIFO，栈变量/忙等覆盖导致内容错乱（全 E 乱码）根因；
  ⑤ hexkey 扫描码表修正（0=0x27...9=0x26，原表 0x1E 起错位致数字键 +1）；
  ⑥ 上传挂接点迁移到统一框架 send_evt_tagcoming（与其他接口一致：TagComing 事件→单标签实时上报），tagInsert_wp 不再直接调用；
  ⑦ bInterval 10ms→1ms + 忙等 XFER_COMPL（定期 osThreadYield 让出 CPU，不阻塞其他任务），上传 590ms→80ms；
- **USB OTA 线程恢复**（ota_usb.c 曾为隔离 HID 调试注释，已重新启用）；
- **调试代码全清**：usb_send/HID 相关所有 TRACE 计数/时间戳/诊断打印全部删除（8 文件扫描 100% 干净）；
- **midwares 原版 USB 库未动**（所有修改在 projects/usb_lib 编译副本）；
- **build_release.py 修复**：VHDR 读取编码 utf-8→gbk（头文件 GBK 编码，原脚本读版本号必失败）；仅此一行改动（git diff 确认）。

**测试要点**：
- HID 模式（hw_inf==3）：USB 仅初始化 HID 键盘，不启动 USB OTA 线程（无 CDC 接口，设计如此）；
- 其它上传模式（hw_inf≠3）/被动模式：默认 CDC+MSC，USB OTA 通道正常；
- OTA 4 通道（串口/USB-CDC/HTTP/U盘）需真机回归。

**产物**：`release/`（fw_0x01150500.otapkg + fw_0x01150600.otapkg 桥包 + FW.BIN + merged_f4a0_v981cm.bin + manifest.txt）。
**固件版本**：0x01150500（FW_VERSION_NUM 单一来源）。

## 2026-08-26 提交点状态（版本 0x01150400）

**已完成并真机验证**：
- 上位机 C# OTA **4 通道全部升级成功**（串口 UART1 / USB-CDC / HTTP 8081 / USB 虚拟 U 盘），
  协议层与 python 参考实现逐字节一致（CRC16/帧构建交叉验证）；修复致命 bug `Write(count=0)` 空写；
- 卡死保护：任何阶段 10s 无设备进度自动退出 + 停止按钮可用；HTTP 上传进度流（可定位流防预缓冲）；
- 版本查询：OTA1 协议新增 VER1 帧（RESUME 类型，旧固件安全），上位机"读版本"按钮 + 包版本自动解析；
- `tools/build_release.py` 一键发布：UV4 编译三工程(--build) + 生成 merged bin / otapkg / FW.BIN / manifest，
  版本单一来源 FW_VERSION_NUM 自动校验，自动清理旧版本产物（保留标准包+桥包）；
- 设备端：版本门槛 `<=`→`<`（同版本可重刷，仅拒降级）、串口 OTA 会话空闲 8s 释放通道锁、
  USB RX 缓冲 8192→9216（修复批 2 帧溢出致 CRC 噪音）、RecvCmd USB1/UART1 让路；
- 发布目录统一 `release/`（旧版本已删）。

**剩余问题（未解决，待继续排查）**：
1. **HID 键盘模式 USB 枚举失败**：设备描述符请求失败（未知设备）。
   已做：按运行参数 upload.hw_inf==3 初始化 HID 键盘（init_usb 分派、send_key/hid_kbd_type_epc 启用、
   新标签 EPC 键盘输出、HID 模式独立 PID 0x4604 + 产品名 "HID Keyboard"、CDC 写静默丢弃）；
   仍枚举失败——待排查键盘类回调/端点(EP3/EP4)/描述符细节；
2. HID 枚举修好后需整机验证标签键盘输入（记事本/Excel 显示 EPC）；
3. HTTP 上传进度条实际显示效果待真机复核（流式发送是否被设备 TCP 反压）；
4. 旧固件（`<=` 门槛）设备升新固件需先用桥包 `fw_0x01150500.otapkg`；
5. 新固件（含全部设备端修改）整机回归尚未完成。

**产物**：`release/`（fw_0x01150400.otapkg + fw_0x01150500.otapkg 桥包 + FW.BIN + merged_f4a0_v981cl.bin + manifest.txt）。
**固件版本**：0x01150400（FW_VERSION_NUM 单一来源，三处头文件同步）。

## 固件发布版本号同步约定（每次生成固件必须遵守）

**版本号单一来源**：`hc32f4a0_driver/projects/MDK/hc32f46_driver.h` 的 `FW_VERSION_NUM`
（`app_conf.h` 的 `OTA_FW_VERSION = FW_VERSION_NUM` 自动跟随）。

每次发布新固件，以下 **4 处版本号必须完全一致**：

| # | 位置 | 同步方式 |
|---|---|---|
| 1 | **代码** `FW_VERSION_NUM`（hc32f46_driver.h） | 改版本唯一入口，如 `0x01150202UL` → `0x01150300UL`，重编三工程 |
| 2 | **boot+app 合并烧录文件 .bin**（DAP-LINK/J-Link 下载烧录用） | 命名带版本，如 `merged_f4a0_v981cl.bin`（`merge_bin.py` 由最新 boot+app hex 生成，boot@0x0 + app@0x10000 连续）；**不用合并 .hex**——两段拼接烧录后地址位置不对，仅 .bin 可用 |
| 3 | **网络/串口/USB-CDC 下载包** .otapkg | `ota_pack.py --version <FW_VERSION_NUM>` 必须等于 1；包名如 `fw_0x01150300.otapkg` |
| 4 | **USB 虚拟 U 盘** `release/FW.BIN` | 同一 `ota_pack.py --version` 输出改名为 `FW.BIN`（8.3 大写），设备检测 `version <= OTA_FW_VERSION` 拒收 |

**发布流程（每次必做，防漏）**：
- 改版只改 `driver_lib/hc32f46_driver.h` 的 `FW_VERSION_NUM`（单一来源），三工程编译；
- 编译完成后必须运行 `python tools/build_release.py`——**一次生成全部 4 个发布产物**：
  `release/merged_f4a0_v981cl.bin`（DAP-LINK 烧录）+ `release/fw_0x<版本>.otapkg`（串口/USB-CDC/HTTP 三通道）
  + `release/FW.BIN`（USB 虚拟 U 盘，与 otapkg 字节一致）+ `release/manifest.txt`（版本/大小/SHA256 清单）；
- 脚本自动从宏读取版本并校验 otapkg/FW.BIN 包头版本 == 宏，不一致即报错退出；
- 脚本**自动清理 release/ 中旧版本产物**（仅保留当前版本 otapkg/FW.BIN/merged bin，manifest 记录清理清单）；
- **禁止手工零散打包**（曾多次漏生成 FW.BIN / merged bin）。

**防错检查**（发布前）：
- 启动日志 `FW version: 0x01150300`（app 打印 OTA_FW_VERSION）；
- 三通道升级包版本与代码一致——任一通道收到 `pkg ver != fw ver` 类拒绝说明某处版本没同步；
- `fw_0x01150300.otapkg` 与 `usb_upgrade/FW.BIN` 字节内容应相同（同一打包命令产物）。

## 2026-08-26：TAG.CSV 乱码修复 —— QSPI 4KB 擦除粒度 vs 512B 写粒度（v9.81cl）

### 问题（用户实测）
- USB 里 TAG.CSV 内容不完整：每 ~20 行有效 EPC 后跟一大段 0xFF 乱码（显示为 ），
  中间还有被截断的半行；不是缺结束符，是磁盘层数据损坏。

### 根因（磁盘层 bug，非 CSV 逻辑）
- QSPI NOR 擦除粒度为 4KB，FatFs/USB 写粒度为 512B；
- diskio.c disk_write 每次写前整块擦除所在 4KB 扇区再写 512B →
  同一 4KB 块内先写的 7 个扇区被后写扇区擦成 0xFF；
- 逐扇区写 20000 行（~500KB）时每 4KB 块只剩最后 1 个扇区（~20 行），
  且 FAT 表同样被破坏 → 文件簇链断裂、数据大面积 0xFF；
- usb_dev_msc_qspi.c msc_write（电脑经 USB 写盘）存在完全相同问题。

### 修复（3 文件）
1. ff16/diskio.c：disk_write 改为按 4KB 块读-改-写——块内部分写先读回整块、
   memcpy 合并、再擦写整块（static 4KB 缓冲，不占任务栈）；整块写仍直接擦+写；
2. usb_dev_msc_qspi.c：msc_write 同样改为 4KB 块读-改-写（static 缓冲）；
3. app/src/tag_csv.c：FAT 卷损坏自愈——f_mount / f_open 失败时自动
   f_mkfs 整卷重建一次再重试（static 4KB 工作缓冲；FF_USE_MKFS=1 已启用）；
4. app/src/tag_csv.c：USB 枚举激活期间（gIsUsbAvailable==1）暂缓生成——
   电脑挂载同一 8MB 分区时设备侧写入会与电脑缓存/写回冲突，保持 dirty
   待 USB 拔出后再生成（串口提示 tag_csv: USB active, defer generation）。

### 构建
- 驱动库 hc32f4a_driver.lib 重建（0E/0W，diskio.o / usb_dev_msc_qspi.o 重编）；
- app 固件 firmware.hex 重建（0E/1W，仅 wl_dedup.h 末行无换行旧提示）；
- ZI-data 125600 -> 137888（+12KB = 两个 4KB RMW 缓冲 + 4KB mkfs 缓冲）。

### 使用注意
- USB 线一直插着时 CSV **不会实时更新**（电脑文件系统缓存旧内容），也不会在
  USB 激活期间生成（固件自动暂缓）；正确流程：拔 USB → 推 ADD/DEL → 等串口
  `tag_csv: N tags` → 插 USB 读取；

## 2026-08-26：bootloader commit 防变砖加固（v9.81cl-boot）

### 问题（用户指出）
- commit 写完另一 bank 的 boot 副本/App 后**不做校验直接 swap**：若自复制/搬运瞬间
  出现静默损坏（位翻转/读碰撞/磨损），swap 后 CPU 从坏 boot 启动，回滚逻辑也在
  boot 里 → 直接变砖；
- 同理担心非法入侵/干扰损坏 boot。

### 攻击面分析（结论：产品接口碰不到 boot）
- USB 虚拟盘只映射 QSPI 8MB 分区（0x1800000），片内 boot（0x0）不在 USB 地址空间；
- 网络/串口/USB OTA 三通道全部强制 HMAC-SHA256 验签 + 包头 CRC32，恶意包过不了
  验签进不了 commit（v9.81 实测错误 key 验签失败）；
- boot 自复制的数据源是**正在运行的 boot 自身**，不是外部数据 → OTA 内容无法注入 boot；
- 剩余入侵路径仅剩：能运行任意代码的实体（恶意 App——被验签挡住；调试口 SWD）。

### 加固（hc32f4a0_boot/projects/boot/src/ota_boot.c，纯增量）
- `verify_staged_pkg` 输出包头期望 CRC32；
- 新增 `verify_other_bank()`：swap 前逐字节比对另一 bank boot 副本（64KB）+ App 区
  CRC32 复核 + 新 App 向量表有效性；任何不一致**放弃 swap**、清另一 bank 标志、
  保留旧固件运行（打印 `BOOT: verify other-bank ret=xx`）；
- boot 编译 0E/0W；App 无需重编（协议不变）。

### 产物
- `hc32f4a0_boot/projects/MDK/output/hc32f4a0_boot.hex`（56481B，2:11:48）；
- `tools/merged_f4a0_v981cl.hex`（boot@0x0 + app@0x10000 合并，一次烧录用）。

### 读卡初始化问题（同一批次暴露，已修复）
- 现象：全片擦除前读卡初始化失败 `ParamSet(MTR_PARAM_POTL_GEN2_Q) err:3`
  （MT_CMD_FAILED_ERR），主动/被动模式都失败；全片擦除后恢复；
- 根因：**非 boot 加固、非跳转地址**——已保存配置 `protocol.gen2.q=-1`（自动Q）
  被模块拒绝；`reader_init.c` 只要 `q != -2` 就发送 → -1 被发 → 卡初始化；
  恢复原因是配置被清空回默认（q=-2 不发送）；
- **配置存储位置：QSPI（v9.81s 起配置区路由到 FlashDB KV，片内不存配置）**；
  用户的全片擦除若含 QSPI，会连白名单 TSDB/TAG.CSV 一起清掉，需确认重推；
- 危险模式：配置非法值让 RFID 模块初始化失败，设备却正常启动（静默失效）；
- 修复（reader_init.c）：
  1) gen2 session/q/target 设置全部降级 best-effort——失败仅记录警告、
     模块用自身默认值继续工作，读卡初始化不再被配置值杀死；
  2) q=-1/-2 都视为不设置跳过；
  3) 模块掉电 100ms→1000ms（真正放电复位）；
  4) 失败时打印 GetLastDetailError 详细错误码。

### 新增：网络下载 TAG.CSV（GET /tagcsv，8080 端口）
- 背景：上位机 readtag 全量 2 万张 staticlist 720KB 超堆 + TSDB 未去重；
  TAG.CSV 生成时已用 wl_dedup 位图去重，下载端流式发送不占大内存；
- 实现（4 文件）：
  1. common.c firmware_upgrade_process：8080 循环放行 GET（原来只认 POST）；
  2. APIHttpRequest.cpp Method()：识别 GET → HttpMethod_GET；
  3. APIHttpResponse.cpp httpRespose：GET /tagcsv 路由 + http_serve_tagcsv()
     （挂载 FAT 卷 → 读 TAG.CSV → chunked 流式发送 → 卸载，静态缓冲不占任务栈）；
  4. tag_csv.c/h：FAT 卷互斥锁（生成 vs 下载，CSV 生成中下载返回 503）；
- 行为：200 + chunked 文本；USB 枚举激活 / CSV 生成中 → 503；文件不存在 → 404；
- 使用：`curl http://192.168.1.250:8080/tagcsv` 或浏览器/HttpClient 直接 GET。

### QSPI 性能优化（同批验证通过）
- 根因链：CSV 生成 47s（逐 512B 写→整 4KB 擦写，写放大 8x）；
  且 QSPI_FLASH_Write 整个写循环关中断（64KB 写 300-500ms）→ RFID 模块
  UART(921600) 丢数据 → TagInventory err:3；
- 优化（真机验证：CSV 6.5s + 无 err:3）：
  1. diskio.c 4KB 写回缓存：同块多次部分写合并一次擦写（擦除 1000→125 次）；
  2. qspi_flash.c 忙锁 + 关中断缩到指令发送临界区（tPP/擦除等待中断放开）；
  3. QSPI 时钟 DIV4(60MHz)→DIV3(80MHz)（app/driver/boot 三工程统一，规格内）；
  4. QSPI_FLASH_Read 写忙时返回错误让上层重试（防 XIP/DirectComm 冲突）。

### OTA 4 通道统一 + USB-CDC 提速（同批真机闭环）
- 统一（4 通道流程一致，代码实据）：
  1. 版本检查全部改为 `pkg ver <= fw ver 才拒`（原串口/USB/HTTP 三处残留 `!=` 要求包版本==当前固件，导致 0x01150300 被拒）；
  2. 主动模式串口 OTA：新增 ota_uart_active_task 监听 UART1（send_func 仅被动模式启动，主动模式原本无人处理 OTA 帧）；
  3. USB 盘通道 ota_flag_mark_ready → ota_mark_ready（补 KV 写入，size 统一 payload 长度）；
  4. 包头写暂存[0:82]、载荷 CRC32、HMAC-SHA256、mark_ready→reboot→boot commit→verify other-bank→swap→confirm 全链路一致；
- USB-CDC 提速（91.7s/4KB/s → 6.1s/64KB/s，进度单调零回退）：
  1. RecvCmd（reader_msg.c）与 send_func 对 USB1 OTA 会话让路——原竞争抢读 OTA 帧 → CRC MISMATCH 重传风暴 + 0xEE 误报刷屏；
  2. USB RX 缓冲 8KB（内存优化）+ 设备 ACK 每 2 帧 + 脚本批 2 帧（8KB 恰好 2 帧不溢出，替代 32KB 静态方案）；
- 真机：USB-CDC OTA 端到端成功（传输→验签→mark_ready→重启→commit ret=0→verify other-bank ret=0→swap ret=0→新固件启动）。

### 备注（可选深化，需 RM 确认后做）
- boot 区 WPRT+WLOCK 硬锁（防已运行的恶意代码改保护位）：默认扇区已写保护，
  恶意代码仍可自行解锁；WLOCK 锁死 WPRT 寄存器可封死该路径，但 commit 自复制
  另一 bank boot 与 J-Link 后续烧 boot 需先解锁，属出厂级决策。
- Windows 挂起总线时设备也可能生成（缓存仍显示旧内容），弹出/写回有覆盖风险，
  强烈建议按上述串行流程操作；
- 旧固件已把卷内 FAT 写坏：新固件重推 ADD 会自动重建 TAG.CSV；
  若挂载/打开失败会先自动格式化。也可在 Windows 下对该盘右键格式化一次（现在安全）。

## 2026-08-25：m_postjson 上限 128KB + 20000 张推送验证（v9.81cj-i-b）

### 变更
- m_postjson 动态上限 512KB -> 128KB：配置请求（<128KB）完整解析；
  大推送（>128KB）EPC 走流式（g_stream_tags_added 分支），缓冲截断无害；
  峰值内存 128K(m_postjson)+38K(ADDlist分批)+64K(tagbuf) ≈ 230KB < 堆 287KB。

### 验证（用户实测，通过）
- **ADD 20000 张：40×500 = 20000 全收**（每批 ~0.19s，~8s 完成）；
- **DEL 20000 张：40×500 = 20000 全删**（每批 ~1.55s）；
- 之前 20278 推送 insert 少 278 是白名单残留重复被 LTFind 去重（正确行为）；
- 结论：白名单 add/del 20000 张完整可靠，内存可控。

## 2026-08-25：m_postjson 动态化 + ADDlist 分批落盘（v9.81cj-i）

### 问题（用户实测）
- m_postjson 静态 32KB 改动态后，4001 张白名单 add 推送丢标签（3406/3882/3885 不稳定）；
- 根因：ADDlist 不分批攒 4001 张（144KB）+ m_postjson 130KB 同时驻留超堆上限（287KB）
  → LTPushFront malloc 失败丢 EPC；del 本就每 500 张分批所以正常。

### 修复（3 文件）
1. **APIHttpRequest.h/cpp**：m_postjson 静态数组 32KB → 动态指针（Content-Length 按需申请，
   上限 512KB，支持 >128KB 大推送；请求间释放；NULL 保护）；
2. **APIHttpRequest.cpp**：OnBodyCallback 流式优先——m_postjson 未分配/超限时不中断
   （EPC 靠流式扫描器，独立于 body 缓冲）；
3. **APIHttpRequest.cpp on_stream_epc**：add 与 del 都每 500 张分批 flush
   （Transfer_EPC_from_list_to_TSDB + Init_list_clear），峰值内存 ~38KB；
4. **HttpModuleAPI.cpp**：httpAPIDispatch 加 json==NULL 保护。

### 验证（用户实测，通过）
- add 4001 张：insert 500x8 + 1 = **4001 全收**（每批 ~0.2s）；
- del 4001 张：delete 500x8 + 1 = **4001 全删**（每批 ~1.2s，del 含哈希构建）；
- HTTP 返回 success；m_postjson 平时不占 32KB。

## 2026-08-25：静态内存优化 + OTA 缓冲握手动态化（v9.81cj-f）

### 背景
- 用户排查读标签慢过程中发现多处大静态缓冲平时不用却常驻 RAM。

### 优化（省 ~24KB 静态 RAM）
1. **cdc_rx_buf 32KB->16KB**（io_stream.h）：QSPI 擦写阻塞仅在 OTA/存配置时发生，平时 CDC 数据量小；16KB 仍装 4KB OTA 帧x4；
2. **USB OTA s_buf 4KB->握手后 malloc**（ota_usb_stream.c）：探测期用 4B s_syncbuf 认 "OTA1" magic，握手成功 malloc(4KB)，会话结束 free；平时只占 4B；
3. **UART OTA otabuf 4KB->握手后 malloc**（Lan2Uart.c）：主循环探测到 "OTA" magic 才申请 4KB，升级完成 reboot 自然释放；
4. **UART send_frame 4KB static->栈上 15B**（ota_transport_uart.c）：只发 ACK/RESUME，无需 4KB 缓冲；ota_transport_uart_feed 确认死代码（链接器剔除，含 s_feed+frame 8KB）。

### 保留不动
- MSC_BOT_Data 12KB：SCSI READ10/WRITE10 单次搬移上限，上位机可能发 >8KB 请求，降小有溢出风险；USB 枚举才激活，可接受。

### 结果
- RW+ZI 从 ~128KB -> 104KB（省 ~24KB）；
- 编译通过（driver 1443672B / app hex 1106732B，0E/0W）。

## 2026-08-25：移除 FAL 日志 ANSI 颜色码 + 版本 0x01150202（v9.81cj-d）

### 问题（用户实测）
- 串口输出出现 `[32;22m[I/FAL]` / `[0m` 等 ANSI 颜色转义乱码（FAL log_i/log_e 宏硬编码
  \033 颜色码），串口助手不解析 ANSI → 显示不兼容。

### 修复（2 文件）
1. `hc32f4a0_app/projects/flashDB/include/fal_def.h`：log_i/log_e 去掉 \033[32;22m /
   \033[31;22m / \033[0m，纯文本 `[I/FAL]` / `[E/FAL]`；
2. `port/fal/inc/fal_def.h` 同步（工程实际 include 前者，双份保持一致）；
3. 版本升 0x01150202（FW_VERSION_NUM，单一来源）重新编译打包。

### 产物
- `usb_upgrade/fw_0x01150202.otapkg` + `FW.BIN`（payload 393092B / pkg 393174B，
  crc 0x443F455E / sha 487a309a2d3dac268b3855244ae14e4df9ec8637b529ae1e4ac8f6a3a1fc6c95）；
- `tools/server_data/firmware/fw_acc.otapkg` + manifest latest 0x01150202。

### 真机验证（用户实测，通过）
- USB 盘符升级 0x01150202 成功：NEED_COMMIT→verify→commit→swap→新固件（v18153986=0x01150202 pending confirm）；
- 启动日志：`[fw] v0x01150202 build`、`FW version: 0x01150202`、FAL 分区表纯文本无 ANSI 乱码；
- 同版本 FW.BIN 重复插入 → `usb_msc_ota: version not newer, skip`（防重复升级保护，正常）；
- 上位机删除 FW.BIN 后重启 → `usb_msc_ota: open FW.BIN fail 4`（FR_NO_FILE，正常，不再触发升级）；
- 网络/RFID/USB 全部正常（InitReader Ok / OpenReader ok / httpapi ret:0 / CDC+MSC 枚举成功）。

## 2026-08-25：USB 盘符升级验证 build 标签统一（v9.81cj-c：0x01150201）

### 完成
- driver + app 重编通过（driver lib 1443032B / firmware.hex 1105958B，0E/0W）；
- 新固件确认：`[fw] v0x%08X build`（旧 v9.81ai 消失）、OTA_FW_VERSION=0x01150201；
- 打包 `usb_upgrade/fw_0x01150201.otapkg` + `FW.BIN`（payload 393180B / pkg 393262B，
  crc 0x01A7562E / sha 9a2c55ec96c1af5e193916659ef2cfe37f220fcaaacf7f129109cbd127910c30）；
- manifest.json 更新为 latest 0x01150201 + 实际 sha；
- 附加排查结论：`TestFFW version:` 前缀非固件字符串——固件只打印 `FW version: 0x%08X`；
  为 printf(TRACE) 多线程并发输出与 `TestFwType_ex wmc:%d` 拼线所致（stdio 无锁，
  uart_send 有临界区但 printf 路径不经过）；暂不修复，不影响功能。

## 2026-08-25：版本号统一为单一来源 FW_VERSION_NUM（v9.81cj-b）

### 问题（用户实测）
- 升级后 TestFFW version 显示 0x01150200 正确，但设备串口启动日志仍是
  `[fw] v9.81ai build`（driver main.c 硬编码的旧 build 标签）→ 两处版本号不一致，看着乱。

### 修复（统一版本来源，4 文件）
1. `hc32f4a0_driver/projects/MDK/hc32f46_driver.h`：新增 `FW_VERSION_NUM 0x01150200UL`
   ——单一版本来源；`driver_lib/hc32f46_driver.h` 同步；
2. `app_conf.h`：`OTA_FW_VERSION` 改为引用 `FW_VERSION_NUM`（不再独立定义）；
3. `driver main.c`：build 标签 `v9.81ai` → 打印 `v0x%08X`（FW_VERSION_NUM）；
4. 语义：以后改版本只改 hc32f46_driver.h 一处，OTA 包/启动打印/build 标签自动一致；
   打包时 `ota_pack.py --version` 仍须手动同步。

### 注意
- driver 工程为库工程（main.c 编译进 hc32f4a_driver.lib，After Build 自动 xcopy 到 driver_lib/）：
  **改 driver 源码后必须先重编 driver 工程，再编 app**。

## 2026-08-25：固件版本号升级 0x01150000 → 0x01150200（v9.81cj）

### 问题（用户实测）
- USB 盘符升级后 TestFFW 仍显示旧版本 0x01150000；
- 根因：打包时 `--version 0x01150100`，但固件内部 `OTA_FW_VERSION`（app_conf.h）未同步更新
  → 升级后新固件内部版本仍是 0x01150000；
- 版本语义：USB 盘符通道要求 FW.BIN 包版本 **高于** 当前固件版本（usb_msc_ota.c），
  流式通道（串口/USB/HTTP）要求包版本 **==** 固件内部版本（ota_*_stream.c）。

### 修复
1. app_conf.h：`OTA_FW_VERSION 0x01150000 → 0x01150200`（与打包 --version 一致）；
2. 重新打包 `usb_upgrade/fw_0x01150200.otapkg`（payload 393176B / pkg 393258B，
   crc 0x2F30A626 / sha 7703a460e65324f1541a17a551ff4ca6d3e80d2d6be9157b41ac6ba45748ab84）；
3. `FW.BIN`（U盘升级用，设备端硬编码文件名）同步为新版本；
4. `tools/server_data/manifest.json` 更新为单 latest 0x01150200 + 实际 sha。

### 产物
- `usb_upgrade/fw_0x01150200.otapkg`、`usb_upgrade/FW.BIN`、`usb_upgrade/fw.otapkg`
- `tools/server_data/firmware/fw_acc.otapkg`（HTTP 通道）

## 2026-08-24：del 大列表分批落盘（v9.81ce：4001 删除完整）

### 问题（用户实测）
- add 4001 + readtag 4001 正常，但 del 4001 后剩 1802（只删了 ~2199）；
- 根因：del 路径内存峰值超堆——DELlist(4001×36B=144KB) + 哈希构建(4001×40B=160KB)
  ≈ 304KB >> 堆余量（readtag 后 ~79KB）→ DELlist 部分添加 + 哈希部分构建 → 删除不彻底；
- staticlist 清理时机也修正（原在 message-complete 后，DELlist 添加（OnBody）已先行）。

### 修复（3 文件）
1. APIHttpRequest.cpp：
   - on_stream_epc：del 每 500 个分批落盘（Transfer_EPC_from_list_to_TSDB + Init_list_clear）
     → DELlist 峰值 18KB + 哈希 20KB ≈ 38KB，任意堆配置都稳；
   - Parse 开头 LTClear(staticlist)（早于 OnBody——readtag 残留清干净，on_stream_epc 有堆可用）；
2. HttpModuleAPI.cpp：httpSaveTagMethod 入口 LTClear(staticlist)（双保险）；
3. List.h：加 extern "C" 保护（List.c C 编译 vs C++ 调用的链接规范问题）。

### 验证（真机）
- cleartag → add 4001 → readtag 4001 → **del 4001 → 删到 0** ✅

## 2026-08-24：白名单读取修复补充（v9.81ce：临时链表释放）

### 问题（用户实测）
- 删除 2196 张时 LTInsert malloc FAIL 刷屏——readtag 后 staticlist（2196 节点 ~79KB）未释放，
  残留占用堆 → DELlist 添加时堆不够；
- 用户要求：临时/中间/链表用完都要释放。

### 修复（HttpModuleAPI.cpp）
- readtag 分块结束：LTClear(staticlist)（释放 ~79KB）；
- writetag 分支末尾：LTClear(taglist)（Check_Buffer_Diff 只落盘 ADDlist/DELlist，taglist 否则泄漏）；
- ADDlist/DELlist 落盘后 Init_list_clear（原有）；g_Uploadtag 由上传线程消费（保留）。

### 待真机
- del 2196 后 malloc FAIL 消失；连续 推送→读取→删除→清空→重推 循环堆稳定。

## 2026-08-24：白名单读取修复（v9.81ce：readtag 分块 + 清空接口 + 乱码修复）

### 现象（用户实测）
- 上位机读白名单（eascfg + method:"readtag"）失败/乱码：cJSON 128KB 溢出 4KB 缓冲、
  W5100S send -7 断连、EPC 中混入乱码（越界读）、旧版本 TSDB blob 错位；
- EAS 设置（get=1）totaltags 恒 0（未重建统计）。

### 修复（4 文件）
1. HttpModuleAPI.cpp：
   - eascfg get：fdb_tsl_query_count 快速计数（原 tsdb_tarversal_total O(n²) 太慢）；
   - readtag：重写为分块（m_IsWhitelist 模式）——"method":"readtag","tagcount":N,"epc":[
     + 分块 EPC 字符串 + ]}；修复 cJSON 128KB -> 4KB m_AddOtherJsonBuf 溢出；
   - 分块 1KB（< W5100S 2KB TX 缓冲，3KB 导致 send -7 + socket 关闭）；
   - stringlen 用实际 strlen（原 len+4/EPC 多算 -> SendChunkedSection 越界读乱码）；
   - 新增 cleartag（fdb_tsl_clean 清空 TSDB，消除旧版本 blob）；
   - 分块 Hex2Str 前 Epclen<=16 截断；
2. HttpModuleAPI.h：+m_IsWhitelist/m_WlIdx/m_WlTotal；
3. tsdb_sample.c：createlist_query_cb 先查 status（跳过 DELETED 不读）+ Epclen 0~16 校验；
4. APIHttpResponse.cpp：send_v 重试 0xffff->0x200（连接断开后 65s 刷屏 -> 0.5s）。

### 验证（真机）
- cleartag 清空 TSDB（FlashDB 全分区格式化）后重推 + readtag：**上位机读取成功**（无乱码）；
- EAS 设置 totaltags 正确。

## 2026-08-24：白名单全量删除提速（v9.81ce：13s → ~4s）

### 现象（用户实测）
- 删除 4001 张白名单耗时 13.062s；添加 4001 张仅 1.275s（64KB tagbuf + epc[16] 生效 ✓）；
- 根因：Del_Tag_TSDB 逐条 set_del_status_cb：4001 条 × LTFind(DELlist 4001) 线性查找 ≈ 1600 万次 memcmp；
- fdb_tsl_clean 会擦 10MB 整分区（可能更慢），不可用。

### 修复（tsdb_sample.c，哈希精确版——用户指出"删除与保存 EPC 必须内容匹配，不能按数量全删"）
- DELlist 构建 512 桶哈希（del_hash_build/del_hash_find/del_hash_free，malloc 临时节点）；
- set_del_status_cb 用 del_hash_find（Epclen+memcmp 全等精确匹配）替代 LTFind 线性查找；
- 任意 DELlist（全量/部分）都精确匹配，无误删。

### 验证（真机）
- 删除 4001 张：**13.062s → 0.774s**（17 倍提速，多次 0.77~0.86s 稳定）；
- 添加 4001 张：1.27s；重启读回正常。

## 2026-08-24：白名单容量扩容（v9.81cd：LTNode epc[32]→epc[16]）

### 背景（用户实测）
- 128KB tagbuf 配置下白名单节点 ~2600 个上限，多次推送累积超限 → malloc FAIL（部分标签丢失）；
- EPCIDMAXLEN 本就 16，List.h 的 epc[32] 冗余（白名单 EPC 12/16 字节，从未用满 32）。

### 修复（2 文件，app 工程）
- List.h：LTDataType.epc 32→16 → LTNode 52B→36B → 白名单容量 ~2600→~3700；
- APIHttpRequest.cpp：on_stream_epc epc_len>32→>16（防 memcpy 越界写坏内存）；
- 全工程核查：ipc.h EPCIDMAXLEN=16、HttpModuleAPI 校验 >EPCIDMAXLEN、ipc.c EPCID[16] 均已一致 ✓。

### 待真机
- 多次累积推送至 ~3700 张；盘存正常。

## 2026-08-24：白名单大推送修复（v9.81cc：scanner 美化JSON + 堆优化）

### 现象（用户实测）
- 700 张白名单（美化 JSON 26.5KB）→ 20003 "the parameter..."（流式零添加 + 全量 cJSON OOM）；
- 修复 scanner 后流式正常，但第 ~400 个 EPC 卡死/复位（LTInsert NULL 解引用崩溃）；
- 根因链：①scanner unknown key off-by-one（美化 JSON 丢 "epc" key）；②method 值解析错（is_add 误判）；
  ③LTInsert 不检查 malloc 失败（堆耗尽时 NULL 解引用崩溃）；④tagbuf 贪婪分配 232KB 吃光堆。

### 修复（6 文件）
1. stream_epc_scanner.c：unknown key 跳转 i=j-1（off-by-one，美化 JSON 丢 "epc" 根因）；
   ST_METHOD_PRE 跳过 method 结束引号 + 新增 ST_METHOD_COLON 状态（is_add 正确，避免 EPC 误加 DELlist）；
2. List.c：LTInsert 增加 newnode==NULL 保护（防崩溃，正式修复）；
3. APIHttpRequest.h：HTTP_POSTJSONLEN 128KB→32KB（大推送由流式豁免兜底，省 96KB 堆）；
4. app_conf.h：新增 MaxTagBufSize 宏（128KB）；
5. Lan2Uart.c：tagbufsize 上限 MaxTagBufSize——原逻辑 left-30KB 贪婪吃光堆(232KB)→left 恒 ~24KB；
6. ipc.c：调试锁超时已恢复 osWaitForever。

### 512KB 内存账目（实测）
- 静态 RW+ZI 127KB（map 0x1ffe0000 起）+ 堆 384KB（0x20000040→0x20060000）；
- 堆内：tagbuf 232KB→128KB、线程栈+小块 82KB、HttpModuleAPI+APIHttpRequest 46KB（含 m_postjson 32KB）、
  剩余 left 24KB→136KB（白名单可用 ~2600 个节点，节点 52B/个）。

### 验证（真机）
- 715 张白名单：流式全添加（on_stream_epc 1..700+）、无 malloc FAIL、FlashDB 落盘 715 tags 0.305s、
  heap_left 200KB（tagbuf 64KB 时）；128KB tagbuf 下白名单 ~2600 个。

### 待真机
- 128KB tagbuf 配置下 700/2000 张再测；重启后白名单持久化验证；被动模式盘存 1500 条缓冲验证。

## 2026-08-24：json_getstring_len cmpmode==2 语义修复（v9.81cb，静态参数不生效根因）

### 现象（用户实测 + rawjson 确认）
- 上位机（C# 网络）设置静态参数不生效；设备 TRACE："ip" dose not find → ethernet is invalid → valid_settings != 0；
- rawjson 打印确认上位机格式完全正确：{"ethernet":{"ip":"192.168.1.250","nm":"255.255.255.0","gw":"192.168.1.1","mac":"fcff321de144","lport":8080},...}；
- 运行参数设置生效（数值字段 PARSE_CHK_RANGE 不受影响）。

### 根因（json_api 桥接兼容性 bug，v9.81by 引入）
- 旧 json-parser json_getstring_len：cmpmode 0=相等、1=上限(>len)、-1=下限(<len)、**2或其它=不检查长度**；
- json_api 错实现 cmpmode==2 为"长度 >= len"→ PARSE_CHK_STR(obj,"ip",strbuf,29,2,...) 要求长度≥29，
  但 "192.168.1.250"(14B) < 29 → 返回 -2 → 判字段缺失 → ethernet 校验失败 → 设置不保存；
- 影响：readercfg.c 所有 PARSE_CHK_STR 字段（ethernet ip/nm/gw/dns/mac、wlan ssid/pwd、蓝牙/4G name 等）。

### 修复（json_api.c，仅重编 app 工程）
- json_getstring_len：cmpmode==2 或其它 → 不检查长度（与旧 json-parser 逐行一致）；
- mac 小写确认：strTohex 大小写都接受（'a'-'f' 分支）✓ 上位机 ToLower() 的 mac 无问题；
- 上位机格式验证：C# JsonConvert.SerializeObject(model).ToLower() → 字段名小写与固件期望一致 ✓。

### 需重编
- 仅 app 工程（json_api.c）。driver 的 rawjson 调试打印（custom_ee_commond.c TRACE）可选保留。

### 待真机
- 重编 app 烧录后：上位机设置静态参数（IP/掩码/网关/射频/串口等）应生效。

## 2026-08-24：HTTP 大推送 4000 EPC 返回 400 修复（v9.81ca）

### 现象
- POST /（白名单 method:"add"）推送 4000 EPC（body ≈112KB）→ 返回 400；
- 根因：HTTP_POSTJSONLEN = 1024*100 = 100KB；OnBodyCallback 累计 m_contentlen
  超限 → m_respcode=400（body 缓冲上限，与流式处理无关——流式喂完才检查）。

### 修复（2 处，app 工程）
- APIHttpRequest.h：HTTP_POSTJSONLEN 100KB→128KB（4000 EPC≈112KB 全量装下；
  m_postjson 为全局单例常驻内存，仅 +28KB）；
- APIHttpRequest.cpp OnBodyCallback：超限时若流式 scanner 已实时处理 EPC →
  跳过全量缓冲、不再 400（>128KB 超大推送走流式豁免；MessageComplete 后
  httpSaveTagMethod 走 g_stream_tags_added>0 流式 success，不 cJSON_Parse 大 body，
  避免大 JSON 解析 OOM）；
- 流式链路确认无阻塞：on_stream_epc→tagtable_list_update（链表内存操作），
  FlashDB 落盘为后台任务（alarm.c/QSPI_Sync.c 消费 FlashDB_Sync_flag）。

### 待真机验证
- 4000 EPC 白名单推送 → 200 success；重启后白名单仍在（落盘）；
- >128KB 超大推送（如 1 万+ EPC）→ 流式豁免路径同样 success。

## 2026-08-24：json_parse 内存泄漏修复（v9.81bz，21 处全堵）

### 背景（用户排查：以前 Json 申请的空间有释放没）
- 旧 json-parser（v9.81q 之前）：json_parse 每次先 `mp_resetpool()` 整池重置，树从固定
  内存池取 → 无泄漏概念（池循环利用），代价：池固定大小 + 旧树在下次 parse 时失效；
- v9.81q 改标准 malloc/calloc 后：21 处 json_parse 调用点**从不 json_value_free** →
  每次 HTTP 请求泄漏整棵解析树 → 长期运行堆耗尽（**疑似 8080 卡死/不可达根因之一**）；
- HTTP 连接 close **不回收堆**（裸机 RTOS 无进程隔离），必须显式释放。

### 修复（4 文件）
- 方案：**json_parse_auto 自动回收**（旧 mp_resetpool 思想的 malloc 版）——释放上次残留
  树 → 解析新树；树存活至下次 json_parse_auto。调用点零出口改动，CHK_*_RET 提前 return
  无需处理（树留待下次回收，安全前提：每函数单次 parse、树不跨请求存储，已逐处核实）。
- HttpModuleAPI.cpp（11 处）：static g_jvalueFree + json_parse_auto，11 处
  json_parse→json_parse_auto（11 个不同函数各 1 次 parse）；
- readercfg.c（8 处，driver 工程）：同上方案（8 函数各 1 次 parse）；
- http_callback.c（1 处）：json_remote_cmd 所有出口汇聚 SEND_RESP → 标签后统一
  json_value_free(jvalue)；
- custom_ee_commond.c（1 处，driver 工程）：MidMsgType_Update_Fw_By_Ftp case 内
  json_value_free(pobj)。

### 需重编
- driver 工程（readercfg.c + custom_ee_commond.c）+ app 工程（HttpModuleAPI.cpp +
  http_callback.c），重新生成 hc32f4a_driver.lib。

### 验证
- git diff 逐行确认：4 文件改动纯净（readercfg.c 用 git 原版字节 Latin-1 无损重建，
  避开 GBK/UTF-8 混合编码损坏陷阱）；
- 待真机：连续多次 HTTP 请求后堆稳定（原泄漏时反复请求会耗尽堆）。

## 2026-08-24：JSON 库统一到 cJSON（v9.81by：json_api 桥接层）

### 背景
- 项目原本双 JSON 库：json-parser（解析，66 处调用：readercfg 54 + HttpModuleAPI 11 + http_callback 1）
  + cJSON（生成，HttpModuleAPI 140 处）；
- json-parser 无生成 API，cJSON 全能 → 统一到 cJSON，但 66 处调用点直接访问 json_value 结构
  （u.array.length/values[i]、点路径 "a.b.c"、数组索引 "[0]"）→ 需兼容层。

### 实现（json_api.c/h，app/jsonlib/）
- json_api.h：原样保留 json-parser 的类型定义（json_value/json_type/json_settings 等）+ 9 个 API 声明；
- json_api.c：用 cJSON_Parse 解析 → 递归转换 json_value 树（object/array/string/number/bool/null）；
  - 点路径 "a.b.c" + 数组索引 "arr[0]"（find_value + split_name）；
  - json_getint 兼容 string→atoi；json_getstring_len cmpmode（0/1/-1）；
  - 返回码语义对齐：-1 找不到、-2/-3 类型/长度错误、0 成功。
- 3 处 json-parser.h 改为转发头（include json_api.h）→ 66 处调用**零改动**；
  app uvprojx：json-parser.c → json_api.c；driver 用类型（实现由 app 提供）；
- 原 json-parser 头备份 .orig（备查）。

### 验证（双重）
- **PC 实测**（tcc 编译 json_api.c + cJSON.c）：点路径 tag_filter.bank/mask/match、
  数组索引 antennas[0]、getstring_len cmpmode 全部正确；
- **设备实测**（HTTP API）：paramget/device_info、getgpi、connected_antennas、
  **startasyncinventory + getasynctags + stopasyncinventory 完整盘存**（EPC E281D0...、RSSI -64）全部正常；
- UV4 编译 0E/0W。

### 收益
- 固件从双 JSON 库 → 单 cJSON（省 json-parser.o ~5.9KB ROM）；
- 66 处调用点零改动、driver 工程不重编。

## 2026-08-24：串口 OTA 全链路打通 + 去掉全部断点续传 + HTTP 进度（v9.81bq~v9.81bw）

### 串口 OTA 升级帧无响应（v9.81bq）
- **根因**：命令口 UART1 RX 缓冲 `MAX_UART1_BUF_SIZE = 3072`（RTOS 下）< 升级帧
  4096 payload + 9 头 + 2 CRC = **4107B** → 缓冲溢出丢帧 → handle_frame 收不全 → 无响应；
  探测帧仅 11B 装得下 → ACK 正常、升级帧失败（与现象吻合）；
- **修复**：`uart.h` `MAX_UART1_BUF_SIZE 3072→8192`（需重编 driver 工程 + app 工程）。
- **验证**：串口 OTA 探测→续传→验签→重启全流程通过 ✅。

### 二次升级残留进度卡住（v9.81br）
- **现象**：第一次 OTA 后残留进度 401898 > 包大小 401734 → 第二次探测返回残留进度，
  包头帧握手 old_total==new_total（同包）→ 走续传 → offset>=total 上位机不发数据 →
  假"无响应"+ modbus 误读残留（`modbus_tcp read_n nlast:5377`）；
- **修复**：`ota_transport_uart.c` 包头帧判定补 `progress >= new_total` → 同包残留也
  clean restart；`ota_agent.c` confirm 后 `ota_set_progress(0)` 清残留。

### 串口上位机 900C 偶发丢包（v9.81bs）
- **根因**：send_func 读模块口广播（uart0fd=UART0）在 `gIsModAPICtrl==1`（httpapi 占用
  模块口）时只设 `gIsUnlockUart0=1` 就继续读模块口 → 与 httpapi **竞争读模块口响应** → 丢包；
  v9.81aj 时代 send_func 读命令口（与模块口分离）无需让出；
- **修复**：`gIsModAPICtrl==1` 时 send_func **跳过读取 + sleep_ms(1)**（真正让出）。
- **验证**：串口上位机连接正常 ✅。

### 去掉全部断点续传（v9.81bt，用户决策）
- **风险**：续传无版本比对——若暂存残留与新包**大小相同**（版本不同），包头帧握手误判
  "一致"→ 续传 → 新旧固件拼接损坏；
- **修复**（所有通道 no-resume）：
  - 串口：包头帧**总是**清进度从头下载（不再比对 old_total）；CRC/超尾错误直接丢帧；
  - 远程下载 `ota_download.c`：进度恒 0、**不发 Range 头**，整包下载；
  - USB/HTTP：本就无断电续传（USB 仅会话内 RAM 状态续传，HTTP 整包 POST）→ 无需改。
- **安全性**：中断/断电后设备继续运行旧固件（暂存区隔离 + 验签门控 + boot 二次校验），
  下次 OTA 从头覆盖半写暂存。

### HTTP OTA 进度显示（v9.81bu/bv/bw）
- `ota_http_send.py` 由 urllib 一次性 POST（无进度）改为**手动 socket 分块 16KB 上传**：
  - v9.81bu：进度百分比 + KB + 速率（设备端 http_handle_conn 本就流式收 body 写暂存）；
  - v9.81bv：进度改为**逐行显示**（与串口/USB 风格一致，原 \r 单行覆盖）；
  - v9.81bw：发送超时兜底 + 明确报错（拔网线 → "network lost or device busy"）。
- **验证**：HTTP OTA 20%→100% 进度 + 设备 resp:OK → 验签重启 ✅。

## 2026-08-23/24：IP 连接修复全链路（v9.81bi~v9.81bp：V1 语义恢复 + 串口/OTA 共存）

### 背景（IP 9023 根因链）
- **V1 验证**：编译 94924c1（无 OTA）实测 **IP 连接正常**（ARM7 + SIM3200，模块软件 25.01.18.00）
  → 协议/上位机 DLL 无问题，是 OTA 改造破坏 IP；
- **根因 1（透传目标）**：v9.80 串口 OTA 改造（5d71975）把 `uart0fd` 从 `COMMON_INTERFACE_UART0`（模块口）
  改成 `COMMON_INTERFACE_UART1`（命令口）→ 主循环 0xff 透传（复用 uart0fd）把 IP detect04 帧
  写到命令口 → 模块收不到 → **9023**；
- **根因 2（响应竞争）**：v9.81bi 曾用主循环 read 回传模块响应，与 send_func 模块口广播**竞争抢读**
  → IP/串口均失败；**V1 正确机制 = send_func 读模块口 → sendforall → sendthread 写回 gCurInfFd**；
- **附带**：v9.81al 波特率 115200（OTA/命令口/boot/printf）、usb_bsp BSP_CLK_Init 重入移除（v9.81ak
  用户编辑，USB 初始化不再重配时钟干扰 W5100S SPI）。

### 修改链（提交 aba02d1 为止，10 个提交）
| 版本 | 关键改动 |
|------|---------|
| v9.81bi~bj | 透传目标改回模块口（write COMMON_INTERFACE_UART0）+ 115200 波特率 + usb_bsp 重入移除 |
| v9.81bk | 实验：注释全部 OTA（定位非 OTA 代码所致） |
| v9.81bl | `uart0fd` 恢复 UART0（V1 语义）；命令口 UART1 加入主循环 uarts（串口链路） |
| v9.81bm | **移除主循环 read 回传**（纯 write，消除与 send_func 竞争）→ **IP 连接恢复** ✅ |
| v9.81bn | 完整方案：send_func 读模块口广播（IP）+ UART1 进主循环（串口）+ OTA 恢复 |
| v9.81bo/bp | 命令口 OTA 帧拦截放 custom 分支（is_custom_cmd 对非 0xff 一律返回 1，ret==0 拦截永不执行）→ 探测 ACK ✅ |

### 最终架构（v9.81bn/bp）
| 功能 | 机制 | 状态 |
|------|------|------|
| IP 连接 | send_func 读模块口(UART0) → sendforall → sendthread 写回 socket（V1 语义） | ✅ |
| 串口连接 | 命令口 UART1 进主循环 uarts，主循环 select 处理 | ✅ |
| 串口 OTA | 主循环 custom 分支拦截 "OTA1" 帧 → 大缓冲 otabuf → handle_frame | 探测 ACK ✅，升级帧待修 |
| HTTP/USB OTA | ota_agent_boot/usb/http 恢复启动 | ✅ |

### 待办（明天）
- **串口 OTA 升级帧无响应**：根因已定位——UART1 RX 缓冲 `MAX_UART1_BUF_SIZE = 3072`（RTOS 下）
  < 升级帧 4096 payload + 9 头 + 2 CRC = 4107 → 溢出丢帧；改 uart.h 缓冲至 8192 后重编 driver + app；
- git push（10 个提交，需用户终端输入 GitHub 凭据/PAT）；
- HTTP/USB OTA 三通道回归；C1/R7/M2 真机回归（搁置项）。

## 2026-08-23：主循环 0xff 透传目标改回模块口（v9.81bi：v9.80 串口 OTA 回归修复）

### 背景
- 原始版（94924c1）：`uart0fd = COMMON_INTERFACE_UART0`（**模块口**）→ 主循环 IP 透传写模块口
  → 模块响应经 sendthread 广播回 socket → **IP 连接正常**；
- **v9.80 串口 OTA 改造**（5d71975）：`uart0fd` 改为 `COMMON_INTERFACE_UART1`（**命令口**）
  → 主循环 0xff 透传（复用 uart0fd）把 IP detecttm/上位机 M5e 帧写到**命令口**，
  模块收不到 → 无响应 → **IP 9023**（用户反馈 IP/串口均连不上）；
- v9.81ap 已修（透传目标显式改回 UART0）——本次回退 v9.81aj 后回归，补回该核心修复。

### 修改（app/src/Lan2Uart.c 主循环 ret==0 透传分支，1 处）
- `write(uart0fd, ...)` → `write(COMMON_INTERFACE_UART0, ...)`（模块口）+ 读模块响应
  （300ms 超时，0xff 帧长解析）回传 `gCurInfFd`（IP socket）；
- 与 v9.81ap 语义一致；串口 OTA（send_func 读命令口 uart0fd）不受影响。

### 验证（待用户编译 + 烧录 + 上位机 IP 连接）
- 重编 app → 生成 v9.81bi 产物 → 烧录；上位机 IP 连接 192.168.1.250:8080 应识别模块（不再 9023）；
- W5100S 4 socket 分配（被动模式）：SOCKET0/1=8080 监听（主循环 apt_pair）、SOCKET2=8081
  （HTTP OTA）、SOCKET3=15000（UDP 广播）；fw_upgrade 仅主动模式启动，被动模式无冲突。

## 2026-08-23：防御性加固 C1/R7/M2（v9.81aj：长度字段越界防护，编译验证）

### 背景
- ISSUES.md 记录未改的 3 项软件可执行加固项（硬件验证项 V1-V11/H1-H9 无法在本环境执行），本轮全部落地；
- 均为"异常输入越界"类防御修复，行为不变量：正常协议流量零影响（各上限远高于合法最大值）。

### 修改

| 项 | 文件 | 修改 |
|----|------|------|
| C1 | app/src/Lan2Uart.c | **IOS**：recvbuf[5]>120 拒绝（GPO 条数，每条 2B，6+2×120=246≤255；硬件仅 5 路 GPO）；**SIO**：recvbuf[3]>120 拒绝（同 IOS）；**透传**：recvbuf[1]>250 拒绝（总长 recvbuf[1]+5≤255，与 OTA 帧 recvbuf[1]>248 拒绝口径一致）。原三个长度字段无上限 → 异常输入可越界 recvbuf[255] |
| R7 | app/src/reader_msg.c | 新增 rdr_name_len()：设备名有界长度（name[129] 未以 0x00 终止时防 strlen 越界读 + 钳位 ≤0xFE 适配 1 字节长度字段）；AddMsgHeader2SockBuffer 与 SetMsgDatalen 统一使用（包头/长度字段/拷贝三处一致） |
| M2 | MQTT/mqtt_interface.c | SBuffer[dlen]=0 加防护：dlen>0 && dlen<TagSendBufLen 才写终止符（原 dlen 达缓冲容量时越界 1 字节） |

### 验证
- UV4 全量重建 app：**0 Error(s), 0 Warning(s)**
- Program Size：Code=373500 RO-data=26504 RW-data=13480 ZI-data=114232
- 编译日志：app_rebuild_c1r7m2.log（已验证 Lan2Uart.c / reader_msg.c / mqtt_interface.c 均重新编译，firmware.axf 时间戳更新）
- 待真机：正常 GPO 设置/透传/MQTT 发布回归（异常输入不再触发复位/越界）

## 2026-08-22：串口 OTA 无响应根因彻底修复 + 调试信息清理（v9.81ad ~ v9.81ah）

### 症状
- 串口 OTA 探测（--probe-only）无 ACK；设备对 CH340（USART1/命令口）下发的 OTA 帧无响应；
- 即使收到探测 ACK，升级中途 struct.error / 无进度；
- v9.81ag 前：设备在"探测即升级旧包"死循环中反复重启。

### 根因链（三个独立缺陷叠加）
1. **USART1 RX 中断被 BSP_PRINTF_Preinit 杀死（CR1 覆盖）**：
   - printf/TRACE 输出口 == USART1（board.h BSP_PRINTF_DEVICE=CM_USART1）；
   - USB 库 usb_bsp.c / main.c / bsp.c 均调用 DDL_PrintfInit -> BSP_PRINTF_Preinit
     （board.c:459）：USART_UART_Init 全量覆写 CR1（写入 SBS|OVER8，无 RE/RIE）+
     USART_SetBaudrate（置 FBME）+ USART_FuncCmd(USART_TX)（只开 TE）-> CR1=A0008008
     （RX/RIE 被关）-> RX 中断永不触发 -> OTA 帧收不到。
2. **INTC 中断路由错配**：USART1_RI(301) 硬件组 IRQ86-91，但 uart2.c 早期用 INT003
   （SEL3 不路由）-> 改 IRQ86/87（SEL86=300 EI / SEL87=301 RI）后路由正确。
3. **探测帧误触发 finish（残留旧包死循环）**：ota_transport_uart.c 探测帧（len=0）
   在 progress>=pkg_total 时直接 local_ota_finish()——客户端一探测就升级暂存区
   残留旧包（版本号相同 0x01150000 拦不住）-> mark_ready->reboot->旧固件->又探测->循环。

### 修复
- **board.c BSP_PRINTF_Preinit**：USART_FuncCmd 补开 (USART_RX|USART_INT_RX)——
  printf 初始化不再关 RX（根治 USB/OTA 重入杀中断）。
- **uart2.c**：IRQ 改 INT086/087（SEL86=USART1_EI / SEL87=USART1_RI）+ NVIC 使能；
  CR1 直写 0x802C（OVER8+RE+TE+RIE，无 SBS 无 parity）。
- **ota_transport_uart.c**：探测帧不再触发 finish（只回 ACK 进度），包一致性由
  会话首帧包头帧比对（old_total != new_total -> clean restart 全新下载）决定——
  彻底消除残留旧包死循环。
- **调试信息清理（v9.81ah）**：移除 Lan2Uart.c [sf] 全部诊断 TRACE（rxcnt/SEL86/87/
  CR1/SR/ISPR2/LOOP/heal）与 uart2.c [u2] 诊断；保留 CR1|=0x24 自愈防护。

### 验证（真机）
- 烧录 v9.81ag bin 后：发 12345 -> LOOP rxcnt=5（RX 中断通）；
- 串口 OTA 完整闭环：探测 ACK -> pkg changed clean restart -> 0-100% -> mark_ready ->
  BOOT commit/swap -> 新固件启动（v9.81ag 与 v9.81ah 两次全流程通过）；
- 探测不再触发 finish 死循环（v9.81af 修复）+ 包头帧握手 clean restart（v9.81ag 修复）。

### 产物
- merged_f4a0_v981ah.bin（SHA 6D9CF865...，含 boot+app，烧录器/J-Flash 用）
- fw_acc_v981ah.otapkg（串口/USB/HTTP OTA 用）

## 2026-08-20：USB CDC OTA 修复（v9.81 根因：USB RX 环形缓冲 1536B 溢出丢帧）

### 症状
- USB 流数据收满（97×4096+376 帧 monitor 均打印）但无 `ss: done`，无 HardFault（固件仍运行）；
- 通道锁 `s_ota_channel_busy` 永久持有 → 串口 OTA 一直被拒（`channel busy (usb in progress)`）。

### 根因（本次定案）
1. **USB CDC RX 环形缓冲仅 1536 字节**（`io_stream.h USB_COMPO_RXBUF_LEN`），连单帧（4107B）都放不下；
   发送器 burst 8 帧=32KB，设备 QSPI 擦除（~40ms/扇区）+ 写期间 `ota_usb_task` 不读 USB，
   数据持续涌入 → 缓冲溢出。
2. **`vcp_rxdata` 满时无 head 检查直接回绕覆盖未读数据**（driver_lib 预编译）→ 帧字节被覆盖损坏
   → `ss_feed_byte` CRC16 静默丢弃（原无 TRACE）→ `s_off` 卡在 total-376，永不 finish。

### 修复
- **driver 侧**（需 Rebuild hc32f4a0_driver 并复制 lib 到 driver_lib/）：
  - `projects/user/inc/io_stream.h`：`USB_COMPO_RXBUF_LEN` 1536→**32768**（容纳完整 burst）
  - `projects/usb_lib/examp/cdc_data_process.c` `vcp_rxdata`：加可用空间检查——满则丢弃新包，
    绝不回绕覆盖未读数据（宁可丢帧由 OTA 重传）
- **app 侧**（ota_usb_stream.c / ota_usb.c）：
  - ACK 流控 8 帧→**4 帧**（收紧节奏，防 burst 溢出）
  - CRC 不匹配/擦除失败/写失败路径全部加 TRACE（原静默，现在可直接定位）
  - `ota_usb_stream_idle_timeout()`：会话空闲 8s 自动释放通道锁（上位机掉线兜底，防锁卡死）
- **上位机**（tools/ota_usb_stream_send.py）：burst 8→4 帧；设备进度落后时 rewind 补发缺口
  （原只前向同步，丢帧会一直赛跑）

### 验证
- 待真机复测：烧录后断电重启（清锁）→ USB 发送器 `python ota_usb_stream_send.py fw_acc.otapkg --port COM8`
  → monitor 观察 `ss: done → verify → crc ok → mark_ready ok → reset`

## 2026-08-20：USB OTA 慢速 60-90s 根因（v9.81b：ACK 栈缓冲被异步 USB 发送破坏）

### 根因
- `usb_deveptx` 是**异步发送**（仅记录 `ep->xfer_buff = pbuf` 指针即返回，USB 中断/DMA 稍后读内存）。
- `ota_usb_stream.c` 里 4 处 ACK 全用**栈上局部 `ackbuf[16]`**：`write()` 返回后栈空间被后续帧处理
  立即覆盖 → ACK 帧在真正发出前已被破坏 → 发送器收不到有效 ACK → 每批 `read_ack(5.0)` 等满超时
  → 25 批 × 5s ≈ 125s（实测 60-90s）。
- 串口 OTA 无此问题（UART 同步发送，数据已拷走）——故只有 USB 慢。

### 修复（v9.81b）
- `ota_usb_stream.c`：4 处 ACK 缓冲全部改 **static `s_ackbuf[16]`**（异步发送要求缓冲存活到传输完成）。
- `ota_usb.c`：`ota_usb_task` 栈 8KB→**16KB**（finish 路径保险余量）。

### 用户观察补充
- 蜂鸣 2 声 = 唯一码验证通过（正常）；3 声 = 失败（rd_idkey_fun 死循环，但那样 USB 不 ACK，已排除）。
- 软狗（SoftWdtISR）超时只打印+停喂，**不会复位**；升级中"重启"来自 thread_check 检测线程
  osThreadError/terminated → system_reset（栈溢出/线程崩溃路径）。

## 2026-08-20：USB OTA 重启+CRC 大量失败的真正根因（v9.81c：send_func 竞争读 USB1）

### monitor 实证（用户提供）
- 大量 `ss: crc MISMATCH len=4096 off=N`（off 全部 4096 对齐 = 帧边界正确但 payload 被破坏）；
- `system reboot......`（system_reset 通用打印）+ 重启后完整初始化日志；
- `read--buf == NULL`（io_stream read 参数异常 = 内存/调用错乱前兆）；
- 乱码字节夹杂 `ss: frame` 打印。

### 根因
- **`send_func` 主循环（Lan2Uart.c:537）用 `apt_multi_infs_select` 轮询，其中 `apt_usb_select_nob` 会选中 USB1**；
- USB OTA streaming 期间 USB1 环形缓冲有数据 → send_func 选中 USB1 → `READ_N_INFS(gCurInfFd=USB1, ...)` **与 `ota_usb_task` 竞争读同一环形缓冲**（usb_head 被双消费者推进）→ 帧数据错乱（CRC MISMATCH）+ 竞争崩溃 → system_reset 复位；
- 串口 OTA 无此问题（UART 不同口无竞争）——解释了为何只有 USB OTA 重启。

### 修复（v9.81c）
- `ota_storage.c/.h`：新增 `ota_channel_busy()` 只读查询；
- `Lan2Uart.c` send_func select 循环：`gCurInfFd==USB1 && ota_channel_busy()` → 跳过（sleep 5ms continue）；
  USB OTA 会话期间 USB1 由 ota_usb_task 独占，不再双读。

## 2026-08-20：USB OTA 升级失败的最后一环（v9.81d：重复帧误判 overflow 清会话）

### monitor 实证（v9.81c 复测）
- 速度 5.86s（66 KB/s）✅、无 system reboot ✅（竞争修复生效）；
- 末尾 `ss: overflow off=397312 plen=4096 total=398060`：设备已写 97×4096=397312，
  又收到 4096 帧（发送器 ACK 迟到→回退重发的重复帧）→ 397312+4096>398060 触发 overflow
  → 释放通道锁+SS_IDLE → 后续 748B 尾帧与 probe 全被忽略 → 升级失败、发送器误判
  `device gone`。

### 修复（v9.81d）
- `ota_usb_stream.c` overflow 分支：越界/重复帧**不释放锁、不清会话**，只 ACK 当前进度，
  让发送器重传真正缺口（末尾 748B 帧）→ 收满 → done。

## 2026-08-20：USB OTA verify CRC32 失败的真正根因（v9.81e：设备无 seq 校验→错位写入）

### monitor 实证（v9.81d 复测）
- ✅ `ss: dup/over ... ignore`、`ss: done 398120, verify...`（收满、无重启）；
- ❌ `ss: crc FAIL exp=AE1865E2`：verify CRC32 失败。
- 关键证据：`ss: crc MISMATCH len=4096 off=28672`（第 8 帧 CRC16 失败被丢弃）后，
  设备继续收第 9 帧并**写到 28672**（它不知道是第 9 帧）→ 后续全部错位一帧
  → 暂存数据与包不一致 → verify CRC32 必失败。

### 根因
- USB 流协议**没有 seq/偏移校验**：设备按"到达顺序"把帧写到 `s_off`，
  任意一帧 CRC16 失败被丢弃后，后续帧全部错位写入。
- UART OTA 无此问题（每帧 ACK/RESUME 带偏移，发送器严格续传）。

### 修复（v9.81e）
- `ota_usb_stream_send.py`：seq 改为**按内容偏移计算**（`off//4096 + 1`），
  重传/rewind 时 seq 与内容一致（原全局计数器在 rewind 后错位）；
- `ota_usb_stream.c`：数据帧增加 **seq 校验**——`fseq == s_off/4096 + 1`，
  不匹配（错位/重复帧）→ 忽略 + ACK 当前进度，等发送器重传正确缺口；
- `ota_usb_stream.c`：seq mismatch 日志**降噪**（同 off 只打一条，发送器重传风暴不再刷屏）。

## 2026-08-20：USB OTA 日志降噪 + 发送器重传风暴修复（v9.81f）

### 降噪（用户反馈 monitor 刷屏）
- ss: frame 逐帧 TRACE 删除（probe len=0 与数据 4096 交替导致每次 len 切换都打印）——正常帧完全静默，monitor 只剩关键事件 + 错误诊断；
- ss: (re)start header burst 重复帧只打首条；
- ss: seq mismatch 同 off 只打首条（发送器重传风暴不再刷屏）。

### 发送器修复（tools/ota_usb_stream_send.py）
- 主循环去掉 rewind（设备 ACK 滞后导致读到旧进度，重发已确认帧导致 seq mismatch 风暴）；
- probe 前 reset_input_buffer() 清积压 ACK，确保读到设备最新进度。

## 2026-08-20：USB OTA 进度真实化（v9.81g：发送器按设备确认进度驱动）

### monitor 实证（用户批评不严谨）
- 旧逻辑：发送器 1.17s 把 398KB 全灌进 USB FIFO 显示 100%，但设备 QSPI 擦+写消化能力 ~80KB/s，32KB 缓冲溢出，设备实际只到 7%（28672），crc MISMATCH + seq mismatch 风暴；
- 进度条显示的是已发送量，不是设备已确认量。

### 修复
- 发送器改为设备进度驱动：每批 4 帧（= 设备 ACK 节奏 16KB）-> flush 陈旧 ACK -> 等设备 ACK -> 回退到设备确认偏移重发缺口；进度条显示设备确认偏移，不再 100% 假象；
- 实测：进度 0-100% 平滑真实递增，速率 ~75KB/s（匹配设备消化能力），6.1s 收满 398264B，无 mismatch、无回退；多次复测稳定 ss: done -> crc ok -> mark_ready ok -> reset。

## 2026-08-21：OTA 全链路审查修复（v9.81p：8 项 bug + 安全一致性）

### P0 严重（续传/下载数据损坏）
- **BUG-1 串口断点续传游标 64KB 对齐**：v9.81o 起 ensure_erased 按 64KB 块擦除，但续传初始化仍按 4KB 扇区对齐
  → 非 64KB 边界地址调 EraseBlock64K 会擦错块/擦掉已写数据 → 续传后 verify CRC 必败（已修）
- **BUG-2 ota_download.c（HTTP 客户端下载）补 64KB 懒擦除**：原无任何擦除，暂存区旧数据残留 → payload 损坏必败（已修）

### P1 功能/安全
- **BUG-3 HTTP-server 通道补版本检查**：ver != OTA_FW_VERSION → 拒收+清进度（与串口/USB 一致；原 HTTP 可刷入跨版本旧包）
- **BUG-4 confirm 延迟到业务自检后**：ota_agent_boot 只置待确认标志，gIsFinInit=1（OpenReader 完成）后 ota_agent_confirm() 才清 NEED_CONFIRM
  → 坏固件初始化失败时 bootloader 的 boot_count 回滚机制真正生效（原启动即无条件 confirm，坏固件永不回滚）
- **安全一致性：三通道补 HMAC-SHA256 验签**（ota_security_verify_staged）：串口 local_ota_finish / USB ss done / HTTP verify 路径
  → 与 ota_agent_run 网络路径一致（原仅网络路径验签，本地三通道只验 CRC32）

### P2 健壮性
- **BUG-5 USB 通道补进度 KV**（64KB 批量同步，断点续传一致性）
- **BUG-6 ota_storage_prepare 4KB→64KB 块擦除**（OTA 全链路统一）
- **BUG-7 KV 同步间隔 32KB→64KB**（串口/HTTP/下载三处统一，与块擦除对齐）
- **BUG-8 EFM 标志页写后 FWMC DISABLE 重新锁定**（成功+失败路径）
- **send_func 线程栈 4KB→8KB**（容纳 mbedtls HMAC 验签栈需求）

### 真机回归（用户实测 3 通道全部通过）
- **串口**：COM3@460800 探测+升级+monitor 正常，断点续传逻辑生效
- **USB**：COM8 流式发送正常，进度真实递增
- **HTTP**：POST 到设备 8081 正常，版本检查/HMAC 验签通过
- 3 通道统一闭环：下载 → CRC32 → HMAC 验签 → 版本检查 → mark_ready → reset → bootloader commit/swap → 新固件自检 confirm

### 产物
- merged_f4a0_v981p.bin（466208B）、fw_acc.otapkg（400754B，payload 400672B，crc 39F7BCAA）
- App 0E/0W；待真机回归：三通道闭环 + 串口断点续传 + HTTP 旧版本拒收 + 坏固件回滚

## 2026-08-21：配置存储彻底 KV 化（v9.81s：片内 Flash 不再存配置数据）

### 架构（v9.81r 重构）
- hwport.c flash_bytes_read/write/erase 拦截配置区地址（0xFA000-0xFDFFF）→ 全部路由到 FlashDB KV（QSPI）
- **配置数据只存 QSPI KV，片内 Flash 绝不写配置**（满足 OTA 保配置 + 片内零配置）
- 读取方零改动：readercfg/common/custom_ee/ipc 仍调 flash_bytes_*，底层已路由 KV
- 非配置区（OTA 标志页 0xF0000、Bank B、EFM 保护位）走原片内逻辑，OTA 机制不受影响
- 支持块内偏移访问（ActiveModeConfig+20、+i*32 等），rdr_cfg_addr_to_key_off 区间匹配
- KV miss 填 0xFF 等同片内空态，调用方按无配置处理
- rdr_cfg_migrate() 一次性迁移：片内旧配置 → KV（flashdb() 后调用一次）

### 产物
- merged_f4a0_v981s.bin、fw_acc.otapkg（402202B）
- 编译通过，尚未真机验证（待回归：配置保存 + OTA 升级后配置保留）

## 2026-08-21：OTA 升级丢配置修复（v9.81r：配置迁 FlashDB KV，方案 A）

### P0 定案：OTA 双 bank swap 后片内配置区（0xFC000，9 项配置）全部丢失
- HC32F4A0 swap = 整 bank 地址重映射，swap 后 0x0000 段命中物理 Bank B
- boot commit 只写固件不复制配置区 → Bank B 配置区 0xFF → 升级回默认配置

### 修复（方案 A：配置迁 FlashDB KV）
- 新增 rdr_cfg_kv.c/.h：9 项配置（Net/Wifi/HwType/Monet/BT/ActiveMode/WorkMode/Passive/Boot）→ QSPI fdb_kvdb1 KV
- QSPI 不参与片内 swap → OTA 后配置天然保留
- pre_set_config：KV 优先读，KV 无则回退片内（兼容旧设备）
- set_config_to_flash：KV + 片内双写（兼容旧固件/烧录工具）
- rdr_cfg_migrate()：一次性迁移（片内有效配置 → KV，cfgver 防重复），flashdb() 后调用
- usb_utility.c 补 usb_dev_driver.h include（消 2 个隐式声明 warning）

### 产物
- merged_f4a0_v981r.bin（468116B）、fw_acc.otapkg（402662B）
- 编译通过，尚未真机验证（待回归：配置保存 + OTA 升级后配置保留）

## 2026-08-21：HTTP OTA 提速系列（v9.81l/m/n：缓冲对齐 + 时序定位）

### v9.81l：HTTP 通道懒擦除
- 原：每次下载先全擦暂存（~4s）；改为按需擦除（写前擦目标扇区）；
- 实测结论：擦除总成本不变（同样要擦满整个包占用空间），无提速——但为后续优化提供正确基准。

### v9.81m：HTTP 写缓冲 1KB→4KB
- 与 4KB 扇区对齐，减少擦/写切换次数；产物 merged_f4a0_v981m.bin（465488B）/ otapkg（400034B）。

### v9.81n：HTTP 时序诊断（真机实测）
- 新增 TRACE：ota_http: t_erase=%lums t_write=%lums total=%lums；
- 400KB 包实测：**t_erase=3646ms t_write=631ms total=8296ms** —— 擦除占总耗时 44%，是最大瓶颈；
- 依据 W25Q256JV 数据手册：4KB 扇区擦除 45-400ms/个（96 个扇区串行 ≈ 3.6s），64KB 块擦除 ~120ms/个 → 理论可降到 ~1.1s。

## 2026-08-21：64KB 块擦除提速（v9.81o：三通道统一）

### 改动
- driver：QSPI_FLASH_EraseBlock64K()（WR_ENABLE + 0xD8 + CheckProcessDone 500U）；driver_lib 头声明同步；
- 三通道 ensure_erased 全部改 64KB 步进（staging 0xE00000 64KB 对齐，安全）：
  - ota_http.c http_ensure_erased、ota_transport_uart.c（串口）、ota_usb_stream.c（USB）；
- 产物：merged_f4a0_v981o.bin（465544B）、fw_acc.otapkg（400090B，sha 7AE64ADD...）。

### 真机实测（v9.81o，用户 monitor 实证）
- ota_http: POST 400090 bytes → t_erase=**1169ms**（3646ms → -68%）→ t_write=634ms → total=5749ms（8296ms → -31%）
- verify 400008B crc=4390D9AD → crc ok → mark_ready ok → reset ✓ 全链路通过
- 64KB 块擦除实测 ~58ms/块（20 块 ≈ 1169ms，手册 tBE=120ms 典型）
- 新瓶颈：TCP 网络接收 ~3.9s（400KB ≈ 100KB/s）——擦除已从 44% 占比降到 20%

### 三通道回归（v9.81o，用户实测全通）
- **USB**：400008B 4.27s = 91KB/s（v9.81g 的 6.1s/75KB/s → 提速 30%，64KB 块擦减少擦除停顿）→ done → verify → reset
- **串口**：400090B 100% 下载 → ota local done ver:18153472(0x01150000) → mark_ready → system reboot
- **HTTP**：POST 400090 → t_erase=1169ms → verify 400008B crc=4390D9AD → crc ok → reset
- 修复：v9.81o 首次打包误用版本 0x01020000，串口通道版本检查拒绝（pkg ver != fw ver）；重新打包 0x01150000 后三通道一致通过

### 预期（待真机复测）
- 400KB 固件擦除 3646ms → ~1.1s（20×64KB 块 × ~120ms）；HTTP OTA 总耗时 8.3s → ~5.5s；
- 三通道回归：串口/COM3、USB/COM8、HTTP/设备:8081 均需重测确认。
## 2026-08-21：HTTP OTA 修复并真机验证通过（v9.81k）

### 修复（v9.81k）
- HTTP 通道原：复用 ota_get_progress() 残留进度 + 不擦暂存直接写 → QSPI 命中旧数据
  → verify 读回旧包头（397752B）crc FAIL；
- 改为全新下载：ota_set_progress(0) + ota_storage_prepare(total) 全擦 + 从偏移 0 写入。

### 真机验证通过（用户 monitor 实证）
ota_http: POST 400002 bytes
ota_http: verify 399920B crc=59446A16
ota_http: crc ok, mark_ready...
ota_http: mark_ready ok, reset...
system reboot......

**三通道 OTA 全部闭环**：串口/COM3、USB/COM8、HTTP/设备:8081，各自独立、
互斥共用暂存区，同一 verify+mark_ready+reset 闭环。

## 2026-08-21：HTTP OTA 独立通道（v9.81j：设备 HTTP 服务器，PC POST 固件包）

### 三通道完全独立（互不交叉）
- 串口：ota_uart_upgrade.py -> COM3（OTA1 帧流）
- USB：ota_usb_stream_send.py -> COM8（4KB 帧流）
- HTTP：ota_http_send.py -> 设备 listenPort+1（HTTP POST 整个 otapkg）

### 设备端（新增 ota_http.c/.h）
- 独立 ota_http_server 线程：socket 2/3 + 端口 listenPort+1
- 解析 HTTP 头 Content-Length，流式 body 写暂存（ota_storage_write_stage），
  通道互斥 + 32KB KV 批处理
- 完成：verify CRC32 -> ota_mark_ready -> reset（与串口/USB 同一闭环）
- 从 user_main 启动（与 ota_usb_start 并列）

### PC 端（新增 ota_http_send.py）
- POST http://<device-ip>:<port>/ota，body = 整个 otapkg
- 不碰旧 httpapi/httpAPIDispatch 服务器（全新独立路径）

## 2026-08-20：HTTP OTA 与串口/USB 完全一致（v9.81i：进度 KV 批处理 + 通道互斥）

### 改动（ota_download.c）
- 进度 KV 32KB 批处理：原 ota_OnBody 每块（~1KB）就 ota_set_progress -> fdb_kv_set_blob 全分区扫描 ~230ms，下载被严重拖慢；现 RAM 缓存进度（s_dl_prog/s_dl_prog_kv），每 32KB 同步一次 KV——与串口/USB 的 OTA_PROGRESS_KV_SYNC 机制一致；
- 通道互斥：ota_download_start 开头 ota_channel_try_acquire()，所有出口（URL 解析失败/TLS 失败/解析错误/s_dl_err/成功）ota_channel_release()——与串口/USB 共用 QSPI 暂存区，禁止并发下载；
- 完成时最终同步 KV：最后一块可能不足 32KB 未写，完成前强制同步一次，并改用 RAM 进度 s_dl_prog（而非 KV 滞后值）判定下载大小。

### 三通道一致性（全部打通）
- 暂存区：同一 QSPI OTA_QSPI_STAGE_BASE
- 进度 KV：同一 32KB 批处理机制
- 通道互斥：同一 ota_channel_try_acquire/release
- 校验/完成：同一 verify(CRC32+HMAC) + ota_mark_ready + reset

## 2026-08-20：串口 OTA 提速（v9.81h：payload 2048→4096，与 USB 帧对齐）

- `ota_frame.h` / `ota_transport_uart.h`：`OTA_FRAME_MAX_PAYLOAD` 2048→4096（帧 4107B）
- `tools/ota_send.py`：`MAX_PAYLOAD` 2048→4096（与设备端一致）
- 缓冲均 static（otabuf/frame 用 OTA_FRAME_MAX_LEN 自动扩大），无栈风险（send_func 4KB 栈不变）
- 效果：195 帧→98 帧，串口传输时间 ~8.7s→~4.4s（460800 线速极限内），与 USB OTA 帧大小对齐

### ✅ 真机验证通过（v9.81e，用户 monitor 实证）
```
ss: done 398284, verify...
ss: crc ok, mark_ready...          ← verify CRC32 通过（seq 校验生效）
ota mark: flag=0
ss: mark_ready ok, reset...
system reboot......                ← 设备重启进 bootloader commit
... InitReader Ok / CDC interface starts ...   ← 新固件正常启动
```
**USB CDC OTA 完整闭环打通**：收满 → verify → mark_ready → reset → bootloader commit → 新固件启动。
与串口 OTA 互不干扰（通道互斥 + send_func 跳过 busy USB1），速度 5.9s/398KB（66 KB/s）。

## 2026-08-19：串口 OTA 真机全链路闭环（v9.80 六层根因修复 + 提速 460800）

### 真机联调定位并修复的 6 层问题（按发现顺序）
1. **栈溢出死机**（osRtxErrorNotify）：payload 512→2048 后 send_func 2KB 线程栈放不下 otabuf[2075]+frame[2059] → 大缓冲改 static + 栈 2KB→4KB
2. **每帧 ~4s / ACK 全丢**：ota_usb_task 高频调试 TRACE 与 ACK 无锁并发写 USART1 TX 数据寄存器互踩（rawlog 实证帧头 'O' 拼进行首）→ 删 USB TRACE + uart_send 加 __disable_irq 临界区
3. **包变更后续传卡死**：暂存包头总长与包不一致，finish 永不触发 → 发送端先发 82B 包头帧握手，设备比对大小（不一致→清残留全新下载）
4. **verify CRC 全错但 17 边界字节全对**：`QSPI_FLASH_Write` 页编程 chunk_size 用常量 u32Addr 而非循环 addr → 非 256 对齐偏移(82+n×2048)时 174B 块跨页回卷写坏（512B 帧时代 256 对齐从未触发）→ 改 `addr % 256`
5. **升级后 RX 死（连续升级 4-5 次后出现）**：非代码问题——merge_hex.py hex 中间产物烧录后 UART1 RX 异常，直出 bin（新增 tools/merge_bin.py 绕过 hex）烧录后 RX 完全正常
6. **多终端/残留进程占 COM3**：Windows 串口独占，串口助手/烧录工具残留导致 PermissionError

### 速度优化（真机验证）
- OTA 口波特率 115200→460800（Lan2Uart.c，上位机 --baud 460800）
- KV 进度同步 8KB→32KB（OTA_PROGRESS_KV_SYNC，KV 停顿从每 4 帧摊到每 16 帧）
- 实测：普通帧 dt 0.19→0.05-0.10s，整包 397878B 下载 ~46s→~26s（~1.8 倍）
- 剩余瓶颈：32KB 一次的 FlashDB KV 写 ~300ms（fdb_kv_set_blob 全分区扫描）

### 诊断清理（按要求回退）
- 移除全部诊断 TRACE：uart2.c ISR 计数、ota rx 帧接收、dbg1 寄存器、ota w e p 计时、FW build 标记、CRC 分段诊断
- 保留功能性修复：QSPI 分块、uart_send 临界区、栈修复、包头握手、ota_switch、confirm KV 清理
- 产物：merged_f4a0_v980.bin（直接合并，463332B）、fw_acc.otapkg（0x01150000，sha 75337bd4...）

### 验证
- 串口 OTA A/B 往返完整闭环：下载→verify→mark_ready→bootloader commit ret=0→swap ret=0→新固件自检→confirm→进度清零
- 探测 1 次即 ACK（460800 稳定），RX 正常

### 待办
- 剩余速度优化：KV 写移出帧路径（异步/仅完成时写）或改 fdb_kv_set_blob 扫描
- USB OTA 复测（ota_usb_stream_send.py）
- H2 网络升级复测

## 2026-08-19：OTA 速度优化（帧 payload 512→2048B）+ A/B 手动切换命令（v9.80）

### 完成（编译 + 软件回归验证）
- **UART OTA 提速：帧 payload 512→2048B**（CHANGELOG 2026-08-18 待办 #1）
  - `ota_frame.h` / `ota_transport_uart.h`：OTA_FRAME_MAX_PAYLOAD 512→2048（设备端协议上限同步）
  - `tools/ota_send.py`：MAX_PAYLOAD 512→2048，新增 `--payload` 参数（默认 2048，≤2048 设备上限保护）
  - `tools/ota_selftest.py`：设备模拟器分片 512→2048 同步
  - 协议 offset 驱动、懒擦除、8KB 进度 KV 节流不变，无错位风险；USB stream 通道本为 4KB 帧不受影响
- **A/B 手动切换命令 `ota_switch`**（CHANGELOG 2026-08-18 待办 #2）
  - `ota_storage.c` 新增 `ota_switch_bank()`：另一 bank App 向量表有效性防呆（防单 bank 首烧设备切到空区变砖）→ 按当前 swap 状态 EFM_SwapCmd 切换（与 bootloader swap_toggle 同构）
  - `http_callback.c` json_remote_cmd 新增 `ota_switch` 分支（HTTP/MQTT 双通道自动获得）：成功→复位进 bootloader 从另一 bank 引导；另一 bank 无效→返回错误码
- **清理存量 Warning**：`ota_usb_stream.c` 删除未引用的 `ss_swap_toggle` 死函数（v9.74 遗留）→ App 0E/0W
- **修复 server_data/manifest.json sha 过期**：0x01150000 条目记录的 sha256 与磁盘 fw_acc.otapkg 不一致（v9.79 会话重建包后未同步）→ 重写为单 latest 条目 + 实际 sha（e5f9132a...），还原 2026-08-18 的清理惯例

### 验证
- App 增量编译：0 Error, 0 Warning（Code=369860 与 v9.79 全量一致，死代码本已由链接器剔除）
- `ota_acceptance.py` 回归 **9/9 PASS**（A1/A2/B/C1/C2/C3/D/E/F；此前因 manifest sha 过期 B/C2/C3 失败，修复后全过）

### 待办
- 真机复测：H2 网络 ota_update / H3 UART OTA（2048B 帧）/ H4 USB + 新增 `ota_switch` A/B 往返验证

### v9.80 修复（真机联调发现：串口探测无响应 + PC 卡 osRtxErrorNotify）
- **根因**：payload 512→2048 后，send_func（2KB 线程栈）内两个大缓冲溢出：`Lan2Uart.c` otabuf[OTA_FRAME_MAX_LEN+16]=2075B、`ota_transport_uart.c` send_frame frame[2059B] → RTX OS_STACK_CHECK 触发 osRtxErrorNotify 死循环，OTA 帧探测无 ACK
- **修复**：otabuf / send_frame frame / feed frame 改 static（移出线程栈）；send_func 线程栈 2KB→4KB（容纳 verify_payload[1024]+flashdb 深度）
- 验证：App 0E/0W（Code=369844，ZI 65424→73688 = 大缓冲入 static + 栈扩容）；ota_acceptance 9/9；产物已重新生成（merged_f4a0_v980.bin 463308B、fw_acc.otapkg 0x01150000，manifest sha 同步为文件 sha）
- 教训：**改帧大小必须全链排查栈占用**（CHANGELOG 2026-08-19 纪律再次验证）


## 2026-08-19：科学工作方式（不滥用 token）——12h USB OTA 联调教训

### 背景
- 一次 12 小时 USB OTA 联调（v9.72-v9.79）token 消耗巨大。复盘：任务本身轮次不可省，但操作方式存在大量可避免的浪费。

### token 消耗构成
- 任务本身（不可省）：8 个固件版本循环（改码→编译→打包→烧录→测试→日志→分析），每次烧录都需完整交互链路
- 操作浪费（可避免，占大头）：
  1. 反复短轮询 job_output 并拉回全量进度（如 OTA 进度 100+ 行 × 30+ 次）
  2. 命令输出全量打印不截断
  3. 反复整文件 read，替代 grep 精确定位
  4. 每个调试步骤写一个临时 Python 脚本（十几个 tmp_*.py）
  5. 已确认失败的操作重复尝试（如 push 认证失败重试 4 次）
  6. 长日志（COM7 TRACE）全量贴回对话

### 科学工作纪律（后续必须遵守）
1. **长任务一次长 wait 拿最终结果**，不短轮询
2. **输出重定向到文件，只取尾部**（Select-Object -Last N）
3. **grep 精确定位后再 read 上下文**，不整文件读
4. **尽量命令内联完成**，少写临时脚本（用完即删）
5. **确认失败的操作不重复试**，先分析根因再动手
6. **长会话主动开新会话**（带结论摘要，不延续完整历史）
7. **先定位根因再动手**——避免"改→测→再改"试错循环；一次加够诊断 TRACE 定位（如 USB 数据"时通时断"应设备端 TRACE 一次性定位，而非反复重试）
8. **操作批量合并**：一个命令串行完成多步（清理+探测+测试）
9. **收尾清理临时文件**（tmp_*.py / build_*.log / 测速脚本）
10. **定期 git commit 保存现场**（本地 commit 不依赖 push）

## 2026-08-18：v9.x 串口 OTA 全链路打通（v9.1-v9.32）+ 验收 9/9

### 背景
- 目标：UART1 串口 OTA（OTA1 帧协议，115200 8N1）+ 片内 dual-bank swap 升级闭环，真机验证通过
- 此前 H2 网络下载链路已通（v8），本次聚焦"本地 UART 通道 + 引导交换"完整闭环

### 完成（均经真机验证）
- **命令口/波特率**：v9 命令口 USART4(921600 RFID 口)→USART1(115200)；v9.2 独立 otaPara 强制 115200（模块探测曾覆盖 uartPara 为 921600）；v9.3 uart2.c RxFull 中断回调补 RX 缓冲写入（此前 RX 空、收不到帧）
- **EFM 烧写链**：v9.8 EFM 函数加 __EFM_FUNC（RAM 执行，Flash 执行会 -1）；v9.10 扇区写保护解除（EFM_SingleSectorOperateCmd(base, ENABLE)，NWPRT 未解会擦写失败）
- **进度/残留**：v9.5/v9.6 包头版本不匹配清进度；v9.11 ota_agent_boot 移到 flashdb 初始化之后（confirm 时序）；v9.30 进度 KV 每 N 帧节流→真机卡死，v9.31 回退每帧写 KV（速度优化改走帧 payload 加大路线，未做）
- **swap 引导交换**（v9.12-v9.29 逐层修复）：
  - v9.12 swap 函数 RAM 执行；v9.17 EFM_FlagShift(EFM_SWAP_ADDR) 完成等待
  - v9.28 boot commit 擦除 8KB 扇区对齐（off & 0x1FFF==0，4KB 边界会卡死/重启复现）
  - v9.27 boot_qspi_read 读边界修复（暂存区 1MB 上限判断错误导致 verify 永超界）
  - v9.29 swap 真因：FAPRT 解锁后写任意 EFM 寄存器会重新锁，swap 前须重新 EFM_REG_Unlock()+EFM_FWMC_Cmd(ENABLE)
- **boot UART 打印**：v9.22 boot_uart_init 显式 DIV1 115200（StructInit 默认 DIV64 导致无输出）；v9.25 boot_putc 字节延时 40000 NOP（TX_EMPTY 在移位开始即置位，300 NOP 不够）
- **v9.32 竞态修复**：read--uart is not open —— uart_open(UART1) 块移到 osThreadNew(send_func) 之前（被动模式下发前串口未就绪）；同时补丢 {/删残留悬空块
- **真机闭环**（H3 串口 OTA，v9.29/v9.31 双次 + A/B 往返）：BOOT: commit ret=0、BOOT: swap ret=0、swap status 0<->1、confirm new fw 执行、探测 ACK 进度 0
- 版本号：OTA_FW_VERSION 0x01130000（app_conf.h 与打包 --version 一致）

### 验收（ota_acceptance.py 9/9 PASS，2026-08-18）
- 修复项：
  - ota_server_selftest.py：当前版本动态取 r["new_version"]（原写死 0x01020000，版本 bump 后失效）
  - server_data/manifest.json：清理 18 个历史版本条目（指向被覆盖的 fw_acc.otapkg，sha 失效），仅保留 latest 0x01130000
  - ota_acceptance.py A2：add 到临时目录（防污染正式版本库）；C3 断点续传断点源改用 C2 已校验的 dl.otapkg（旧代码读残留 fw_01020000.otapkg 半包 + 服务器新包 -> 内容混搭 -> 校验失败）
- 结果：A1/A2/B/C1/C2/C3/D/E/F 全部 PASS

### 产物
- tools/merged_f4a0_v932.bin（457420B，SHA256 07EC45D66E2601DBC44E97072D2F5F1D5FBDC8C811C717E2B722901B329F7B05）
- tools/server_data/firmware/fw_acc.otapkg（391966B，0x01130000）

### 待办
- 速度优化：帧 payload 512->2048B（当前每帧 512B 写 QSPI + 每帧 KV 写为瓶颈；v9.30 节流方案真机卡死已回退）
- A/B 手动切换命令（当前仅自动升级切换+回滚）

---

## 2026-08-15：编码统一收尾（乱码清零）


### 完成
- http_callback.c:143 time() @brief 重写（历史 ???????? 字节丢失型，git 原始提交即 ????????，按函数行为补写"获取当前系统时间戳(基于SysTick)"）
- F460 两份 w25qxx.c（user/src + bootloader/user/src）：清除 UTF-8 全角空格 U+3000 残留，恢复纯 ASCII，GBK/UTF-8 双解码均通过
- 全工程扫描（F4A0 app/boot + F460 user/inc/bootloader）：GBK 解码 0 异常、0 U+FFFD、0 连续 ? 乱码
- 编译验证：F4A0 App 0E/0W（Code=361404）、F460 App 0E/0W（Code=286196）、F460 Boot 0E/0W，与基线一致

### 说明
- stream_epc_scanner.c / APIHttpRequest.cpp / HttpModuleAPI.cpp 乱码重写已含于本批
- 编码规范维持：业务源码统一 GBK（Keil 默认显示），第三方库（FWlib/LL/jsonlib/MQTT/mbedtls）未动

## 2026-08-17：F4A0 真机启动修复 + H2 联调排查

### 完成（真机验证通过）
- **F4A0 首烧/启动问题根因修复**：
  - `startup_hc32f4a0.s` Reset_Handler 加 `CPSIE i`——boot 跳转 app 时中断处于关闭（PRIMASK=1），app 启动未恢复导致 RTX 初始化失败（osRtxErrorNotify/ClibMutex）。修复后 boot 0x0 + app 0x10000 正常跳转启动
  - 量产烧录改用 `merged_f4a0.bin`（fireDAP 不支持单 hex 多地址段，bin 无此限制；已验证可启动）
- `merge_hex.py` 工具修复：校验和算法（(0x100-sum)&0xFF）、16 字节数据记录（兼容 fireDAP）、type5 Start Linear Address 记录（末尾、入口=boot Reset 向量）、bin 输出
- Keil 工程配置：app 工程 `CreateHexFile=1`（Keil 直接输出 hex）；boot 工程 OCR_RVCT4 size 0x10000
- 编译基线：F4A0 App 0E/0W（Code=361404）、Boot 0E/0W（Code=5596）
- git 提交：`c6d1556`（代码修复）、`cb632ca`（固件产物）

### H2 网络升级联调排查（未完成，问题已定位）
- 设备 HTTP API（httpAPIDispatch）无 ota_update 路由，命令走 json_remote_cmd（HTTP 响应下发）
- 排查发现：设备 connect 到 PC 的 TCP 连接不成功（服务器收不到设备连接，但 PC→设备 8080 可达）；socket 冲突（HTTP 监听占 SOCKET0/1，OTA 下载需用 SOCKET2）；QSPI 暂存区下载前未擦除（ota_storage_prepare 缺失）
- 以上为临时调试改动，**已全部回退**，待后续联调

### 待办
- git 推远端（6 个提交待推送，github 443 网络受限）
- H2 网络升级联调（设备 connect 问题）
- F460 联调（merged_f460.hex 同样建议生成 bin）
- 生产化（正式密钥、HTTPS、灰度）

---

## 2026-08-15：真机联调一页速查

### 新增
- `doc/真机联调一页速查.md`：H1-H12 全流程速查（烧录/服务器/网络+本地升级/断点/断电回滚/安全/RK/F460 + 常用命令 + FAQ）
## 2026-08-15：F460 真机联调准备

### 完成
- F460 OTA 包打包验证（platform=1，307546B：magic/CRC/SHA/HMAC 全过）
- 合并烧录文件 merged_f460.hex（boot 0x0 + app 0x16000，314000B）
- 测试矩阵 +H10-H12（F460 首烧/本地升级/断电回滚，待真机）

### 待真机
- F460：merged_f460.hex 首烧 → H11 本地升级 → H12 断电/回滚（DAP-LINK 已具备）
## 2026-08-15：F460 本地升级链路（OTA1 帧通道）

### 完成
- F460 ota_frame（复用核心）+ ota_transport_uart（帧→QSPI 暂存→进度→完成触发升级）
- Lan2Uart UART0 接入 OTA1 magic 分支；user_main 调 ota_agent_boot
- 编译 0E/7W（Code=286196，ota 模块已链接）

### 待真机
- F460 首烧 + 本地升级/断电/回滚实测（DAP-LINK 已具备）
## 2026-08-15：F460 App 侧 ota 模块（flag/storage/state/agent）

### 完成
- F460 ota_flag/ota_storage/ota_state/ota_agent（QSPI 暂存+备份+进度+标志页状态机+Agent）
- 本地升级闭环：暂存→备份→NEED_COMMIT→复位→bootloader 覆盖写→自检→confirm
- 编译 0E/7W（原有 warning）；修复 uvprojx IncludeInBuild=0 排除条目

### 待接线
- 下载通道（网络/本地帧）+ user_main 接入 ota_agent_boot
## 2026-08-15：F460 Phase 5 主体（全新 OTA，不依赖旧工程）

### 完成
- F460 App 偏移 0x16000（Firmware.sct），编译 0E/7W
- F460 标志页 ota_flag.h/c（双标志+反码+boot count @0x7F000）
- F460 精简 Bootloader（LL 库重构）：引导+QSPI 覆盖写+备份恢复+boot count 回滚，0E/4W（Code=4856）
- 不保留网络升级（B1）；备份由 App 侧写 QSPI（bootloader 只读暂存+备份）

### 待真机
- F460 首烧 + 升级/断电/回滚实测（DAP-LINK 已具备）
## 2026-08-15：OTA 交付总览文档

### 新增
- `doc/OTA交付总览.md`：固件/协议/工具链/平台无关核心/文档/待办/验证基线全览（交付物 21/21 核对存在）
## 2026-08-15：OTA 密钥管理正式化

### 新增
- `tools/ota_keygen.py`：32B HMAC 密钥生成 / 显示 / C 头片段输出
- `ota_pack.py --key-file`：从密钥文件打包（优先于 --key）
- .gitignore 排除密钥文件（*.key / key_*.bin / ota_key.h）

### 验证
- 新密钥打包 → 同 key 验签通过；错误 key 验签失败（密钥生效）
## 2026-08-15：上位机批量设备管理（Phase 6 补充）

### 新增
- `ota_upgrade_tool.py devices`：CSV 设备清单批量 check（汇总 devices_report.csv）+ `--download` 逐台下载校验
- 自测：3 台（2 旧版+1 最新）→ 2/3 需升级，下载+校验通过
## 2026-08-15：F460 FlashDB/FAL 移植指南

### 新增
- `doc/F460_FlashDB移植指南.md`：文件清单（同源拷贝）+ fal_cfg 分区表模板 + fal_flash 驱动（w25qxx 映射骨架）+ flashdb.c 初始化 + fdb_cfg 配置 + ota_state 对接 + 验证路径
## 2026-08-15：single_bak_qspi 策略算法验证（F460 前置）

### 新增
- `tools/single_bak_sim.py`：F460 单 bank 备份-覆盖-恢复模拟（片内 512KB + QSPI 8MB）
- 5 场景 PASS：正常 / 暂存损坏 / 备份前断电 / 覆盖中断电恢复 / 覆盖后校验失败
- F460 Phase 5 移植直接按此实现
## 2026-08-15：网络升级端到端演示文档

### 新增
- `doc/OTA网络升级演示.md`：全链路操作手册（打包→版本库→服务器→上位机 check/download→设备端网络/串口触发→一键验收→真机清单 H1-H9→FAQ）
- 文档命令链已本机验证（list/check/download/断点续传/验收 9/9）
## 2026-08-15：帧协议核心平台无关化（ota_frame）

### 新增
- `app/inc/ota_frame.h` + `app/src/ota_frame.c`：OTA1 帧纯逻辑核心（CRC16/组帧/解析/流式状态机，零硬件依赖）

### 修改
- `ota_transport_uart.c` 重构复用 ota_frame（删除重复实现，对外 API 不变）；F460 可直接复用帧协议

### 验证
- App 编译 0E/0W（Code=361404）；ota_acceptance 回归 9/9 PASS
## 2026-08-15：安全层平台无关化（读回调抽象）

### 修改
- `ota_security`：新增 `ota_security_verify_ex(ota_sec_read_fn, void*)` 读回调接口（off 相对偏移）
- `ota_security_verify_staged()` 保留为 F4A0 便捷入口（内部 QSPI 回调）
- F460/RK MCU 移植可复用验签/SHA256 核心（仅换读回调）

### 验证
- App 编译 0E/0W（Code=361220）；ota_acceptance 回归 9/9 PASS
## 2026-08-15：一键构建/发布/验收（build_ota.py）

### 新增
- `tools/build_ota.py`：单命令完成 编译App+Boot(0E/0W) → hex → 体积监控 → 打包+版本库 → 合并烧录hex → 全链路验收(9/9)
- 实测全流程一次通过；--skip-build/--skip-acceptance 可选
## 2026-08-15：固件体积监控（验收标准 8）

### 新增
- `tools/fw_size_monitor.py`：解析 Keil .map → Total ROM Size，对比阈值
  - F4A0：App 区硬上限 896KB / 预警 761.6KB；退出码 0/1/2
  - 当前固件 379.9KB（剩余 516KB）；自测 3 场景（正常/预警/超限）PASS
## 2026-08-15：hex 合并烧录工具（真机联调配套）

### 新增
- `tools/merge_hex.py`：合并 Intel HEX（boot@0x0 + app@0x10000 → merged_f4a0.hex）
  - 自动检测地址重叠；>64KB 用 Extended Linear Address；--list 查看范围
- 验证：合并后与源文件字节级一致（boot 6276B + app 389000B，无丢失/多余）

### 修改
- `doc/OTA测试矩阵.md` H1 首烧改用 merge_hex 产物
## 2026-08-15：全链路验收脚本 + 测试矩阵

### 新增
- `tools/ota_acceptance.py`：一键验收（打包/版本库/服务器/上位机/设备端模拟/USB feed/RK Agent，9/9 PASS）
- `doc/OTA测试矩阵.md`：验收标准落地（软件 9 项自动 + 硬件 H1-H9 待真机）
## 2026-08-15：F460 Phase 5 勘察 + bootloader 编译修复

### 完成
- F460 App 基线编译通过（0E/7W，Code=283404）——F460 工具链可用
- 定位并修复 bootloader main 冲突：`hc32f46_driver.lib` 含 `main.o` → `armar -d` 剥离生成 `boot_driver.lib`

### 待用户确认（Phase 5 前提）
- F460 App 链接地址 / 片内分区（bootloader/App/标志区）
- 升级执行模型（bootloader 覆盖写 vs App IAP）
- bootloader 缺失依赖（RTX/json/堆等库路径）
## 2026-08-15：RK Agent（Phase 2，应用原子升级）

### 新增
- `tools/rk_ota_agent.py`：RK3506G/RK3566 应用升级 Agent
  - 统一语义：check → download(断点续传) → verify(CRC32/SHA256/HMAC) → 备份 → rename 原子替换 → systemd restart → 版本记录
  - 失败回滚（备份恢复）；坏包不触碰旧应用；只升应用不升系统

### 验证
- --selftest 4 项 PASS：包校验 / 原子替换+版本 / 回滚 / 坏包拒绝
## 2026-08-15：上位机升级工具（Phase 6 第一步）

### 新增
- `tools/ota_upgrade_tool.py`：统一升级 CLI
  - `list/add/remove`：版本库管理（add 自动校验 OTA 包 + 更新 latest，版本号规范化）
  - `check`：查询升级；`download`：HTTP+Range 断点续传 + CRC32/SHA256/HMAC 校验
  - `uart`：串口升级（OTA1 帧协议，复用 ota_send，进度显示 + 中断续传）

### 修改
- `ota_server.py`：new_version 显示规范化（8 位十六进制）

### 验证
- list/add/check/download 自测通过；断点续传（半文件→Range→字节一致）通过
## 2026-08-15：USB1(CDC) 本地 OTA 通道

### 新增
- `app/src/ota_usb.c` + `app/inc/ota_usb.h`：USB1(CDC) 轮询线程（read→流式 feed），ACK/RESUME 走 write(USB1)
- `ota_transport_uart_feed()`：流式组帧状态机（魔数同步/续接/重同步/非法长度防护），fd 参数化回发（与 UART0 整帧入口共存）

### 修改
- `ota_transport_uart.c`：整帧处理重构为 `handle_frame_on(fd,...)`（回发通道参数化）
- `user_main.c`：`ota_usb_start()` 启动 USB1 通道

### 验证
- 编译 0E/0W（Code=361176）
- feed 状态机 Python 复刻 6 项自测 PASS（整帧/逐字节/随机分块/半帧恢复/连续多帧/非法长度）
## 2026-08-15：OTA 安全层正式化（HMAC 验签 + 安全等级开关）

### 新增
- `app/inc/ota_security.h` + `app/src/ota_security.c`
  - `ota_security_verify_staged()`：HMAC-SHA256 验签（覆盖 `[0:50]+payload`）+ payload SHA256 完整性
  - 流式计算（512B 分块读 QSPI 暂存），无大 RAM 占用；密钥 `OTA_SEC_KEY` 内置
- 安全等级编译开关 `OTA_SECURITY_LEVEL`（1=网络+本地默认 / 2=仅本地强制验签 / 3=仅本地+强制签名）

### 修改
- `ota_agent.c`：`verify_staged_pkg` 接入验签（CRC32 + HMAC + SHA256）
- `tools/ota_pack.py`：修复 HMAC 覆盖范围 bug（原 [0:82]+payload → 协议 [0:50]+payload）

### 验证
- 打包→Python 重算 magic/crc32/sha256/hmac 全匹配（388666B 包）
- App 编译 0E/0W（Code=360756）
## 2026-08-15：OTA 服务端三接口（Phase 1）落地

### 新增
- `tools/ota_server.py`：OTA 服务器最小实现
  - `POST /ota/check`：版本比对 → upgrade/url/new_version/size/sha256/sign
  - `GET /ota/pkg?id=<file>`：固件下载 + **HTTP Range 断点续传**（206 + Content-Range）
  - `POST /ota/report`：升级结果落盘 reports.log
  - 版本库：`server_data/manifest.json` + `firmware/*.otapkg`（启动自动补 sha256）
  - 安全：id 参数 basename 防目录穿越
- `tools/ota_server_selftest.py`：三接口自测（6 项全 PASS）
- 版本库示例：`server_data/firmware/fw_01020000.otapkg`（0x10000 布局 App，387954B）

### 验证
- check 旧版→upgrade=true / 当前版→false
- pkg 完整下载 SHA256 一致；Range 从中间续传拼接 SHA 一致
- report 落盘；目录穿越 404

### 说明
- 设备端网络升级链路（`ota_download.c` HTTP+Range）与服务器已对齐：
  下载进度（`iap_progress`）作为 Range 起点，服务器返回 206 从断点续传




## 2026-06-28：流式 EPC 扫描器 + HTTP 白名单推送优化

### 背景

读写器作为 HTTP Server 接收白名单推送时，`httpSaveTagMethod()` 使用 `cJSON_Parse()` 解析完整 body。
当白名单标签数达 5 万条时 JSON body 约 2MB，远超 MCU 的 512KB SRAM，导致 OOM。

### 解决方案

流式扫描器 `StreamEpcScanner`——在 HTTP body 分块到达时就逐个字节扫描，
每识别出一条 EPC 立即入库，不等待 body 收完，不建 JSON 解析树。

**内存消耗与标签总数无关**，始终约 150 字节。

### 修改清单

#### 新建文件

| # | 文件 | 行数 | 说明 |
|---|------|------|------|
| 1 | `user/inc/stream_epc_scanner.h` | ~45 | 扫描器接口，声明 `StreamEpcScanner` 类型和 API |
| 2 | `user/src/stream_epc_scanner.c` | ~165 | 状态机实现。匹配 `{"method":"add/del","epc":["hex",...]}` |

#### 修改文件

| # | 文件 | 修改内容 |
|---|------|---------|
| 3 | `httpapi/APIHttpRequest.h` | 添加 `#include "stream_epc_scanner.h"`、成员 `m_scanner`、`getScanner()`、析构函数 |
| 4 | `httpapi/APIHttpRequest.cpp` | 构造中创建扫描器、析构释放；`OnBodyCallback` 喂数据给扫描器；`Parse` 中 reset；添加 `on_stream_epc` 回调（EPC→ADDlist/DELlist） |
| 5 | `httpapi/HttpModuleAPI.cpp` | 添加全局计数 `g_stream_tags_added`、`g_stream_tag_is_add`；`httpSaveTagMethod` 增加流式绕过分支 |
| 6 | `user/src/http_callback.c` | 从 hc32f46 备份恢复后做了 10 项兼容适配（见下） |
| 7 | `MDK/hc32f4a0_app.uvprojx` | 添加 `stream_epc_scanner.c` 到 user Group |

#### http_callback.c 兼容适配项（hc32f46→hc32f4a0）

| 问题 | 修复方式 |
|------|---------|
| `gpio_action.h` 不存在 | 删除 include |
| `json_remote_cmd` 返回 `int`，头文件声明 `void` | 改为 `void`，去掉所有 `return N` |
| `set_sys_date_by_milisec` 未定义 | 替换为 `gSysSecBase = getSysTick()/1000; gUtcSecBase = servertime/1000;` |
| `gTagInvSwitch` 未定义 | 替换为 `if (0)` + goto SEND_RESP |
| `valid_static_settings_byobj` 未定义 | 替换为 `validret = 0;` |
| `gIsBtRdrPreInvInit` 未定义 | 注释掉 |
| `pause_inv_th` / `resume_inv_th` 未定义 | 注释掉 |
| `remote_cmd_reply` 字段不存在 | 替换为 `if (0)` |
| `fire_EEcmdGpoSet` 未定义 | 末尾添加空函数体 `void fire_EEcmdGpoSet(void) {}` |
| `isreboot` 未定义 | 替换为 `0` |

### 编译结果

```
Program Size: Code=356412 RO-data=26136 RW-data=13288 ZI-data=89840
0 Error(s), 0 Warning(s)
```

### 数据流（改动后）

```
HTTP POST /tagdata {"method":"add","epc":["E3...F8","E3...96",...]}
  ↓
APIHttpRequest::Parse()
  ↓ read() → http_parser_execute()
    ↓
  OnUrlCallback()       → 记录 URL
  OnBodyCallback(chunk) → stream_epc_scanner_feed(scanner, chunk)
    ↓                        ↓ 逐字节扫描
   识别 "method":"add"     → is_add=1
   逐个提取 EPC hex 串      → on_stream_epc()
                              → tagtable_list_update(ADDlist/DELlist, epc, OPTION_ADD)
                              → g_stream_tags_added++
  OnMessageComplete()
    ↓
httpAPIDispatch → httpSaveTagMethod()
  → if (g_stream_tags_added > 0)   ← 跳过 cJSON_Parse!
      tag_method = g_stream_tag_is_add ? OPTION_ADD : OPTION_DEL
      FlashDB_Sync_flag = 11
      return {"code":1,"msg":"success"}
```

### 注意事项

1. **扫描器需要添加到 Keil 工程**——已在 uvprojx 中添加，如果重新生成工程需要手动加
2. `fire_EEcmdGpoSet` 是空 stub——在 hc32f4a0 中实际未使用（仅在 Custom_By_Caipan 等宏中调用，这些宏均为 0）
3. `http_callback.c` 中有部分 hc32f46 特有的 `set_temp_static_conf` / `pause_inventory` / `resume_inventory` 命令被禁用——如果 hc32f4a0 需要这些功能需重新实现
4. **备份路径**：`J:\codex_deepseek\workshop\HC32F4A020260320_RYK\hc32f4a0_app_bak_20260628_190809\`

### 废弃/清理（同一天尝试过但未采用）

| 文件 | 状态 | 原因 |
|------|------|------|
| `user/src/sram_alloc.c` | **保留但未接入工程** | 早期尝试统一 SRAM 池分配，后改为集中改造流式扫描器。代码完整可用，需用时在 Keil 中添加 |
| `user/inc/sram_alloc.h` | 同上 | 含 yyjson/cJSON 内存钩子 `sram_json_alc_new()` / `sram_cjson_set_hooks()` |
| `user/src/json_helper.c` | **已删除** | 空文件，已清理 |
| `user/inc/json_helper.h` | **已删除** | 空文件，已清理 |
| `user/src/http_stream_integration.c` | **已删除** | 早期的 `on_epc_from_body` 回调拆分文件，后合并到 `http_callback.c` 再清理到 `APIHttpRequest.cpp` |


## 2026-06-29：目录结构整理

### 变更

| 旧路径 | 新路径 |
|--------|--------|
| `user/src/*.c` | `app/src/*.c` |
| `user/inc/*.h` | `app/inc/*.h` |
| `user/inc/*.c`（死代码） | `backup/` |
| `httpapi/*.cpp` | `app/src/*.cpp` |
| `httpapi/*.h` | `app/inc/*.h` |
| `jsonlib/http_parser.*` | `middleware/http_parser.*` |

### 清理
- 删除 `sram_alloc.c` / `sram_alloc.h`（从未接入工程）
- 删除 `json_helper.c` / `json_helper.h`（空文件）
- 死代码 `https_run.c`、`main_dev.c`、`main_test.c`、`user_main_20230807.c`、`TagBuffer.c`（重复副本）、`http_upload.txt` → `backup/`

### 编译
0 Error(s), 5 Warning(s)。

Program Size: Code=356436 RO-data=26136 RW-data=13296 ZI-data=89832

### 备份
`hc32f4a0_app_bak2_20260629_113558/`
## 2026-08-14：芯片资源与系统时钟核查（HC32F4A0PITB）

### 背景

对项目所用芯片型号与时钟链路做全量核查，依据：
- 数据手册 `doc/DS_HC32F4A0系列数据手册_Rev1.50.pdf`（Rev1.50，2025-01）
- 工程代码：`hc32f4a0_driver/projects/user/src/main.c`、`projects/source/board.c`、
  `drivers/hc32_ll_driver/src/hc32_ll_clk.c`、`drivers/cmsis/Device/HDSC/hc32f4xx/Source/system_hc32f4a0.c`
- MDK 工程：`hc32f4a0_driver.uvprojx` / `hc32f4a0_app.uvprojx`、链接脚本 `HC32F4A0xI.sct`

### 芯片型号：HC32F4A0PITB（LQFP100，2MB，工业级）

命名规则（手册 1.1，逐位解码）：

| 位 | 含义 | HC32F4A0PITB |
|----|------|--------------|
| P | 引脚数 | **100Pin**（S=176 / R=144 / T=208） |
| I | Flash 容量 | **2MB**（G=1MB） |
| T | 封装类型 | **LQFP**（H=BGA） |
| B | 温度范围 | **-40~105℃ 工业级** |

核心资源（手册表 1-1 / 订购信息）：

| 项目 | 参数 |
|------|------|
| 内核 | Cortex-M4F（FPU+MPU+DSP），240MHz，300DMIPS / 825 CoreMark |
| Flash / OTP | 2MB dual-bank（支持 BGO）/ 134KB |
| SRAM | **512+4KB**：SRAM1(128K)+SRAM2(128K)+SRAM3(96K)+SRAM4(32K)+SRAMH(128K 高速 0 等待)+SRAMB(4K VBAT 保持) |
| GPIO | 83 个（其中 79 个 5V-tolerant） |
| 通信 | 10×UART(ISO7816-3)、6×SPI、6×I2C(SMBus)、4×I2S、2×SDIO、1×QSPI(240Mbps XIP)、2×USB(HS+FS OTG)、1×ETHMAC(MII/RMII)、CAN 2.0B、EXMC |
| 模拟 | 3×12bit ADC（LQFP100 引脚限制 16ch/单元）、4×12bit DAC、4×PGA、4×CMP、OTS |
| 定时器 | Timer0×2、Timer2×4、TimerA×12、Timer4×3、Timer6×8、HRPWM×16、RTC、WDT、SWDT |
| 安全/加速 | AES256、HASH(SHA256/HMAC)、TRNG、DCU×8、FMAC×4、MAU(Sin/Sqrt)、CRC、DVP、FCM |

工程侧确认：
- 链接脚本 `HC32F4A0xI.sct`：IROM 0x00000000 2MB / IRAM 0x1FFE0000 512KB / 备份 SRAM 0x200F0000 4KB
- 应用、驱动两工程均使用 `HC32F4A0xI.sct`，与 PITB 的 2MB Flash 对应

### 系统时钟：实际运行 240MHz（重要纠正）

**结论：系统主频 = 240MHz**（PLLH：8MHz XTAL × 120 ÷ 4 = 240MHz，HCLK Div1）。

证据链：
1. `main.c` 时钟初始化唯一入口是 `BSP_CLK_Init()`；`sysinit.c` 的 168MHz 配置（`sysinit()`）**全工程无调用，属死代码**
2. `board.c` 的 `__WEAKDEF BSP_CLK_Init()` 位于 #if 块外、无条件编译：
   PLLM=1、PLLN=120、PLLP=4、PLLSRC=XTAL，`CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL)`，
   注释 "VCO = (8/1)*120 = 960MHz" → 960/4 = **240MHz**；HCLK/PCLK0=240M，PCLK1/4=120M，PCLK2/3=60M，EXCLK=120M
3. 驱动库公式（`hc32_ll_clk.c` 断言 / `system_hc32f4a0.c` SystemCoreClockUpdate，CKSWR=0x05 分支）：
   `SystemCoreClock = XTAL_VALUE/(PLLM+1) × (PLLN+1) / (PLLP+1) = 8M/1 × 120 / 4 = 240,000,000`
   `XTAL_VALUE = 8000000UL`（system_hc32f4a0.h，RTE/Device/HC32F4A0PITB/ 下同）
4. 配套时序吻合 240MHz：EFM 5 wait、GPIO RD 4 cycles（>200MHz）、SRAMH 0 wait
5. 运行时验证：读 `SystemCoreClock`（=240000000）或 `CLK_GetBusClockFreq(CLK_BUS_CLK_HCLK)`

### 历史误判纠正

| 来源 | 数值 | 实际地位 |
|------|------|---------|
| `sysinit.c`：8M/1×42/2 | 168MHz | **死代码**——`sysinit()` 全工程无调用（F460 DDL 风格残留，用 CLK_MpllConfig） |
| `usb_lib/bsp/ev_hc32f460_lqfp100_v2.c`：8M/1×50/2 | 200MHz | **不在 MDK 工程**（usb_lib 下未加入 uvprojx） |
| uvprojx 中 `CLOCK(12000000)` | — | 仅 Keil 调试器/仿真器晶振假设，与运行时无关 |

注意：`BSP_EV_HC32F4A0_LQFP176 = 0` 表示**选中** LQFP176 评估板（#if 为真），board.c 板级 BSP 代码正常编译；
项目实际量产芯片为 HC32F4A0PITB（LQFP100），GPIO 由自定义 `sysinitGPIO_Configuration()` 配置。


## 2026-08-14：工程接入审计（死代码 / 未接线模块盘点）

### 方法

解析两个 MDK 工程的 <FilePath> 清单，与磁盘上的全部 .c/.cpp/.s 源文件对比，
找出"磁盘存在但未被任一工程编译"的文件，并核查关键引用关系。

- driver 工程 hc32f4a0_driver.uvprojx：106 个文件，全部在磁盘
- app 工程 hc32f4a0_app.uvprojx：66 个文件，全部在磁盘
- 磁盘未引用源文件：290 个（绝大多数为官方 SDK 完整源码：midwares/USB/SDIO/STL、bsp components、
  未启用外设的 hc32_ll_*.c、FlashDB 官方 demo/tests/sfud 等，属"保留未接入"）

### 核查结论（关键引用关系）

1. GPIO_Configuration() 实际定义在 driver/projects/user/src/common.c:152（在工程内）。
   user/src/hwport.c（未接入）中的同名函数已被注释；sysinit.c 的 sysinitGPIO_Configuration() 同样未接入。
2. sysinit.c 未接入工程 → 其 168MHz 时钟配置与 168MHz 注释均为死代码（佐证 240MHz 结论）。
3. EASConfigHandler.cpp（app/src，23.7KB）无任何外部引用——全工程仅自身与头文件出现，
   未 include、未调用。EAS 配置实际由 HttpModuleAPI::httpEascfg() 实现（HttpModuleAPI.cpp 内）。两套实现并存，前者未接线。
4. FlashDB 存储链路自洽：fal_flash_sfud_port.c（在工程）中 sfud 相关代码全部被注释，
   read/write/erase 实际调用 QSPI_FLASH_Read/Write/EraseSector（来自预编译库 driver_lib/hc32f4a_driver.lib）。
   因此 sfud.c 不在工程不影响链接（假依赖，仅遗留 #include <sfud.h>）。
   分区表（flashDB/include/fal_cfg.h）：fdb_kvdb1(0~2MB) + fdb_tsdb2(2~4MB) + fdb_tsdb1(4~16MB)。

### 项目自有死代码清单（非 SDK 源码，可清理候选）

#### driver 工程（磁盘有、工程未编译）

| 文件 | 大小 | 说明 |
|------|------|------|
| user/src/sysinit.c | 5.3KB | 168MHz 时钟死代码（sysinit() 无调用） |
| user/src/hwport.c | 3.1KB | GPIO_Configuration 已注释；实际在 common.c |
| user/src/adc.c | 9.8KB | 未接入 |
| user/src/flash.c | 8.3KB | 与 source/flash.c（在工程）重复/替代 |
| user/src/flash_op.c | 24.8KB | 未接入的 Flash 操作 |
| user/src/hpm6340.c | 8.7KB | 未接入的 SPI 读卡 |
| user/src/qspi_flash.c | 13.2KB | 与 source/qspi_flash.c（在工程）重复 |
| user/src/Retarget.c | 0.5KB | 与 source/Retarget.c（在工程）重复 |
| source/usart.c | 6.3KB | 与 user/usart_driver.c（在工程）重复 |
| projects/jsonlib/*（5 文件） | ~460KB | driver 侧 json 库副本（app 用自己那份） |
| MDK/startup_hc32f4a0.s | 21.8KB | 与 cmsis startup（在工程，21.5KB）版本不同 |
| MDK/RTE/Device/HC32F4A0PITB/*、RTE/MicroBoot/* | — | RTE 生成物未用 |
| projects/usb_lib/ 大部分 | — | 工程仅编译 device_class 中 4 个 + device_core + 部分 examp |

#### app 工程（磁盘有、工程未编译）

| 文件 | 大小 | 说明 |
|------|------|------|
| app/src/EASConfigHandler.cpp | 23.7KB | 无任何引用，独立死代码（功能由 httpEascfg 实现） |
| app/src/hpm6340.c | 12.4KB | 未接入 SPI 读卡 |
| app/src/w25qxx.c | 17.9KB | 未接入（外部 Flash 走 QSPI_FLASH_* 预编译库） |
| projects/backup/*（6 文件） | ~90KB | 已知死代码（2026-06-29 归档） |
| projects/httpServer/*（5 文件） | ~40KB | 已知死代码 |
| projects/usb_lib/、midwares/hc32/usb|sdioc|iap|stl | — | app 未接入 |
| flashDB 的 demos/tests/sfud | — | 官方示例；sfud 为假依赖（代码已注释） |

### 备注

- 本轮仅审计、未删除任何文件；清理前需先建立编译基线（Keil 命令行编译验证）。
- hc32f4a_driver.lib（1.29MB）为预编译库，app 通过它获得 QSPI/串口等驱动；
  driver 工程源码与其是否同步需进一步核对（建议将 driver 工程作为库工程管理或统一源码构建）。

## 2026-08-14：编译基线建立 + 固件资源分析 + 架构修正

### 编译基线（Keil UV4 命令行）

工具：D:\Keil_v5\UV4\UV4.exe（Compiler V5.06 update 7 / ARMCC）

| 工程 | 命令 | 结果 | 产物 |
|------|------|------|------|
| hc32f4a0_app | UV4 -r (全量 rebuild) | 0 Error, 5 Warning | output/firmware.axf + rfidapp.hex (1.06MB) |
| hc32f4a0_driver | UV4 -b | 0 Error, 0 Warning | output/hc32f4a_driver.lib (1.29MB) |

app 全量 rebuild 尺寸与 2026-06-29 记录完全一致：
Program Size: Code=356436 RO-data=26136 RW-data=13296 ZI-data=89832
→ 当前磁盘源码与 CHANGELOG 历史编译状态吻合，基线可靠。

### 架构修正：driver 工程 = 平台库，app 工程 = 业务（单固件链接模型）

固件 map（firmware.map）符号证实：

| 符号 | 来源 | 说明 |
|------|------|------|
| main @0x343fd (140B) | hc32f4a_driver.lib(main.o) | **固件入口在驱动库中**（app 工程无 main.c） |
| init_thread @0x30aa5 | main.o | 创建 user_main 线程、DHCP、固件升级线程 |
| dhcp_func @0x24e3d | main.o | DHCP 线程 |
| BSP_CLK_Init @0x7151 (196B) | board.o | **240MHz 时钟初始化已编译进固件** |
| user_main / user_main_active / user_main_passive | user_main.o（app） | 业务入口，由 init_thread 创建 |
| OpenReader @0xc8a1 (3444B) | reader_init.o（app） | 读卡器初始化 |
| QSPI_FLASH_Init @0xda31 | qspi_flash.o（库） | 外部 Flash 驱动 |
| SystemCoreClock @0x1FFE0000 | system_hc32f4a0.o | 运行时主频变量（240000000） |

**结论**：driver 工程以"创建库"模式编译（无独立固件），其 main.c + board.c + 平台驱动打入
hc32f4a_driver.lib；app 工程链接该库并提供 user_main 业务，最终链接成单一固件 firmware.axf。
修改 driver 源码后必须重编 driver 工程（After Build 自动 xcopy lib/h 到 driver_lib/），再编 app。

### 固件资源占用（基于 firmware.map / rebuild 尺寸）

| 区域 | 占用 | 上限 | 使用率 |
|------|------|------|--------|
| Flash（LR_IROM1 0x00000000） | 0x60A5C ≈ 387KB | 2MB | **18.9%** |
| SRAM 静态（RW_IRAM1 0x1FFE0008） | 0x182D0 ≈ 97KB（含 64KB 主栈 STACK） | 512KB | ~19% |
| 备份 SRAM（RW_IRAMB 0x200F0000） | 0x1000 = 4KB（ipc.o tagtmpbuf） | 4KB | 100% |
| 堆（main.c _init_alloc） | 0x20000000~0x20060000 = 384KB | — | 动态 |

注：ZI=89832B 中不含动态堆；RTX 线程栈/控制块在 .bss.os（rtx_lib.o 0x2000+），主栈 64KB。
SRAM4（32KB）未启用（sysinit 未接入、board.c SRAM_Init 默认参数不含 Sram4Idx）。

### 备注

- 死代码清理、SRAM4 启用等改动可在本基线之上进行，用 UV4 -r + 日志比对回归。
- 编译日志存档：doc/_build_app_rebuild.log、doc/_build_driver_baseline.log。


## 2026-08-14：问题清单落盘（ISSUES.md）

核查发现的问题/bug 已整理至根目录 `ISSUES.md`（含：潜在风险 R1-R5、死代码 D1-D3、待验证 T1-T7、编译基线速查、SRAM 布局修正）。
重点：堆覆盖 SRAM4/SRAMH 的时钟门控与 ECC 未显式配置（R1/R2）、堆起点硬编码浪费 31.5KB 高速 SRAM（R3）、
MQTTClient.c:146 条件赋值歧义（R5）、5 个编译 Warning（json-parser×4 + MQTTClient×1）。


## 2026-08-14：编译 Warning 清零（5→0）

### 修改
| 文件 | 位置 | 修改 |
|------|------|------|
| projects/jsonlib/json-parser.c | 125/140/153/169 | 拆分"赋值+判空"（#1293-D assignment in condition），行为不变 |
| projects/MQTT/MQTTClient.c | 146 | 条件中赋值拆分为独立语句 + 显式比较（原 Paho 写法 rc=(读!=rem_len) 语义保留） |

### 验证
- app 工程 UV4 增量编译：**0 Error, 0 Warning**（原 5 Warning）
- Program Size 与基线完全一致：Code=356436 RO-data=26136 RW-data=13296 ZI-data=89832
  → 纯风格修改，二进制行为不变
- 对应 ISSUES.md 条目 R5 / T3 已标记完成


## 2026-08-14：堆起点动态化（R3，+31.25KB SRAMH 高速堆）

### 修改
- 位置：hc32f4a0_driver/projects/user/src/main.c（main()）
- 内容：heap_base 由硬编码 0x20000000 改为 &Image\$\$RW_IRAM1\$\$ZI\$\$Limit（动态跟随静态区末尾），保留 64 字节对齐；heap_top 保持 0x20060000
- 效果：堆起点 0x1FFF82D8→对齐 0x1FFF8300，堆大小 384KB → ≈415KB（+31.25KB SRAMH 高速 0 等待区）

### 验证
- driver 工程全量重建：0E/0W（lib 自动同步 driver_lib/）
- app 工程全量重建：0E/0W
- 尺寸变化：Code 356436→356456(+20，动态取址指令)，RO-data 26136→26132(-4)，RW/ZI 不变 → 静态布局无影响
- map 确认：Image\$\$RW_IRAM1\$\$ZI\$\$Limit = 0x1FFF82D8
- 待真机验证：malloc 至 SRAMH 区（0x1FFF8300~0x20000000）读写正常


## 2026-08-14：SRAM 时钟门控显式使能（R1 稳定性加固）

### 修改
- 位置：hc32f4a0_driver/projects/source/board.c（BSP_CLK_Init，SRAM_Init() 之前）
- 内容：新增 FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_SRAMH|SRAM1|SRAM2|SRAM3|SRAM4|SRAMB, ENABLE)
- 目的：显式使能全部 SRAM 分区时钟门控，消除"低功耗代码改动 FCG0 后堆访问 HardFault"的隐患（堆 0x20000000~0x20060000 覆盖 SRAM1-4）

### 验证
- driver 全量重建：0E/0W（lib 自动同步）
- app 全量重建：0E/0W
- 尺寸：Code 356456→356464(+8B)，RO/RW/ZI 不变 → 静态布局无影响


## 2026-08-14：系统框架文档（README.md）

新建根目录 README.md，固化系统框架认知：芯片资源速查、目录结构、双工程单固件模型（driver=平台库+main，app=业务）、
线程模型、标签数据流、存储布局、编译/烧录指南、配置开关、文档索引与注意事项。


## 2026-08-14：释放备份 SRAM 4KB（R2 落实）

### 背景
备份 SRAM（0x200F0000，4KB，VBAT 掉电保持）被 tagtmpbuf[0x1000] 占用，但该缓冲为死代码：
仅 httpbuf_init() 中 memset 清零，无任何实际读写（HttpModuleAPI.cpp 中同名变量为局部 char[32] 遮蔽全局）。

### 修改
| 文件 | 修改 |
|------|------|
| app/src/ipc.c | 删除 tagtmpbuf 定义（at(0x200F0000)）与 memset 调用 |
| app/src/HttpModuleAPI.cpp | 删除 tagtmpbuf extern 声明 |
| projects/MDK/config/linker/HC32F4A0xI.sct | 删除 RW_IRAMB 区域（0x200F0000 4KB） |

### 验证
- app 全量重建：0E/0W
- 尺寸：Code 356464→356452(-12B)、RO 26132→26120(-12B)、**ZI 89832→85736(-4096B)**
- map：RW_IRAMB 区域已移除 → 备份 SRAM 4KB 完全空闲（可留未来掉电保持功能）


## 2026-08-14：死代码归档清理（D1/D2）

### 操作
将 13 个"项目自有、未接入工程"的死文件移入根目录 _archive_deadcode/（含 MANIFEST.txt 记录原路径）：
- driver 侧 9 个：user/src/{sysinit.c(168MHz 死代码), hwport.c, adc.c, flash.c, flash_op.c, hpm6340.c, qspi_flash.c, Retarget.c}、source/usart.c
- app 侧 4 个：app/src/{EASConfigHandler.cpp(零引用), hpm6340.c, w25qxx.c}、app/inc/EASConfigHandler.h

### 验证
- 移动前精确路径核对：13 个文件均不在任何 uvprojx（排除同名误报）
- driver 全量重建 0E/0W、app 全量重建 0E/0W
- 尺寸与清理前完全一致：Code=356452 RO=26120 RW=13296 ZI=85736 → 零回归

### 说明
- EAS 配置功能不受影响（由 HttpModuleAPI::httpEascfg 实现）；如需启用 EASConfigHandler 可从归档恢复
- driver 的 jsonlib 副本、MDK/RTE 生成物、usb_lib/midwares SDK 目录本轮未动（保留待后续决策）


## 2026-08-14：主栈 64KB→32KB（R4，堆增至 ~457KB）

### 修改
- 位置：hc32f4a0_app/projects/cmsis/Device/HDSC/hc32f4xx/Source/ARM/startup_hc32f4a0.s
- 内容：Stack_Size EQU 0x10000 → 0x8000（64KB→32KB）。RTOS 线程栈由 rtx_lib.c 单独分配，主栈仅用于 main() 初始化与 ISR（MSP）

### 验证
- app 全量重建：0E/0W
- map：STACK 0x10000→0x8000；ZI 85736→52968(-32KB)；ZI Limit 0x1FFF82D8→0x1FFF02D8
- 堆（R3 起跟随 ZI Limit）：0x1FFF0300~0x20060000 ≈ 457KB（+32KB）
- 待真机验证：中断嵌套深度无栈溢出（ISSUES T 项）；必要时可再缩至 16KB


## 2026-08-14：FlashDB 示例调用污染修复 + 存储分析（F1/F2）

### 发现
flashdb()（每次启动调用，ipc.c tagfiltbuff_init → flashdb()）里调用了 FlashDB 官方示例 kvdb_basic_sample(&AlarmDB)，
向生产 KVDB(fdb_kvdb1) 写入示例键 boot_count(+1) / boot_time(local_Rtc+=100 假时间)——污染生产配置区 + Flash 写损耗 + 启动耗时。

### 修复
- flashdb.c：移除 kvdb_basic_sample(&AlarmDB) 调用与 extern 声明
- app 全量重建 0E/0W；链接器移除 kvdb_basic_sample.o(-692B)，Code 356452→355088(-1364B)
- 业务函数（save_tag_flashDB/save_tag_TSDB/Match_EPC_inTSDB 等，混在 sample 文件内）确认保留

### 存储分析（F2）
- 分区：kvdb1(0-2M) + tsdb2(2-4M) + tsdb1(4-16M)，16MB 外部 QSPI Flash → 当前合理，无需调整
- 白名单 TSDB 容量余量约 20 倍（5 万标签 ≈1.6MB / 12MB）
- 注意：sample 文件混有业务函数，文件不可删除；仅示例调用是污染源（已移除）


## 2026-08-14：jsonlib 依赖修正 + 累计优化成果汇总

### 修正
- 此前将 driver/projects/jsonlib 判定为"死副本"不准确：readercfg.c(在工程) 大量调用 json_parse/json_getint/json_getobject/json_getstring_len，
  IncludePath 显式含 ..\jsonlib；.c 未编译，函数实现由 app 工程的 json-parser.o 提供（map 确认 readercfg.o refers to json-parser.o）。
  → jsonlib 目录必须保留（头文件依赖），相关 ISSUES/CHANGELOG 描述已修正。

### 累计优化成果（Round 1-11，均编译验证）
| 项 | 内容 | 效果 |
|----|------|------|
| 编译基线 | UV4 命令行全量重建 | driver 0E/0W、app 0E/0W，回归可复现 |
| Warning 清零 | json-parser.c×4 + MQTTClient.c×1 | 5→0，产物尺寸不变 |
| R3 堆动态化 | main.c heap_base 跟随 ZI Limit | 堆 384→415KB（+31KB SRAMH） |
| R4 主栈 | startup Stack_Size 64K→32K | 堆再 +32KB（→457KB），ZI -32KB |
| R1 SRAM 时钟 | BSP_CLK_Init 显式使能 FCG0 SRAM | 防御性加固 |
| R2 备份 SRAM | 删除 tagtmpbuf 死缓冲 + sct RW_IRAMB | ZI -4KB，备份 SRAM 4KB 释放 |
| D1/D2 死代码 | 13 文件归档 _archive_deadcode/ | 仓库瘦身，零回归 |
| F1 数据污染 | 移除 kvdb_basic_sample 示例调用 | 生产 KVDB 不再被污染，Code -1364B |
| README | 系统框架文档 | 双工程单固件模型等固化 |
| 架构核查 | driver=平台库(app 无 main.c)、240MHz、SRAM 布局 | 认知修正并固化 |


## 2026-08-14：HTTP API 核查 + 流式白名单落盘修复（F3）

### HTTP API 核查
- 端点清单（17 个）：/moduleapi/{paramset,paramget,startasyncinventory,stopasyncinventory,getasynctags,syncinventory,
  readtagbank,writetagbank,writetagepc,locktag,killtag,getgpi,setgpo,reboot,resetrfidmodule,eascfg} + /tagdata
- 路由用 strcmp 精确匹配，未知路径返回 404；url+11 偏移写（ReqType 提取）在 m_url[50] 内安全（无越界）
- 记录：HttpApiUrl_method 为空串（空路径也会走 tagdata 处理）；url+11 为 magic number（可优化）

### F3 修复（真实功能 bug）
- 流式白名单分支（g_stream_tags_added>0）漏置 FlashDB_Sync_flag → 白名单不落盘（重启丢失）
- 修复：补 FlashDB_Sync_flag=11；app 0E/0W，Code 355088→355096(+8B)
- 待真机验证：流式推送后重启白名单保留

### F4 防御性建议（记录未改）
- method 缺失 → strcmp(NULL) 崩溃风险；method 非 add/del → plist=NULL 崩溃风险；EPC 长度截断


## 2026-08-14：F4 防御性修复（httpSaveTagMethod 非流式分支）

### 修改（app/src/HttpModuleAPI.cpp）
- method 字段缺失：新增 name==NULL 检查，返回 "method missing"（HMApiErr_Param_Missing），消除 strcmp(NULL) 崩溃
- method 非 add/del：新增 else 分支返回 "method not supported"（HMApiErr_Param_Err），消除 plist=NULL → tagtable_list_update(NULL) 崩溃

### 验证
- app 全量重建：0E/0W，Code 355096→355272(+176B)
- 遗留 F4b（记录未改）：EPC 数组元素 valuestring NULL、EPC hex 长度 0xFF 截断


## 2026-08-14：F4b 修复（EPC 数组防御，HTTP API 收尾）

### 修改（app/src/HttpModuleAPI.cpp httpSaveTagMethod 循环）
- EPC 数组元素 NULL（非字符串类型）→ 跳过该条（防 strlen(NULL) 崩溃）
- EPC hex 长度非法（0 或 > EPCIDMAXLEN=16）→ 跳过该条
- app 全量重建：0E/0W，Code +24B


## 2026-08-14：Modbus 协议核查（M1）

- 寄存器映射表 20 段（0x0000~0x0B00）地址布局无重叠、处理器完整
- modbus_func 分发健全：NULL handler/未知寄存器/CRC/非法功能码均有异常码防护（0x01/0x02/0x04）
- 支持 RTU(UART2/3) 与 TCP(MBAP) 双模式；写操作前 valid_regval 校验；mberr==10 特殊重启
- 结论：未发现崩溃/越界风险，实现质量良好


## 2026-08-14：MQTT 核查（M2）+ 真机验证清单（V1-V11）

### MQTT 核查结论
- mqtt_task：连接/重连（失败计数+reset_uart1_ex_dev 兜底）、条件订阅（sub_u/sub_b 去重）、Yield 保活、Publish 上报
- 错误路径统一 reinit_mbedtls + 重连；TLS 支持完整
- 小观察点（低风险，未改）：Publish 前 SBuffer[dlen]=0，dlen==缓冲大小时越界 1 字节
- MQTTClient.c:146（R5 修复）语义确认正确

### 真机验证清单
ISSUES.md 新增 V1-V11 硬件实测清单（主频/堆读写/中断栈/白名单持久化/异常输入/引脚覆盖等）


## 2026-08-14：自定义命令核查（C1）+ D3 评估 + ISSUES.md 重建

### C1 观察（未改）
- IOS/SIO/透传命令长度字段（recvbuf[5]*2 等）无上限校验，异常输入可能越界 recvbuf[255]（低-中风险，建议加校验）

### D3 评估（不建议直接归档）
- SDK 保留代码约 4.6MB；driver usb_lib 部分在工程、midwares 可能未来用 → 保持现状
- 若瘦身：优先 app usb_lib 与 flashDB demos/tests；移动前逐个核实 uvprojx 引用

### ISSUES.md 重建（重要）
- 修复：此前写附录时误用 limit=2 的 read 结果覆盖文件，导致 ISSUES.md 截断为 29 行
- 已按会话完整记录重建为 157 行（R1-R5/F1-F4/M1-M2/C1/D1-D3/T1-T7/SRAM 布局/V1-V11/状态）


## 2026-08-14：上报协议核查（R7）+ README 一致性更新

### R7 核查结论
- 上报帧格式（AddMsgHeader2SockBuffer）：0xff+namelen+预留+mtype+0x00+4B错误码(100000偏移)+设备名
- send_evt_* 系列完整；HTTP/MQTT JSON、TCP/UART 二进制；CRC 与 ACK 支持
- 低风险观察：设备名长度无边界校验（正常 <20B）

### README 更新
- 片内 SRAM 布局更新至最新（主栈 32K、堆 457KB、备份 SRAM 空闲）
- 头部注明最后更新 2026-08-14 与已修复清单


## 2026-08-14：最终编译验证 + ISSUES.md 二次重建

### 最终验证（18/20 轮）
- driver 全量重建：0E/0W（lib 自动同步）
- app 全量重建：0E/0W（0 Warning）
- 固件尺寸：Code=355296 RO-data=26116 RW-data=13288 ZI-data=52976
- 归档验证：_archive_deadcode/MANIFEST.txt 13 项全部存在

### ISSUES.md 二次重建（教训记录）
- 问题：R17 追加 R7 时再次误用 limit=2 的 read 结果做写入 base，文件被截断为 8 行
- 处理：已按会话完整记录重建为 164 行（R1-R5/F1-F4/M1-M2/C1/R7/D1-D3/T1-T7/SRAM/V1-V11/状态）
- 教训：后续修改 ISSUES.md 必须用 edit 工具（先全量 read）或读取完整内容拼接，禁止用限行 read 结果做 write base


---

# 2026-08-14 开发会话总结（18 轮成果总览）

## 一、发现并修复的 Bug（4 个功能级 + 2 个防御级）

| Bug | 问题 | 影响 | 修复 |
|-----|------|------|------|
| F1 | flashdb() 调用官方示例 kvdb_basic_sample | 生产 KVDB 被示例数据污染 + Flash 损耗 | 移除调用，Code -1364B |
| F3 | 流式白名单漏置 FlashDB_Sync_flag | 白名单不落盘，重启丢失 | 补 FlashDB_Sync_flag=11 |
| F4 | method 缺失/非法 → strcmp(NULL)/plist=NULL 崩溃 | HTTP 异常输入崩溃 | 加 name 判空 + else 分支 |
| F4b | EPC 数组元素 NULL/长度非法 | strlen(NULL) 崩溃 | 判空跳过 + 长度校验 |
| R5 | MQTTClient.c:146 条件赋值歧义 | 编译 Warning + 可读性 | 拆分条件（语义保留） |
| T3 | json-parser.c×4 + MQTTClient×1 Warning | 5 个编译 Warning | 全部清零（产物不变） |

## 二、内存/存储优化（编译验证，合计释放/扩充）

| 项 | 内容 | 效果 |
|----|------|------|
| R3 | 堆起点动态化（跟随 ZI Limit） | 堆 384→415KB（+31KB SRAMH 高速区） |
| R4 | 主栈 64KB→32KB | 堆 →457KB（累计 +73KB），ZI -32KB |
| R2 | 删除 tagtmpbuf 死缓冲 | 备份 SRAM 4KB 释放（ZI -4KB） |
| R1 | SRAM 时钟门控显式使能 | 防御性加固（防低功耗改动后 HardFault） |
| 基线 | 编译 Warning 5→0 | 产物尺寸与基线一致 |

## 三、协议核查（5 个协议层，全部完成）

| 协议 | 结论 |
|------|------|
| HTTP API（17 端点） | 路由/缓冲安全；F3/F4/F4b 修复 |
| Modbus RTU/TCP（20 段寄存器） | 实现健全，无 bug |
| MQTT（连接/订阅/发布/TLS） | 实现健全；1 个低风险观察 |
| 自定义命令（ASCII 命令集） | 1 个长度校验观察（C1） |
| 上报协议（send_evt_* 帧） | 实现健全；1 个名称边界观察（R7） |

## 四、工程治理

- 编译基线建立（UV4 命令行可复现回归）：driver 0E/0W、app 0E/0W
- 架构认知修正：driver=平台库（main/init_thread/BSP_CLK_Init 240MHz），app=业务（user_main），单固件链接模型
- 芯片资源核实：HC32F4A0PITB = LQFP100 / 240MHz / 2MB Flash / 516KB SRAM（数据手册 Rev1.50）
- 死代码归档：13 文件 → _archive_deadcode/（MANIFEST 可恢复）；jsonlib 依赖误判修正
- 文档：README.md（框架）、ISSUES.md（问题+真机验证清单 V1-V11）、CHANGELOG.md（18 节记录）

## 五、剩余待办（需人工决策或硬件验证）

1. 真机验证清单 V1-V11（主频/堆读写/中断栈/白名单持久化/异常输入/引脚覆盖等）
2. 边界校验建议：C1（命令长度）、R7（设备名长度）—— 可选加固
3. D3：SDK 目录瘦身（4.6MB，可选）
4. T4-T6：FPU 精度确认、LQFP100 引脚核对、EASConfigHandler 接线决策

## 六、最终代码状态

- 编译：driver 0E/0W、app 0E/0W（全量重建验证）
- 尺寸：Code=355296 RO-data=26116 RW-data=13288 ZI-data=52976
- 固件：hc32f4a0_app/projects/MDK/output/firmware.axf + rfidapp.hex


## 2026-08-14：统一 OTA Phase 0/3 起点（接口定稿 + F4A0 骨架落地）

### Phase 0：接口头文件（doc/ota_interfaces/）
ota_pkg/ota_transport/ota_storage/ota_state/ota_security 五件套（4 平台统一，未接入编译）

### Phase 3 起点：F4A0 ota_state + ota_storage 落地
- 新增 hc32f4a0_app/projects/app/{src,inc}/ota_state.{c,h}：升级状态机 + FlashDB KV（AlarmDB）持久化，掉电安全
- 新增 ota_storage.{c,h}：QSPI 暂存（QSPI_FLASH_*）+ 片内 dual-bank commit（EFM_Program→Bank B + EFM_SwapCmd）
- 已接入 hc32f4a0_app.uvprojx，编译 0E/0W
- 确认：HC32F4A0 硬件引导交换原生支持（EFM_SwapCmd），dual-bank A/B 可行

### 说明
- ota 模块当前未被业务调用（链接按需包含，固件尺寸不变）——Phase 3 下载 Agent 接入后生效


## 2026-08-14：下载 Agent 骨架接入（Phase 3，编译 0E/0W）

### 新增
- ota_download.h/c（app/{inc,src}）：HTTP GET + Range 断点续传 → ota_storage_write_stage（QSPI 暂存）
  - 复用现有 HTTP 基础设施（CheckServerConnection/mbedtls/http_parser/url_get_domain）
  - 独立 http_parser 实例（不影响 http_callback 上报解析）
  - on_body → 写 QSPI 暂存 + ota_set_progress（断点续传进度）
  - 从 ota_get_progress() 续传（Range: bytes=N-）
- 已接入 hc32f4a0_app.uvprojx user group，全量重建 0E/0W

### 修复
- uvprojx XML 结构（此前 ota 文件插入破坏嵌套，已修复为规范 File 块，File/FileName 标签 69=69）

### 现状
- ota 三模块（state/storage/download）均编译通过、.o 齐全；未被业务调用（链接按需包含，固件尺寸不变）
- 待接线：调用 ota_download_start(url) 的触发点（HTTP 命令/定时检查）+ 下载完成后的验签/commit 流程



## 2026-08-18：H2 网络下载验证（设备出站 TCP 已打通）

### 关键进展（真机验证）
- **W5100S 驱动库**：app 链接 driver_lib/hc32f4a_driver.lib（预编译库，源自 hc32f4a0_driver 工程；MD5 与 output 一致，同步正常）。本次调试未改驱动源码，无需重编 driver 工程；若改则需编译+复制 lib 两步
- **PHYLINK=0 澄清**：W5100S SPI 模式 PHYCFGR 不可读（读恒 0），非真实链路状态，网络实际正常
- **socket() 成功返回 sn**（rv>=0 判断正确）；TMSR/RMSR=55/55（每 socket 2KB 默认）
- **诊断固件 connect 9999 成功**：rv=1(SOCK_OK, 704ms)、Sn_SR=17(ESTABLISHED)、DHAR=D8:9E:F3:72:FB:D2(ARP 成功=PC MAC)；此前 rv=-13 为偶发
- **PC 9999 服务器实收**：CONNECTION from 192.168.1.250:49152 + "GET /ota/check HTTP/1.1"（设备 connect+send 全通）
- **v3 固件 connect 8080 失败**：rv=-4(SOCKERR_SOCKCLOSED, Sn_SR=00)，收到 RST 症状；PC 8080 监听正常自连 True；根因待 v4 定案
- **ota_server 路由**：/ota/check 为 POST（GET 404）；manifest platform 2 / app_id 0 / latest 0x01020000 / fw_acc.otapkg

### 待办（下次继续，第一步）
1. 烧 tools/merged_f4a0_v4.bin（0x0 起，456364B）→ 复位→ 只贴 T1:/T2: 开头行
2. 判读：T1(9999) OK / T2(8080) FAIL → 8080 服务器侧；都 FAIL → 设备侧偶发/竞争；都 OK → 直接看 POST /ota/check 响应
3. 打通后：GET /ota/pkg?id=fw_acc.otapkg 下载 → QSPI 0xE00000 暂存 → 校验
4. 清理 user_main.c 临时测试块（PHASE2 双端口 + PHASE1 QSPI 自测）并评估提交

---


## 2026-08-18 H2 网络升级链路打通（重大里程碑）

### 完成（真机验证全部通过）
- **设备出站 TCP 全通**：T1/T2 connect rv=1 (1ms)、Sn_SR=17(ESTABLISHED)、Sn_IR=01、DHAR=PC MAC
- **POST /ota/check 协议交互成功**：设备发 168 字节 JSON body（platform 2/app_id 0/version 0x01010000），服务器回复 353 字节 HTTP 200 + JSON：
  {"upgrade": true, "url": "/ota/pkg?id=fw_acc.otapkg", "new_version": "0x01020000", "size": 389314, "sha256": "49e8ac73...", "sign": "demo-hmac"}
- **根因定案（复位后首次 ARP 失败）**：W5100S 复位后 PHY/芯片未就绪，立即 connect 导致首次 ARP 失败（-13/804ms/DHAR=FF）；connect 前 osDelay(2000) 后完全正常。正式代码需加网络就绪等待（或重试机制）
- **排除的疑点**：
  - lib 差异无关：旧 lib(51470fcf) 与重编 lib(f9756f8a) 链接后 app hex 完全相同（137c4e05...）
  - 防火墙无关（三 profile 全关）；PHYLINK=0 为 W5100S SPI 模式 PHYCFGR 不可读（假象）
  - socket 号无关（socket 0 正常）；httpServer 模块未被调用（无冲突）
- **详情**：ota_server 路由确认（/ota/check 为 POST，GET 返 404）；PC 9999 实收设备 CONNECTION + GET

### 待办（下次继续）
1. **GET /ota/pkg?id=fw_acc.otapkg 下载固件包**（389314B）→ 分段 read(2KB)+写 QSPI 0xE00000 暂存区（已验证擦写 OK）
2. 校验 sha256/sign → 触发升级（boot 交换）
3. 清理 user_main.c 临时测试块（PHASE2 + PHASE1），正式化为 ota_download 流程
4. 待办项：F460 联调、生产化（HTTPS/密钥/灰度）

---


## 2026-08-18 H2 下载链路完整打通（v7 真机验证全通）

### 完成（全链路字节级验证）
- **T1** connect 9999 rv=1（DHAR=PC MAC）→ 链路 OK
- **T2** POST /ota/check → HTTP 200 + {"upgrade":true, url, size:389314, sha256, sign}
- **T3** GET /ota/pkg?id=fw_acc.otapkg → HTTP 200 Content-Length:389314 → 分段下载写 QSPI 0xE00000（先擦 96 个 4KB 扇区）→ total=389314 完全匹配
- **读回校验**：QSPI[0..15] = 4F54413100000201020070F005004447 与 PC 源文件 fw_acc.otapkg 字节级一致（OTA1 magic + 版本 0x01020000）
- 测试固件：tools/merged_f4a0_v7.bin（0x0，456856B）；代码：user_main.c PHASE2 块（T1/T2/T3）

### 技术备忘（正式化时要用）
- 复位后首次网络操作前需 osDelay(2000) 等 W5100S/PHY 就绪（否则首次 ARP 失败 -13/804ms/DHAR=FF）
- 下载流程：socket0 → connect 8080 → GET /ota/pkg → 解析 HTTP 头（找 

）→ 循环 read(512B)+写 QSPI（QSPI_FLASH_Write 内部 256B 页分割）
- 暂存区 0xE00000（1MB）：96 个扇区 = 393216B > 389314B OK；尾部留余 3902B

### 待办（下次继续）
1. **sha256 校验**（设备端算哈希 vs manifest 49e8ac73...）+ sign 校验
2. **触发升级**：boot 交换（ota_agent 流程对接）
3. 清理 user_main.c 临时测试块，正式化为 ota_download 流程（带重试/进度回调）
4. git 提交当前进展（临时测试代码先留保留，标注明确）

---

---

## 2026-08-18：正式化三项改动（清理测试块 + 网络就绪等待 + 网络路径 CRC32）+ 验收回归

### 完成
- 清理 `user_main.c` 临时测试块（PHASE1 QSPI 自测 + PHASE2 T1/T2/T3 网络测试，含写死 IP/包长），正式流程保留 `ota_agent_boot()`/`ota_usb_start()`，下载触发走 `ota_update` 命令 → `ota_agent_run`
- `ota_download.c`：首次网络操作前补 W5100S/PHY 就绪等待（`sleep_ms(2000)`，静态标志仅一次；2026-08-18 真机定案的首次 ARP 失败防护）；删除无用 `s_dl_crc` 死代码
- `ota_agent.c`：`verify_staged_pkg` 补载荷 CRC32 与包头 crc32 字段（[14:18]）比对（复用 `ota_storage_verify_payload`，与本地 UART 通道同一校验）；删除本地重复 crc32 实现
- 编译：App 0E/0W（Code=361300，-104B）、Boot 0E/0W（Code=5596）
- `ota_acceptance.py` / `rk_ota_agent.py`：临时目录改用 makedirs+uuid（`tempfile.mkdtemp` 目录在受限沙箱不可写）；验收 9/9 PASS
- 产物：`tools/merged_f4a0_v8.bin`（454664B，0x0-0x6F007）；版本库 fw_acc.otapkg 更新（sha 97161c94...）

### 待办
- 真机 H2/H3 复测（merged_f4a0_v8.bin 首烧 → 网络 ota_update / UART0+USB1 OTA1 帧升级）
- CHANGELOG.md 编码修复：2026-08-18 旧条目 GBK 段已转 UTF-8（`_append_changelog*.py` 曾以 gbk 追加导致混合编码）

