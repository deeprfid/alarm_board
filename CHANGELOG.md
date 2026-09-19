## [Unreleased]

- `[hc32f460]` **change(boot): 槽B 指示灯由【蓝】改【红】—— 蓝灯与 App 的雷达信号指示混淆** —— 双 target 重编 0 Error / 0 Warning，**待上板复测**。
  - **现场背景**：`BOOT_DEFAULT_SLOT = OTA_SLOT_B` 的改动**上板已验证通过**（蓝灯亮 2 秒 -> 跳 B 槽 -> App 起来）。但蓝灯与 App 的雷达信号指示混在一起看不懂。
  - **改法**：`boot_led_slot()` 里槽 B 用 `BOOT_LED_RED` 取代原来的蓝灯（`3u`）。灯语变为 —— **跳 A = 绿灯常亮 2 秒；跳 B = 红灯常亮 2 秒**。
  - **与「Boot 卡住」告警的区分（同色但不同形态，不会认错）**：槽指示是「常亮 2 秒 -> 熄灭 -> 立刻跳转」，告警是「1 秒亮/1 秒灭**无限循环**、永不跳转」—— 即「亮 2 秒就走」vs「一直闪」。已在 `boot_led.h` 的判读规则里写明。
  - **顺带清理**：`boot_led.c` 里裸的 `2u`/`3u` 魔法数字命名化为 `BOOT_LED_GREEN/RED/BLUE`（此前极易看错，是 LED 分组/极性搞错那类事故的温床）。蓝灯已不用于任何状态，但 `boot_led_init()` 仍把它压灭，避免悬空或残留点亮。
  - **验证**：`iap_boot_Debug` / `iap_boot_Release` 均 **UV4 exit 0、0 Error / 0 Warning**；Code Debug 5632（不变）、Release 3800 -> **3796**；`.uvprojx` 注册数 6；RAMCODE 复核 `BOOT_OTA_Run @0x20018032`、`EFM_Program @0x2001822c`、`EFM_SectorErase @0x20018448`、`FLASH_EraseSector @0x20018684`、`ota_flag_write @0x20018b44`，**全在 0x20018xxx**。

- `[hc32f460]` **feat(boot): 新增 BOOT_DEFAULT_SLOT —— 兜底路径的优先启动槽改为槽 B** —— 双 target 重编 0 Error / 0 Warning，**待上板复测**。
  - **动机**：把板子钉在 B 槽上做调试（槽 B 此前从未上过板）。
  - **改法**：`boot_ota.c` 新增 `#define BOOT_DEFAULT_SLOT (OTA_SLOT_B)`，`boot_pick_valid()` 从「按 A->B 顺序扫」改为「**优先槽 -> 另一个槽**」：`u32First = BOOT_DEFAULT_SLOT`，不可用才回退 `OTA_SLOT_OTHER(u32First)`。要切回 A 槽只改这一行。
  - **作用范围（重要，别误以为覆盖一切）**：只影响两类兜底 —— ① 标志区无效（首次烧录/被擦）；② 标志指定的槽不可用需换槽。**一旦标志区有效且 active 已定，仍以标志为准** —— 这是 OTA 的正确性前提：升级完必须听标志，否则 Boot 会把刚升上去的槽抢回旧槽，升级等于白做。当前 `OTA_APP_ENABLE = 0`，App 从不写标志，故标志区始终无效 → **Boot 恒走兜底路径 → 恒定跳 B 槽**。
  - **失效安全**：`boot_slot_usable()` 先验向量表，**槽 B 没烧时自动回退 A 槽**（不会卡在 Boot）。现场灯语：跳 B = **蓝灯亮 2 秒**（`boot_led_slot(1)` -> id 3 -> 蓝），跳 A = 绿灯 2 秒 —— 一眼可辨。
  - **验证**：`iap_boot_Debug` / `iap_boot_Release` 均 **UV4 exit 0、0 Error / 0 Warning**；Code Debug 5892 → **5632**、Release 4056 → **3800**（循环展开成直路，反而变小）；`.uvprojx` 关键源注册数仍为 6；RAMCODE 复核 —— `BOOT_OTA_Run @0x20018032`、`EFM_Program @0x2001822c`、`EFM_SectorErase @0x20018448`、`FLASH_EraseSector @0x20018684`、`ota_flag_write @0x20018b44`，**全在 0x20018xxx**。槽宏核对：`OTA_SLOT_A=0 / OTA_SLOT_B=1`、`OTA_SLOT_BASE(1)=0x00028000`、`OTA_SLOT_OTHER` 定义正确。

- `[hc32f460]` **fix(rs485): USART 发送超时给够并检查返回值 —— Release 专有的「静默截断」** —— 4 个 App target 重编 0 Error / 0 Warning，**已上板实测：A 槽 Release 运行正常**。
  - **现场症状**：Release 上电后 STM32 **只收到 1 个正确上传包，之后失效**；Debug A/B 均正常；`for(;;)` 主循环仍在转，**无看门狗复位** —— 即**不是崩溃，是收发链路状态问题**。
  - **先确认一个事实（否则会往错误方向查）**：反汇编 `CalcCRC` 证明 **Debug 本身就是 -O3**（`PUSH {r3-r10}` + `IT/ITT` + 常量寄存器分配），两 target 的 `<Optim>` 都是 4，Code 差 9116 字节**全部来自 `__DEBUG` 打开的 `DDL_ASSERT`**（`DDL_ASSERT` 全工程只出现在驱动里）。**所以「Release 失败 / Debug 正常」= 断言代码把某个潜在缺陷掩盖了**，方向是**时序**而不是优化等级。
  - **缺陷**：`USART_UART_Trans(USART_UNIT, out, total, 100)` 的第 4 个参数**是自旋次数，不是时间** —— DDL `hc32_ll_usart.c:277 USART_WaitStatus` 原文：*"Maximum count of trying to get status"*，即 `while (flag != SET) { if (u32To > u32Timeout) { LL_ERR_TIMEOUT; break; } u32To++; }`。
    460800bps 下**一个字节 ≈ 10/460800 = 21.7us**，而 100 次自旋在 Release 的紧凑循环里只有**几 us** → 会在 `TX_EMPTY` / `TX_CPLT` 置位前 `break`，**帧被静默截断**；原调用还把返回值 `(void)` 丢了，失败**完全不可见**。Debug 因为循环更慢，恰好蒙对。
  - **改法**：新增 `FRAME_TX_SPIN`（`common.c`）/ `OTA_TX_SPIN`（`ota_recv.c`）= **20000** 次（200MHz 下自旋约 4~8 周期/次，覆盖 21.7us 需 ~1100 次，此处约 25 倍余量，≈0.5ms/字节；发送正常时根本到不了上限），并**检查返回值**：`if (USART_UART_Trans(...) != LL_OK) { return 0; }` —— 失败不再静默。
  - **验证**：4 个 target 均 **UV4 exit 0、0 Error / 0 Warning**；Code Debug 38446 → **38458**、Release 29322 → **29334**；`.uvprojx` 关键源注册数仍为 12（3 文件 × 4 target）。
  - **上板实测（2026-09-19，烧槽 A Release）**：**运行正常** —— 原先「只收到 1 个正确上传包后失效」的现象消失。
  - **归因（逻辑上的唯一解）**：本次上板镜像里同时含 `6c1e63b`（嗅探器吞字节）与 `4e919aa`（TX 超时）两笔，但**只有后者能解释「Release 挂 / Debug 正常」这个非对称** —— 嗅探器缺陷是**纯逻辑错误**，Debug 与 Release 会**同样**丢帧（约 8~15%），解释不了编译目标间的差异；而「自旋次数不够」是**时序相关**的，只有 Release 的紧凑循环才会触发。故本次现象消失**指向 TX 超时那条**；`6c1e63b` 修的是另一条独立缺陷（仍然有效，只是不是本次症状的成因）。
  - **仍未验**：槽 B（Release_B）未上板；`6c1e63b` 想解决的**随机丢帧率**没有量化测过（修复前后各统计丢帧率才能证明）。
  - **仍在查（未改）**：RS485 **RX 取数路径的不对称** —— `RX_DMA_TC_IrqCallback()` 把**整个 512 字节窗口**推入环形缓冲（`BUF_Write(..., RS485_RX_WIN)`），而 `USART_RxTimeout_IrqCallback()` 只推 `got = 512 - DMA_GetTransCount()` 字节，**两个回调都没有复位 DMA 传输计数**。若自链接的 LLP 描述符没有自动重装，则第 2 帧起 `got` 会包含历史字节 → 环形缓冲被重复数据灌满 → 解析彻底失步，**恰好表现为「第 1 帧正常、之后失效」**。需上板确认（读 `DMA_GetTransCount` 在帧间的变化）。

- `[hc32f460]` **fix(ota): OTA 嗅探器在业务态吞字节 —— 「App 能启动但通信不正常」的根因** —— 4 个 App target 重编 0 Error / 0 Warning，**待上板复测**。
  - **先纠正一个错误前提**：原本怀疑「Release 与 Debug 优化等级不同」。反汇编 `CalcCRC` 后否定 —— Debug 就是 `PUSH {r3-r10}` + `IT/ITT` 条件执行 + 寄存器分配，**Debug 本身就是 -O3**；两 target 的 `<Optim>` 都是 4，Code 差 9116 字节**全部来自 `__DEBUG` 打开的 `DDL_ASSERT`**（`DDL_ASSERT` 在整个工程里只出现在驱动中，业务源码一处都没有）。**所以这不是 Release 特有现象，问题在代码里。**
  - **缺陷**：`ota_recv.c` 的 `consume_byte()` 设计上「先解析后进入」（只有 CRC16 合法的 DATA 帧才切升级态），**但它在业务态（`s_state == OTA_RX_OFF`）就已经返回 1**：
    `return ((s_have > 0u) || (s_state != OTA_RX_OFF)) ? 1u : 0u;`（原第 342 行）
    而 `common.c` 是 `if (ota_recv_sniff(b) != 0u) { continue; }` —— 返回 1 即**不喂业务解析器**。
  - **后果**：OTA1 的 magic 是 `"OTA1"` = `4F 54 41 31`，于是**业务流里任何一个 `0x4F`（'O'）都会被吞掉**。业务帧从此少一个字节 → 整帧 CRC16 校验失败被丢弃。N 字节的帧命中概率约 `1-(255/256)^N`，20~40 字节载荷即 **8~15% 丢帧**；若载荷里恰好出现 `4F 54 41 31` 连串，则**整帧剩余部分全被吞**。
  - **当初为什么没发现**：定协议时只核对过「OTA1 magic 与业务帧头 `0xFF`/`0x55`/`0xAA` 不冲突」——**只比了帧头，没比载荷里的任意字节**。这正是这次排查补上的盲区。
  - **改法**：新增 `claim_byte()` —— **只有 `s_state != OTA_RX_OFF`（真正确认进入升级态）时才独占线路**；业务态下一律返回 0，字节照常喂给业务解析器。缓冲与 CRC 判定**完全不受影响**（`s_rx` 仍照常累积、`handle_frame()` 仍照常触发），升级入口语义不变。
  - **代价（已评估，可接受）**：进入升级态的**那第一帧** OTA1 字节会同时被业务解析器看到，最坏情况是多发一条业务应答；之后 `ota_recv_busy()` 立即为 1，链路归 OTA 独占。
  - **验证（编译层）**：`usart_uart_dma_Debug/_Release/_Debug_B/_Release_B` 均 **UV4 exit 0、0 Error / 0 Warning**；Code Debug 38130 → **38446**、Release 29014 → **29322**；构建后 `.uvprojx` 关键源注册数仍为 24（6 文件 × 4 target）。
  - **仍未做**：上板复测业务通信；另注意**本缺陷与优化等级无关**，若上板后 Debug 通信也异常属预期，**不能**据此判断修复无效。

- `[hc32f460]` **fix(boot): 亮灯等待期间补喂看门狗 —— 消除「跳转前 2 秒不喂狗」的复位隐患** —— 双 target 重编通过，**已上板实测无回归**。
  - **问题**：`boot_led_error()` 的死循环里专门写了 `SWDT_FeedDog()`（注释已说明 ICG 同时配了 `ICG_REG_WDT_CONFIG | ICG_REG_SWDT_CONFIG`，复位后可能已按 ICG 使能），但 `boot_led_slot()` 里一整段 `DDL_DelayMS(2000)` **一次狗都没喂** —— 而它正好占着跳转前的 2 秒窗口。**两只 LED 函数的喂狗策略不对称**，是这个隐患的直接来源；
  - **为什么以前没暴露**：现场实测「绿灯亮 2 秒 → 跳槽 A → App 正常起」只能证明**当前这一颗 ICG 配置**下 SWDT 溢出周期 > 2s；一旦有人把 SWDT 超时改短、或换一版 ICG 烧进去，就会在亮灯中途被咬复位、循环重启 —— **症状与当初「上电不运行」几乎一样，极难定位**；
  - **改法**：新增 `delay_fed_ms()`，把任意长延时切成 100ms 分片、**每片喂一次狗**，狗的溢出周期只要 > 100ms 就都安全；`boot_led_slot()` 与 `boot_led_error()` **统一走它**（后者去掉裸 `DDL_DelayMS`），从根上消除不对称；
  - **顺带**：删掉 `boot_ota.c` 里 `boot_halt()` 上方两行内容重复的注释；
  - **验证（编译/map 层，硬件未测）**：`iap_boot_Debug` / `iap_boot_Release` 均 **UV4 exit 0、0 Error / 0 Warning**；Code 5812 → **5892**（+80B，即新增的 `delay_fed_ms`）；构建后 `.uvprojx` 关键源文件注册数仍为 10（5 文件 × 2 target），未被 UV4 改坏；
  - **RAMCODE 布局复核**（`docs/ota_boot_design.md` §15.5 那条安全规则）：Debug map 逐符号确认 —— `EFM_Program @0x200183ad`、`EFM_SectorErase @0x200185c9`、`FLASH_EraseSector @0x20018805`、`FLASH_WriteData @0x200188f1`、`ota_flag_write @0x20018c49`，**全部 0x20018xxx**；Release 无调试符号表，改按执行域核对 `Execution Region RW_RAMCODE (Exec base: 0x20018000)`，段内含 `boot_ota.o`/`hc32_ll_efm.o`/`flash.o`（`FLASH_EraseSector @0x20018524`、`EFM_Program @0x200182b8`）。两 target 均**无任何擦写函数落在 0x0000xxxx**。
  - **上板实测（2026-09-19，烧 `output\debug\iap_boot.hex`）**：行为与改动前**完全一致** —— `绿灯亮 2 秒 → 跳槽 A → App 正常起来`，无复位重启、无异常灯态。即本次喂狗加固**未引入回归**；同时它也就此把「SWDT 溢出周期 > 2s 才安全」这个隐式依赖，换成了「> 100ms 即可」的显式约束。
  - **仍未做**：§15.2 的量产时钟配置分步排查（Boot 现仍跑复位默认 HRC）；Release 产物未上板验证（只烧了 Debug）。
- `[hc32f460]` **feat(ota): 自检确认 + 打包工具 + F4A0 分发集成层（第 2、1、3 项一起做完）** — 至此升级闭环的每一环都有代码，只差真机联调。
  - **修两处我自己引入的严重缺陷**（都会让升级 100% 失败的级别）：
    - ① **槽镜像 CRC32 自指**：原设计「CRC32 覆盖整镜像（含 17B 槽头）」，而 CRC32 字段自身就在被覆盖范围内 —— 打包侧根本算不出、设备侧永远校验不过。改为 **CRC32 只覆盖槽头之后的 payload**；
    - ② **槽头与向量表重叠**：元数据原本放槽起始处，但 **App 就链接在槽基址**（0x8000/0x28000）、向量表在最前 8 字节 —— 直接冲突。改为 **槽尾 trailer（槽末尾 32B）**，App 二进制原样从槽基址起；CRC32 覆盖 `[槽基址, +ImageLen)`，元数据在覆盖范围之外，既不自指又保证校验的是 Flash 实际内容；
  - **第 2 项 · App 自检确认**：新增 `ota_app.c/h`，`ota_app_boot_confirm(OTA_SLOT_OF_ADDR(SCB->VTOR))` 在 `main()` 里 **`WDT_Config()` 之前**调用（`ota_flag_write` 要擦 8KB 标志扇区约 20~30ms，此时看门狗还没开，避免在擦写窗口被狗咬）；作用是把本槽置 RUNNABLE、清 NEED_CONFIRM、boot_count 归零 —— **不做的话 Boot 每 3 次启动就把刚升的槽判 FAILED 并回退**；槽号由 `SCB->VTOR` 反推，与 VTOR 设置同源不会不一致；
  - **第 1 项 · 打包工具** `tools/ota_pack_f460.py`：按槽分别打包（A/B 是两份独立编译产物）；包格式与 `tools/ota_pack.py` 逐字段一致（82B 头：magic/ver/platform/app/len/CRC32/SHA256/HMAC）；**payload = App 二进制本体**（槽尾元数据由设备端收满后自行写入，其 CRC32 由 Flash 实际内容算出）；带**起始地址防呆**（拿错产物即报错）；
  - **打包实测**：`radar_slotA_v01020304.otapkg` / `radar_slotB_v01020304.otapkg`，各 30574B（镜像 30492B）；回读校验两个包的 **len/CRC32/SHA256/HMAC 全部 True**；A/B 因基址不同 CRC 天然不同（0x431CB3CB vs 0x1AADDA2D）；
  - **第 3 项 · F4A0 集成层**（F4A0 仓库 `hc32f4a0_app`）：新增 `ota_dist.c/h` —— 提供 `ota_host` 需要的 4 个 IO 回调（`read/write(fd)` + `osKernelGetTickCount()`）、分发状态、以及 **业务帧闸门 `ota_dist_busy()`**；在 `alarm.c` 与 `mqtt_interface.c` 的两份 `ipc_hpm_message()` 里都加了闸门（升级期间**全系统**停发业务帧，符合既定口径）；
  - **构建**：HC32 四个 App target 全 0 Error / 0 Warning；F4A0 app 0 Error，唯一警告是既有的 `ota_usb_stream.c(47)` 未使用变量（与本笔无关）；
  - **说明**：F4A0 侧本笔只把**闸门接通**（`ota_dist_busy` 被业务路径引用，故真的链接进去了）；`ota_dist_start/poll` 尚无调用点，被链接器回收 —— Code 仅 +24B。**触发方式（谁在什么条件下发起分发）仍是待定项**。
  - **仍未做**：真机联调（烧 Boot+App → 用 .otapkg 走一次完整升级）；F4A0 侧触发入口与进度/结果上报界面。
- `[hc32f460]` **feat(ota): App 槽化收尾 —— 切 A/B 分散加载 + 移除 ICG + 出 A/B 双份产物（第 3 项完成）** — 至此 Boot 与 App 可配套烧录。
  - **App 工程 4 个 target**：`usart_uart_dma_Debug` / `_Release`（**槽 A**，`HC32F460xE_slotA.sct`）+ 新增 `_Debug_B` / `_Release_B`（**槽 B**，`HC32F460xE_slotB.sct`）。原有两个 target 名字与输出目录（`output\debug`、`output\release`）保持不变以免打断既有流程；B 用 `output\slotb_debug` / `slotb_release` 且 `OutputName` 为 `usart_uart_dma_b`；
  - **移除 ICG**：App 四个 target 均删掉 `hc32_ll_icg.c` 条目 —— ICG 固定在 `0x400`，App 链接基址改为 0x8000/0x28000 后若继续携带会落到链接区之外；**ICG 归 Boot 独占**（Boot 的 map 已核对 `.ARM.__AT_0x00000400 @0x400/32B`）；
  - **验证（4 target 全部 0 Error / 0 Warning）**：`RO-data 942 → 910`，**正好 -32 字节 = ICG 段**，从数据上印证已移除；Code 槽A/槽B 完全相同（Debug 38130 / Release 29014），只有基址不同；
  - **基址逐项核对**：槽 A map `Load Region LR_IROM1 (Base: 0x00008000, Max: 0x00020000)`、`Reset_Handler @0x00008321`；槽 B hex 扩展线性地址 `:02000004 0002` 即 `@0x00028000`，首条数据里复位向量 `0x00028321` —— 两槽正好相差 0x20000，栈顶同为 `0x1FFFB598`；
  - **产物**：`output\debug`+`output\release` 为槽 A，`output\slotb_debug`+`output\slotb_release` 为槽 B（各 .axf/.hex）；Boot 产物在 `projects\boot\MDK\output\{debug,release}\iap_boot.hex`；
  - **烧录方式（重要）**：App 已不能单独烧到 0x0 运行 —— **必须 Boot + App 一起烧**（Boot@0x0、App@0x8000 或 0x28000），且**首次烧录要同时写标志区 0x7E000**（否则 Boot 走「无标志 → 按 A→B 扫第一个有效槽」的兜底路径，也能起，但 active 未定）；现场已装板子需整片重烧，旧 0x0 固件不再兼容。
