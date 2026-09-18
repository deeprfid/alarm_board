# 工具链基线（定稿：两个工程统一 AC5）

> 2026-09-17 定稿。记录**工具链口径**、**发布前必须核对的项**，以及 **AC6 迁移的待办与已排除项**（重启迁移时照此执行，不必重走弯路）。

## 1. 口径

- `STM32F0_linux_v4.31` 与 `Radar_V4.2_2026_0425_MOS` **统一使用 ARMCC 5.06 update 7**（MDK，`uAC6=0`），优化档 `Optim=4`（-O3）；HC32 侧 `DebugInformation=0`、`useUlib=0`（标准 C 库）、`vShortEn=1`（短枚举）。
- 依据：现场长期验证的就是这一套（HC32 的 **Debug** target：AC5 + -O3 + `__DEBUG` + 短枚举 + 硬件单精度 FPU）。
- **实测口径（现场）**：**AC5 + O3（`-Otime`）通信正常**；AC6 + O0/-O1 正常，但 **AC6 + O2/-O3（含 LTO）通信异常**（详见 §3.2）。也就是说这**不是 AC5 的问题**，而是 **AC6 独有**的。
- **AC6（ARMCLANG 6.16）迁移暂时搁置**，原因见 §3。切换编译器时务必重跑 §2 全部检查项。

## 2. 发布前必须核对（每次改 target 配置后逐条过）

1. **FPU / ABI 两个 target 必须一致**：核对 `output/<target>/usart_uart_dma.lnp` 里的 `--cpu=` 行，必须是 `--cpu=Cortex-M4.fp.sp`（硬件单精度）。
   - 反例（真实事故）：Release target 曾配成 `--cpu=Cortex-M4 --fpu=SoftVFP`（软件浮点 ABI）→ **-O2 及以上必然不工作**（AC5/AC6 都一样），而低优化档或关内联时能跑，极具迷惑性；AC5 那条线的全部原因就是它。切换 AC5↔AC6 时 MDK 可能重置该选项，必须每次核对。
2. **看门狗**：量产固件 `STM32F0_IWDG_ENABLE` 必须为 `1U`（调试期常被改成 `0`，容易忘了改回；看门狗超时约 1.0 秒）。
3. **ICG 段**：核对 map 里存在 `.ARM.__AT_0x00000400 @ 0x00000400`（32 字节）。AC5 下由 `at()` 的根段语义天然保留，不需要保活代码。
4. 两个 target 均 `0 Error / 0 Warning`。

## 3. AC6 迁移待办（重启迁移时看这一节）

### 3.1 已查明 / 已有可行做法

- **FPU 选项**：切到 AC6 后 Keil 可能把 Target 页的 Floating Point Hardware 重置为 Not Used，必须改回 **Single Precision**（否则重演 AC5 那条线的失效）。
- **ICG 保活**：AC6（尤其开 LTO）会把无代码引用的 `u32ICGValue[]` 当未用符号在**编译/LTO 阶段**消除 → 镜像里没有 ICG 段 → HC32F460 复位后按擦除态默认值启动（看门狗/时钟源/SWD/缓存等配置全失）→ 表现为固件完全不工作。
  - 链接器 `--keep` **无效**（已实测）：LTO 阶段符号就已消失，链接器看不到它。
  - 可行做法（**不改库代码**）：工程侧引用一次它的地址（曾实现为 `bsp_trng.c` 的 `ICG_KeepAlive()`，由 `TrngConfig()` 调用；实测 AC6+LTO 下 ICG 段确实回到 `0x00000400`/32 字节，代价 +8 字节）。当前已回退（AC5 不需要）。

### 3.2 未解决

- **AC6 独有**：AC6 + 优化 ≥ -O2（含 LTO）下**通信异常**（固件能进 `for(;;)` 主循环、LED/蜂鸣器正常，只是通信不对），根因未定；AC6 + O0/-O1 正常，加 `-fno-inline-functions -fno-inline` 也正常。**AC5 在 O3 下通信正常**（现场实测）。
  - AC6 在 -O2 相对 -O1 的两个明确变化是**开始自动内联**与更激进的重排（与实测「关内联即可绕过」吻合）。因此重启排查时应优先怀疑「**只有在内联成立时才会出现**的耦合」——缺 `volatile` 的共享量、只被硬件/DMA 写的数据、跨函数值传播 —— 而不是继续逐个核对配置项。
- 已排除项（都有实测或代码证据，重启迁移时不必重查）：
  - 栈溢出：最坏栈深 **608 B / 3 KB**（armlink 调用图实测）；
  - 严格别名：**AC5 不做 TBAA**（连 `-fno-strict-aliasing` 这个选项都不认）；AC6 侧全仓只查出 1 处真违规（`Get_pdu_data()`，已修）；
  - `__DEBUG`：全工程 13 处**全是断言**（`DDL_ASSERT`/`PLLxParamCheck`），无功能分支；
  - `DebugInformation`：实测改成 1 对代码体积**零影响**；
  - 短枚举：`alarm_pdu`/`alarm_confirm_package` **没有 enum 成员**，线格式不受影响；
  - DDL 延时被优化：`DDL_DelayMS/US` 有 `__NO_OPTIMIZE` 保护；
  - 逐文件选项覆盖：工程里 **0 处** `<FileOption>`；
  - 半主机：AC6 镜像的反汇编/符号/map 里 `semihost`、`_sys_`、`svc 0xab` **全部 0 命中**；
  - 启动代码/向量表：AC5/AC6 两份镜像的前 160 项向量表结构一致（SP、复位入口、handler 链正常）；
  - 弱符号中断分发：229 个弱入口只是空壳，真正的分发表在 `hc32_ll_interrupts.c`（`m_apfnIrqHandler[]`）；
  - DMA 接收缓冲缺 `volatile`（`m_au8RxBuf` 只被 DMA 硬件写、软件只读）：已实现 volatile 中转搬运，**AC6 仍异常**，已回退；
  - 启动 DMA 前缺 `__DSB()`：已在四处交接点（两个接收中断的重新武装、LLP 描述符使能、发送 DMA 启动）加屏障，**AC6 仍异常**，已回退；
  - 工程侧 ICG 保活（`bsp_trng.c` 的 `ICG_KeepAlive()`）：实测在 AC5 下反而失效、回退后恢复 —— 不是 AC6 异常的根因；
  - PLL 等待死循环 / ICG 没烧：实测 LED、蜂鸣器均正常、从无看门狗复位、PLL 从未失效，排除。
- 继续排查的手段（按性价比排序）：
  1. **LED 探针**：在 `main()` 每个初始化步骤后点亮不同 LED，并在主循环/中断里做心跳 —— 一次烧写即可看出卡在哪一步、中断有没有在跑，不需要调试器；
  2. **逐文件优化二分**：每轮把一半文件设为 O3、另一半 O0（关 LTO，否则跨文件内联会让「哪个文件」失去意义），6 轮锁定到具体文件后单点审查；
  3. 调试器观察：`main()` 首行断点是否命中 + 卡住时的 PC/调用栈 + CFSR/HFSR。

## 4. 本轮（AC5 口径）顺带修掉的真 bug

见 `CHANGELOG.md` 的 `[Unreleased]`：`common.c` 的严格别名（`Get_pdu_data()` 改 `memcpy` 本地副本）、系统时基 `m_u32Tickms` 缺 `volatile`（6 处声明）、环形缓冲索引缺 `volatile`（`ring_buf.h`）。这些与工具链迁移无关，是独立隐患。
