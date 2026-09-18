/* usb_dev_msc_qspi.c - USB MSC media = QSPI FAT volume (v9.81ci)
 * 与 FatFs diskio.c 共用同一 QSPI 分区（0x1800000，8MB）：
 *   - 电脑经 USB 写文件 → msc_write → QSPI FAT 卷
 *   - 固件读文件（FatFs）→ disk_read → 同一 QSPI 分区
 */
#include "usb_dev_msc_mem.h"
#include "usb_dev_msc_class.h"
#include "qspi_flash.h"
#include <string.h>   /* memcpy (v9.81cl) */

/* FAT 卷 = QSPI 最后 8MB（与 ff16/diskio.c 一致） */
#define MSC_QSPI_BASE     (0x01800000UL)
#define MSC_QSPI_SIZE     (8UL * 1024 * 1024)
#define MSC_BLOCK_SIZE    512UL
#define MSC_BLOCK_NUM     (MSC_QSPI_SIZE / MSC_BLOCK_SIZE)

__IO static uint8_t u8MscStatusReg = 0U;
#define MSC_STATUS_BIT_INI   (0x01U)

/* v9.81cl: 4KB 读-改-写缓冲（同 diskio.c，USB 写盘同样要防块内部分写擦掉邻居扇区） */
static uint8_t s_msc_rmw_buf[4096];

/* Inquiry data (36 bytes, LUN 0) */
static const int8_t msc_inquirydata[] = {
    0x00, 0x80, 0x02, 0x02,
    (USB_DEV_INQUIRY_LENGTH - 4U),
    0x00, 0x00, 0x00,
    'H', 'D', 'S', 'C', ' ', 'M', 'C', 'U', ' ',
    'Q', 'S', 'P', 'I', ' ', 'D', 'i', 's', 'k', ' ',
    '1', '.', '0', ' '
};

static int8_t msc_init(uint8_t lun);
static int8_t msc_getcapacity(uint8_t lun, uint32_t *block_num, uint32_t *block_size);
static int8_t msc_getmaxlun(void);
static int8_t msc_ifready(uint8_t lun);
static int8_t msc_read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t msc_write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t msc_ifwrprotected(uint8_t lun);

static USB_DEV_MSC_cbk_TypeDef qspi_fops = {
    &msc_init,
    &msc_getcapacity,
    &msc_getmaxlun,
    &msc_ifready,
    &msc_read,
    &msc_write,
    &msc_ifwrprotected,
    (int8_t *)msc_inquirydata
};

USB_DEV_MSC_cbk_TypeDef *msc_fops = &qspi_fops;

static int8_t msc_init(uint8_t lun)
{
    /* QSPI 已由 driver_hw_init_late() 初始化 */
    u8MscStatusReg |= MSC_STATUS_BIT_INI;
    return 0;
}

static int8_t msc_getcapacity(uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    *block_size = MSC_BLOCK_SIZE;
    *block_num  = MSC_BLOCK_NUM;
    return 0;
}

static int8_t msc_getmaxlun(void)
{
    return 0;   /* 单 LUN */
}

static int8_t msc_ifready(uint8_t lun)
{
    return (u8MscStatusReg & MSC_STATUS_BIT_INI) ? 0 : -1;
}

static int8_t msc_ifwrprotected(uint8_t lun)
{
    return 0;   /* 可写 */
}

static int8_t msc_read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    if (QSPI_FLASH_Read(MSC_QSPI_BASE + blk_addr * MSC_BLOCK_SIZE, buf,
                        blk_len * MSC_BLOCK_SIZE) != 0)
        return -1;
    return 0;
}

static int8_t msc_write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    uint32_t addr = MSC_QSPI_BASE + blk_addr * MSC_BLOCK_SIZE;
    uint32_t len  = blk_len * MSC_BLOCK_SIZE;
    uint32_t off  = 0;

    while (len > 0) {
        uint32_t a         = addr + off;
        uint32_t blk_start = a & ~(4096UL - 1UL);   /* 所在 4KB 块基址 */
        uint32_t blk_off   = a - blk_start;
        uint32_t chunk     = 4096UL - blk_off;
        if (chunk > len) chunk = len;
        if (blk_off == 0 && chunk == 4096UL) {
            /* 整块写：直接擦+写 */
            if (QSPI_FLASH_EraseSector(blk_start) != 0)
                return -1;
            if (QSPI_FLASH_Write(blk_start, buf + off, chunk) != 0)
                return -1;
        } else {
            /* 部分写：读-改-写，保护同 4KB 块内其它已写扇区 */
            if (QSPI_FLASH_Read(blk_start, s_msc_rmw_buf, 4096UL) != 0)
                return -1;
            memcpy(s_msc_rmw_buf + blk_off, buf + off, chunk);
            if (QSPI_FLASH_EraseSector(blk_start) != 0)
                return -1;
            if (QSPI_FLASH_Write(blk_start, s_msc_rmw_buf, 4096UL) != 0)
                return -1;
        }
        off += chunk;
        len -= chunk;
    }
    return 0;
}