- `[hc32f460]` **feat(ota): Boot 源码 + A/B 槽分散加载 + App 侧 VTOR（落地第 3、4 项的内容部分）** — 代码已就位并逐文件编译验证；**Keil 工程接线（Boot target、App 切槽分散加载、ICG 移出）尚未做**，故当前两个 target 仍按 0x0 链接、仍带 ICG。
  - **`projects/boot/source/boot_jump.c/h`**：栈顶（须落在 SRAM 0x1FFF8000-0x20027000）+ 复位向量（须落在本槽内）双重校验；交接前 `CLK_SetSysClockSrc(HRC)` → `CLK_PLLCmd(DISABLE)` → `EFM_SetWaitCycle(0)` → `SCB->VTOR`；再用 `__set_MSP` + 跳转。**原则：不把配好的 PLL 交出去**（借鉴 Decoder bootloader 的 `fw_jump_helper.c`，让 App 从近复位态自行初始化），避开「Boot 配好时钟 + App 再配一次」的耦合；
  - **`projects/boot/source/boot_main.c`**：读双份标志 → 选中槽校验（向量表 + 槽镜像 CRC32）→ RUNNABLE 直接跳 / TRIAL 则 `boot_count++`、超 `OTA_FLAG_MAX_BOOT` 标 FAILED 并切另一槽 / 标志无效则按 A→B 取第一个有效槽 / 两槽皆无效则停在 Boot（不跳任何槽 = 不砖）。Boot 刻意最小：**不配时钟、不开串口、不开看门狗、不初始化任何外设**（下载在 App 里做）；
  - **`projects/boot/config/linker/HC32F460xE_boot.sct`**：Boot 32KB @0x0，`ER_IROM1` 限死 32KB 使 Boot 不可能越界覆盖槽区；**ICG 归 Boot 独占**；
  - **`projects/MDK/config/linker/HC32F460xE_slotA.sct` / `_slotB.sct`**：App 链接基址 0x8000 / 0x28000，各 128KB，RAM 布局与原 `HC32F460xE.sct` 完全一致（保住此前 64B 对齐修复）；
  - **`projects/source/main.c` 加 VTOR**：`SCB->VTOR = (uint32_t)&main & ~(OTA_SLOT_SIZE-1)` —— 槽按 128KB 对齐且链接区限死 128KB，故把本函数地址向下对齐即得本槽基址，**无需 per-slot 编译宏、也就无从配错**；未槽化（仍链接 0x0）时该式为 0，与复位默认一致，所以这次改动对当前固件是安全的（实测 Code 仅 +24B）；
  - **「两槽皆无效」在 OTA 路径下不可达**（已写进 boot_main.c 注释）：A/B 无搬运下下载只写非活动槽、激活只在整镜像 CRC32 通过后发生 → **运行槽恒有效**，Boot 恒有槽可跳。只有用 SWD 等外部手段烧进坏镜像才可能构造该状态，那种情况需调试器恢复（Boot 不带下载通道）——这是本设计有意接受的边界；
  - **命名映射（旧 DDL → 本工程 Rev3.3.0，照抄参考实现会编译不过）**：`M4_SYSREG`→`CM_CMU`/`CM_PWC`、`EFM_Unlock()`→`EFM_REG_Unlock()`、`enIrqResign()`→`INTC_IrqSignOut()`；另 `boot_jump.c` 需显式包含 `hc32_ll_clk.h`/`hc32_ll_efm.h`（`hc32_ll.h` 不含它们）；
  - **验证**：`boot_jump.c` / `boot_main.c` / `ota_flash.c` / `ota_frame.c` / `ota_recv.c` 五个文件经 ARMCC 5.06u7 `--c99 -O1 -DHC32F460 -DUSE_DDL_DRIVER` 单文件编译**全部 0 error / 0 warning**；App 两个 target 加 VTOR 后仍 **0 Error / 0 Warning**（Debug Code 38106→38130、Release 28990→29014）；
  - **待做（工程接线）**：Boot target（新 target 或独立工程）；App 两个 target 切到 slotA/slotB 分散加载并各自出 A/B 产物；App target 移除 `hc32_ll_icg.c`；打包工具（否则无法真机跑通一次完整升级）。
- `[hc32f460]` **feat(ota): 接收端接线 + 业务态自动进入 + 升级期业务帧闸门（落地第 1 项）** — 做完这步，报警板端「可被上位机升级」的软件路径打通（尚缺 App 槽化与 Boot）。
  - **自动进入（先解析后进入）**：`ota_recv_sniff()` 在业务态逐字节嗅探 OTA1；**只有收到 CRC16 合法、type=DATA、0<len≤256 的完整帧才切升级模式** —— 不因「见到 'O'」或半帧/坏帧而进入，避免误触发让该链路业务停摆 10s；半帧 50ms 未补齐即丢弃（与业务帧守卫同量级）；
  - **`Check_Uart_Pdu()` 双模式**：升级模式下列表只跑 OTA1（按 64B 批量 `BUF_Read` 喂 `ota_recv_feed`，业务解析停摆并 return）；业务态下 OTA1 帧字节**不喂业务解析器**（continue），避免 OTA 载荷里的 0xFF/0xAA 被误当成业务帧头；
  - **升级期停业务帧**：`frame_var_send()` 加闸门（该函数是本板 RS485 业务帧的唯一发送出口）—— `ota_recv_busy()` 时直接 return 0，符合「升级期关闭全部业务帧、链路独占」口径；
  - **为什么自动进入是安全的**：A/B 无搬运下误触发最坏只是往**非活动槽**写字，运行区一个字节不碰；**激活只在整镜像 CRC32 校验通过后发生** → 不会因误触发变砖；
  - **验证**：两个 target 均 **0 Error / 0 Warning**；模块本步才被真正链接（此前无调用点被链接器整体回收）—— Debug Code 33864 → **38106**（+4242）、Release 25364 → **28990**（+3626），ZI +1836（主要是 s_rx[267] 收帧缓冲）；
  - **踩坑记录**：往 `.uvprojx` 程序化插入 `<File>` 条目后，有一轮构建把 3×2 个条目全部挤掉（该轮链接失败）；从 git 恢复后重建即正常。**此后每轮构建都回查注册条目数**；该工程 `.uvoptx` 内不含这些条目（只有 `common.c`），其 mtime 停在 08:49 未随更新；
  - **仍未做**：App 槽化（链接基址 0x8000/0x28000 + VTOR + ICG 移出）与 Boot —— 在此之前板子仍无法真正完成一次升级。
- `[hc32f460]` **feat(ota): 报警板侧 OTA 接收端 + 片内 Flash 擦写层 + 布局/标志（设计稿 §14 落地第 1~3 项）** — A/B 双槽无搬运模型的设备端第一批模块；两个 target 均 0 Error / 0 Warning，且因暂无调用点被链接器整体回收，**现有固件体积一字未变**。
  - `ota_layout.h`：分区常量（Boot 32K@0x0 / 槽A 128K@0x8000 / 槽B 128K@0x28000 / 标志双份 8K@0x7E000）、槽镜像头（17B，magic `SLOT`）、选择器标志记录（40B，crc32 覆盖 [0,36)）；
  - `ota_flash.c/h`：EFM 擦/写（标 `__RAM_FUNC` 使其 RAM 驻留）、回读校验、IEEE CRC32（与 `tools/ota_pack.py` 的 `zlib.crc32` 一致）、标志双份读写（择 seq 更大且 CRC 有效的一份）、槽镜像校验；
  - `ota_recv.c/h`：OTA1 接收端 —— 包头帧受理回 ACK(82) / 数据帧按 8KB 扇区**惰性擦写** + 每块回读校验 + 回 ACK(已写绝对偏移) / 帧 CRC16 错回 RESUME(当前偏移) 请重发 / 收满做整镜像 CRC32 校验后**一次标志写入**完成激活（active 切目标槽 + TRIAL + NEED_CONFIRM）/ 会话 10s 空闲自动退出升级模式；
  - `ota_frame.c/h`：帧核心，与 F4A0 工程**逐字节一致**（同一份源码两端共用）；
  - **不需要改 scatter**：本工程两套 scatter 的 `RW_IRAM2` 已含 `.ANY (RAMCODE)`，且 DDL `hc32_ll_def.h` 已定义 `__RAM_FUNC = __attribute__((section("RAMCODE")))` —— 扫描板 `boot_iap` 另加 `RW_RAMCODE` 执行段的做法在本工程**不必要**；
  - **EFM 调用序列（本工程 DDL 实测）**：`EFM_REG_Unlock` → 每个操作前 `EFM_FWMC_Cmd(ENABLE)` → `EFM_SectorErase`/`EFM_Program` → `DISABLE` → `REG_Lock`。`EFM_Program`/`SectorErase` 退出时会把 PEMOD 复位为只读，**故每个操作前都要重新 ENABLE**；
  - **设计稿一处修订（§14）**：原写「槽镜像头由设备端生成」，实现改为【包 payload 即槽镜像（含槽头），由打包侧生成、设备原样落盘】—— 免去设备侧按偏移搬移，TargetSlot 随镜像走；代价是**主机须先查目标槽再选 A/B 镜像**，为此新增附加式查询帧 `RESUME(payload="SLOT")` → ACK(当前运行槽)；
  - **尚未接线**：`ota_recv` 未挂到业务收帧入口 `Check_Uart_Pdu()`，且「进入升级」请求的承载帧仍待定（§12）→ **本步只验证了编译**，链接与运行需接线后验证；
  - 构建：Debug Code=33864 / Release Code=25364（与改动前同值），两个 target 均 0 Error / 0 Warning。
- `[ota]` **feat(ota/): 新增 OTA1 帧协议可移植核心 + 上位机端发送器(非阻塞状态机)** — 为「把 C# 上位机的固件下发功能由 HC32F4A0 实现」落第一块代码: 纯逻辑、零硬件依赖, 4 个文件可整体拷进 F4A0 工程。
  - `ota/ota_frame.h/.c`: OTA1 帧核心(CRC16-CCITT-FALSE 查表 / 组帧 / 解析 / 流式状态机), 与 F4A0 工程既有 `ota_frame.*` **逐字一致**(平台无关纯逻辑, 报警板接收端复用同一份);
  - `ota/ota_host.h/.c`: **上位机端发送器**, 移植自 C# `ReaderUI_v1_MCU/OtaUpdater.cs` 的 `Update()` 串口分支与 `OtaProtocol.cs`, 与 `tools/ota_send.py` 语义一致 —— 包头握手(只发一次/4s) → 分块(停等/3s) → 超时发 len=0 探测帧(2.5s)跟随设备偏移 → 设备确认偏移 10s 不推进则中止 → 进度满发探测帧兜底;
  - **实现为非阻塞状态机**(`ota_host_poll()` 周期推进), 不阻塞 F4A0 的 RTOS 任务 —— 与本仓库既有 `radar_poll()` 非阻塞事务引擎同一工程口径(那条经验: 主循环单次占用必须有界);
  - **偏移不乐观推进**: `off` 只在设备确认后跟随, 故 `on_progress` 报的是**已确认进度** —— 与 C# 的「乐观推进再跟随」在线上等价, 但上报与诊断更准;
  - **集成层只需 4 个回调**(write / read / tick_ms / ctx), F4A0 里即 `read/write(fd)` + `osKernelGetTickCount()`;
  - **编译验证**: AC5(ARMCC 5.06u7) 单文件 `--c99 -O1` 编译, `ota_frame.c` 与 `ota_host.c` 均 **0 error / 0 warning**; 顺带核对出两个工程的 `uC99=1`(C99 已开 —— `ota_frame.c` 的 `for (uint32_t i=...)` 依赖它, 缺 `--c99` 时 AC5 报 8 个错);
  - 新增 `ota/README.md`: 记录去向(F4A0 主机 / 报警板各自需要哪些文件)、集成方式与关键约束(串口语义绝对偏移 / 首帧只发一次 / 分块 256B / 不乐观推进 / 掉电整包重传)。
  - **未做**: 报警板侧接收端、A/B Boot、F4A0 侧集成层(通道选择 / 业务帧互斥 / 进度与结果上报)。
- `[ota]` **docs(设计稿 v0.2: 分发者改为 HC32F4A0 直连 + 升级期链路独占 + 协议定为 OTA1 + 维持 A/B 双槽无搬运)** — 本轮把 OTA 的分发拓扑、协议与槽模型定稿, 全部写入 `docs/ota_boot_design.md` v0.2(含 v0.1→v0.2 修订记录表), 并新增 `docs/decisions_2026-09-18.md` 记录当日决策。**本次不改任何固件代码**, 仅文档。
  - **分发者变更**: v0.1 的「Linux → STM32 中继 → HC32」改为 **HC32F4A0 直连 RS485 逐板分发** —— 去掉两跳; F4A0(HC32F4A0PITB, 2MB Flash / 512KB RAM)有文件系统 + QSPI + USB-MSC, 可自行存放固件与版本, 现场无 PC 也能升级。STM32F0 中继板与 Linux 主机的 OTA 本次范围外、暂缓。
  - **升级期口径变更**: v0.1 §12 默认「升级期业务照常」改为 **关闭全部业务帧、链路独占、只跑固件下发** —— 半双工 RS485 上 34KB 下载会饿死业务; 独占后两端状态机大幅简化。超时兜底: 任一方 10s 无进展即退出升级模式恢复业务, 避免链路被永久占死。
  - **协议定稿**: 采用既有 **OTA1 帧**(魔数 `OTA1`, type 0x50 DATA / 0x51 ACK / 0x52 RESUME, 偏移为 4B LE, CRC16-CCITT-FALSE poly 0x1021 init 0xFFFF, 覆盖 [0:9+len]), 取代 v0.1 §9 建议的自建 0xA5/0xAA OTA 命令族 —— 理由是该协议在 C# 上位机(`OtaUpdater.cs`/`OtaProtocol.cs`)、Python(`tools/ota_send.py`)、F4A0 与扫描板(`ota_transport_uart.c`)三端已实现且经真机调优; 魔数首字节 0x4F 与现有 0xFF/0x55/0xAA 帧头全不冲突, 可在既有 `common.c` 的 `frame_rx_feed()` 上加第三路分支, 不动原两路。
  - **偏移语义**: 采用**串口语义 = 含 82B 包头的绝对偏移**(total = len(pkg)), 与 USB-CDC/WinUSB 的「载荷相对偏移」严格区分 —— C# 侧曾在串口重复发包头导致包头被写进载荷、卡在 41370, 故串口首帧只发一次。
  - **载荷粒度定 256B**(非 4096B): 雷达板 RS485 是 512B DMA 窗口(`RS485_RX_WIN`) + 1KB 环形缓冲(`CBUF_SIZE`), 267B 的帧正好落在一个窗口内, **接收路径零改动**; 4096B 需 8 个窗口且环形缓冲 22ms 内被冲掉。34KB 固件 ≈136 帧 ≈1~1.5 秒。
  - **槽模型维持 A/B 双槽无搬运**(本次确认): 按槽编译两份镜像, **App 启动早期写 `SCB->VTOR = 本槽基址`**; 布局采纳 Boot 32K@0x0 / 槽A 128K@0x8000 / 槽B 128K@0x28000 / 标志双份 8K@0x7E000(该地址按 8KB 扇区对齐选取, 不要沿用扫描板 `boot_iap` 的 0x7F000 —— 它落在扇区中间, 擦除会连带邻区)。
  - **借鉴来源明确**: 报警板侧借鉴 `Scanner_20260901/boot_iap`(`flash.c` 擦写 / `boot_ota.c` 状态与跳转 / `fw_jump_helper.c` 时钟恢复 / `RW_RAMCODE` RAM 驻留段的链接脚本写法)与 `hc32f46_app/.../ota_transport_uart.c`(设备端 OTA1 帧处理) —— **但 `boot_iap` 是 single_bak(QSPI 暂存 + 备份 + 搬运)模型, 本设计是 A/B 无搬运, 只借鉴机制代码, 不照搬其 QSPI 依赖、备份区与搬运流程**(报警板无 QSPI 外设, 不是未贴片); F4A0 侧借鉴 C# 串口 OTA 的发送主循环(包头握手 / 分块 / ACK 跟随 / 探测 / 10s 卡死保护 / 收尾)。
  - **实现顺序**: F4A0 侧 host 发送器(纯逻辑, 只依赖「写字节/读字节」回调, 可离线自测) → 报警板侧接收端(OTA1 帧解析 + 写非活动槽) → 报警板侧 A/B Boot(读标志 / 校验 / 跳槽 + TRIAL 回退)。
- `[hc32f460]` **fix(Release target 的 FPU 选项错误 —— 这是「高优化档就不工作」的真正根因)**: Release target 的 Floating Point Hardware 一直是 **Not Used**, 链接器命令行是 `--cpu=Cortex-M4 --fpu=SoftVFP`(软件浮点 ABI); 而 Debug target 与全部现场固件是 **Single Precision**(`--cpu=Cortex-M4.fp.sp`)。两个 target 的其余功能差异(只有 `__DEBUG` 断言、`DebugInformation`、短枚举三项)经逐项实测**都不影响功能**。
  - **症状与规律**: 同一份源码, Release 在 **-O2 及以上**必然不工作(**AC5 与 AC6 都一样**, 开 LTO 时阈值降到 -O1); 加 `-fno-inline-functions -fno-inline` 或用低优化档(-O0/-O1)则正常 —— 低优化档的保守代码生成把 FP ABI 的差异掩盖了, 所以现象看起来像「编译器/优化档问题」, 实际是**目标配置问题**。
  - **修法**: Release target 的 Floating Point Hardware 改为 **Single Precision**(与 Debug 一致)。修后两个 target 的链接器命令行都是 `--cpu=Cortex-M4.fp.sp`, AC5+O3 与 AC6 各档位均正常(Release Code=25364 / Debug Code=33864, 0 Error / 0 Warning)。
  - **发布检查清单新增一条**: 改动 target 配置后, 用 `output/<target>/usart_uart_dma.lnp` 里的 `--cpu=` 行核对两个 target 的 **FPU/ABI 必须一致**。

- `[hc32f460]` **fix(四处「编译器不可见耦合」隐患)**: 与上面的 FPU 问题无关, 但都是真 bug, 一起修掉。
  - **ICG 启动配置字**: `hc32_ll_icg.c` 的 `u32ICGValue[]`(复位后硬件要读的 0x400 处配置字)没有任何代码引用、只靠链接器定位; AC5 用 `at()` 会生成根段得以保留, **AC6 分支用的是普通 `section()` 属性, 被 armlink 的未用段消除直接删掉** —— AC6 镜像里完全没有 ICG 段, 芯片按擦除态默认启动。**AC5 下不需要任何保活**: AC5 走 `at()` 分支生成的是链接器根段, 天然保留 —— 回退后两个 target 的 map 里 ICG 段仍位于 `0x00000400` / 32 字节。**只有 AC6(尤其开 LTO)才需要保活**, 且链接器 `--keep` 救不了它(实测: LTO 阶段符号就已被消除, 链接器看不到), 可行做法是工程侧引用一次 `u32ICGValue` 的地址(曾实现为 `bsp_trng.c` 的 `ICG_KeepAlive()`, 实测 AC6+LTO 下 ICG 段确实回到 0x400; 因 AC6 迁移搁置已回退)。详见 `docs/toolchain_baseline.md`。
  - **严格别名**: `common.c` 的 `Get_pdu_data()` 原来把字节缓冲强转成结构体指针读字段(`alarm_pdu *getpdupack = (alarm_pdu *)pdubuff`), 同时又用 `CalcCRC()` 逐字节读**同一块内存** = UB(AC6/clang 在 -O2 起启用 TBAA 会据此重排读取)。改为 `memcpy` 到本地副本再读, 与 STM32 侧 `app.c` 的写法一致。
  - **系统时基缺 volatile**: `m_u32Tickms` 在 `SysTick_Handler()` 里 `++`、被主循环到处读, 但 6 处声明都不是 `volatile` -> 全部改为 `volatile uint32_t`。否则高优化档下主循环可能一直读到寄存器里的陈旧值, `Check_UidKey()` 里靠 `m_u32Tickms % 50 / % 1000` 驱动的周期任务(报警状态机、LED、密钥)会停摆。
  - **环形缓冲索引缺 volatile**: `ring_buf.h` 的 `stc_ring_buf_t` 中, 中断里的 `BUF_Write` 与主循环的 `BUF_Read` 共享 `u32In/u32Out/u32FreeSize` 却没有 `volatile` -> 加 `volatile`(LTO 内联时尤其危险, 主循环可能永远看不到中断推进的索引)。
  - **构建**: 两个 target 均 0 Error / 0 Warning。

