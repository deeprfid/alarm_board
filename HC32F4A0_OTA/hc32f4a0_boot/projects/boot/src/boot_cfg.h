/**
 * @file boot_cfg.h
 * @brief F4A0 Bootloader 布局与标志区配置（Phase 4，dual-bank swap）
 *
 * 物理布局（2MB dual-bank，每 bank 1MB）：
 *   Bank A 0x00000000-0x000FFFFF
 *     Bootloader 0x00000000-0x0000FFFF（64KB）
 *     App        0x00010000-0x000EFFFF（896KB，链接基址 0x10000）
 *     标志区     0x000F0000-0x000FFFFF
 *   Bank B 0x00100000-0x001FFFFF（与 A 镜像）
 *     Bootloader 副本 0x00100000（commit 时自复制）
 *     App 新固件      0x00110000
 *     标志区副本      0x001F0000
 *
 * 关键地址均用"虚拟地址"（FSWP 无关）：当前 bank = 0x0000 段，另一 bank = 0x0010 段。
 * 依赖硬件引导交换（EFM_SWAP_ADDR + EFM_SwapCmd），swap 后 0x0000 段映射到另一物理 bank。
 */
#ifndef BOOT_CFG_H
#define BOOT_CFG_H

#include <stdint.h>

/* ---- 布局 ---- */
#define BOOT_BOOT_BASE          0x00000000UL   /* bootloader（当前 bank） */
#define BOOT_BOOT_SIZE          0x00010000UL   /* 64KB */
#define BOOT_APP_BASE           0x00010000UL   /* App 链接基址（当前 bank） */
#define BOOT_APP_MAX_SIZE       0x000E0000UL   /* App 区上限（896KB） */
#define BOOT_FLAG_BASE          0x000F0000UL   /* 标志区（当前 bank） */
#define BOOT_OTHER_BOOT_BASE    0x00100000UL   /* 另一 bank bootloader 区 */
#define BOOT_OTHER_APP_BASE     0x00110000UL   /* 另一 bank App 区 */
#define BOOT_OTHER_FLAG_BASE    0x001F0000UL   /* 另一 bank 标志区 */

/* ---- QSPI 暂存（统一 OTA 包，含 82B 包头） ---- */
#define BOOT_QSPI_STAGE_BASE    0x00E00000UL
#define BOOT_QSPI_STAGE_MAX     0x00100000UL   /* 1MB */
#define BOOT_OTA_HDR_LEN        82UL           /* 统一 OTA 包头 */
#define BOOT_OTA_MAGIC0         'O'
#define BOOT_OTA_MAGIC1         'T'
#define BOOT_OTA_MAGIC2         'A'
#define BOOT_OTA_MAGIC3         '1'
#define BOOT_OTA_VER_OFF        4UL
#define BOOT_OTA_LEN_OFF        10UL
#define BOOT_OTA_CRC_OFF        14UL

/* ---- 升级状态 ---- */
#define BOOT_FLAG_MAGIC         0x4F544131UL   /* "OTA1" LE */
#define BOOT_FLAG_NEED_COMMIT   (1UL << 0)     /* App 已下载完，待 commit */
#define BOOT_FLAG_NEED_CONFIRM  (1UL << 1)     /* 新固件待自检确认（boot count） */
#define BOOT_MAX_BOOT_COUNT     3UL            /* 新固件自检失败上限（超限回滚） */

/* 标志页（24 字节，4KB 扇区内） */
typedef struct {
    uint32_t magic;       /* BOOT_FLAG_MAGIC */
    uint32_t flags;       /* NEED_COMMIT | NEED_CONFIRM */
    uint32_t size;        /* 新固件 payload 长度 */
    uint32_t version;     /* 新版本 */
    uint32_t boot_count;  /* 新固件启动计数 */
    uint32_t crc32;       /* 覆盖 [0,20) */
} boot_flag_t;

#endif /* BOOT_CFG_H */
