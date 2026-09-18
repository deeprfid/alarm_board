/**
 * @file ota_flag.c
 * @brief 升级标志页实现（App 侧）：EFM 擦写当前 bank 标志区
 * @note 与 bootloader boot_cfg.h 的 boot_flag_t 布局一致；写前解锁 EFM
 */
#include <string.h>
#include "qspi_flash.h"        /* 必须在 hc32f46_driver.h 之前（hc32f4a0.h:3630） */
#include "hc32f46_driver.h"
#include "hc32_ll_efm.h"
#include "ota_flag.h"

/* ---- CRC32（IEEE，与统一 OTA 包一致） ---- */
static uint32_t s_crc_tab[256];
static int s_crc_ok = 0;
static void crc_tab_init(void)
{
    if (s_crc_ok) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        s_crc_tab[i] = c;
    }
    s_crc_ok = 1;
}
static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    crc_tab_init();
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
        crc = s_crc_tab[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
static uint32_t flag_crc(const ota_flag_t *f)
{
    return crc32_update(0, (const uint8_t *)f, 20);
}

int ota_flag_read(ota_flag_t *f)
{
    if (f == NULL) return -1;
    memcpy(f, (const void *)OTA_FLAG_BASE, sizeof(ota_flag_t));
    if (f->magic != OTA_FLAG_MAGIC) return -1;
    if (f->crc32 != flag_crc(f))    return -1;
    return 0;
}

static int flag_write_at(uint32_t base, const ota_flag_t *f)
{
    ota_flag_t tmp = *f;
    int e1, e2;
    tmp.crc32 = flag_crc(f);

    /* unlock EFM, erase sector, program (4KB aligned) */
    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);
    /* AN: unprotect target sector before erase (FxNWPRTy; LL EFM_SectorErase lacks this) */
    EFM_SingleSectorOperateCmd((uint8_t)(base / 0x2000UL), ENABLE);   /* sector 8KB */
    e1 = EFM_SectorErase(base);
    if (e1 != 0) {
        EFM_FWMC_Cmd(DISABLE);   /* v9.81p: 失败路径也收尾锁定 */
        return -1;
    }
    e2 = EFM_Program(base, (const uint8_t *)&tmp, sizeof(tmp));
    EFM_FWMC_Cmd(DISABLE);       /* v9.81p: 写后重新锁定 FWMC（防意外擦写） */
    if (e2 != 0)
        return -1;
    return 0;
}

int ota_flag_mark_ready(uint32_t size, uint32_t version)
{
    ota_flag_t f;
    memset(&f, 0, sizeof(f));
    f.magic   = OTA_FLAG_MAGIC;
    f.flags   = OTA_FLAG_NEED_COMMIT;
    f.size    = size;
    f.version = version;
    return flag_write_at(OTA_FLAG_BASE, &f);
}

int ota_flag_confirm(void)
{
    ota_flag_t f;
    memset(&f, 0, sizeof(f));
    f.magic = OTA_FLAG_MAGIC;
    f.flags = 0;                      /* 清 NEED_CONFIRM + boot_count=0 */
    return flag_write_at(OTA_FLAG_BASE, &f);
}