- `[stm32f030]` **fix(看门狗超时修正 + 下行转发不再静默丢帧)**:
  - **看门狗**: 原 `Prescaler=4 / Reload=4095` 只有 **0.41 秒**, 而主循环最长合法耗时实测约 **30ms**(`radarQueryAll()` 5 口 x `UartTxWait(5ms)` + `ipcReportStatus()` 的 `UartTxWait(COM1,5ms)`, 且只在 TX 忙时才真的等) -> 只留 13 倍余量, 偏紧。改为 `Prescaler=64 / Reload=624` = **约 1.0 秒**(25 倍以上; LSI 容差 30~50kHz 对应 0.8~1.33s)。`STM32F0_IWDG_ENABLE` 仍保持 0(调试期必需: STM32F0 的 IWDG 走内部 LSI, **一旦启动就停不下来**, 调试器 halt 时照样计数, 单步会被复位), 量产置 1 即可。顺带把 `MX_IWDG_Init` 的声明与定义一起放进条件编译, **消掉了项目一直存在的 `#177-D: declared but never referenced` 告警**。
  - **下行转发静默丢帧**: `uart4/6_dma_tx_start()` 在 TX 忙时是**直接 return 把整帧丢掉**; 而 20ms 一次的 0x10 查询与 Linux 转下来的下行包走**同一根 TX** —— `stmVarSend()`(发查询)有 `UartTxWait()`、`ipc_hpm_message()`(转下行)**没有**, 所以**被丢的总是下行包**(Linux 侧还以为发出去了), 概率约 **1/140**。现场症状就是「下发参数/命令偶尔不生效, 而且查不出来」。改为**「待发槽 + 发送泵」**: 收包上下文里**只登记**(不阻塞), 发送泵只在 `UartTxEmpty()` 为真时才真正 `comSendBuf`, 撞上就留到下一轮主循环 —— **不丢、不阻塞**, 常见情况(TX 空闲)延迟仍为 0。被顶掉的帧数挂到上行帧 `GPIO[0]` 供诊断(**正常应恒为 0**)。
  - **构建**: `IWDG_ENABLE=0/1` 两种配置均 **0 Error / 0 Warning**。

- `[hc32f460]` **fix(急救包改「靶向」+ 长时兜底, 现场两次回归定稿)**: 裸 0x00FE 的补发条件彻底重做。
  - **第一版「30 秒全静默就补发」被现场否掉**: 当时的假设是「APP 会话期间线上一直有字节, 门控挡得住」。**这个假设是错的** —— 客户在 APP 参数页**盯着不动**时, 线上**一个字节都没有**, 30 秒一到门控就放行, 0x00FE 插进 APP(BLE)会话把模块拉出配置态 -> 现场「设置参数 6401 失败 + 雷达 lock=0」。
  - **改成靶向**: 只有「**我们自己**的配置事务把模块留在配置态」(结束帧 0x00FE 没被确认, 或 0x00FF 发出后事务中途中止)才武装急救; **客户 APP 的会话永远不会触发它**。客户在参数页停留多久都与我们无关 -> 现场验证: **APP 设置恢复正常, 不再 6401**。
  - **再加长时兜底**(`RADAR_RESCUE_ABANDON_MS`, 默认 **5 分钟**): 客户 APP 配完参数**不退出页面直接最小化/关掉**, BLE 会话断开 -> 那帧 0x00FE **永远不会来**, 模块卡在配置态、彻底静默, 固件只能不停重扫 8 档、锁不上、回落到 256000 再循环, **雷达一路死到断电**。现在「彻底静默 >= 5 分钟」就补发一轮(每档一帧, 两轮之间至少隔 5 分钟, 锁上即撤销)。`0` = 关闭这条兜底。
  - **代价不对称, 所以默认取保守值**: 误伤 = 客户下一条设置命令失败(重试即可); **漏救 = 一路雷达一直死到断电**。
  - **两处伴生修复**: ① `s_link_last_rx_ms` 改成「来字节就刷」—— 原来只在链路监控每秒结算时刷, 1 秒粒度会让「全静默」判据失真(急救包门控与失联兜底都用它); ② 参数/下行状态机改成**先收事务结果、再判互锁** —— 原来链路一不新鲜就永远收不回结果, 该口会一直占着事务槽。
  - **构建**: `PARAM_EN` x `DL_SENS_EN` x `PROBE_FAILSAFE` x `RESCUE_ABANDON_MS` 各组合全部 **0 Error / 0 Warning**。

- `[hc32f460]` **refactor(radar.c 命令层全面改非阻塞事务引擎)**: 主循环单次占用从**最坏 600ms** 降到**有界**(每拍每口最多「发一帧」或「查一次 ACK」)。
  - **问题(为什么必须改)**: 原 `radar_cmd_port()` 是「发一帧然后 `while` 死等 ACK」, 单帧最坏 `RADAR_CMD_TIMEOUT_MS` = 200ms; 配置事务 = 使能配置 + 业务命令 + 结束配置 = **最坏 600ms 主循环完全停摆**。后果: ① STM32 每 20ms 问一次、`radarStaleMs = 200` 就判「无人」→ **那一路直接漏报**; ② 这段时间 `Check_Uart_Pdu()` 不跑, RS485 环形缓冲(2048B, 460800 下约 **44ms**)写满后 `BUF_Write()` **静默截断** → 下行命令丢; ③ 声光报警(`alarm_thread`)被推迟。更糟的是它**恰恰在模块不正常时最长**(ACK 不来才吃满超时), 平时根本看不出来。另外这 200ms 是给 9600 档留的余量, **不能靠调小超时来绕**。
  - **改法**: 新增**每口一笔的非阻塞事务引擎** `s_txn[RADAR_PORT_CNT]`, 由 `radar_poll()` 每拍推进一步: `等TX空 -> 发一帧 -> 等该帧的 ACK -> 下一帧`。一笔事务 = 1~3 帧(裸命令 / 使能配置+业务命令+结束配置), 取指定那一帧的 ACK 当结果; **使能帧失败时仍补发结束帧**(现场教训保留), 本次结果记为失败; 收尾帧(0x00FE)的结果不影响本次事务。
  - **全部命令接口统一改成 begin/poll 两段式**: `radar_cmd_*`、`radar_read_params_*`、`radar_set_sensitivity_*`、`radar_set_max_gate_*`、`radar_set/read_aux_control_*`、`radar_read/set_resolution_*`、`radar_read_fw_version_*`、`radar_read_mac_*`、`radar_set_uart_baud_index_*`、`radar_restart_*`、`radar_factory_reset_*`、`radar_eng_mode_*`、`radar_noise_start/status_*`, 另加 `radar_cmd_busy(port)`; `poll` 返回 `LL_ERR_BUSY` = 还没做完(下一拍再来)。**旧的阻塞接口与全部「口 0 兼容入口」删除** —— 全工程只有 `common.c` 用到 `radar_set_downlink_range()`(本来就是非阻塞), 其余调用点都在 `radar.c` 内部。
  - **四个内部状态机一并改造**: ① 参数自动配置改成「读一次 → 算完整待写掩码 → 逐项写 → 复检」, 事务数从 `~3N` 降到 `N+3`; ② 报警下行灵敏度; ③ 产线配置(写波特率 → 重启模块); ④ `radar_read_all()` 删除, 改成 `radar_dump_start()` + 由 `radar_poll()` 逐拍读完 5 项(带重试与项间间隔)。
  - **三个口可并行推进**: 事务不再阻塞, 所以删掉了「每拍只轮转一个口」的将就做法(`s_param_rr` / `s_dl_rr`), 口与口之间不再互相饿死。
  - **构建**: `PARAM_EN(0/1/2) × DL_SENS_EN(0/1) × DUMP_ONCE(0/1) × BAUD_TARGET(0/460800)` 八种组合全部 **0 Error / 0 Warning**; 默认配置 `Code=29460`(改造前 28996, +464B)。

- `[stm32f030]` **perf(收包解析与 20ms 轮询解耦)**: STM32 那盏「有人」灯比 HC32 明显滞后, 根因不是 20ms 本身, 而是**回包要等到下一拍才被解析**。
  - **现象与算账**: 查询在 `radarQueryAll()`(每 20ms), 而 `radarPumpPort()`(收包解析)排在它**前面**且同样关在 20ms 闸门里 —— 所以本拍发出的查询, 其回包(HC32 侧约 3ms 就到)只能等**下一拍**才被看到, 白等约 17ms。改前延迟约 `U(0,20)+20` ms(均 30 / 最差 40), 改后约 `U(0,20)+3` ms(**均 13 / 最差 23**)。
  - **改法**: `radarPumpPort()` 与 `radarTriggerOut()` 从闸门里挪出来, **每轮主循环都跑**; 只有 `radarQueryAll()` 与 `ipcReportStatus()` 留在 20ms 节拍上。
  - **线上流量一个字节都没变**: 查询包数量、帧格式、CRC、HC32 侧代码全不动 —— 改的只是「STM32 什么时候去读自己的接收缓冲」。**零协议风险**, 最坏情况与改前完全一致。
  - **顺带**: `radarTriggerOut()` 改成按需刷新 LED(刚点亮那一拍 `ucEnalbe==0` 立刻刷, 之后每 50ms 补刷, 抵消 `LED_Start(on,off,cycle=1)` 的 100ms 自动熄灭), 并只在电平变化时写 `Host_IRQ` —— 实际 `LED_Start` 调用从 50 次/秒**降到 20 次/秒**。
  - **构建**: 干净重建 A/B/A2 对照(同一套工程设置) `Code` 12016 vs 基线 11984(**+32B**)、`ZI` 21696 vs 21672(**+24B**), **0 Error**。

- `[hc32f460]` **feat(报警下行下发雷达灵敏度)**: 报警下行包 `alarm_pdu` 的 **`Alarm_Duration[1]`**(即 `common.c` 的 `radar_range`)现在会写到雷达模块上。
  - **映射**: `0` = 不设置(保持模块现状, 连待办都不建) / `1~10` -> **动态(运动)灵敏度 10~100**(灵敏度 = 值 × 10) / 其它值(如报警流程会把 `radar_range` 改成 `0xFF`)直接忽略。**静态(静止)灵敏度恒为 100**。写/比的门为 **0~8** 全部 9 个门(`0x0064` 是按门逐个写的)。
  - **幂等(核心)**: 这个值**每次报警都会带下来**, 但 **① 值没变 -> 一条命令都不发**; **② 值变了 -> 先 `0x0061` 读回模块现存的灵敏度逐门比对, 只写不一致的门**(模块自己把灵敏度存 flash, 所以固件重启后第一次下发同样先读后写, 不会白写一遍)。
  - **不阻塞主循环**: `common.c` 只调 `radar_set_downlink_range()` **登记目标值**(RS485 收包上下文里绝不做串口事务), 真正的读回/写由 `radar_poll()` **每拍只推进一个口的一个门**(命令事务是阻塞的, "9 门 × 3 口"一次做完会把 `Check_Uart_Pdu()` 饿死 —— STM32 那边 20ms 一问、200ms 就判"无人")。
  - **不在报警期间动雷达(重要)**: 这条参数是**跟着报警包一起下来的**, 而读/写事务期间模块会**停上报**(`0x00FF` 让它进配置态)—— 报警期间恰恰最需要雷达数据。所以 `radar_set_downlink_range()` **每条 PDU 都记一次时刻**(标记"现在处在报警活动期"), 真正的读/写要等到"距最近一次报警下行 >= `RADAR_DL_SENS_QUIET_MS`(默认 5s)"的**平静期**才做; 等待期间状态码 **14**。**不设强制超时** —— 宁可晚生效, 也不在报警期间把雷达打哑(连续不断报警时会一直等第一个空隙, 看状态码 14 就知道)。
  - **逐口独立 + 只对在线口生效**: 每个口一套 `s_dl_st/s_dl_need/s_dl_try`; 未锁定(不在线)的口保持待办并标 13, 等它上线自动补写。失败**最多试 3 次**就放弃(绝不无限重试), 标 12, 等下次下行值变化再试。
  - **现场读法(仍是 3 个 Watch 变量)**: `g_radar_comm` 的 **bit11..15** 现在有两个**编译期互斥**的来源 —— ① 参数自动配置(`RADAR_PARAM_EN != 0`); ② **本功能**(`RADAR_PARAM_EN == 0` 且 `RADAR_DL_SENS_EN != 0`): `0` 没下发过 / `1..10` **已生效档位**(= 下行原值) / `11` 正在写 / `12` 写失败已放弃 / `13` 该口未锁定 / `14` 在等平静期(报警期间不动雷达)。
  - **与 `RADAR_PARAM_EN` 的关系**: 灵敏度**只认下行这一个来源**; 参数自动配置保持出厂关闭(`0`), 它那套 `RADAR_PARAM_MOVE_SENS/STILL_SENS` 宏不再参与(两个都开会互相覆盖)。新开关 `RADAR_DL_SENS_EN`(`0` 关 / `1` 开, 当前默认)。
  - **构建**: 默认(`P=0 DL=1`) `Code=28996`、`P=0 DL=0` `27780`、`P=2 DL=0` `29312`, 均 **0 Error / 0 Warning**; `RADAR_PARAM_EN` 与 `RADAR_DL_SENS_EN` **同时开**会被 `radar_cfg.h` 的 `#error` 直接拦住(两边都写 `0x0064`、也都要占 `bit11..15`)。

- `[hc32f460]` **fix(急救包撞坏客户 APP 的配置会话)**: 现场手机 APP 报「设置距离门灵敏度失败 返回码：6401」, 且**时好时坏**。
  - **根因不是 0x00FE 本身, 是"什么时候发"**: 急救包原来只要"本轮是重扫"就**每一档都补发**, 而重扫的触发条件是链路失联兜底 —— **3 秒**收不到字节就扫; APP 配模块时模块同样停止上报、但 APP 的命令/ACK 一直在线上跑, **人一停顿超过 3 秒**急救包就插进 APP 的事务、把会话提前收尾, APP 下一条命令被判"无效"("时好时坏"正是因为人停顿多久是随机的)。并行原因还有一条: `RADAR_PARAM_EN = 2` 当时一直开着, 它自己也在跟模块抢同一套 `0x00FF -> 命令 -> 0x00FE` 事务。
  - **急救包加门控**: 新增 `RADAR_FAILSAFE_SILENT_MS`(默认 **30 s**), `radar_probe_failsafe()` 判"该口连续 ≥30 s 收不到**任何字节**"才真的发 —— 卡在配置态的模块是**彻底静默**的(连 ACK 都不回), 门控必然放行, **该救的照样救**; APP 会话期间线上一直有字节, **插不进去**。判据用 `s_link_last_rx_ms`, 本固件自己的配置事务收尾会把它推到"现在", 所以刚配完也不会立刻放行。
  - **`RADAR_PARAM_EN` 出厂默认改为 `0`**: 参数配置是产线/现场调试动作、不是运行时需求; 要配参数时改成 `2`, 逐口读数确认后改回 `0` 再出货。
  - **残留风险写明**: 人若真把 APP 开着 30 秒不碰, 门控仍会放行 —— 阈值是把概率压到可接受, **不是数学上不可能撞**; 另外"我们那帧与 APP 命令相撞"是**反推**结论(本板无调试口, 抓不到时序波形), 不是直接观测。详见 `docs/radar_baud_debug_notes.md` §11。
- `[hc32f460]` **feat(参数配置逐口化 + 三重自愈)**: 把「雷达参数设置」做成可按口执行, 并补上三级自愈与前置校验。
  - **参数/读回接口按口拆分**: 新增 `radar_read_params_port()` / `radar_set_max_gate_port()`(0x0060) / `radar_set_sensitivity_port()`(0x0064) / `radar_read_aux_control_port()`(0x00AE) / `radar_set_aux_control_port()`(0x00AD) / `radar_param_state_port()`; 不带口号的旧 API 一律保留为「口 0 兼容入口」。**每块模块的参数存在它自己的 flash 里, 所以必须逐口配。**
  - **参数状态机逐口化**: `s_param_st`/`s_param_cur`/`s_param_aux`/`s_param_code` 全部 `[RADAR_PORT_CNT]`(不再共用 `s_dump`); `radar_param_diff()` 改成纯函数(门号走出参); **只对已锁定(在线)的口执行**, 未锁定的口标 31 跳过但不锁死, 上线后下一拍自动接手。
  - **`g_radar_comm` 借用 bit11..15** 编码逐口进度/结果码(0 未做 / 1 一致 / 2 不一致 / 3~5 已写 0x0060·0x0064·0x00AD / 6 复检通过·成功 / 7 复检不一致 / 16~20 各步失败 / 31 未锁定), **不新增 Watch 变量**, 低位位图与 BRR 指纹与帧率编码一字未改。
  - **`RADAR_PARAM_EN` 扩成三档**: `0` 关闭(**出厂默认**, 理由见上面 fix 条目) / `1` **只读自检(不写模块)** / `2` 完整幂等配置。
  - **参数前置校验**(手册给了范围就该校验): `0x0060` 运动/静止门 **2~8**、`0x0064` 门 **0~8** 且灵敏度 **0~100**、`0x00AA` 分辨率 **0/1**, 超范围直接返回 `LL_ERR_INVD_PARAM`、**连帧都不组**; 另加**编译期** `#error` 拦住写错的 `RADAR_PARAM_MAX_*_GATE`。
- `[hc32f460]` **验证(现场, 两次受控实验)**: 用 `g_radar_comm` 的 bit11..15 编码实验码(不新增 Watch 变量), 把「裸发 0x00FE 能不能当急救包」这件事实测钉死:

  | 模块状态 | 裸 `0x00FE` 的结果 | 上报 | 读数 |
  | --- | --- | --- | --- |
  | **正常态** | ACK 但 **status≠0**(判无效) | **完全不受影响** | `0x0A1A1F03`(码 3) |
  | **配置态**(故意用 0x00FF 弄哑) | ACK 且 **status=0**(接受) | **恢复 10Hz 上报** | `0x0A1A2703`(码 4) |

  对照口全程 `0x0A1A0703`(码 0) → 实验只影响被测口, 未串到其他口。
  **结论: 手册 2.2.1「任何其他命令必须先发使能配置, 否则无效」只在模块「不在配置态」时生效; 模块一旦卡在配置态, 0x00FE 正是它的收尾命令** —— 即 `0x00FE` **只在需要它的时候生效**, 安全性与有效性同时成立。
  完整数据与手册原文见 `docs/radar_baud_debug_notes.md` §10。
- `[hc32f460]` **feat(重扫急救包)**: `RADAR_PROBE_FAILSAFE`(`0` 关 / `1` **只有重扫发**(当前默认) / `2` 上电首次也发); `radar_switch_baud()` 换到每一档之后补发一帧**裸 0x00FE**(`radar_probe_failsafe()`); **该帧另有一道门控** —— 该口需已长时间全静默(`RADAR_FAILSAFE_SILENT_MS`), 见上面 fix 条目。
  每档都发是因为模块卡住时不知道它在哪一档; 发错档即乱码, 魔术字拦得住, 无害。**绝不发 0x00FF**(它才会把模块推进配置态, 现场明令禁止)。时序上放在 `radar_port_set_baud()` 之后(TX 空闲), 一帧 11 字节 @9600 仅 11.5ms, 监听窗口 300ms 来得及。
- `[hc32f460]` **fix(配置事务两处结构缺陷, 现场实测暴露)**:
  1. **`radar_cfg_cmd()` 改为无条件发 `0x00FE`** —— 原来在 `0x00FF` 失败时直接 `return`, 结束帧永远发不出去; 但 **`0x00FF` 的 ACK 丢了不等于模块没进配置态**, 于是模块被留在配置态、现场只能断电救。
  2. **参数配置改为每拍只推进一个口**(轮转) —— 命令事务是**阻塞**的(一笔最坏 3×200ms), 三个口同一拍会把彼此的雷达泵与链路监控**饿死**; 现场实测口0 排在前没事, 口1/口2 的 `0x00FF` 因此超时。顺带把 `radar_cfg_cmd()` 收尾时的**链路监控窗口整体后移**, 免得「配置期静默」被失联兜底判据误判成断链。
