/*-----------------------------------------------------------------------*/
/* Low level disk I/O for QSPI FLASH FAT volume (v9.81ci)                */
/* 卷 0 = QSPI 后 16MB 空闲区 (0x1000000 - 0x2000000)                     */
/*-----------------------------------------------------------------------*/
#include "ff.h"
#include "diskio.h"
#include "qspi_flash.h"
#include <string.h>   /* memcpy (v9.81cl) */

/* FAT 卷 = QSPI 最后 8MB（24MB 偏移，0x1800000 - 0x2000000）
 * 前 24MB 已用：KV 2MB + TSDB 2MB + 白名单 10MB + OTA 暂存 ~1MB + 预留 */
#define FATVOL_BASE     (0x01800000UL)   /* 24MB 偏移 */
#define FATVOL_SIZE     (8UL * 1024 * 1024)   /* 8MB */
#define SECTOR_SIZE     512UL

static DSTATUS s_disk_status = STA_NOINIT;

/* v9.81cl: 4KB 写回缓存。QSPI 擦除粒度为 4KB，FatFs 写粒度为 512B。
 * 原 RMW 每次 512B 写都擦写整个 4KB（写放大 8x，CSV 500KB 需 1000 次擦除 ~45s）；
 * 缓存把同块多次部分写合并为一次擦写（500KB 只擦 125 次），读命中返回缓存数据。 */
static uint8_t  s_wcache[4096];
static uint32_t s_wcache_blk = 0xFFFFFFFFUL;   /* 缓存块基址（卷内绝对地址） */
static int      s_wcache_valid = 0;
static int      s_wcache_dirty = 0;

static DRESULT wcache_flush(void)
{
    if (s_wcache_valid && s_wcache_dirty) {
        if (QSPI_FLASH_EraseSector(s_wcache_blk) != 0)
            return RES_ERROR;
        if (QSPI_FLASH_Write(s_wcache_blk, s_wcache, 4096UL) != 0)
            return RES_ERROR;
        s_wcache_dirty = 0;
    }
    return RES_OK;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return s_disk_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    /* QSPI 已由 driver_hw_init_late() 初始化 */
    s_disk_status = 0;   /* STA_NOINIT cleared */
    s_wcache_valid = 0;  /* v9.81cl: 新挂载会话，缓存失效 */
    s_wcache_dirty = 0;
    return s_disk_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    uint32_t addr, len, off;
    if (pdrv != 0) return RES_PARERR;
    addr = FATVOL_BASE + sector * SECTOR_SIZE;
    len  = count * SECTOR_SIZE;
    off  = 0;
    while (len > 0) {
        uint32_t a         = addr + off;
        uint32_t blk_start = a & ~(4096UL - 1UL);
        uint32_t blk_off   = a - blk_start;
        uint32_t chunk     = 4096UL - blk_off;
        if (chunk > len) chunk = len;
        if (s_wcache_valid && s_wcache_blk == blk_start) {
            /* v9.81cl: 命中缓存——未冲刷的脏数据必须从缓存读，否则读到 flash 旧值 */
            memcpy(buff + off, s_wcache + blk_off, chunk);
        } else {
            if (QSPI_FLASH_Read(a, buff + off, chunk) != 0)
                return RES_ERROR;
        }
        off += chunk;
        len -= chunk;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    uint32_t addr, len, off;
    if (pdrv != 0) return RES_PARERR;
    addr = FATVOL_BASE + sector * SECTOR_SIZE;
    len  = count * SECTOR_SIZE;
    off  = 0;
    while (len > 0) {
        uint32_t a         = addr + off;
        uint32_t blk_start = a & ~(4096UL - 1UL);   /* 所在 4KB 块基址 */
        uint32_t blk_off   = a - blk_start;
        uint32_t chunk     = 4096UL - blk_off;
        if (chunk > len) chunk = len;
        if (blk_off == 0 && chunk == 4096UL && s_wcache_valid && s_wcache_blk == blk_start) {
            /* 整块写且命中缓存：直接整块进缓存 */
            memcpy(s_wcache, (const uint8_t *)buff + off, 4096UL);
            s_wcache_dirty = 1;
        } else if (blk_off == 0 && chunk == 4096UL) {
            /* 整块写、缓存不在本块：冲刷旧缓存后直接擦+写 */
            if (wcache_flush() != RES_OK)
                return RES_ERROR;
            if (QSPI_FLASH_EraseSector(blk_start) != 0)
                return RES_ERROR;
            if (QSPI_FLASH_Write(blk_start, (uint8_t *)buff + off, 4096UL) != 0)
                return RES_ERROR;
        } else {
            /* 部分写：先确保缓存载入本块（换块时冲刷并读回整块） */
            if (!s_wcache_valid || s_wcache_blk != blk_start) {
                if (wcache_flush() != RES_OK)
                    return RES_ERROR;
                if (QSPI_FLASH_Read(blk_start, s_wcache, 4096UL) != 0)
                    return RES_ERROR;
                s_wcache_blk   = blk_start;
                s_wcache_valid = 1;
                s_wcache_dirty = 0;
            }
            memcpy(s_wcache + blk_off, (const uint8_t *)buff + off, chunk);
            s_wcache_dirty = 1;
        }
        off += chunk;
        len -= chunk;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
    case CTRL_SYNC:
        return wcache_flush();   /* v9.81cl: 冲刷写回缓存 */
    case CTRL_TRIM:
        return RES_OK;          /* v9.81cl: 卸载/删除不主动擦除 */
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = FATVOL_SIZE / SECTOR_SIZE;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = SECTOR_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 4096UL / SECTOR_SIZE;  /* 擦除块 = 4KB = 8 扇区 */
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
