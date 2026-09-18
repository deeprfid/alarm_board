/**
 * @file  ota_flash.c
 * @brief 片内 Flash 擦写与选择器标志实现
 *
 * DDL 调用序列（hc32_ll_efm.c 实测）：
 *   EFM_REG_Unlock();  EFM_FWMC_Cmd(ENABLE);   <- 后者置 FWMC.PEMODE=1（IS_EFM_FWMC_UNLOCK）
 *   ... EFM_SectorErase() / EFM_Program() ...
 *   EFM_FWMC_Cmd(DISABLE); EFM_REG_Lock();
 * 注意 EFM_Program/EFM_SectorErase 退出时会把 PEMOD 恢复成只读，故【每个操作前都要重新 ENABLE】。
 */
#include <string.h>
#include "ota_flash.h"
#include "hc32_ll_efm.h"

/* App 自检确认（见 ota_flash.h 的说明：并入本文件以确保与 ota_flash.o 同在 RW_RAMCODE 中执行） */
int32_t ota_app_boot_confirm(uint32_t slot)
{
    ota_flag_t flag;

    if (0 != ota_flag_read(&flag)) {
        /* 标志无效（首次烧录未写标志区）：建一份，直接把本槽置 RUNNABLE */
        (void)memset(&flag, 0, sizeof(flag));
        flag.magic   = OTA_FLAG_MAGIC;
        flag.active  = slot & 1UL;
        flag.state_a = (uint32_t)OTA_SLOT_EMPTY;
        flag.state_b = (uint32_t)OTA_SLOT_EMPTY;
    }

    flag.active = slot & 1UL;
    if (flag.active == OTA_SLOT_A) {
        flag.state_a = (uint32_t)OTA_SLOT_RUNNABLE;
        flag.fail_a  = 0UL;
    } else {
        flag.state_b = (uint32_t)OTA_SLOT_RUNNABLE;
        flag.fail_b  = 0UL;
    }
    flag.boot_count = 0UL;
    flag.flags     &= ~OTA_FLAG_NEED_CONFIRM;

    return (0 != ota_flag_write(&flag)) ? -1 : 0;
}

/* ---------------- CRC32（IEEE，与 zlib.crc32 一致） ---------------- */
static uint32_t s_crc32_tab[256];
static uint8_t  s_crc32_tab_ready = 0u;