- `[hc32f460]` **build**: `PARAM_EN`(0/1/2) × `FAILSAFE`(0/1/2) 全部 **0 Error / 0 Warning**; `PARAM_EN=0 + FAILSAFE=0` 时 `Code=27416`, 与引入本功能前**完全一致**(证明开关关掉时零影响)。加门控后逐项复测: 新默认(`P=0 F=1`) `Code=27720`、`P=0 F=0` `27416`、`P=1 F=2` `29252`、`P=2 F=0` `29164`、`P=2 F=1` `29252`; 门控只在 `FAILSAFE != 0` 时进代码(+28B), `FAILSAFE=0` 时 `Code` 与加门控前一致。
- **硬件（现场排障结论，非固件问题）**：**雷达3 口（USART3/PB14/PB15）在 38400 及以上收不到数据，根因是 RX 线（PB15）上一颗对地滤波电容偏大** —— 拆掉该电容后三路 460800 全部自适应通过。
  判据（把「波特率选错」和「信号带宽不够」分开）：`RADAR_BAUD_LOCK_FRAMES = 1`，模块恒定 10 Hz 上报，每档 300ms 窗口内约 3 帧 —— **只要正确那一档信号干净，一轮扫描（2.4s）必然锁定**；正确档扫到了却不锁 ⇒ 不是波特率没选对，而是该波特率下信号本身不合格。
  量化窗口：19200 能过（bit 52.1µs，采样点 26.0µs）、38400 过不去（bit 26.0µs，采样点 13.0µs）⇒ 该路 **RC 时间常数约 6~11 µs**（如 1kΩ×6~11nF、4.7kΩ×1.3~2.3nF、10kΩ×0.6~1.1nF）；正常 ESD 滤波应在 100pF 量级（τ≈100ns~1µs）。
  **关键旁证（排除固件）**：19200 与 38400 走的是**同一个 DIV64 分频、同一个 0.136% 误差**，固件里不存在能切在两者之间的判据；且口0/口1 用同一份 `radar_port.c` 分别跑通 460800 / 115200。
  详见 `docs/radar_baud_debug_notes.md` §9 与 `docs/radar_multiport_todo.md` §8。
- `[hc32f460]` **验证（现场，三口同时锁定）**：三路雷达各接一个模块同时上电，三口**各自独立锁定在不同波特率**，互不干扰：

  | 口 | USART / 引脚 | `g_radar_lock` | `g_radar_baud` | `g_radar_comm` | 分频 | BRR 整数 | 帧率 |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | 0 | USART1 / PA2,PA3 | 1 | 460800 | `0x0A1A0703` | DIV1 | 26 (0x1A) | 10 Hz |
  | 1 | USART2 / PA0,PA1 | 1 | 115200 | `0x0A6B0707` | DIV1 | 107 (0x6B) | 10 Hz |
  | 2 | USART3 / PB14,PB15 | 1 | 9600 | `0x0A130708` | DIV64 | 19 (0x13) | 10 Hz |

  三个 BRR 整数与分频公式预测逐一吻合，`0x100/0x200/0x400` 三标志全置位 —— **逐口独立探测、逐口独立换分频（9600 走 DIV64、另两口走 DIV1）、逐口 Watch 刷新全部成立**。
  顺带把 `radar_baud_debug_notes.md` §6 曾放弃的 9600 也补齐了实测背书。
  另：引脚功能号经数据手册核对无误 —— PA0/PA1 属 Func_Grp1（36/37 = USART2_TX/RX）、PB14/PB15 属 Func_Grp2（32/33 = USART3_TX/RX）；PB15 的 Func7 列直接标注 `USART3_CK` 可交叉验证。
- `[hc32f460]` **refactor（第3步：radar.c 每口一份 + 按口循环）**：三路雷达（USART1/2/3）的状态与流程全部按口拆分，各跑各的。
  - **状态数组化** `[RADAR_PORT_CNT]`：`s_rx`(分帧器)/`s_ack`/`s_ack_ready`/`s_baud_locked`/`s_rep_frames`/`s_ack_frames`/`s_reports`/`s_comm_map`/`s_probe_st`/`s_probe_idx`/`s_probe_t0`/`s_probe_rx0`/`s_rep_last_ms`/`s_fps_ms`/`s_fps_cnt`/`s_fps`/`s_link_ms`/`s_link_bytes0`/`s_win_ack_cnt`/`s_link_last_rx_ms`/`s_link_sweep_ms`/`s_presence_src`；`s_prov_*` 同步数组化；
  - **接收跳板**：`radar_port_set_rx_handler` 的回调只带 data/len、不带口号，故每口一个跳板 `radar_rx_cb0/1/2` → `radar_on_bytes(port, data, len)`，分帧/解析/计数全部落到该口；
  - **函数加口号**：`radar_pump` / `radar_probe_tick` / `radar_probe_next` / `radar_probe_accept` / `radar_switch_baud` / 链路监控（新拆出 `radar_link_tick`）/ `radar_cfg_cmd` 全部带 `uint8_t port`；
  - **主循环 / 初始化**：`radar_poll()` 里 `for (p = 0; p < RADAR_PORT_CNT; p++)` 逐口推进；`radar_init()` 里逐口 `radar_port_init(p)` + `radar_port_set_rx_handler(p, rx_cb[p])`（**登记必须在 init 之后** —— `radar_port_init()` 内部会把该口回调清零）；
  - **Watch 仍是 3 个变量**：`g_radar_lock` / `g_radar_baud` / `g_radar_comm` 变成 `[RADAR_PORT_CNT]` 数组，**位图 / BRR 整数分频指纹 / 帧率编码一字未改**；
  - **新增按口命令入口** `radar_cmd_port()` / `radar_restart_port()` / `radar_set_uart_baud_index_port()`；不带口号的旧 API 一律保留为「口 0 兼容入口」，`radar.h` 增加 `#include "radar_port.h"` 以取得 `RADAR_PORT_CNT`（不动第 2 步已完成的 `radar_port.h`）；外部调用点（`main.c` / `common.c`）零改动；
  - **OUT 脚按口**：新增 `s_out_port[]/s_out_pin[]`（口0 = 原 `RADAR_UART_DEV_OUT_*`，逐位等价；口1/2 = `RADAR_PORT1/2`），与 `bsp_report.c` / `common.c` / `bsp_gpio.c` 读三路 OUT 的写法一致；
  - **单实例维护功能不随口拆分**：参数自动配置（`s_param_*`）、只读回读（`s_dump`）、产线 dump 仍走口 0；
  - **清理**：删掉 3 处与实现自相矛盾的过期注释（仍写着「发 0x00FF 探测」「重扫先试上次锁定档」「大跨档需断电重启」），并去掉 `g_radar_lock`/`g_radar_baud` 一段完全重复的连续赋值；
  - **构建验证**：Keil 无头全量重建，5 种配置（默认 / `TARGET=460800` / `DUMP_ONCE=1` / `PARAM_EN=1` / 三者全开）**均 0 Error / 0 Warning**，默认配置 `Code=27416 RO-data=920 RW-data=80 ZI-data=9704`。
    其中 **`DUMP_ONCE=1` 单独**这一组抓出一处真 bug：`radar_link_ready()` 被误挪进 `#if (RADAR_PARAM_EN)` 内，该组合下报 `#223-D: declared implicitly` + `L6218E: Undefined symbol radar_link_ready`，已改回 `#if ((RADAR_PARAM_EN != 0U) || (RADAR_DUMP_ONCE != 0U))`。
- `[hc32f460]` **fix（现场反问查出的真 bug）**：上一版为『先试原档』加的 `s_first_baud` 快捷路径有缺陷 —— 它把**重扫的第 0 窗**用来试原档，之后 `radar_probe_next()` 把 `s_probe_idx++` 后从**第 1 档开始**，于是**第 0 档（256000）被整轮跳过**。现场表现正是『改档后不重扫，重启 HC32 才行』：因为**上电探测是从第 0 档开始按表扫的**，一定覆盖 256000；而重扫路径漏了它，若模块被改到 256000 就永远扫不到。
  修法：**删除 `s_first_baud` 快捷路径**，重扫一律从候选表第 0 档（256000）开始按顺序扫完 8 档 —— 简单、且与上电探测行为完全一致（上电探测已被现场反复验证）。代价：若模块其实没改档，恢复时间由 0.3 秒变为约 1~2 秒，可接受。Code 26684，构建 0 Error / 0 Warning。

- `[hc32f460]` **fix（现场实测暴露的两处问题）**：模块被 APP 改档后**自动重启、本板不重启**时，重扫条件没被触发。原因有二：
  ① **『字节增量 ≥32』这条前提在某些跨档组合下不成立** —— 实测模块 38400 而我们仍在 460800 上听时，framing error 的字节被硬件直接丢弃，通道全静默，`字节增量 = 0` → 判据永不满足（上一版已注明未覆盖，现场撞上）；
  ② 判据里误用了 `s_fps`（**上一秒**结算出来的帧数）而不是当前窗口的 `s_fps_cnt`，即使有乱码也要晚 1~2 秒才可能满足『0 帧』。
  修法：把 `dFrames` 改为 `s_fps_cnt + s_win_ack_cnt`（当前窗口，含 ACK）；并新增**失联兜底判据** —— 锁定状态下**连续 `RADAR_LINK_SILENT_MS = 3000ms` 一个字节都收不到**就做一轮重扫，且两次兜底重扫之间至少间隔 `RADAR_LINK_SWEEP_MIN_MS = 10000ms`（避免长时间失联或 APP 配置期间反复白扫；只改本板自己的波特率，不发任何命令）。Code 26712，构建 0 Error / 0 Warning。

- `[hc32f460]` **feat（运行中感知波特率变化并自动重扫）**：锁定后持续监控链路，判据按现场定：**最近 1 秒内『接收字节增量 ≥ 32』且『解出的合法帧（上报帧 + ACK 帧）== 0』→ 判定波特率不匹配，自动重扫**，重扫顺序为**先试上次锁定的那一档（300ms，恢复代价最小）**，再按候选表依次，最坏约 2.4 秒跟上新档。
  两点设计要点：① 『字节增量』条件自动排除『模块进配置模式且安静』（那时零字节）→ 不误扫；② 合法帧把 **ACK** 也算进来 → APP 正在配置（有 ACK）时同样不误扫。仍全程纯监听、不发任何命令。
  **已知未覆盖**（代码注释中已写明）：模块被换到『连乱码都收不到』的大跨档时（实测 38400 模块在 460800 上听 → 全静默），字节增量为 0，本判据不触发，需断电重启；若要覆盖需另加『连续 N 秒零字节则低频兜底扫』（未实施，等现场定）。Code 26652，构建 0 Error / 0 Warning。

- `[hc32f460]` **验证（现场，提速后复测）**：模块 9600 → `g_radar_lock=1`、`g_radar_baud=9600`、`g_radar_comm=0x0A130708`（169019144）：bit24..31=0x0A ⇒ **帧率 10 Hz（9600 档同样 10Hz，证明上报节奏与波特率无关）**、bit16..23=0x13=19 ⇒ DIV64（C=1.5625MHz）下 9600 的 BRR 整数分频、低位 bit3+`0x700` ⇒ 9600 档解出合法帧且正在通信。探测提速后（每档 300ms）依旧稳定锁定。

- `[hc32f460]` **fix（探测提速与稳态）**：① `RADAR_PROBE_LISTEN_MS` 由 **2000ms 缩到 300ms** —— 现场实测帧率 **10 Hz**（帧间隔约 100ms），300ms 可覆盖约 3 帧，原来的 2 秒是按『上报可能很慢』的**未证实推测**设的；一整轮 8 档扫描由约 16 秒降到约 2.4 秒。② 新增 `RADAR_PROBE_RETRY_MS = 5000ms`：**一整轮都没锁定时，每 5 秒重新扫一轮**（原来失败一次就永久停在 fallback、必须断电重启才能恢复；模块比本板上电晚、或某轮刚好错过上报时会踩到）。Code 26572，构建 0 Error / 0 Warning。

- `[hc32f460]` **验证（现场，帧率实测）**：模块设为 **230400** → `g_radar_lock=1`、`g_radar_baud=230400`、`g_radar_comm=0x0A3507F7`（171247607）：**bit24..31 = 0x0A = 10 ⇒ 合法上报帧率 10 Hz**（此前的推算值 10.8Hz 得到实测确认，与 LD2410 约 100ms 上报周期一致）；bit16..23 = 0x35 = 53 = BRR 整数分频（DIV1 下 230400 的理论值）；低位 `0x07F7` 中 bit7 = 230400 档解出合法帧、`0x700` 标志全置位。
  由此确认：**探测窗口 `RADAR_PROBE_LISTEN_MS = 2000ms` 可覆盖约 20 帧，窗口时长不是『偶尔自检不到』的原因** —— 更可能是模块上电晚于本板、或扫描只跑一次没有重试（待做『未锁定时周期性重扫』）。

- `[hc32f460]` **refactor**: 新增**统一换档入口** `radar_switch_baud()`（`radar.c`）：换波特率时一次性完成『`radar_port_set_baud()`（关收发→停TX DMA→`USART_DeInit`→重新初始化→BRR 回读校验→清环形缓冲）+ `radar_frame_init()` 复位分帧器 + 清接收计数』，替换原先分散在 `radar_probe_tick()`/`radar_probe_next()` 里手工配对的 `radar_port_set_baud` + `radar_frame_init` + 计数清零（三处调用点）。
  目的：换档瞬间线上那一帧会被拆开（前半截在旧档、后半截在新档或丢失），**分帧器状态与计数必须同步复位**，否则可能把两段拼成一帧、或把旧档的帧计入新档。收成一个入口后从结构上杜绝漏做。函数注释同时写明前置条件：**必须在 TX 空闲时换档**（换档会复位 USART，发送中途会被打断）。Code 26548，构建 0 Error / 0 Warning。

- `[hc32f460]` **feat**: `g_radar_comm` 增加 **bit24..bit31 = 最近 1 秒收到的合法上报帧数（即帧率 Hz）**，仍不新增 Watch 变量（继续复用同一个数）。用于现场直接读帧率：9600 档按低电平占比推算约 10 Hz（23 字节帧占线 24ms、周期约 100ms），此位可给出实测值。Code 26552，构建 0 Error / 0 Warning。

- `[hc32f460]` **fix**: 探测候选表顺序调整 —— `256000` 提到第一档（实测模块出厂/APP 默认值），`460800` 退到第二档。原因：上电时端口本来就按 `RADAR_BAUD_FALLBACK = 256000` 初始化，而探测 `case 0` 会立刻切到候选表第 1 档、并**丢弃之前那一秒收到的帧**；原来第 1 档是 460800，于是模块在 256000 时必须等到第 2 个窗口（约 3~4 秒）才锁定。改序后出厂模块第 1 个窗口即命中（约 1~2 秒），也减少在前几档误锁的机会。
  同步更新 `radar.c` 里 `g_radar_comm` 位图注释的档位顺序（bit0=256000、bit1=460800，其余不变）。Code 26512，构建 0 Error / 0 Warning。

- `[hc32f460]` **fix**: 运行中重初始化 USART（换波特率）时，**TX DMA 的残留传输一并停掉**（`DMA_ChCmd(DISABLE)` + 清 TC 标志），放在关收发之后、`USART_DeInit()` 之前。
  说明：RX 侧已无 DMA（逐字节 RI 中断方案），不需要处理；TX 侧的 **DMA 通道配置与 AOS 触发映射都在 DMA/AOS 外设里，不受 USART 重初始化影响，无需重新初始化**，但**在途传输的状态必须清**——否则 USART 被 DeInit 后 DMA 还挂着半截发送，会留下脏状态。

- `[hc32f460]` **fix（按现场口径）**：**换波特率一律整套重来、不留任何痕迹** —— `radar_port_set_baud()` 现在固定执行：关收发 → `USART_DeInit()`（CR1/CR2/CR3/PR/BRR 全部清回默认）→ `StructInit` + `USART_UART_Init()`（分频随波特率选、`CKOutput` 不输出、8 倍过采样）→ 清 PE/FE/ORE/TX_CPLT 状态 → 清 RX/TX 的 NVIC 挂起 → 复位 `s_tx_busy` → 重新使能 `RX|TX|INT_RX` → 清环形缓冲。删除了上一版『只改 PR+BRR』的最小写法。
  保留一道**回读校验**（Init 后按 PR 反推 C，核对 BRR 整数分频 = `C/(B*8*(2-OVER8))-1`），失败即置 `s_baud_ok=0` —— 这是针对现场『返回 LL_OK 却没写进 BRR』那次静默失败的保险。`USART_DeInit()` 同时保留在**初始化路径**（FCG 使能之后），使初始化幂等。Code 26488，构建 0 Error / 0 Warning。

- `[hc32f460]` **fix**: 初始化前加 `USART_DeInit()`（在 FCG 使能之后），使雷达串口初始化**幂等**、不受上电前残留配置影响；运行时换档则改为**先关收发**(`RX|TX|INT_RX` DISABLE，不改其它寄存器) → 写 `PR`+`BRR` → **回读确认** → 恢复收发，确认失败才退回完整初始化。
  结论：`USART_DeInit()` 适合放在**初始化前**（保证确定初态），不适合当**运行中换波特率**的常规手段 —— 它会把 `CR1/CR2/CR3/PR` 一并复位（REN/TEN、格式、流控全丢），之后必须整套重配再使能收发，等于『完整初始化多绕一步』；它在运行时的价值是作为**外设疑似卡死时的兜底复位**。Code 26652，构建 0 Error / 0 Warning。

- `[hc32f460]` **fix**: 换档改为**最小改动 + 回读确认**：先只写 PR（按波特率选分频）与 BRR（`USART_SetClockDiv` + `USART_SetBaudrate`），随后**回读 BRR 整数分频与期望值比对**；一致则不动其它寄存器（RX/TX 保持使能），不一致才退回完整 `USART_UART_Init()` 兜底（并重新使能 RX/TX）。
  此前直接拿 `USART_UART_Init()` 换档是拿大锤敲钉子 —— 它会重写 CR1/CR2/CR3/PR/BRR（连 REN/TEN 一起清掉）。同时这一步也把『`SetBaudrate` 返回 LL_OK 却静默没写进 BRR』变成**可检测**（现场就是靠 BRR 指纹 `0x2F`(=256000) 发现软件记录 460800 是假的）。
- `[hc32f460]` **fix（启动顺序）**：`LL_PERIPH_WE/WP` 改为 SDK 例程的顺序 —— `WE` 放在所有初始化之前、`WP` 放在 `radar_init()` 之后。原来 `WP` 在所有初始化之前，等于让 GPIO/FCG/PWC/EFM/SRAM 各组寄存器**初始化期就处于写保护**，这类写丢失是静默的（`LL_PERIPH_SEL` 不含 USART，因此不影响 BRR，但会影响引脚复用/外设时钟/时钟配置等）。

- `[hc32f460]` **验证（现场，三档连测）**：`115200` → `comm=0x6B0707`（bit2 锁定，BRR 整数 107 = DIV1 预测值 108.5-1）；`230400` → `comm=0x3507F7`（bit7 锁定，BRR 整数 53 = 54.25-1）；`460800` → `comm=0x1A0701`（bit0 锁定，BRR 整数 26 = 27.13-1）。三档 `g_radar_lock` 全为 1、`g_radar_baud` 均为模块真值。
  **至此 8 档中的 6 档（9600/38400/115200/230400/256000/460800）现场实测通过，BRR 整数分频与分频规则预测逐档吻合**（DIV64：9600→19、38400→4；DIV1：115200→107、230400→53、256000→47、460800→26），锁定档位 bit 与 `g_radar_baud` 始终一致。仅 19200、57600 未单独实测。

- `[hc32f460]` **验证（现场）**：模块设为 **38400** → `g_radar_lock=1`、`g_radar_baud=38400`、`g_radar_comm=0x04073E`（263998）：bit5（38400 档）解出合法帧、`0x100/0x200/0x400` 全置位、bit16..23=`0x04`(**4**)=BRR 整数分频 —— DIV64 下 `C/(B*8)=1.5625e6/307200=5.086 → 整数 4`，**与规则预测完全一致**。低位 `0x3E` 说明 256000/115200/9600/19200/38400 各档都收到过字节（波特率不匹配时的乱码也算），到第 5 档 38400 才解出真帧。

