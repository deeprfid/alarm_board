#include "hwport.h"
#include <string.h>          /* v9.81s: memset（KV miss 填 0xFF） */
#include "hc32f46_driver.h"
#include "rdr_cfg_kv.h"      /* v9.81s: 配置区拦截 → KV */

/*
 * v9.81s: 配置区（RDR_CFG_AREA_START ~ RDR_CFG_AREA_END）的读写擦全部路由到
 * FlashDB KV（QSPI），片内不存配置；非配置区（OTA 标志页、Bank B 等）仍走片内 EFM。
 * KV 就绪前（flashdb 初始化早期）回退片内，避免启动早期 KV error。
 */

/* ---------- 片内 EFM 公共操作（仅非配置区使用） ---------- */

static void efm_wait_ready(void)
{
    while (SET != EFM_GetStatus(EFM_FLAG_RDY) || SET != EFM_GetStatus(EFM_FLAG_RDY1))
        ;
}

static void efm_begin(uint8_t u8SectorNum)
{
    __disable_irq();
    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);
    EFM_SetBusStatus(EFM_BUS_HOLD);
    (void)EFM_SingleSectorOperateCmd(u8SectorNum, ENABLE);
}

static void efm_end(uint8_t u8SectorNum)
{
    (void)EFM_SingleSectorOperateCmd(u8SectorNum, DISABLE);
    EFM_FWMC_Cmd(DISABLE);
    EFM_REG_Lock();
    __enable_irq();
}

/* ---------- 读 ---------- */

void flash_bytes_read(uint32_t addr, void *buf, uint16_t len)
{
    uint32_t c_off = 0;
    const char *key = (rdr_cfg_kv_ready()) ? rdr_cfg_addr_to_key_off(addr, &c_off) : NULL;

    if (key != NULL)
    {
        /* 配置区 → KV 读（支持块内偏移） */
        if (rdr_cfg_kv_get_at(key, c_off, buf, len) != 0)
            memset(buf, 0xFF, len);   /* KV 无配置 → 等同片内空态(0xFF)，调用方按无配置处理 */
        return;
    }

    /* 非配置区 → 片内 EFM */
    efm_wait_ready();
    (void)EFM_ReadByte(addr, (uint8_t *)buf, len);
}

/* ---------- 擦 ---------- */

int flash_sector_erase(uint32_t dest)
{
    if (rdr_cfg_kv_ready() && dest >= RDR_CFG_AREA_START && dest < RDR_CFG_AREA_END)
        return LL_OK;   /* 配置区 → KV 覆盖写即清除，无需擦除 */

    uint8_t u8SectorNum = dest / EFM_SECTOR_SIZE;
    efm_wait_ready();
    efm_begin(u8SectorNum);
    int ret = EFM_SectorErase(dest);
    efm_end(u8SectorNum);
    return ret;
}

/* ---------- 写 ---------- */

int flash_bytes_write(uint32_t addr, void *buf, uint16_t len)
{
    uint32_t c_off = 0;
    const char *key = (rdr_cfg_kv_ready()) ? rdr_cfg_addr_to_key_off(addr, &c_off) : NULL;

    if (key != NULL)
    {
        /* 配置区 → 只写 KV（片内不存配置，支持块内偏移） */
        uint8_t *oldv;
        int rc;

        /* v9.81t: 2048B 栈数组→堆——send_func 8KB 栈，oldv+tmp 嵌套超 4KB 会溢出复位 */
        oldv = malloc_hexp(len);
        if (oldv == NULL)
            return LL_ERR_INVD_PARAM;

        /* v9.81s-h: 读旧值对比——未修改的配置块跳过写入（省 FlashDB 全分区扫描 ~310ms/块） */
        if (rdr_cfg_kv_get_at(key, c_off, oldv, len) == 0 && memcmp(oldv, buf, len) == 0)
        {
            free_hexp(oldv);
            return LL_OK;
        }

        if (c_off == 0 && len == rdr_cfg_kv_len(key))
            rc = (rdr_cfg_kv_set(key, buf, len) == 0) ? LL_OK : LL_ERR_INVD_PARAM;
        else
            rc = (rdr_cfg_kv_set_at(key, c_off, buf, len) == 0) ? LL_OK : LL_ERR_INVD_PARAM;

        free_hexp(oldv);
        return rc;
    }

    /* 非配置区 → 片内 EFM */
    uint8_t u8SectorNum = addr / EFM_SECTOR_SIZE;
    efm_wait_ready();
    efm_begin(u8SectorNum);
    int ret = EFM_Program(addr, buf, len);
    efm_end(u8SectorNum);

    if (LL_OK == ret)
        TRACE("Flash Program successful......%d\n", ret);
    else
        TRACE("LL_ERR_NOT_RDY: EFM program if not ready.......%d\n", ret);

    return ret;
}