static void crc32_tab_init(void)
{
    uint32_t i, k, c;
    if (s_crc32_tab_ready != 0u) { return; }
    for (i = 0u; i < 256u; i++)
    {
        c = i;
        for (k = 0u; k < 8u; k++)
        {
            c = ((c & 1u) != 0u) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
        s_crc32_tab[i] = c;
    }
    s_crc32_tab_ready = 1u;
}

uint32_t ota_crc32(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    crc32_tab_init();
    for (i = 0u; i < len; i++)
    {
        crc = s_crc32_tab[(crc ^ buf[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ---------------- 片内 Flash 基本操作（擦/写 = RAM 驻留） ---------------- */
__RAM_FUNC int32_t ota_flash_erase(uint32_t addr, uint32_t size)
{
    uint32_t i;
    uint32_t sectors;
    int32_t  ret = 0;

    if (size == 0UL) { return -3; }
    if (size > OTA_FLASH_SIZE) { return -1; }
    if ((addr + size) > (OTA_FLASH_BASE + OTA_FLASH_SIZE)) { return -1; }
    if (0UL != (addr % OTA_FLASH_SECTOR)) { return -2; }

    sectors = (size + OTA_FLASH_SECTOR - 1UL) / OTA_FLASH_SECTOR;
    EFM_REG_Unlock();
    for (i = 0u; i < sectors; i++)
    {
        EFM_FWMC_Cmd(ENABLE);   /* 每个操作前重新解锁 FWMC */
        if (LL_OK != EFM_SectorErase(addr + (i * OTA_FLASH_SECTOR))) { ret = -4; break; }
    }
    EFM_FWMC_Cmd(DISABLE);
    EFM_REG_Lock();
    return ret;
}

__RAM_FUNC int32_t ota_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    int32_t ret;

    if ((buf == NULL) || (len == 0UL)) { return -1; }
    if (len > OTA_FLASH_SIZE) { return -2; }
    if ((addr + len) > (OTA_FLASH_BASE + OTA_FLASH_SIZE)) { return -2; }
    if (0UL != (addr % 4UL)) { return -3; }   /* DDL 要求字对齐 */

    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);
    ret = (LL_OK == EFM_Program(addr, (uint8_t *)(uint32_t)buf, len)) ? 0 : -4;
    EFM_FWMC_Cmd(DISABLE);
    EFM_REG_Lock();
    return ret;
}

void ota_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if ((buf == NULL) || (len == 0UL)) { return; }
    (void)memcpy(buf, (const void *)(uint32_t)addr, len);
}

int32_t ota_flash_verify(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    const uint8_t *p = (const uint8_t *)(uint32_t)addr;
    if ((buf == NULL) || (len == 0UL)) { return -1; }
    for (i = 0u; i < len; i++)
    {
        if (p[i] != buf[i]) { return -2; }
    }
    return 0;
}

uint32_t ota_crc32_flash(uint32_t addr, uint32_t len)
{
    /* 分块读取，避免大缓冲（CRC32 是流式的） */
    uint8_t  chunk[64];
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t done = 0u;
    uint32_t n, i;

    crc32_tab_init();
    while (done < len)
    {
        n = len - done;
        if (n > sizeof(chunk)) { n = sizeof(chunk); }
        ota_flash_read(addr + done, chunk, n);
        for (i = 0u; i < n; i++)
        {
            crc = s_crc32_tab[(crc ^ chunk[i]) & 0xFFu] ^ (crc >> 8);
        }
        done += n;
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ---------------- 选择器标志 ---------------- */
static int32_t flag_copy_valid(const ota_flag_t *f)
{
    if (f->magic != OTA_FLAG_MAGIC) { return 0; }
    return (ota_crc32((const uint8_t *)f, OTA_FLAG_CRC_OFF) == f->crc32) ? 1 : 0;
}

int32_t ota_flag_read(ota_flag_t *out)
{
    ota_flag_t a, b;
    int32_t    va, vb;

    if (out == NULL) { return -1; }
    ota_flash_read(OTA_FLAG_COPY_ADDR(OTA_FLAG_COPY0), (uint8_t *)&a, (uint32_t)sizeof(a));
    ota_flash_read(OTA_FLAG_COPY_ADDR(OTA_FLAG_COPY1), (uint8_t *)&b, (uint32_t)sizeof(b));
    va = flag_copy_valid(&a);
    vb = flag_copy_valid(&b);

    if ((va != 0) && (vb != 0)) { *out = (a.seq >= b.seq) ? a : b; return 0; }
    if (va != 0) { *out = a; return 0; }
    if (vb != 0) { *out = b; return 0; }
    return -1;
}

int32_t ota_flag_write(const ota_flag_t *in)
{
    ota_flag_t rec;
    int32_t    ret = 0;

    if (in == NULL) { return -1; }
    rec = *in;
    rec.magic = OTA_FLAG_MAGIC;
    rec.seq  += 1UL;
    rec.crc32 = ota_crc32((const uint8_t *)&rec, OTA_FLAG_CRC_OFF);

    /* 整扇区擦除会同时清掉两份，故先擦一次再写两份（份内各 4KB，同一 8KB 扇区） */
    if (0 != ota_flash_erase(OTA_FLAG_BASE, OTA_FLAG_SIZE)) { return -2; }
    if (0 != ota_flash_write(OTA_FLAG_COPY_ADDR(OTA_FLAG_COPY0), (const uint8_t *)&rec, (uint32_t)sizeof(rec))) { ret = -3; }
    if (0 != ota_flash_write(OTA_FLAG_COPY_ADDR(OTA_FLAG_COPY1), (const uint8_t *)&rec, (uint32_t)sizeof(rec))) { ret = -4; }
    return ret;
}

uint32_t ota_flag_active_slot(void)
{
    ota_flag_t f;
    if (0 != ota_flag_read(&f)) { return OTA_SLOT_A; }
    return (f.active == OTA_SLOT_B) ? OTA_SLOT_B : OTA_SLOT_A;
}

/* ---------------- 槽镜像头 ---------------- */
void ota_img_read_hdr(uint32_t slot, uint32_t *version, uint32_t *img_len, uint32_t *crc32, uint8_t *slot_field)
{
    uint8_t hdr[OTA_IMG_HDR_LEN];
    uint32_t base = OTA_SLOT_BASE(slot) + OTA_IMG_TRAILER_OFF;   /* 元数据在槽末尾 */

    ota_flash_read(base, hdr, OTA_IMG_HDR_LEN);
    if (version != NULL)    { (void)memcpy(version, &hdr[OTA_IMG_OFF_VERSION], 4); }
    if (img_len != NULL)    { (void)memcpy(img_len, &hdr[OTA_IMG_OFF_IMGLEN], 4); }
    if (crc32 != NULL)      { (void)memcpy(crc32,   &hdr[OTA_IMG_OFF_CRC32], 4); }
    if (slot_field != NULL) { *slot_field = hdr[OTA_IMG_OFF_SLOT]; }
}

int32_t ota_img_check(uint32_t slot)
{
    uint8_t  hdr[OTA_IMG_HDR_LEN];
    uint32_t base = OTA_SLOT_BASE(slot);
    uint32_t magic, img_len, crc_hdr;
    uint8_t  slot_field;

    ota_flash_read(base + OTA_IMG_TRAILER_OFF, hdr, OTA_IMG_HDR_LEN);
    (void)memcpy(&magic, &hdr[0], 4);
    (void)memcpy(&img_len, &hdr[OTA_IMG_OFF_IMGLEN], 4);
    (void)memcpy(&crc_hdr, &hdr[OTA_IMG_OFF_CRC32], 4);
    slot_field = hdr[OTA_IMG_OFF_SLOT];

    if (magic != OTA_IMG_MAGIC) { return -1; }
    if ((img_len < 64UL) || (img_len > OTA_IMG_MAX)) { return -2; }
    if (slot_field != (uint8_t)slot) { return -3; }   /* 防错槽 */
    /* CRC32 覆盖 [槽基址, +ImageLen) 的 App 二进制本体；元数据在覆盖范围之外，不自指 */
    if (ota_crc32_flash(base, img_len) != crc_hdr) { return -4; }
    return 0;
}

/* 写槽尾元数据：CRC32 由【Flash 实际内容】算出（先写完 App 二进制再写这里） */
int32_t ota_img_write_trailer(uint32_t slot, uint32_t img_len, uint32_t version)
{
    uint8_t  hdr[OTA_IMG_HDR_LEN];
    uint32_t base = OTA_SLOT_BASE(slot);
    uint32_t magic = OTA_IMG_MAGIC;
    uint32_t crc;
    uint8_t  slot_field = (uint8_t)slot;

    if ((img_len < 64UL) || (img_len > OTA_IMG_MAX)) { return -1; }

    crc = ota_crc32_flash(base, img_len);
    (void)memcpy(&hdr[0], &magic, 4);
    (void)memcpy(&hdr[OTA_IMG_OFF_VERSION], &version, 4);
    (void)memcpy(&hdr[OTA_IMG_OFF_IMGLEN], &img_len, 4);
    (void)memcpy(&hdr[OTA_IMG_OFF_CRC32], &crc, 4);
    hdr[OTA_IMG_OFF_SLOT] = slot_field;

    /* 元数据落在槽最后一个扇区内，先擦该扇区 */
    if (0 != ota_flash_erase(base + OTA_IMG_TRAILER_OFF, OTA_IMG_TRAILER_SIZE)) { return -2; }
    if (0 != ota_flash_write(base + OTA_IMG_TRAILER_OFF, hdr, OTA_IMG_HDR_LEN)) { return -3; }
    return 0;
}