- `[hc32f460]` **验证（现场，里程碑）**：模块设为 **9600** 后自适应**成功锁定** —— `g_radar_lock=1`、`g_radar_baud=9600`、`g_radar_comm=0x130708`（1246984）。解码：**bit3=9600 档收到字节并解出合法上报帧**、`0x100/0x200/0x400` 全置位（解出帧 / 正在通信 / 换档返回 OK）、bit16..23=`0x13`(**19**)=硬件 BRR 整数分频 —— 按规则 <115200 走 **DIV64**（C=1.5625MHz），`C/(B*8)=20.345 → 整数 19`，正是 9600 的理论值。
  **意义**：这是本任务最初『9600 永远收不到』的那个档，现已端到端打通（分频随波特率算 + 换档走初始化路径 + 逐字节 RI 中断接收 + 纯监听探测）。至此两个极端档都有现场实测背书：**9600（DIV64, int 19）** 与 **256000（DIV1, int 47）**。

- `[hc32f460]` **验证（现场，关键）**：换档修复后读数为 `g_radar_lock=1`、`g_radar_baud=256000`、`g_radar_comm=0x2F0703`（3081987）。解码：bit0=460800 档收到字节（波特率不对时的乱码也算字节）、**bit1=256000 档收到字节并解出合法上报帧**、`0x100` 曾解出帧、`0x200` 正在通信、`0x400` 换档返回 LL_OK、bit16..23=`0x2F`(47)=硬件 BRR 整数分频（DIV1 下对应 256000）。
  **结论**：① 运行时换档修复生效 —— 探测在第 0 档（460800）没有锁定（只有乱码），到第 1 档（256000）才锁定，证明硬件确实按档切换了；② `g_radar_baud` 改成从 PR+BRR 反推后，显示的是**硬件真值 256000**，与模块实际波特率一致；③ 模块侧一切正常：**出厂默认/APP 设置就是 256000，APP 写入是生效的** —— 此前"APP 设置不成功"的表象，根因是我们自己运行时换档没写进 BRR，软件记录值与硬件真值不一致造成的误判。

- `[hc32f460]` **验证（现场，2026-09-14）**：雷达波特率自适应**打通** —— 模块**真断电重启**后：`g_radar_lock=1`、`g_radar_baud=460800`、`g_radar_comm=0x301`（bit0=460800 档收到字节 + 0x100 解出合法上报帧 + 0x200 最近 1 秒仍在收 = 正在正常通信）。这套组合即本轮定型的接收方案：**逐字节 RI 中断**（不再用 RX DMA 窗口/AOS 重装/空闲超时上抛）+ **分频随波特率自动选** `(baud<115200)?DIV64:DIV1`（照抄扫描台主板量产写法）+ `CKOutput=DISABLE` + 返回码不再吞掉（`radar_port_baud_ok()`）。
- **现场教训（重要）**：LD2410C **一旦处于配置态（0x00FF 之后没被 0x00FE 收尾）就完全停止上报**，此时线上整段空闲高电平，**8 档全程零字节**（`g_radar_comm=0`）——纯监听方案对此**无法自救**，必须**真正断电重启**模块（APP 里的重启命令不够）。本次 `comm=0` 就是这个原因，断电重启后立刻恢复正常。
- **事实修正**：手机 APP『恢复出厂设置』+ 断电重启后，模块**仍在 460800 上报**（`g_radar_lock=1 / g_radar_baud=460800 / comm=0x301`，实测），与『LD2410C 出厂默认 256000』的文档说法不符 —— 该说法可能来自其它型号/版本文档。APP 侧设 256000 为何不生效（写入失败，或恢复出厂不改 UART 波特率）**待模块/APP 侧确认**；固件口径是**只跟随、不写模块**（`RADAR_BAUD_TARGET=0`），因此不卡功能：模块在哪一档，固件就锁哪一档。

- **Watch 口径收敛**：现场明确『Watch 最多留 3 个变量』，因此雷达只暴露这 **3 个单值**，且**以后不许再加第 4 个**（要更多信息就改 `g_radar_comm` 的取值定义，不加变量）：
  `g_radar_lock` = 1 已锁定模块波特率 / 0 未锁定；`g_radar_baud` = 当前波特率（锁定后即模块真实波特率）；
  `g_radar_comm` = 通信状态（0 一个字节都没收到 / 1 收到字节但解不出合法帧 / 2 曾解出合法上报帧后又断 / 3 最近 1 秒内仍有合法上报帧＝正在正常通信）。
  实现：`radar.c` 新增 3 个全局 + 上报帧时刻戳，在 `radar_poll()` 末尾更新；`radar.h` 加 extern。Code 26108 → 26184，构建 0 Error / 0 Warning。

- `[hc32f460]` **fix**: 分频规则改为**照抄扫描台主板（同款 HC32F460）量产在用的写法**：`(baud < 115200) ? DIV64 : DIV1`（8 倍采样周期 `UsartSampleBit8`、LSB 优先、1 停止位、无校验一致）。换算到本工程（C = PCLK1 = 100MHz/分频）：<115200 → C=1.5625MHz（9600 整数分频 20，误差 +0.13%）；≥115200 → C=100MHz（460800 整数分频 27，误差 +0.08%）。**取代我上一版自拟的三段式 {DIV4/DIV16/DIV64}** —— 有量产参考就不自创规则。
- `[hc32f460]` **fix**: `stcUartInit.u32CKOutput` 由 `USART_CK_OUTPUT_ENABLE` 改为 **`USART_CK_OUTPUT_DISABLE`**（与扫描台参考实现『时钟不输出』一致；异步 UART 下本不该输出 CK，原先与参考实现不一致）。Code 26132 → 26108，构建 0 Error / 0 Warning。

- **口径修正（重要）**：撤回此前由我自行写入 `radar_cfg.h`/docs 的**『雷达口固定 460800、不做运行时换档、上电不发命令』**一说 —— 那是我的推断，**不是客户要求**。真实口径：**客户会用手机 APP 自由设置模块波特率，固件必须自适应任意档**。因此：`RADAR_BAUD_INIT_FIXED = 0`（上电按 `RADAR_BAUD_FALLBACK` 收，再由探测逐档纯监听锁定）、`RADAR_BAUD_TARGET = 0`（只跟随模块，不写模块）；唯一保留的硬约束是**禁止发送 0x00FF/0x00FE**（客户/现场要求：会让模块进配置态并停止上报）。
- `[hc32f460]` **fix**: 雷达口**时钟分频改为按波特率自动选**（`radar_port.c` 新增 `radar_pick_clk_div()`），不再写死一个值。根因：DDL 的 BRR 整数分频只有 8 位，`DIV_Integer = C/(B*8*(2-OVER8)) - 1` 必须 ≤255 即 `C ≤ B*8*(2-OVER8)*256`；写死 `DIV4`（C=25MHz）+ 8 倍过采样时 **9600 需要 324 > 255 → `USART_SetBaudrate()` 返回错误且一个字节都不写 BRR，端口静默停在旧波特率** —— 这是『460800/256000 能通、改到 9600 不行』的直接原因之一。现按『满足约束的最小小分频』选：B≥12207→DIV4(25MHz)、B≥3052→DIV16(6.25MHz，9600 用这档)、B≥763→DIV64(1.5625MHz)；过采样保持 **8 倍**（与 SDK `usart_uart_int` 例程及已验证配置一致）。新增 `RADAR_UART_PCLK_HZ = 100MHz`。
- `[hc32f460]` **fix**: 雷达**接收改为逐字节 RI 中断**（SDK `usart_uart_int` 例程同款做法：RI 收满中断 + EI 错误中断分开注册，错误中断读 RDR 并清 PE/FE/ORE，最后统一 `USART_FuncCmd(USART_RX|USART_TX|USART_INT_RX)` 使能）。原因是原『256B DMA 窗口 + AOS/LLP 重装 + 空闲超时中断上抛』存在结构性缺陷：**空闲超时上抛从来没工作过**（实测该中断 0 次/秒），高波特率下窗口 20ms 就填满所以被掩盖，低波特率下窗口要 1 秒以上才填满、且实测 RX DMA 每秒只搬走约 15 字节，帧永远到不了解析层。逐字节中断对波特率零依赖，**换档不再需要碰 DMA**，自适应探测因此才可靠。同时删除 RX DMA/AOS/LLP/TMR0 空闲超时整套配置（Code 27664 → 26132）。
- `[hc32f460]` **fix**: 不再吞掉返回值 —— `USART_UART_Init()` / `USART_SetBaudrate()` 的返回码现在会被检查，结果放在新增的 `radar_port_baud_ok()` 里（0 = 该档分频表示不出来，探测时应跳过）；SDK 例程在初始化失败时是点红灯停机，我们至少不再静默继续。

- **规范**：**今后完全抛弃『往 Keil Watch 加变量』的调试方法**。现场多次确认：本板无调试串口，Watch 只能一个个人工抄单值，
  结构体/数组/函数表达式都取不出来，按秒统计+多标量交叉判读在现场『没法调试』。今后需要观测只允许三条路：
  ① 肉眼可见通道（板载 LED/蜂鸣器闪码）；② 既有 RS485 上行帧字段（32B `gpio_pdu`）；③ 调试器 Memory **整块**导出，且临时观测代码用完即删。
  判断标准：凡要求『人坐在 Keil 前逐个抄数字』的方法一律不合格。规范与本次删除清单见 `docs/radar_baud_debug_notes.md` §7。
- `[hc32f460]` **chore**: 按上述规范**删除全部调试脚手架** —— 删除 `radar_dbg.h`（含 `g_radar_dbg` 快照结构、`radar_dbg_poll()`）、
  `radar_cfg.h` 的 `RADAR_DBG_EN` / `RADAR_DBG_PERIOD_MS` / `RADAR_DBG_RX_STALL_MS`、`main.h` 的 `#include "radar_dbg.h"`、
  `main.c` 的 `radar_dbg_poll()` 调用、`radar.c` 末尾的调试段与其前置声明、
  `radar_port.c/.h` 的 3 个 TX 诊断计数（`g_radar_tx_dma_tc_cnt` / `g_radar_tx_tci_cnt` / `g_radar_tx_timeout_cnt`）——
  **TX 看门狗功能本身保留**（`radar_port_tx_watchdog()`）。Code 27696 → 27664，构建 0 Error / 0 Warning。
- `[hc32f460]` **docs**: 诊断版实测数据补齐到 `docs/radar_baud_debug_notes.md` §6.1（模块按 APP 设为 9600、驱动上电即 9600、不改档不发命令）：
  `g_radar_low_pct=13`（与『9600 + ~10Hz 上报』推算的 12% 低电平占比吻合）、`g_radar_dma_fill=15` 字节/秒、`g_radar_to_hz=0`、
  `g_radar_win_hz=0`、`g_radar_bps=0`、`g_radar_rx_err=0`、`g_radar_poll_hz=204509`。
  结论修正为：**模块、波特率、分频都没有问题，卡点在接收链** —— RX DMA 每秒只搬 ~15 字节（到不了窗口满 256），
  而**空闲超时上抛路径（`to_hz`）从未工作过**；460800 下窗口 ~23ms 就填满，刚好把这条路的失效掩盖了。
  §6『先放弃』的产品口径不变（雷达口仍固定 460800、不换档不发命令），但记录中明确：放弃的是低波特率诉求与手工抄单值的诊断手段，**不是问题本身**。
- `[hc32f460]` **revert**: 雷达口**放弃 9600 / 低波特率 / 运行时换档调试**，回到现场确认可用的**固定 460800**：
  `RADAR_BAUD_INIT_FIXED = 460800UL`（初始化路径设定）、时钟分频 `USART_CLK_DIV4` + **8 倍过采样**、
  上电**不做任何运行时波特率切换、不发任何命令**（纯听模块上报）。
  `radar_port.c` / `radar_port.h` / `radar_cfg.h` 回退到现场验证版（`b6d8210`），并**删除本次调试新增的全部诊断变量与 1 秒窗口统计**：
  `g_radar_brr`、`g_radar_rx_bytes`、`g_radar_pin_low`、`g_radar_dma_left`、`g_radar_rx_err`、`g_radar_low_pct`、`g_radar_bps`、
  `g_radar_dma_fill`、`g_radar_poll_hz`、`g_radar_to_hz`、`g_radar_win_hz` 及配套静态量与 `radar_port_poll()` 的统计分支。
  `radar_cfg.h` 保留一段分频/过采样选法注释（DIV4+8 倍的适用边界、`USART_CLK_DIV64` 只适合单档低波特率）；
  `RADAR_RX_TIMEOUT_BITS` 仍为 100（约 3.2ms，460800 下最长帧 1ms）。Code 27696，构建 0 Error / 0 Warning。
- `[hc32f460]` **docs**: `docs/radar_baud_debug_notes.md` 新增第 5、6 节。
  **§5 分频/过采样约束**（依据 DDL 源码）：`PR.PSC` 为 2 位、实际分频 = 4^PSC（仅 /1 /4 /16 /64）；BRR 整数分频只有 8 位，
  `DIV_Integer = C/(B*8*(2-OVER8)) - 1` 超限时 `USART_SetBaudrate()` 返回错误**且不写 BRR**；给出四档分频 × 两种过采样下
  协议表 6 全 8 档的可表示性（DIV4+8 倍算不出 9600；DIV16+8 倍与 DIV4+16 倍全档可用；DIV64 在 8 倍下只到 115200），
  以及『SDK 例程选 `USART_CLK_DIV64` 是单档低波特率的精度优化、不是通用答案』的结论。
  **§6 放弃结论**：9600 的 BRR 回读正确（`0xA1FF` / `0x50FF`，bit7 为 DDL 写掩码外的残留位）、三档分频都能表示 9600（误差 ≤0.22%），
  故『收不到帧』与分频/BRR 无关；关键现象是**运行中 `g_radar_rx_bytes` 恒 0、Keil 暂停再恢复即变 `0x100`（整窗 256B）**，
  指向『运行时接收链被打断』而非波特率；并记录**调试手段本身失效**（本板无串口、Watch 只能抄单值，多标量交叉判读现场不可行），
  后续再碰此类问题须先解决可观测性。

- `[hc32f460]` **fix**: UART4 RX DMA 窗口由固定 32B 改为 **512B 整帧窗口**（`bsp_rs485.c` 新增 `RS485_RX_WIN=512`，`m_au8RxBuf` 随之放大，TC/空闲超时上抛均按窗口计算）——消除不定长帧 >32B 时每 32B 边界“DMA TC 停→AOS_SW_Trigger 重装”窗口丢字节，整帧一次落入 DMA 块、空闲 flush 才上抛；针对 0xAA 128B 负载（134B 帧）偶发整帧丢失（errcnt=0、rxcnt 短少）修复。legacy 32B 帧路径不受影响。

### Added

- **docs**：新增《RS485 雷达/摄像头有人状态实时上传链路设计》（`docs/rs485_io_upload_design.md`，设计稿 v0.1）。覆盖：单字节状态码语义（有人=0x55/无人=0xAA、无人静默、20ms 心跳、下沿 0xAA）、RS485 非对称 LBT 仲裁（G_m/G_s/G_edge、关键帧双发、主站让行窗）、STM32 中断内归属补全上报（[0x7E][串口号][通道号][state]）、延迟预算（雷达 100ms 量化为主导）与两端实施清单。**未改动任何固件代码**。
- **docs**：设计稿升级 **v0.2**（`docs/rs485_io_upload_design.md`）：STM32→Linux 上报由“逐口 4B 即时帧”改为 **5 通道聚合 [0x7E][mask]** + **窗口上报（W=20ms 默认，可配 10ms）** + **全无人静默不报**；任一路 0→1 立即首报（不经过窗口，保 RFID 启动实时）；STM32 侧改为“RX 中断维护 5 位掩码 → 窗口任务组帧非阻塞入队 COM1”。含口→通道位序映射与 Linux 超时判清建议。**未改动任何固件代码**。

- **docs**：新增/升级设计稿后，本 Unreleased 追加**固件 MVP 改动**（首次改动固件代码）：

- `[hc32f460]` **feat**: 恢复“主机查询-报警板上报有人状态”老链路（MVP 第一步，复用 Radar V3.2 2025-0430 实现）：
  - `bsp_rs485.c` `Uart4_int()` 使能 `USART_TX`（此前仅 RX，发送功能未启用）；
  - `common.c` 新增 `Send_RadarStatus_to_Master()`：应答 32B `GPIOHEAD(0x55)` 帧，`Radarcfg[0]`=1/0（雷达 GPIO(PC14/PC13/PH2) 或 摄像头 IN1(PB00) 有人），阻塞 `USART_UART_Trans` 发送；
  - `Get_pdu_data()` 的 GPIOHEAD 分支由空转改为调用上述应答（`common.c`），`bsp_rs485.h` 增加原型。
- `[stm32f0]` **feat**: `GET_RADAR_ENABLE` 0→1（`BSP/bsp.h`）：启用既有 50ms 周期雷达状态轮询（`Broadcast_Get_Radar_Status` 广播 GPIOHEAD 查询）、应答缓存（`Chaneel_ID[]`）与按 AntID 查询应答（`Send_RadarStatus_to_Master`/`radar_pdu`）——拉取式上传链路打通，用于验证；
- 备注：MVP 保留老 50ms 周期与“每问必答 0/1”语义；周期压缩、STM32→Linux 聚合帧 `[0x7E][mask]`、0x55/0xAA 推送等优化放下一迭代（设计文档 v0.2 暂缓执行）。
- `[stm32f0]` **fix**: 恢复 `Radar_thread` 原版 50ms 广播节奏（每 50ms 调 `Broadcast_Get_Radar_Status()`，与出厂一致）；删除逐口错峰 `send_legacy_query/s_query_next` 与 0xAA 自检 ping（`FRAME_TEST_PING=0`，代码 `#if` 保留开关）；保留逐口帧解析泵、RX_GUARD、诊断计数与 `refresh_chaneel`（150ms 新鲜度）。0xAA 接收能力保留、不再主动并发探活。

- `[stm32f0]` **fix**: 定位并缓解 4 号口(USART5)持续丢回显——根因 **RX 溢出(ORE)**：

- `[stm32f0]` **fix**: `UartSend` 不再用 `HAL_NVIC_DisableIRQ(uartirq)` 屏蔽整个共享中断来写 TX FIFO，改为**只清/置本口 `USART_CR1_TXEIE`**（TX 事件源门控）——消除“发一个口时把 USART3..6 四个口的 RXNE 一起关窗”造成的 ORE；TX FIFO 容量等待改为无锁读 `usTxCount`（ISR 只减不增，读旧值只会多等不会溢出）。
  - `bsp_uart_fifo.c`：`UartIRQ` RXNE 单字节处理改 **while drain**（一口气收完该口 pending 字节并刷新 ISR）；`USART3_6_IRQHandler` 改为**按 ISR 标志只服务有数据的 USART**（不再空轮询 4 口）；
  - `app.c`：50ms 老格式 GPIOHEAD 广播由“5 口同时发”改为**逐口错峰 5ms**（`s_query_next` 调度），拆开 5 口同时回显的峰值；
  - 保留诊断计数（`dbg_uart_ore/fe/full`、`rxByteCnt/varCrcFail/legCrcFail`）便于复测；预期 `dbg_uart_ore[4]` 大幅下降、`varCnt[4]` 追平。

- `[hc32f460]` `[stm32f0]` **feat**: 不定长帧接收增加**超时复位（RX_GUARD=50ms）**：解析器新增 `lastByteMs` 时间戳与 `frame_rx_guard()`，用系统 tick 差值判定半包悬挂（不新增定时器/不阻塞），超时即回 IDLE 防死锁；HC32 在 `Check_Uart_Pdu` 开头调用（`m_u32Tickms`），STM32 在 `Radar_thread` 逐口 pump 前调用（`HAL_GetTick`）。

- **验证记录（定长/不定长广播收发，驱动未改）**：交替广播模型（一拍只广播一类包：定长 0x55 ↔ AA 0xAA，负载档 0/8/32/N 轮换，收端 Check/aa5 分别计 rxcnt）：
  - 50ms 拍 + 50B：txcnt=rxcnt、errcnt=0（0 丢包，~4min）；
  - 50ms 拍 + 128B(134B帧)：24600/24598、errcnt=0（丢2包 0.008%，~4min）；
  - 20ms 拍 + 128B：55160/55153、errcnt=0（丢7包 0.013%）；
  - 结论：链路无 CRC 错；偶发整帧丢失率随 广播频率×帧长 轻微上升，机理为收端共享中断(USART3_6)在 5 口同时回显高峰偶发溢出整帧丢弃（非主循环/FIFO 满）。业务 ≤50B@50ms 实测 0 丢；>50B/高并发如需 0 丢 → 大包逐口发或 RX-DMA（未实施，备选）。

