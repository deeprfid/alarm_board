# 对官方 hc32_ll DDL 的本地补丁清单（PATCHES）

> 目的：官方 DDL 升级（如 Rev2.5）后，按本清单重新合入本地改动，防止补丁丢失
> 官方源位置：hc32f4a0_driver/drivers/hc32_ll_driver/（华大半导体系列 DDL）
> 自编译 lib：hc32f4a0_driver 工程 Rebuild 产出 hc32f4a_driver.lib（提交到 driver_lib/）

---

## 补丁总览

| # | 文件 | 改动 | 用途 | 引入版本 |
|---|---|---|---|---|
| P1 | hc32_ll_efm.c | EFM_SwapCmd/Program/ProgramWord/SectorErase 加 __NOINLINE __EFM_FUNC | EFM 擦写/交换函数放 RAM 执行 | v9.32 |
| P2 | qspi_flash.c | 新增 QSPI_FLASH_EraseBlock64K()（0xD8 64KB 块擦除） | OTA 提速 | v9.81o |
| P3 | qspi_flash.c | QSPI_FLASH_Write 页编程 chunk_size 用循环变量 addr | 跨页回卷损坏修复 | v9.80 |
| P4 | io_stream.h | USB_COMPO_RXBUF_LEN 1536→32768 | USB OTA RX 缓冲 | v9.81 |
| P5 | cdc_data_process.c | vcp_rxdata 加可用空间检查（满则丢弃） | USB OTA 帧保护 | v9.81 |

---

## P1: hc32_ll_efm.c — EFM RAM 执行属性

### 位置
- driver/drivers/hc32_ll_driver/src/hc32_ll_efm.c（官方源，未合入）
- 对照：app/projects/app/src/hc32_ll_efm.c（同源副本，含补丁）

### 改动（4 处，加函数前缀）
    __NOINLINE __EFM_FUNC int32_t EFM_SwapCmd(en_functional_state_t enNewState)
    __NOINLINE __EFM_FUNC int32_t EFM_Program(uint32_t u32Addr, const uint8_t *pu8Buf, uint32_t u32Len)
    __NOINLINE __EFM_FUNC int32_t EFM_ProgramWord(uint32_t u32Addr, uint32_t u32Data)
    __NOINLINE __EFM_FUNC int32_t EFM_SectorErase(uint32_t u32Addr)

### 说明
- __EFM_FUNC 宏默认 = __RAM_FUNC（官方定义），本补丁只对 4 个关键函数显式启用
- 官方原版仅对 EFM_SequenceProgram / EFM_ChipErase 用了该属性
- 目的：擦写/交换 Flash 期间代码从 SRAM 执行，避免从 Flash 取指命中正在擦写的扇区
- 状态：**待合入 driver 官方源**（当前仅在 app 副本中）

---

## P2: qspi_flash.c — 64KB 块擦除 API

### 位置
- driver/projects/source/qspi_flash.c（driver 工程源，非官方 drivers/ 目录）

### 改动
- 新增 QSPI_FLASH_EraseBlock64K()：WR_ENABLE + 0xD8 + CheckProcessDone(500)
- 头文件 driver_lib/qspi_flash.h 同步加声明

### 说明
- W25Q256 64KB 块擦 tBE~120ms，等效 16 次 4KB 扇区擦，OTA 提速 ~5 倍
- v9.81o 起三通道 ensure_erased 全部 64KB 步进
- 注意：qspi_flash.c 是项目自建驱动（非官方 DDL），官方升级不影响

---

## P3: qspi_flash.c — 页编程 chunk_size 修复

### 改动（v9.80 根因修复）
- 原代码：chunk_size = 256 - (u32Addr % 256);  ← 常量参数地址
- 修复：  chunk_size = 256 - (addr % 256);     ← 循环变量 addr（当前写地址）

### 说明
- 512B 帧时代偏移 256 对齐未触发；4096B 帧 + 82B 包头 → 非对齐起点 → 页边界回卷覆盖已写数据
- 状态：已合入 driver 源 ✓

---

## P4: io_stream.h — USB RX 缓冲 32KB

### 改动
    #define USB_COMPO_RXBUF_LEN 32768   /* 原 1536 */

### 说明
- 原 1536B 连单帧(4107B)都放不下，QSPI 擦/写阻塞期间 USB burst 覆盖未读数据
- 状态：已合入 driver 源 ✓（driver/projects/user/inc/io_stream.h）

---

## P5: cdc_data_process.c — vcp_rxdata 满丢弃

### 改动
- vcp_rxdata 开头加可用空间检查：Len > avail 时直接 return（丢帧不覆盖）

### 说明
- 环形缓冲满时宁可丢帧由 OTA 重传，绝不回绕覆盖未读数据
- 状态：已合入 driver 源 ✓（driver/projects/usb_lib/examp/cdc_data_process.c）

---

## 官方 DDL 升级流程（checklist）

1. 备份当前 driver 工程（git 已追踪）
2. 替换 driver/drivers/hc32_ll_driver/ 为官方新版
3. 按本清单重新应用补丁：P1（4 行）→ P2/P3（qspi 项目源不受影响）→ P4 → P5
4. Rebuild driver 工程 → 新 hc32f4a_driver.lib
5. 复制 lib 到 driver_lib/，Rebuild app，回归 OTA 三通道

## 相关文件
- driver_lib.rar（2025-03-29 官方原始基线，可对照）
- driver_lib/hc32f4a_driver.lib（自编译，git 追踪）
- doc/工程体检与优化建议.md（架构背景）