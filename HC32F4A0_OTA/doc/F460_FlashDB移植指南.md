# F460 FlashDB/FAL 移植指南（Phase 5）

> 目标：F460（Decoder_20260326/hc32f46_app）接入 FlashDB KV，支撑 ota_state 升级状态机。
> F4A0 已用同源 FlashDB（hc32f4a0_app/projects/flashDB），文件可同源拷贝。

## 1. 文件清单（从 F4A0 复制）

| 来源（F4A0 projects/flashDB） | 目标（F460） | 说明 |
|---|---|---|
| src/fdb.c fdb_file.c fdb_kvdb.c fdb_tsdb.c fdb_utils.c | app 工程 src/ | FlashDB 核心（平台无关 C） |
| include/fal.h fal_def.h fal_cfg.h | app 工程 inc/ | FAL 抽象 + 分区表（fal_cfg.h 需改） |
| include/fdb_cfg.h fdb_def.h fdb_low_lvl.h flashdb.h | app 工程 inc/ | FlashDB 配置（fdb_cfg.h 需改扇区大小） |
| port/fal/src/fal.c fal_flash.c fal_partition.c | app 工程 src/ | FAL 框架 |
| include/sfud*.h（如需 SFUD 路径） | — | F460 建议直接用 w25qxx（已有驱动），不引 SFUD |

## 2. 适配点

### 2.1 fal_cfg.h（分区表，F460 QSPI 布局待部署模型确认）

```c
#include "fal.h"
extern struct fal_flash_dev nor_flash0;   /* w25qxx 适配的 FAL 设备 */

#define FAL_FLASH_DEV_TABLE  { &nor_flash0, }

/* F460 QSPI 建议：KVDB 分区 4KB*N（进度/标志持久化）；暂存/备份区不走 FAL */
#define FAL_PART_TABLE \
{ \
    {FAL_PART_MAGIC_WORD, "fdb_kvdb1", "norflash0", 0, 64*1024, 0}, \
}
```

### 2.2 fal_flash 驱动（nor_flash0 → w25qxx API 映射）

```c
/* fal_flash_w25qxx.c：F460 用已有 w25qxx.c 实现 FAL 设备 ops */
#include "fal.h"
#include "w25qxx.h"          /* F460 已有驱动 */

static int init(void)        { w25qxx_init(); return 0; }
static int read(long off, uint8_t *buf, size_t size)
{ w25qxx_read(off, buf, (uint32_t)size); return size; }
static int write(long off, const uint8_t *buf, size_t size)
{ w25qxx_page_write(off, (uint8_t*)buf, (uint32_t)size); return size; }
static int erase(long off, size_t size)
{ /* w25qxx 4KB 扇区擦 */ for (long a = off; a < off + size; a += 4096) w25qxx_erase_sector(a); return size; }

struct fal_flash_dev nor_flash0 = {
    .name       = "norflash0",
    .addr       = 0,
    .len        = 8 * 1024 * 1024,      /* W25Q64 8MB */
    .blk_size   = 4096,
    .ops        = { init, read, write, erase },
    .write_gran = 1,
};
```
> w25qxx.c API 名以 F460 实际为准（读/写/擦），映射逻辑同上。

### 2.3 flashdb.c 初始化（对齐 F4A0）

```c
#include "flashdb.h"
struct fdb_kvdb AlarmDB = { 0 };

static void lock(fdb_kvdb_t db)  { /* 无 RTOS 可空实现 */ }
static void unlock(fdb_kvdb_t db) {}

void flashdb_init(void) {
    fal_init();
    fdb_kvdb_control(&AlarmDB, FDB_KVDB_CTRL_SET_LOCK, (void*)lock);
    fdb_kvdb_control(&AlarmDB, FDB_KVDB_CTRL_SET_UNLOCK, (void*)unlock);
    struct fdb_default_kv default_kv;
    default_kv.kvs = NULL; default_kv.num = 0;
    fdb_kvdb_init(&AlarmDB, "para", "fdb_kvdb1", &default_kv, NULL);
}
```

### 2.4 fdb_cfg.h（FlashDB 配置）

```c
#define FDB_WRITE_GRAN 1                 /* w25qxx 按字节写 */
#define FDB_SECTOR_SIZE 4096             /* w25qxx 扇区 4KB */
#define FDB_BIG_ENDIAN 0
/* 按需裁剪 FDB_USING_KVDB=1、FDB_USING_TIMESTAMP、FDB_USING_PAGINATION 等 */
```

## 3. 对接 ota_state

- ota_state.c 引用 `extern struct fdb_kvdb AlarmDB;` + `fdb_kv_set_blob/get_blob`（与 F4A0 完全一致）
- 移植 ota_state.h/c 即可（无平台耦合；进度/标志 KV 键 `iap_*` 保持一致）

## 4. 验证

1. 编译：新增文件加入 F460 uvprojx → 0E/0W
2. 运行时自测：`flashdb_init()` 后 `fdb_kv_set_blob("test",...)` → 读回一致 → 掉电重启读回
3. 与 ota_state 联调：`ota_set_progress/get_progress` 读写验证

## 5. 待部署模型确认后补充

- QSPI 分区布局（KVDB/暂存/备份区偏移）——single_bak_sim.py 已验证备份-覆盖-恢复算法
- 若 F460 bootloader 需要读 KV：建议 bootloader 用独立标志页（F4A0 Phase 4 方案），不引入 FlashDB