- `[hc32f460]` **refactor**: 重构 `Radar_Led_update`（`bsp_exint.c`）：新增统一助手 `led_blink_update(有效则Start/无效则Stop)`，按“报警中 / 安装模式(C1 AICAM+Radar / C2 AICAM / C3 Radar / C4 EAS / C5 Light)”分组，逻辑清晰化；**修复 C3(仅雷达)缺失的停止分支**——`bsp_get_radar_singal()==false`(无人)时停止 Radar_LED/B 灯（节拍 300）；C4/C5 原“B 灯选中即闪”行为按遗留保持。

- `[hc32f460]` **feat**: STM32↔HC32 链路支持不定长(0xAA)帧收发（老格式冻结不变）：
  - `bsp_rs485.c`：UART4 RX DMA 满 32B 的 TC 立即上抛并 `AOS_SW_Trigger()` 重挂（连续字节流可跨 32B 块），空闲超时只补推实际尾部字节（`got = 32 - count`，>0 才写）——支持 >32B/不定长连续接收；
  - `common.c`：新增帧核心（0xAA：AA+Len+Cmd+Addr+Payload+CRC16，Len=Addr+Cmd+Payload；老 0xFF/0x55 按 32B 帧扫描），`Check_Uart_Pdu` 改为逐字节泵；老帧解析/GPIOHEAD 应答/报警判定逻辑不变；0xAA cmd 0x01 回 0x81 回显（变长验证用）。
- `[stm32f0]` **feat**: 通道口(COM2..6) 不定长(0xAA)收发（IPC/COM1 Linux 段未动）：
  - `app.c` 新增同款帧核心与每口解析泵（仅 GET_RADAR_ENABLE=1 时编译）；legacy GPIOHEAD 应答更新 `Chaneel_ID[]`，改为“150ms 无新帧才清”（带迟滞，防误判无人）；
  - 每 1s 向 5 口发 cmd 0x01 变长 ping（负载 0/8/32/80B 轮换），HC32 回 0x81 验证双向变长收发；
  - 50ms 老 GPIOHEAD 轮询保留。

- **docs**：新增《Boot/OTA 设计稿 v0.1》（`docs/ota_boot_design.md`）：**A/B 双槽即运行区、无搬运**；选择器标志（双份+CRC）为选槽唯一真值，版本号不参与选槽；Boot 只做“读标志→校验(Magic/ImageLen/整包CRC32/TargetSlot)→跳槽”，TRIAL 试运行窗口(3s)+失败计数自动回退；按槽分别编译两份镜像（STM32 M0 无 VTOR 需向量表重映射，HC32 用 VTOR）；下载期页粒度擦写非活动槽，掉电矩阵任意时刻不砖；含 Linux→STM32→HC32 中继与内存布局（STM32 Boot16K/A64/B64/标志4K；HC32 Boot32K/A128/B128/标志8K）。**未改动任何固件代码**。

- **docs**：新增《设计定稿备忘 2026-09-04》（`docs/decisions_2026-09-04.md`）：汇总当日决策——老格式冻结(0xFF/0x55)+0xAA 变长新帧(Len+Cmd+Addr+Payload+CRC16)、按帧头分流的状态机接收引擎与三判决点/滑窗重同步、实时性=事件+周期+迟滞、流水线轮询、OTA A/B 槽/代理缓存、已确认产品口径与明天开工顺序。未改动任何固件代码。
### [stm32f0] BSP / app

- **docs**: 按要求**精简**为"上行包格式定义 + 数据包解析"：
  - `docs/stm32_uplink_interface.md` 只保留：上行包帧结构表(32B/帧头 `0xFF`/CRC 覆盖 0..29 小端)、**数据包结构体定义**（与固件 `BSP/app.h` 的 `gpio_pdu` 完全一致：`FrameHead/Pdu_len/DeviceID/AntID/Rad_Status[8]/Alarm_Done[8]/GPIO[10]/uint16 crc`，32B 无填充）、字段定义与示例帧；删除了下行协议、串口参数、时序/心跳、健壮性建议、天线映射推导等与"上传包格式"无关的内容；
  - 解析代码收敛为单文件 `docs/stm32_gpio_pdu.hpp`：`crc16_ccitt()` + `GpioPdu` + `parse_gpio_pdu()`（校验帧头/帧长/CRC 后取字段），**不含收发、缓存、状态管理、统计与示例程序**；删除 `docs/cpp/`（示例程序、README、旧解析器）。

- **docs**: 新增 **Linux 侧接口文档 + 解析库**（供 Linux 主机解析 STM32F0 中继板上行帧）：
  - `docs/stm32_uplink_interface.md` **v1.0**：串口参数（COM1 @115200 8N1）、0xFF/32B 定长帧壳与 CRC-16/CCITT-FALSE（poly 0x1021/init 0xFFFF，覆盖 0..29，小端，自检向量 `"123456789"`→`0x29B1`）、上行 `gpio_pdu` 字段表（`Rad_Status[8]`/`Alarm_Done[8]` 按 **8 支 RFID 天线**、`GPIO[10]` 预留）、5 路雷达板→8 天线映射、发送时机（变化即报 / 1s 心跳 / 命令回执）、下行 `alarm_pdu` 字段表与 `AntID`=天线号 1..8 语义、LED 颜色码、键壮性/超时建议、CRC 参考实现（C/Python）与待定字段清单；
  - `docs/cpp/stm32_gpio_pdu.hpp`：仅头文件 C++11 解析库 —— `crc16_ccitt()`、`GpioPdu`（按天线号 1..8 的 `present()/alarming()`）、流式 `GpioPduParser`（任意切分喂入、逐字节重同步、CRC 校验、帧/CRC 错/重同步统计、回调或 `pop()` 取帧）、`GpioPduStatus`（状态快照 + 变化判定 + 链路判活，默认 2.5s）、`build_frame()` 自测组帧；
  - `docs/cpp/example_gpio_pdu.cpp` + `docs/cpp/README.md`：示例程序（`--selftest` 无硬件自测 / `-d /dev/ttySx -b 115200` 读串口 / 从 stdin 读）与编译说明（`g++ -std=c++11 -I docs/cpp ...`）。
  - 解析算法已用等价 Python 镜像验证：CRC 自检向量、整帧/逐字节喂入、噪声+半帧重同步、坏 CRC 丢弃、心跳帧不判变化。

- `[stm32f0]` **refactor**: LED 并发策略改为**"单一所有者 + 请求标志"**（承接上一条）：`LED_T` 增加 `ucStopReq`；`LED_Pro()`（SysTick 中断里运行，是 `LED_T` 的唯一所有者）开头先处理停止请求并调用 `Led_pwr_init()` 收尾（清全部计数 + `bsp_LedOff`）；`Led_Stop()` 退化为**只投递请求** —— 写 `ucEnalbe=0` + `ucStopReq=1` + 立即 `bsp_LedOff()`，**不再调用 `Led_pwr_init()`、彻底不需要临界区**（`mutex_led_lock/unlock` 与其 PRIMASK 备份变量整体删除），可在任意上下文安全调用；`LED_Start()` 改为"先写完整参数、最后置 `ucEnalbe=1`（单字节写原子）并撤销未决停止请求"，消除中断读到半套参数的撕裂。代价：`Led_Stop()` 后状态机最迟在下一个 10ms tick 内彻底停止（物理熄灭是立即的）。`bsp_led.h` 同步更新 `LED_T` 定义与注释。

- `[stm32f0]` **refactor**: `bsp_led.c` 的 LED 互斥改为标准临界区。原来 `mutex_led_lock/unlock` 用 `HAL_SuspendTick()/HAL_ResumeTick()`（低功耗 API）来挡 SysTick，等于**每次 `Led_Stop()` 都停掉全局时基** —— 临界区内的 tick 被永久丢失、`HAL_GetTick()` 少走，而 `bsp_RunPer10ms`/`BEEP_Pro`/雷达判活(`radarStaleMs`)/触发保持窗口/上行心跳全部依赖该时基；同时 `mutex_led` 标志只写不读，是假互斥。现改为 `__get_PRIMASK()` + `__disable_irq()` / `__set_PRIMASK()`：屏蔽期间 SysTick 异常只是**挂起**、解锁后立即补执行，**时基不丢**，也不再动用 HAL 低功耗 API；临界区仅几十条指令（约 1~2µs），对 460800 串口中断无影响。删除只写不读的 `mutex_led`；`bsp_led.h` 注明 lock/unlock 需成对且不可嵌套。

- `[stm32f0]` **chore**: 调试期配置调整（现场调试用）—— `bsp.h` 关闭看门狗 `STM32F0_IWDG_ENABLE (1U) → (0U)`；`bsp.c` 把 `rd_idkey_fun()` 移出 `#if STM32F0_IWDG_ENABLE` 改为**上电必调**（UID 校验不再随看门狗开关失效），并把本板 UID 期望值由 `0x587B3B44` 更新为 `0x03852952`；`MDK-ARM/STM32F030.uvprojx` 编译优化由 `-O3/oTime` 改为 `-O0`（便于单步调试）。**注：量产烧录前需把 IWDG 打开、优化调回。**

- `[stm32f0]` **fix**: 修复"关闭某路雷达后 LED 常亮不更新 / 触发信号一直为 1"——根因:**STM32 侧的雷达状态只在收到 HC32 的 Cmd 0x10 应答时才写入**, 该口若不再应答(该路雷达被关闭、板子掉线、接线断开), `sPorts[i].radarVal` 会**冻结在最后一次的 1**, 保持窗口永不结束 → LED 常亮、`Host_IRQ` 恒为 1。修法(STM32 侧自己判活/实时刷新, 不依赖 HC32 侧):
  - `portRx_t` 增加 `lastRxMs`(该口最后一次**有效** Cmd 0x10 应答时刻), `radarPumpPort()` 收到有效应答时刷新;
  - 新增 `radarStaleMs = 200ms`(约 10 个轮询周期)与 `radarPortFresh()`/`radarPresence()`:**应答超时一律按"无人"处理**;
  - `radarTriggerOut()` 改用 `radarPresence()`, 保持窗口结束时**显式 `Led_Stop()` 熄灭该口 LED**(不再只靠 LED 驱动收尾), `Host_IRQ` 每拍都按当前状态重写;
  - 上行 `gpio_pdu` 组帧同样门控: 应答过期的口 `Rad_Status`/`Alarm_Done` 上报 0(新增 `sIpcReportVals` 存本拍实际值, 变化判定基于它), 避免 Linux 端看到冻结值。

- `[stm32f0]` **refactor**: 简化雷达触发输出实现——去掉自定义的 LED 指针表/刷新逻辑，**直接调用已有驱动**：`switch(口)` → `LED_Start(&Port_x_LED, PORTLED_x, 10, 10, 1)` 仅在"有人"上升沿调用一次（闪烁节拍交给 LED 驱动，由 `bsp_RunPer10ms()`→`LED_Pro()` 推进）；触发保持只用一个时刻数组 `sTrigMs[stmPortCnt]` + `radarTrigHoldMs = 1000ms`，窗口内为 1、全部口超时后 `HAL_GPIO_WritePin(..., GPIO_PIN_RESET)`。

- `[stm32f0]` **feat**: 新增**雷达有人触发输出** —— `app.c` 新增 `radarTriggerOut()`，在 `Radar_thread()` 每 `radarPollMs`(20ms) 一拍调用：任一口轮询到的雷达状态为"有人"（`sPorts[i].radarVal == 1`，来自 HC32 0xAA Cmd 0x10 应答 Byte0 的雷达位）时，①输出触发信号 `HAL_GPIO_WritePin(Host_IRQ_GPIO_Port, Host_IRQ_Pin, GPIO_PIN_SET)`；②点亮该口对应 LED `LED_Start(&Port_x_LED, PORTLED_x, 10, 10, 1)`；两者保持 `radarTrigHoldMs = 1000ms`（窗口内持续有人则每拍刷新续期，保持为 1/常亮；窗口结束后 `Host_IRQ` 拉低、LED 由 `LED_Pro` 收尾熄灭）。口→LED 映射：COM6→Port_1_LED/PORTLED_1、COM2→Port_2、COM3→Port_3、COM4→Port_4、COM5→Port_5。
- `[stm32f0]` **fix**: `bsp.c` 中 `Host_IRQ(PA12)` 上电初始化电平由 `GPIO_PIN_SET` 改为 `GPIO_PIN_RESET`（触发信号为 1，空闲必须为 0，避免上电即出现假触发）；并清掉 `radarPumpPort()` 里遗留的空 `if` 块。

- `[stm32f0]` **clean**: 清理 `BSP/bsp.c`、`BSP/bsp_beep.c` 中未使用的代码（`bsp.c` 353 → 313 行，`bsp_beep.c` 175 → 132 行，`bsp_beep.h` −4 行，`bsp.h` −1 行）：
  - `bsp.c`：删除 `STM32F030_delay()`（0 调用，`bsp.h` 中对应原型一并删除）与 HAL 断言钩子 `assert_failed()`（`stm32f0xx_hal_conf.h` 里 `USE_FULL_ASSERT` 处于注释状态，`assert_param` 展开为 `(void)0`，无人调用——**若今后启用 `USE_FULL_ASSERT` 需把它加回**），连同其 `#ifdef USE_FULL_ASSERT` 空壳与两段孤立注释；删除 `bsp_Init()` 中指向已删函数的注释调用 `// EXTI4_15_IRQHandler_Config();`。
  - `bsp_beep.c`：删除 `mutex_beep_lock()`/`mutex_beep_unlock()`（只被 `BEEP_Stop` 调用）、`BEEP_Stop()`（只被 `BEEP_Pause`/`BEEP_Resume` 调用）、`BEEP_Pause()`、`BEEP_Resume()`、`BEEP_KeyTone()`（三者 0 调用）及静态变量 `mutex_beep`、`bsp_beep.h` 中对应 5 条原型；另外删掉被注释掉的旧 `BEEP_ENABLE/BEEP_DISABLE`（引用已删的 `GPO_BZ_GPIO_Port/GPO_BZ_Pin`）与 `BEEP_InitHard()` 里注释掉的 HC32 风格 GPIO 初始化片段。保留 `BEEP_InitHard`/`BEEP_Start`/`BEEP_Pro`、`g_tBeep`、在用的一对 `BEEP_ENABLE/BEEP_DISABLE`（`GPO_BZ3V3_*`，被 `BEEP_Start`/`BEEP_Pro` 使用）。

- `[stm32f0]` **clean**: 仅清理 `BSP/bsp_uart_fifo.c`/`bsp_uart_fifo.h` 中未使用的接口（`bsp_uart_fifo.c` 1441 → 1133 行，−308 行；**其它文件未动**）：删除 `comSendChar()`（唯一调用者 `fputc()` 本次一并删除）、`fputc()`/`fgetc()`（stdio 重定向，工程内无 `printf`；且 `fgetc` 阻塞在 COM1 会破坏 IPC）、`uart_recv()`、`UartGetRxcnt()`、`comClearTxFifo()`、`comClearRxFifo()`、`comSetBaud()`、`bsp_SetUartParam()`（只被 `comSetBaud` 调用）、`ComToUSARTx()`（只被 `comSetBaud` 调用）、`UartSendBlocking()`（含文件顶部的前置声明），以及它们在 `bsp_uart_fifo.h` 的全部原型和各函数上方的 doc 注释块。保留全部在用接口：`bsp_InitUart`/`UartVarInit`/`InitHardUart`/`UartSend`/`UartGetChar`/`UartIRQ`+`uart_dma_rx_move`/`uart_dma_rx_cfg`/`UartIRQ_DmaIdle`/`uart4_dma_tx_start`/`uart6_dma_tx_start`/`UartTxEmpty`/`UartTxWait`/`comSendBuf`/`comGetChar`/`ComToUart`、6 个 `MX_USARTx_UART_Init`、5 个中断处理函数，以及 `UART_T` 全部成员、`UARTx_FIFO_EN`/`UARTx_BAUD`/`UARTx_*_BUF_SIZE`/`UARTx_DMA_RX`/`UART_DMA_LEN` 宏与 6 组收发缓冲。

- `[stm32f0]` **clean**: `BSP/bsp.h` 删除未使用的宏与声明（126 → 116 行）：`__STM32H7_BSP_VERSION`（含其上方注释行）、`EXTI9_5_ISR_MOVE_OUT`、`ERROR_HANDLER()`、`GPI_IN1_GPIO_Port`/`GPI_IN2_GPIO_Port`/`GPI_IN3_GPIO_Port`（`bsp.c` 的 `MX_GPIO_Init` 直接写 `GPIOC`，只用到 `GPI_INx_Pin`）、`GPO_BZ_GPIO_Port`（仅被 `bsp_beep.c` 里注释掉的旧 `BEEP_ENABLE/DISABLE` 行引用）以及既无定义也无引用的原型 `bsp_GetCpuID()`。保留全部在用项：`GPI_INx_Pin`/`GPI_IN4_GPIO_Port`、`GPO_BZ3V3_*`、`LED_B/R/G_*`、`MCULED_x_*`、`GPO1/2_*`、`CM4RESET_*`、`Host_IRQ_*`、`GPO_BZ_Pin`、`STM32F0_IWDG_ENABLE`(`1U`)/`GET_RADAR_ENABLE`、`ENABLE_INT`/`DISABLE_INT`、`BSP_Printf`、`TRUE`/`FALSE`、`STM32_V7`/`USE_RTX`、各 `#include` 与 `bsp_Init`/`bsp_Idle`/`System_Init`/`Error_Handler`/`STM32F030_delay`/`CM4_System_Reset` 原型。同时纳入工作区里 `STM32F0_IWDG_ENABLE` 由 `0U` 改 `1U` 的改动。

- `[stm32f0]` **clean**: 仅清理 `BSP/bsp_gpio.c`/`bsp_gpio.h` 中未使用的代码（**其它文件未动**）：删除 `beep_on()`、`beep_off()`、`gpo_set()`、`PIO_GpioRead()`、`PIO_GpioSet()`、`gpi_get()`、`gpi_get_all()`、`EXTI4_15_IRQHandler_Config()` 及其在 `bsp_gpio.h` 的全部原型（`gpo_set` 只被自身原型引用，`beep_on/off` 只被 `gpo_set` 调用，`PIO_GpioRead` 只被 `gpi_get/gpi_get_all` 调用，`EXTI4_15_IRQHandler_Config` 在 `bsp.c:95` 只有一行注释引用）。保留 `PIO_GPIOInit()`（`bsp.c:99` 调用）、`EXTI4_15_IRQHandler()`（向量表）、`HAL_GPIO_EXTI_Callback()`（HAL 回调）及文件内的 `/* END OF FILE */` 注释；`bsp_gpio.c` 135 行 → 44 行，大括号 4/4 配平。备注：`bsp.h` 里的 `GPI_IN1..4_Pin/GPIO_Port` 随 `PIO_GpioRead` 一起失去引用，但按要求未动 `bsp.h`。

- `[stm32f0]` **clean**: 仅清理 `BSP/bsp_led.c`/`bsp_led.h` 中未使用的函数、变量与原型（**其它文件未动**）：删除 `LED_GPIO_Init()`、`Alarm_Off()`、`Check_alarm_status()`、`Green_pass_Tag()`、`Reguler_Tag()`、`Relay_AM_EAS()`、`Alarm_SilenceCmd()`、`Alarm_On()`（后 4 个仅被前 4 个死函数调用，构成调用闭环，全工程引用数 0 已逐项核对）、变量 `alarm_duration`/`rgb_led_status`、`extern BEEP_T g_tBeep;`，以及 `bsp_led.h` 中无定义也无引用的原型 `bsp_LedToggle()`。保留全部在用接口：`bsp_InitLed`/`bsp_LedOn`/`bsp_LedOff`/`LED_Start`/`Led_Stop`/`LED_Pro`/`Led_status_update`/`Led_pwr_init`/`mutex_led_lock`/`mutex_led_unlock`/`bsp_RunPer10ms` 及 `PORTLED_x`/`LED_xLED` 宏。

