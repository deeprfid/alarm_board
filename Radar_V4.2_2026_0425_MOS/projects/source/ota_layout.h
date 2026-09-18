/**
 * @file  ota_layout.h
 * @brief OTA 片内布局、槽镜像头与选择器标志（Boot 与 App 共用）
 *
 * 依据：docs/ota_boot_design.md v0.2（A/B 双槽即运行区、无搬运、选择器标志选槽）
 *
 * 片内 Flash 512KB / EFM 扇区 8KB：
 *   Boot      0x00000000 - 0x00007FFF   32KB   （含 ICG@0x400、向量表）
 *   槽 A      0x00008000 - 0x00027FFF  128KB
 *   槽 B      0x00028000 - 0x00047FFF  128KB
 *   保留      0x00048000 - 0x0007DFFF  ~216KB
 *   标志区    0x0007E000 - 0x0007FFFF    8KB   （双份各 4KB，整扇区，放最高地址防越界）
 *
 * 本文件不依赖任何芯片/驱动头文件。
 */
#ifndef OTA_LAYOUT_H
#define OTA_LAYOUT_H

#include <stdint.h>

/* ---------------- 片内 Flash ---------------- */
#define OTA_FLASH_BASE          0x00000000UL
#define OTA_FLASH_SIZE          0x00080000UL
#define OTA_FLASH_SECTOR        0x00002000UL   /* EFM 扇区 8KB */

/* ---------------- 分区 ---------------- */
#define OTA_BOOT_BASE           0x00000000UL
#define OTA_BOOT_SIZE           0x00008000UL   /* 32KB */

#define OTA_SLOT_A              0u
#define OTA_SLOT_B              1u
#define OTA_SLOT_SIZE           0x00020000UL   /* 128KB */
#define OTA_SLOT_A_BASE         0x00008000UL
#define OTA_SLOT_B_BASE         0x00028000UL

#define OTA_SLOT_BASE(s)        (((s) == OTA_SLOT_B) ? OTA_SLOT_B_BASE : OTA_SLOT_A_BASE)
#define OTA_SLOT_OTHER(s)       (((s) == OTA_SLOT_B) ? OTA_SLOT_A : OTA_SLOT_B)
#define OTA_SLOT_OF_ADDR(a)     (((a) >= OTA_SLOT_B_BASE) ? OTA_SLOT_B : OTA_SLOT_A)

#define OTA_FLAG_BASE           0x0007E000UL   /* 8KB 扇区，按扇区对齐选取 */
#define OTA_FLAG_SIZE           0x00002000UL
#define OTA_FLAG_COPY_SIZE      0x00001000UL   /* 双份，各 4KB（份 0 / 份 1） */
#define OTA_FLAG_COPY0          0u
#define OTA_FLAG_COPY1          1u
#define OTA_FLAG_COPY_ADDR(c)   (OTA_FLAG_BASE + ((c) * OTA_FLAG_COPY_SIZE))

/* ---------------- 槽尾部元数据（trailer，不放在槽起始处！） --------------
 * 为什么放尾部：App 就链接在【槽基址】(0x8000/0x28000)，向量表在最前 8 字节；
 * 若把元数据放槽起始处会与向量的重叠。故元数据置于槽末尾 32B，App 二进制原样从槽基址起。
 * CRC32 覆盖 [槽基址, 槽基址+ImageLen) 的 App 二进制本体，元数据在覆盖范围之外 ——
 * 既不会自指，又保证校验的是 Flash 实际内容。见 docs/ota_boot_design.md §4/§14。 */
#define OTA_IMG_MAGIC           0x534C4F54UL   /* 'S','L','O','T'（LE） */
#define OTA_IMG_HDR_LEN         17u
#define OTA_IMG_OFF_VERSION     4u             /* 主.次.构建（仅展示/防呆，不参与选槽） */
#define OTA_IMG_OFF_IMGLEN      8u             /* App 二进制字节数（= OTA1 包头里的 payload len） */
#define OTA_IMG_OFF_CRC32       12u            /* App 二进制的 CRC32（IEEE 0xEDB88320） */
#define OTA_IMG_OFF_SLOT        16u            /* TargetSlot：A/B，防错槽运行 */
#define OTA_IMG_TRAILER_SIZE    32u            /* 槽末尾预留 32B（扇区对齐友好） */
#define OTA_IMG_TRAILER_OFF     (OTA_SLOT_SIZE - OTA_IMG_TRAILER_SIZE)
#define OTA_IMG_MAX             (OTA_SLOT_SIZE - OTA_IMG_TRAILER_SIZE)  /* App 二进制上限 */

/* ---------------- 槽状态与选择器标志 ---------------- */
typedef enum
{
    OTA_SLOT_EMPTY = 0,     /* 空/无效 */
    OTA_SLOT_RUNNABLE,      /* 可运行（已确认） */
    OTA_SLOT_TRIAL,         /* 试运行中（跳转后未确认） */
    OTA_SLOT_FAILED         /* 连续启动失败，不再自动选 */
} ota_slot_state_t;

#define OTA_FLAG_MAGIC          0x4F544131UL   /* "OTA1"（LE） */
#define OTA_FLAG_NEED_COMMIT    (1UL << 0)     /* 暂存就绪，Boot 需搬运/激活 */
#define OTA_FLAG_NEED_CONFIRM   (1UL << 1)     /* 新固件待自检确认 */
#define OTA_FLAG_MAX_BOOT       3UL            /* TRIAL 下启动计数上限，超出回退 */

/* 记录 40B：crc32 覆盖其前的 [0,36) */
typedef struct
{
    uint32_t magic;         /* OTA_FLAG_MAGIC */
    uint32_t seq;           /* 写入序号（单调递增，双份择新用） */
    uint32_t active;        /* 当前运行槽（OTA_SLOT_A / OTA_SLOT_B） */
    uint32_t state_a;       /* ota_slot_state_t */
    uint32_t state_b;
    uint32_t fail_a;        /* 槽 A 连续启动失败次数 */
    uint32_t fail_b;
    uint32_t boot_count;    /* 新槽启动计数（TRIAL 用） */
    uint32_t flags;         /* OTA_FLAG_* */
    uint32_t crc32;         /* 覆盖 [0,36) */
} ota_flag_t;

#define OTA_FLAG_CRC_OFF        36u            /* = offsetof(ota_flag_t, crc32) */

#endif /* OTA_LAYOUT_H */
