/**
 * @file ota_flag.h
 * @brief 升级标志页（App 侧）：与 bootloader boot_cfg.h 布局一致（Phase 4 dual-bank）
 *
 * 标志区：当前 bank 虚拟地址 0x000F0000（swap 无关）。App 只写"当前 bank"标志；
 * bootloader commit 时写另一 bank 标志 + 清当前 bank 标志。
 * 结构 24B：magic + flags + size + version + boot_count + crc32（与 boot_cfg.h 的 boot_flag_t 一致）
 */
#ifndef OTA_FLAG_H
#define OTA_FLAG_H

#include <stdint.h>

#define OTA_FLAG_BASE           0x000F0000UL   /* 当前 bank 标志区 */
#define OTA_FLAG_MAGIC          0x4F544131UL   /* "OTA1" LE */
#define OTA_FLAG_NEED_COMMIT    (1UL << 0)     /* 已下载完，待 bootloader commit */
#define OTA_FLAG_NEED_CONFIRM   (1UL << 1)     /* 新固件待自检确认 */

typedef struct {
    uint32_t magic;
    uint32_t flags;
    uint32_t size;        /* 新固件 payload 长度 */
    uint32_t version;
    uint32_t boot_count;
    uint32_t crc32;       /* 覆盖 [0,20) */
} ota_flag_t;

/* App 下载完+验签通过 → 置 NEED_COMMIT（bootloader 下次启动 commit） */
int  ota_flag_mark_ready(uint32_t size, uint32_t version);
/* 新固件自检通过 → 清 NEED_CONFIRM + boot_count（防回滚） */
int  ota_flag_confirm(void);
/* 读当前 bank 标志；无有效标志返回 -1 */
int  ota_flag_read(ota_flag_t *f);

#endif /* OTA_FLAG_H */