- `[stm32f0]` **clean**: `BSP/app.c`/`app.h` 删除未使用的应用层代码（脚本全工程符号引用统计确认 0 引用，**库文件 `Drivers/`、armfly BSP 驱动、HAL 回调/中断向量/stdio 重定向一律未动**）：未用函数 `rfid_app()`、`Alarm_CMD()`（其唯一调用者）、`CalcCRC()`（`ipcCrc()` 保留且仍用 `CRC_calcCrc8()`）；未用宏 `frameTestPing`（连同 `#if frameTestPing` 的 `pingOne()` 自检块）、`ipcReportPort3b`、`NONE_EAS_CODE`/`AUX_EAS_CODE`（各重复定义两份）、`ALARM_NONE_CODE`、`ACT_TAGDATA`/`ACT_GPICHANGE`/`ACT_TAGCOMING`、`MAX_EPCLEN`、`MAX_BOARD_CNT`；未用变量 `uart3Tx..uart6Rx` 及只写不读的 `radarCntTx()`/`radarCntRx()` 计数链、`portRx_t.lastRcvMs`（成员与其赋值）；未用原型 `rfid_app`/`Alarm_CMD`。**保留**：`ipcReportVar20En=0` 包住的 0xAA Cmd 0x20 变长上行（按约定后续要用）、`frameEvVarBad` 返回值、`frameAaEn`/`GET_RADAR_ENABLE` 等特性开关。

- `[stm32f0]` **clean**: Linux 链路帧头统一为 **PDUHEAD(0xFF)**，**GPIOHEAD(0x55) 废弃并删除**（响应帧头 = 下行帧头 = PDUHEAD）：上行 `gpio_pdu` 的 `FrameHead` 改为 `PDUHEAD`；`app.h` 删除 `GPIOHEAD`/`RADAR_PDU_LEN`/`radar_pdu` 结构体/`Send_RadarStatus_to_Master()` 原型；`app.c` 删除 `Send_RadarStatus_to_Master()`(8B 应答)、`Broadcast_Get_Radar_Status()`、`Check_RadarStatus()`、`stmHandleLegacy()`(通道口 0x55 解析)、旧计数式 `Check_Uart_Pdu()`(`#if !frameAaEn` 分支)、`Chaneel_ID[]`/`refresh_chaneel()`（随 8B 应答一起失去作用）及 `txcnt/rxcnt` 诊断计数；帧核心去掉 `frameHdrLegGpio`，定长 32B 分支只由 `0xFF` 触发；`ipcHandleLegacy()` 只认 PDUHEAD。最终：COM1 下行 = PDUHEAD 32B（0xAA 变长预留），上行 = PDUHEAD `gpio_pdu` 32B（变化即报 + 1s 心跳 + 查询立即应答）。**STM32↔HC32 侧仍为 0xAA Cmd 0x10 查询/3B 应答，未改**。

- `[stm32f0]` **change**: STM32→Linux(COM1) 上行**统一为 `gpio_pdu` 单一格式**，老的 8B `radar_pdu` 应答停用（`ipcRadarPduRptEn=0`，函数与代码保留）：Linux 下发 `0xFF` PDUHEAD(AntID≠0) 时不再回 8B 帧，改为 **立即应答一帧 32B gpio_pdu**（`ipcReportOnQuery=1` → `ipcReportForce()`）。gpio_pdu 组帧/发送拆为 `ipcReportBuild()`/`ipcReportSend()`，周期路径 `ipcReportStatus()` = 变化即报 + 1s 心跳，查询路径 `ipcReportForce()` = 立即发送。**0xAA 变长上行（旧 Cmd 0x20）后续要用，以 `ipcReportVar20En=0` 保留**；STM32↔HC32 侧 0xAA Cmd 0x10 链路不变。

- `[stm32f0]` **chore**: 明确 COM1 上行**两路并存**并加开关——①主动：32B `gpio_pdu`（变化即报 + 1s 心跳）；②应答：Linux 下发 `0xFF` PDUHEAD 且 AntID≠0 时回 8B `radar_pdu`（`GPIOHEAD`，`alarm_done=Chaneel_ID[AntID]`）。新增 `ipcRadarPduRptEn`（默认 1=保留老应答，置 0 即只走 32B 主动上报）；`Send_RadarStatus_to_Master()` 增加通道号 1..8 越界保护（Linux 可下发任意 AntID，原实现 `Chaneel_ID[antid]` 会越界读）并在发送前 `UartTxWait(COM1,5ms)`，避免与 32B 心跳帧撞车。

- `[stm32f0]` **change**: STM32→Linux(COM1) 上行格式改为 **Linux 给定的 `gpio_pdu` 定长 32B 帧**（`app.h` 新增该结构体）：`[0x55][Pdu_len=32][DeviceID][AntID][Rad_Status[8]][Alarm_Done[8]][GPIO[10]][CRC16_L][CRC16_H]`（CRC 覆盖前 30B，与老帧一致）——**变化即报 + 1s 心跳**。`Rad_Status[n]`/`Alarm_Done[n]` 由 5 个通道口状态按“通道号 1..8 → 下标 0..7”填入（一个口可覆盖 2 个通道）；顺带把 `Chaneel_ID[8]` 扩为 `Chaneel_ID[9]`，修掉通道 8 的越界写。`GPIO[10]`/DeviceID/AntID 语义待 Linux 侧确认（暂填 0）。旧 0xAA Cmd 0x20（5×3B）上报以 `ipcReportVar20En=0` **代码保留**（`ipcReportStatusVar20()`）。**STM32↔HC32 侧 0xAA Cmd 0x10 查询/3B 应答完全不变。**


- **fix**: COM1(Linux IPC) 接收由“`UartGetRxcnt>=32` 定长取包”改为**帧头分流状态机**（`app.c` 新帧核心 `frameRxInit/frameRxGuard/frameRxFeed` + `frCrc16`）：`0x55`/`0xFF` 按 32B 定长、`0xAA` 按 `AA+Len+Cmd+Addr+Payload+CRC16` 变长；`PDUHEAD` 帧分发 `ipc_hpm_message()`（GPIOHEAD 查询应答暂 `#if 0`）。修复两处接收失效：泵内未刷新 `lastByteMs` 导致 `frameRxGuard` 每拍误复位状态机（Linux 包收不到）；CRC 失败时误调 `comClearRxFifo` 破坏与中断共享的 FIFO 索引（改为直接丢弃、下一个帧头重同步）。
- **feat**: 上行新增 **0xAA Cmd 0x20 五口状态聚合帧**：`[AA][Len=17][0x20][00][5×(gpioIn,workMode,alarmDone)][CRC16]`，数据源为 0xAA Cmd 0x10 应答（雷达位图 / 安装模式位图 / 报警完成），**变化即报 + 1s 无变化心跳**；`Radar_thread` 周期（`radarPollMs=2000`）向 5 口发 Cmd 0x10 查询（COM6/COM2/COM3/COM4/COM5 ↔ 通道 1..9）。
- **fix**: 发送前增加 **TX 空闲等待** `UartTxWait(port, 5ms)`（`bsp_uart_fifo.c/.h` 新增，基于 `UartTxEmpty`）：消除 UART4/UART6 DMA 忙时 `uart4/6_dma_tx_start` **静默丢包**（雷达查询与 Linux 报警包并发时丢报警）。
- **perf**: `UartGetChar` 增加“FIFO 空”快速路径（无临界区直接返回），仅在有数据时进一次临界区出队——主循环逐字节泵的关中断开销显著下降。
- **refactor**: `app.c` 本轮新增标识符统一改为 **lowerCamelCase**（函数/变量/宏共 51 个，如 `frameRxFeed`/`stmVarSend`/`radarQueryAll`/`sPortCom`/`frameRxGuardMs`/`frameAaEn`）；`portRx_t` 删除未使用成员（`port/rxByteCnt/varCnt/varCrcFail/legCrcFail/varAddr`）。
- **chore**: `bsp.c` 中 `CM4_System_Reset()` 移入 `#if STM32F0_IWDG_ENABLE`（与看门狗使能一致，不再无条件拉复位脉冲）。

### [hc32f460] projects/source

- `[hc32f460]` **fix**: 0xAA Cmd 0x10 应答 Byte2 `alarm_done` 原来是**写死的 1**（占位: "answering 期间恒 1"），导致 STM32 上行 `gpio_pdu.Alarm_Done[8]` 8 个通道全是 1，即使 Linux 从未下发报警包。改为**实时报警状态**：`out[2] = (R_tLED.ucEnalbe || g_tBeep.ucEnalbe) ? 1 : 0`（红灯在闪或蜂鸣器在响 = 1，报警结束 `Alarm_Off/Check_alarm_status` 后自动回 0）；`bsp_report.c` 增加 `extern LED_T R_tLED; extern BEEP_T g_tBeep;`，`bsp_report.h` 注释同步。STM32 侧无需改动（Byte2 透传到 `Alarm_Done[ch]`，并已按应答新鲜度门控）。注：绿放行 `G_tLED` 不计入报警，如需计入再加一个判断。
- `[stm32f0]` **docs**: 明确 `gpio_pdu` 的 `Rad_Status[8]`/`Alarm_Done[8]` 是**按 RFID 模块 8 支天线**（下标 = 天线号 1..8），而 STM32 侧只有 **5 路雷达板**（COM6/COM2/COM3/COM4/COM5）：一块雷达板覆盖 1~2 支天线，板级状态复制到其全部天线 —— `COM6→天线1、COM2→2,3、COM3→4,5、COM4→6,7、COM5→8`（`sPortCh` 加中文注释说明，组帧处措辞由"通道"改为"天线"）。`Alarm_Done` 语义确认为 **A：该天线所属雷达板正在声光报警**（HC32 红灯/蜂鸣器在动作，停即回 0）。README 的 `gpio_pdu` 字段表与映射说明同步更新。
- `[hc32f460]` **feat**: **重写雷达驱动**（原 `bsp_radar.c` 753 行整段删除）—— LD2410C 从"仅读模块 OUT 引脚"扩展到"串口精细判定 + 参数读写 + 底噪自检"，分层、全非阻塞：
  - 新增 `radar_cfg.h`（配置集中：UART 实例/引脚/波特率候选/DMA/TMR0/缓冲/超时/设备数）、`radar_port.c/.h`（USART1 + DMA2 CH1/RX + TMR0 空闲超时 + 软件环形缓冲，按**实收字节数**上抛；DMA2 CH0/TX 非阻塞发送 + busy 标志）、`radar_frame.c/.h`（纯逻辑分帧：4 字节魔术字滑窗 → 按 `datalen` 变长收帧 → 帧尾校验 → 半包超时复位 + 统计，可 PC 单测）、`radar_proto.c/.h`（命令组帧、ACK 按命令字匹配、上报帧解析 0x02 普通/0x01 工程、参数解析）、`radar.c/.h`（设备状态 + `radar_poll()` 非阻塞主循环 + 命令事务 + 参数/底噪 API + 有人判定策略）；
  - **波特率自适应**：由 `radar_poll()` 非阻塞推进，依次试 256000/460800/115200，谁能回"使能配置(0x00FF)"ACK 就锁定谁，全失败回落 256000；`radar_ready()` 表示探测结束；
  - **解析**：目标状态(0 无/1 运动/2 静止/3 动静/4 底噪检测中/5 成功/6 失败)、运动与静止距离(cm)与能量、探测距离；工程模式额外 9 个距离门能量 + 光感值 + OUT 脚状态；
  - **命令**：`radar_read_params()`(0x61)、`radar_set_sensitivity()`(0x64)、`radar_set_max_gate()`(0x60)、`radar_set_resolution()`(0xAA)、`radar_set_uart_baud_index()`(0xA1)、`radar_eng_mode()`(0x62/0x63)、`radar_noise_start()`(0x0B)/`radar_noise_status()`(0x1B)，内部自动"使能配置→命令→结束配置"；
  - **有人判定可选源** `radar_set_presence_src()`：OUT(默认，保持现行为) / UART / 二者取或 / UART 优先掉线回落 OUT；串口数据 1s 无更新视为离线；
  - **集成**：`main.c` 在 `SysTick_Init()` 之后 `radar_init()`、主循环 `radar_poll()`（替换原 `Check_Radar_state()`）；`main.h` 改 include `radar.h`；`common.c` 老 32B 确认帧的雷达字段改取 `radar_report(0)`；`bsp_report.c` 注释同步；`alarm_board.uvprojx` 增删源文件；
  - **删除**：`bsp_radar.c`/`bsp_radar.h` —— 含从未被调用的 `Rardar_init()`、阻塞收发、固定 45B 匹配、`get_pdu_len`/`config_frame_init` 调试代码，以及 3 个只有声明没有定义的 `Radar_singal_input/output`/`Alarm_Out_Enable`；
  - **待硬件确认**：雷达串口按旧宏沿用 **USART1 / PA2(TX,FUNC32) / PA3(RX,FUNC33)**（旧文件注释里的 PB9/PE6 是错的）；若 PCB 实际不同，只需改 `radar_cfg.h` 中的 8 行。
- `[hc32f460]` **fix**: 雷达驱动重构后的**编译修复**（Keil MDK ARMCC V5.06 update 7，目标 `usart_uart_dma_Debug`）：
  - `main.h`：补回 `stc_radar_scan_data_t` 结构体（老 32B 确认帧的雷达字段，字段名与 `common.c` 中 `radar_report_t` 的赋值一一对应）与 `HashConfig()` 原型；
  - `bsp_led.h` / `bsp_led.c` / `bsp_gpio.h`：补回 `BOARD_LED_1/2_PORT/PIN`、`RADAR_BOARD_LED_G_PORT/PIN` 宏与 `Board_LED_Init()`（板载指示灯初始化，`main.c` 上电自检调用；`Board_LED_On/Off/Toggle` 无调用者，未恢复）；
  - `radar.c`：补 `#include "radar_port.h"` —— 原先缺失导致 9 个 `radar_port_*` **隐式声明**告警（C 中隐式声明按 int 返回，`radar_port_get_baud()` 的返回值会被截断，属实质缺陷而非纯告警）；
  - **构建验证**：以 Keil 命令行无头构建复核（`UV4.exe -b alarm_board.uvprojx -j0 -o <log>`）—— `Code=29636 RO-data=760 RW-data=80 ZI-data=8232`，**0 Error / 0 Warning**（Debug 目标；Release 目标共用同一份源文件清单）；
  - **硬件配置复核**（与 git 历史中的旧 `bsp_radar.c/.h` 逐项比对，非推测）：串口 `CM_USART1`、TX=**PA2/FUNC32**、RX=**PA3/FUNC33**、OUT0/1/2=**PC14/PC13/PH02** —— 新 `radar_cfg.h` 与旧工程完全一致（旧文件里的 `PB9`/`PE6` 是写错的残留注释）。

- `[hc32f460]` **feat(调试)**: 新增雷达**上板验证**调试段（声明在 `radar_dbg.h`，实现在 `radar.c` 末尾的 `#if (RADAR_DBG_EN != 0U)` 段；总开关 `radar_cfg.h` 的 `RADAR_DBG_EN`，验证通过后可整体删除）：
  - 输出：结构体 `g_radar_dbg`（Keil Watch 一眼看全：rdy/lock/baud/rep/fok/fer/rx/drp/st/运动与静止距离能量/dd/out/online/pre）＋文本行 `g_radar_dbg_line`＋事件行 `g_radar_dbg_evt`（boot / 自适应探测结果 / 目标状态跳变 / 帧错误 / rx 停滞）；另可选 ITM(SWO) 与 RS485 主机口 ASCII 输出（`RADAR_DBG_SINK_ITM` / `RADAR_DBG_SINK_RS485`，默认关，后者会与 STM32 的 20ms 查询抢总线故默认关闭）；
  - 只调用驱动公开接口读状态，不碰驱动内部；不引入 printf（自带极简整数转 ASCII），`RADAR_DBG_EN=0` 时为空实现、不占 Flash；
  - 新增常驻诊断接口 `radar_rx_bytes()` / `radar_rx_drop()`（串口累计收字节数 / 环形缓冲丢弃数）——区分“没收到字节(接线/波特率)”与“收到但分帧失败(格式)”，现场排查用；
  - `main.c` 主循环增加 `radar_dbg_poll()`（内部 500ms 节流）；验证步骤 / 字段速查 / 现象判读表见 `docs/hc32_radar_bringup.md`；
  - **刻意不新建 .c 文件**：Keil GUI 打开工程时会用内存中的工程覆盖 `.uvprojx` 的改动（实测把已加入工程的 `radar_dbg.c` 覆盖掉，链接报 `L6218E: Undefined symbol radar_dbg_poll`），故调试段并入 `radar.c`，只需重新编译、不动工程文件；该工程目标另改为便于调试的设置（DebugInformation=1、Optim/oTime 由 -O3 改 0）。
- `[hc32f460]` **feat/fix(雷达波特率)**: 上板发现 `lock=0, baud=256000`（自适应三个候选都没收到 ACK，回落默认），针对性改动：
  - 新增 **固定波特率** `RADAR_BAUD_FORCE`（`radar_cfg.h`，当前 **460800UL**）：非 0 时跳过自适应、直接用该波特率（`radar_init()` 里生效，`radar_baud_locked()` 置 1）；填 `0` 恢复自适应。现场怀疑模块不在候选波特率里时逐个试最省事；
  - **放宽自适应命中判据**：原来要求 ACK 且 `status==0` 才算命中，现在 ① 只要收到 `0x00FF` 的 ACK（无论状态）即判定波特率正确（状态非 0 只表示命令没被接受）；② 没等到 ACK 但已收到 `RADAR_BAUD_LOCK_FRAMES(3)` 个**合法帧**也算命中（典型场景：模块 TX→MCU RX 通、MCU TX→模块 RX 断，此前会一路试到 115200 再回落）；
  - 调试段新增 **原始字节抓包** `g_radar_dbg_hex`（换波特率即清空，抓前 24 字节十六进制）：开头 `F4 F3 F2 F1`/`FD FC FB FA` 说明波特率正确，乱码说明波特率不对，空说明 RX 方向不通——这是区分“接线问题”和“波特率问题”的关键证据；
  - 调试段新增 **一次性改模块波特率** `RADAR_DBG_SET_BAUD_IDX`（默认 0=关；填 8 = 发 `0x00A1` 让模块切到 `RADAR_DBG_SET_BAUD_VALUE(460800)`，成功后驱动同步切波特率并重建分帧，结果记在 `g_radar_dbg_evt`）；
  - **候选波特率扩到协议表 6 全部 8 档**（256000/460800/115200/9600/19200/38400/57600/230400，无 921600）：
    模块波特率是**掉电保存**的配置项（协议 §2.2.9，出厂默认 0x0007=256000，改过就一直是改过的值），不能假设 256000；
    8 档全扫约 8×305ms≈2.5s（非阻塞），`RADAR_BAUD_FORCE` 复位为 `0`(自适应)，需要钉死时再填具体值。
  - `RADAR_DBG_SET_BAUD_IDX` 一次性改模块波特率改为**完整时序**：0x00A1 设置 → 0x00A3 重启模块（协议规定配置"重启后生效"，模块未切前驱动必须留在旧波特率）→ 800ms 后驱动再切到 `RADAR_DBG_SET_BAUD_VALUE` 并重建分帧，每步都记事件。
  - 构建验证：4 种组合（FORCE=0 自适应8档 / FORCE=460800 / FORCE=0+SET_BAUD_IDX=8 / FORCE=256000）均 0 Error 0 Warning。
- `[hc32f460]` **feat(把模块波特率改成 460800)**: `RADAR_DBG_SET_BAUD_IDX=8` 的一次性流程补齐**自检与回退**——使能配置 → `0x00A1(0x0008)` → `0x00A3` 重启模块（协议规定该配置"重启后生效"，模块未切前驱动必须留在旧波特率）→ 800ms 后驱动切到 `RADAR_DBG_SET_BAUD_VALUE` 并重建分帧 → 自检 2.5s 看有无上报帧：成功记 `baud verify OK 460800`；失败自动回退旧波特率再看 2.5s，分别记 `old baud still OK` / `no data on either baud: check wiring`。注：厂家固件里的"出厂默认 256000"无法更改（`0x00A2` 恢复出厂即回到 256000），本流程是把 460800 写进**模块自己的 flash**，从此这块模块上电就是 460800（每块需各做一次）。
- `[hc32f460]` **fix/feat(只读读回可诊断 + 重试)**: 加上 TX 兜底后 `s_dump.ok=3`（前 3 项读回成功, 命令通道已通）, `last_ret=-8(LL_ERR_TIMEOUT)` —— 最后两项（0x00A0 固件版本、0x00A5 MAC）模块没回 ACK。为定位并提高成功率：
  - `radar_dump_t` 增加**逐项返回码** `ret_params/ret_res/ret_aux/ret_fw/ret_mac`（一眼看出哪条命令失败、失败码是什么；`last_ret` 保留为最后一项的值）；
  - `radar_read_all()` 每项最多重试 `RADAR_READ_TRY(2)` 次, 项间插入 `RADAR_READ_GAP_MS(50ms)` 间隔（模块连续命令之间需要喘口气）, 延时期间仍由 `radar_pump()` 继续搬字节；
  - 构建验证: 0 Error 0 Warning（Code 28724）。
- `[hc32f460]` **fix(雷达 TX 卡死 -> 命令全部 LL_ERR_BUSY)**: 上板 `s_dump.ok=0`、`last_ret=0xFFFFFFFA`（= -6 `LL_ERR_BUSY`）—— 说明 `radar_port_tx_busy()` 一直为 1：发送完成链（DMA2_CH0 TC -> 使能 USART1 TCI -> 清 busy）任何一环没来，`s_tx_busy` 就永远不清，之后所有命令都发不出去（探测阶段的 `radar_send_raw` 也会静默跳过 -> 只能靠"上报帧"锁定波特率、产线配置必然失败）。改动：
  - **兜底自愈**：新增 `radar_port_tx_watchdog()`（由 `radar_pump()` 每拍调用），超过 `RADAR_TX_TIMEOUT_MS(50ms)` 仍未收到完成中断 -> 关 TX DMA/关 USART TX/清标志/重新使能，然后**放行后续发送**（否则一条卡死会永久废掉命令通道）；
  - **定位用计数**（Keil Watch 直接看名字）：`g_radar_tx_dma_tc_cnt`（TX DMA 完成次数）、`g_radar_tx_tci_cnt`（USART1 发送完成中断次数）、`g_radar_tx_timeout_cnt`（兜底复位次数）。判读：DMA TC=0 → DMA 没跑/没触发；TC>0 而 TCI=0 → 中断映射/使能问题；timeout>0 且命令能通 → 只是完成通知没来，已被兜底放行；
  - 构建验证: 0 Error 0 Warning（Code 28560）。
- `[hc32f460]` **feat(雷达参数: A 探测行为参数 + C 只读/维护)**:
  - **A 组新增写接口**: `radar_set_aux_control(mode, threshold, out_level)`（0x00AD 光感辅助/OUT 默认电平）—— 补齐 A 组最后一条；其余（`radar_set_max_gate` 0x0060 / `radar_set_sensitivity` 0x0064 / `radar_set_resolution` 0x00AA / `radar_eng_mode` 0x0062·0x0063 / `radar_noise_start·status` 0x000B·0x001B）此前已有；
  - **A 组自动配置（幂等，默认关闭）**: `radar_cfg.h` 的 `RADAR_PARAM_EN` + 一组目标值宏（最大运动/静止距离门、无人持续时间、9+9 门灵敏度、光感辅助、OUT 默认电平）。上电在自适应锁定波特率后：先 `0x0061`/`0x00AE` 读回当前配置与目标逐项比对 → **只写不一致的项**（每拍只发一条命令，不长时间占住主循环）→ 写后读回复检；全一致则一条命令都不发。状态 `radar_param_state()`（4 成功/本来就一致, 5 失败）。门 0/1 的静止灵敏度按协议不可设置，比对与写入均跳过；距离分辨率（需重启生效）不纳入自动配置；
  - **C 组新增只读/维护接口**: `radar_read_resolution()`（0x00AB）、`radar_read_aux_control()`（0x00AE，配 `radar_aux_t`）、`radar_read_fw_version()`（0x00A0，配 `radar_fw_t`）、`radar_read_mac()`（0x00A5）、`radar_factory_reset()`（0x00A2）—— 加上已有的 `radar_read_params()`（0x0061）与 `radar_restart()`（0x00A3），C 组全部补齐；
  - 新增 `radar_read_all()` + 汇总结构 `radar_dump_t`（`s_dump`）：一次把 参数/分辨率/辅助控制/固件版本/MAC 全读回，`RADAR_DUMP_ONCE(默认 1)` 时上电自动读一次，Keil Watch 直接看 `s_dump`（`ok=5` 表示 5 项全读回成功）—— 无串口条件下唯一的"看当前配置"手段；
  - `radar_proto` 层新增 `radar_proto_parse_aux/parse_fw/parse_bytes` 与命令字 `0x00AD/0x00AE`；`radar_link_ready()`（探测结束且产线配置跑完才允许发命令）；
  - 构建验证: 参数配置=0/1 × 读回=0/1 × 调试开/关 共 6 种组合均 0 Error 0 Warning（Code 27712 ~ 29056）。
- `[hc32f460]` **clean(删除 `out_pin`)**: 删掉 `radar_report_t.out_pin` —— 它是"**工程模式**上报帧里的 OUT 脚状态"字节，正常工作模式帧里根本没有该字段，我们工程模式默认关闭 → 该字段恒 0，纯死数据；OUT 的真实电平用 `radar_dev_t.out_present`（直接读 PC14，与模块配置无关）即可，两者信息重复。改动：
  - `radar_proto.h` 删除字段；`radar_proto.c` 删除普通帧的清 0 与工程模式帧的解析；`common.c` 删除 `HC32_RS485_corfirm_PDU.radar.pinout = rr->out_pin;`（该确认帧目前**只填不发**，其 32B 布局里的 `pinout` 占位保留、恒 0，已加注释说明）；
  - 构建验证: 0 Error 0 Warning（Code 27712，比改动前少 20 字节）。
- `[hc32f460]` **clean(调试快照瘦身)**: `g_radar_dbg` 删掉全部冗余成员（~200B → 72B），只留真正要看的：
  - 保留: `ms`(主循环活着) / `probe_st`(探测阶段) / `lock`(是否找到波特率) / `baud` / `prov_st`(目标波特率配置结果) / `rep`(上报数) / `fer`(分帧错) / `rx`(收字节数) / `rx_head[8]` / 目标状态与距离: `st/mv_dist/mv_eng/st_dist/st_eng/dd` / `out/online/pre`；
  - 删除: `repf/ackf/fok`（与 `rep/fer` 重复）、`drp`、`probe_idx`（`baud` 已表示正在试哪一档）、`ack_cmd/ack_status/ack_len/ack_data[8]`（ACK 布局排查用, 已完成使命）、4 条**事件环** `evt_cnt/evt_code[]/evt_val[]/evt_ms[]`；
  - 随之删除 `radar_dbg_note_u32()` 与全部 13 处调用、`radar_dbg_ack_dump()` 及其调用 —— 驱动代码更干净（`radar.c` 从 798 → 710 行）；
  - 构建验证: 目标460800+调试开 / 目标=0 / 调试关 / 两者都关 四种组合均 0 Error 0 Warning（Code 28108 / 27480 / 27732 / 27092）。
- `[hc32f460]` **refactor(波特率配置收敛为一个开关)**: 按使用方口径收敛 —— **上电自适应(不管模块当前是多少) → 用当前波特率把模块改成 RADAR_BAUD_TARGET(默认 460800, 写模块 flash) → 0x00A3 重启 → 重跑自适应复检 → 之后每次上电模块自己就是 460800**：
  - 配置项只留一个 `RADAR_BAUD_TARGET (460800UL)`（0 = 不自动配置, 只跟随模块）；原 `RADAR_PROVISION_BAUD` 并入它, `RADAR_PROVISION_RESTART_MS` → `RADAR_PROV_RESTART_MS(500)`（内部）；
  - **删除 `RADAR_BAUD_FORCE`**（强制固定波特率）与 `radar_init()` 里的对应分支：与"自动配置"重复, 且设错会让链路直接不通 —— 一律走"自适应 + 自动配置"；
  - 候选表把 `460800` 提到第一档（配置好之后一档命中, 约 1.1s 出数据）, 其余 7 档仅兼容未配置的模块；
  - 构建验证: `RADAR_BAUD_TARGET=460800` / `=0` / `=460800+调试关` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(产线配置复检方式)**: 上板"运行 30 秒后仍是 `baud=256000 lock=1`、模块没切成 460800" —— 原实现是"固定等 2.5s 听一个波特率", 模块重启耗时不定, 且若模块其实**已经**切到 460800 而这两秒半里没听到, 会**错误地回退**到 256000（此时模块在 460800、驱动在 256000 → 链路反而断了）。改为:
  - 写完 `0x00A1` + `0x00A3` 重启后, 等 `RADAR_PROVISION_RESTART_MS(500ms)` 就**重新武装自适应探测**（`s_probe_st=0`, 内部自带 `RADAR_PROBE_BOOT_MS(1s)` 启动延时）, 按同样的"上报帧"判据重新确定模块当前波特率；
  - 复检结果: 又锁定在目标波特率 → 成功（`prov_st=4`）; 锁定在别的波特率 → 保持那个波特率继续工作（`prov_st=5`）, 不再"猜一个波特率硬听再回退"；
  - 删除 `RADAR_PROVISION_VERIFY_MS` 与 `s_prov_rep`/`s_prov_baud`（不再需要固定时间窗与回退路径）；
  - 构建验证: 产线配置=460800 / =0 两种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(复位循环)**: 上板出现"一直在重启", 根因是上一版引入的**无限递归** —— `radar_provision_tick()`（由 `radar_poll()` 调用）里调用了阻塞式的 `radar_set_uart_baud_index()`/`radar_restart()`, 而它们内部走 `radar_cmd()` 等 ACK 时会调 `radar_poll()` → 又回到产线配置状态机 → 无限递归 → 栈溢出/主循环饿死 → 看门狗复位（周期约 10.7s = 65536×8192/PCLK3(50MHz)）。修法（结构性，不靠喂狗）：
  - 拆出**底层泵** `radar_pump()`（只做 分帧超时 + 搬运字节, 不跑探测/产线配置状态机）, `radar_poll()` = `radar_pump()` + 探测 + 产线配置 + OUT 脚采样；
  - `radar_cmd()` 等 ACK / 等 TX 的循环改用 `radar_pump()` —— 阻塞命令从此**不可能**再进状态机, 递归在结构上被消除；
  - 另加 `s_prov_busy` 忙标志作为兜底（阻塞命令执行期间 `radar_provision_tick()` 直接返回）；
  - 构建验证: 产线配置=460800 / =0 两种组合均 0 Error 0 Warning。
  - 注: 看门狗本身没问题 —— `WDT_FeedDog()` 由主循环里的 `Check_UidKey()` 每圈投喂, 超时按 `WDT_CNT_PERIOD65536`+`WDT_CLK_DIV8192`(PCLK3=50MHz) 约 10.7s, 平时远够。
- `[hc32f460]` **feat(产线配置模块波特率)**: 把"让模块波特率固定为 460800"做成**常驻功能**（在此之前只有调试段里那个临时开关，默认关、且会随调试代码一起删除）：
  - 新增 `RADAR_PROVISION_BAUD`（`radar_cfg.h`，**当前默认 460800UL**；0 = 关闭）、`RADAR_PROVISION_RESTART_MS(800)`/`VERIFY_MS(2500)`、`RADAR_BAUD_IDX_TABLE`（波特率 → 协议表 6 索引）；
  - 新增 `radar_provision_tick()`（`radar.c`，由 `radar_poll()` 驱动, 非阻塞、**幂等**）：自适应找到模块当前波特率后 —— 已是目标值则**什么都不发**；否则 `0x00A1` 写入 → `0x00A3` 重启模块（协议规定该配置"重启后生效", 故模块重启前驱动留在原波特率）→ 800ms 后驱动切到目标波特率 → 自检 2.5s：收到上报即成功, 收不到则**自动回退原波特率**继续工作；
  - 新增查询接口 `radar_provision_state()`（0 待做 / 1 写入中 / 2 等重启 / 3 自检中 / 4 成功或本就是目标值 / 5 失败已回退 / 6 无法配置），调试快照增加 `g_radar_dbg.prov_st`；
  - **删除**调试段里的一次性开关 `RADAR_DBG_SET_BAUD_IDX`/`RADAR_DBG_SET_BAUD_VALUE`（已被上面这条正式流程取代）；事件码 12~18 归入"产线配置流程"（去掉原 19/20）；
  - 产线用法（详见 `docs/hc32_radar_bringup.md`）：直接烧本固件, 每块板上电约 4.5s 完成配置（`prov_st=4` 即成功）, 模块 flash 里从此是 460800；出正式版本时可把 `RADAR_PROVISION_BAUD` 改回 0, 并**建议保留自适应**以防有人误做"恢复出厂"（模块会回到 256000）；
  - 构建验证：产线配置=460800 / =0 / =256000 / 产线开+调试关 / `FORCE=460800` 五种组合均 0 Error 0 Warning。
- `[hc32f460]` **refactor(调试输出)**: **删除雷达调试的全部串口/printf/文本输出** —— 本板没有连电脑的串口, 这些通道无用且占空间。具体:
  - 删除 ITM(SWO) 与 RS485 两个输出通道(`RADAR_DBG_SINK_ITM`/`RADAR_DBG_SINK_RS485`、`dbg_send_itm/dbg_send_rs485/dbg_sink`、`bsp_rs485.h` 依赖)；
  - 删除状态文本行与事件字符串(`g_radar_dbg_line`/`g_radar_dbg_evt`/`g_radar_dbg_hex`/`g_radar_dbg_ack` 及全部整数转 ASCII 助手)；
  - 调试信息改为**纯数值快照** `g_radar_dbg`（Keil Watch 展开看）: 在原有字段上补充 `probe_st`/`probe_idx`（探测状态机）、`repf`/`ackf`（上报帧/ACK 帧计数）、`rx_head[8]`（本档收到的最前面 8 字节, 判波特率用）、`ack_cmd/ack_status/ack_len/ack_data[8]`（最近一帧 ACK），以及 4 条**数值事件环** `evt_cnt/evt_code[]/evt_val[]/evt_ms[]`（码表见 `radar_dbg.h`）；
  - `radar_dbg_note_u32(code,val)` 取代原字符串版本, 探测每一步都记码；
  - 代码量下降（Code 28520 → 27916）, `RADAR_DBG_EN=0` 时 27120；
  - 构建验证: 自适应 / `SET_BAUD_IDX=8` / `RADAR_DBG_EN=0` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(波特率自适应判据)**: 上板出现"第一次 `lock=1 baud=9600`、第二次又回到 `256000`"的抖动 —— 说明判据不可靠。改动：
  - **判据从"任意 ACK"改为"上报帧"为准**：某档收到 ≥`RADAR_BAUD_LOCK_FRAMES(3)` 个上报帧直接认定；只收到 ACK 时先发 `0x00FE` 退出配置态, 再等 `RADAR_PROBE_VERIFY_MS(1000ms)` 用上报帧验证, 收不到就继续试下一档（ACK 帧仅 10 字节, 错波特率下的乱码/残留字节可能凑出魔术字+帧尾被误判；上报帧 13 字节且内容自校验）；
  - **上电先等 `RADAR_PROBE_BOOT_MS(1000ms)` 再探测**：模块没启动完成时命令不会被应答, 会让第一档(256000)误判失败, 一路试到尾后回落 256000；
  - **换波特率时丢弃接收缓冲残留字节**：新增 `radar_port_rx_flush()`（`radar_port.c`）并在 `radar_port_set_baud()` 内部调用 —— DMA 窗口/环形缓冲里上一档的字节不再参与本档判定；
  - **补 `radar_restart()`（0x00A3）语义化接口**：改波特率(0x00A1)/分辨率(0x00AA)/蓝牙(0x00A4)/密码(0x00A9)/恢复出厂(0x00A2) 需重启才生效, 灵敏度(0x0064)/最大距离门(0x0060) 立即生效 —— 已写进注释；上板调试的一次性改波特率流程改用它；
  - 调试信息加强：`g_radar_dbg.repf`/`g_radar_dbg.ackf`（上报帧/ACK 帧计数）与 `radar_dbg_note_u32()`；每个探测步骤都记事件, `g_radar_dbg_evt` 可直接读出扫描过程(try baud × / ack, verify baud × / lock × / no report at × / probe FAIL, fallback 256000)；
  - 构建验证：自适应 / `SET_BAUD_IDX=8` / `FORCE=9600` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix(雷达 ACK 匹配)**: 上板 `lock=0` 的**另一个重要嫌疑**——原来要求 `ACK.cmd == 0x00FF && status == 0` 才算命中, 但协议 V1.09 的 ACK 示例里命令字**高字节写作 01**(如使能配置 ACK `FD FC FB FA 08 00 FF 01 00 00 01 00 40 00 ...`), 按 cmd(2)+status(2) 解析会得到 `cmd=0x01FF`, 即使接线完全正常也**永远匹配不上**。改动：
  - 新增 `RADAR_ACK_CMD_MATCH(ack_cmd, cmd)`（`radar_proto.h`）——只比命令字**低字节**（LD2410 命令全是 `0x00xx`, 低字节唯一, 可同时兼容文档的两种写法）, `radar_cmd()` 改用该宏；探测阶段改为**收到任意合法 ACK 帧即锁定波特率**（探测期只发过 0x00FF, 任何 ACK 都是它回的）；
  - 调试段新增 `g_radar_dbg_ack`：最近一帧 ACK 的 `c=xx / st=xx / d=N` + 原始字节十六进制, 用于现场核对 ACK 真实字段布局（文档示例自身不一致）；
  - 构建验证：自适应8档 / `FORCE=460800` / `SET_BAUD_IDX=8` 三种组合均 0 Error 0 Warning。
- `[hc32f460]` **fix**: `bsp_rs485.h` 补 **include guard** —— 该头文件原先没有 guard，同一编译单元被包含两次即报 `#256: invalid redeclaration of type name "alarm_pdu"`（本次由 `radar_dbg.c` 显式包含时暴露）。同目录 `bsp_alarm.h` / `bsp_exint.h` / `bsp_pwm.h` 同样缺 guard，当前无二次包含，未改动。
- **构建验证**：Keil 命令行无头构建 4 种组合全部 **0 Error / 0 Warning** —— 默认（DBG_EN=1、两个 SINK=0）`Code=28180`；SINK_ITM=1 `Code=28260`；SINK_RS485=1 `Code=28296`；恢复默认后全量重建 `Code=28180`。

- **clean**: 与 STM32 侧同步清除 GPIOHEAD —— 删除 `common.c` 的 `Send_RadarStatus_to_Master()`（32B `GPIOHEAD(0x55)` 应答，已无触发来源）与 `Get_pdu_data()` 里的 `GPIOHEAD` 分支（该分支只做应答，删除后落入原有 `else` 复位路径）、帧核心 `FRAME_HDR_LEG_GPIO`（定长 32B 分支只由 `0xFF` 触发）、`bsp_rs485.h` 原型、`main.h` 的 `GPIOHEAD` 宏。HC32↔STM32 仍为 0xAA Cmd 0x10 查询 / 3B 应答 + 0xFF 32B 报警命令转发，协议未改。README 协议章节同步更新（下行 PDUHEAD 唯一、上行 `gpio_pdu` 字段表、通道链路 0xAA 说明）。
- **feat**: 新增 `bsp_report.c/.h`：`bsp_report_build()` 组 3B 上报负载——Byte0 GPIO_IN 位图（bit0..2 雷达 PC14/PC13/PH2 高有效、bit3 GPIO_IN1=摄像头 PB0 低有效、bit4 GPIO_IN2=继电器 PB1 低有效）、Byte1 安装模式位图（LIGHT/SYNC/RADAR/AICAM/EAS）、Byte2 alarm_done；`common.c` 0xAA **Cmd 0x10** 查询回该 3B，`CMD 0x01` 仍回 0x81 回显。
- **refactor**: `Radar_Led_update()`（`bsp_exint.c`/`main.h` 原型）改为 **void 返回 + 直接 IO 控制**（旧 `LED_Start/Led_Stop` 定时闪烁逻辑 `#if 0` 保留）：报警(R/G) 优先；蓝灯 = presence 且**保持 1s**（`RADAR_PRESENCE_HOLD_MS`，消除雷达 ~100ms 脉冲间隙导致的闪烁）、`LIGHT_ON` 常亮；板载 Radar_LED 跟随实时 presence（不保持）。

## [0.1.0] - 2026-09-04

### Added

- **建立 git 单仓库**：在 `J:\dsh\alarm_board` 根目录 `git init`，纳入 `STM32F0_linux_v4.31` 与 `Radar_V4.2_2026_0425_MOS` 全部源码；新增 `.gitignore` 排除 IDE 生成物/编译输出，新增本文件与 `README.md`（架构、报警链路、协议说明）。
- **基线代码快照**：两套固件保持源码原状入库（未做任何逻辑改动），打基线 tag `v0.1.0`。
- 说明：仓库内源码含 GBK 编码注释（中文），提交/检出不改变字节内容（`core.autocrlf=false`）。

### [stm32f0] BSP

- （基线，无修改）
### [hc32f460] projects/source

- （基线，无修改）