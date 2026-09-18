/* usb_msc_ota.c - USB 虚拟盘符升级检测 (v9.81ci)
 * 启动时（ota_agent_boot 后）调用：
 *   1. 挂载 FAT 卷（QSPI 最后 8MB，0x1800000）
 *   2. 查找 fw.bin（完整 OTA 包：82B 包头 + 载荷）
 *   3. 流式搬移到 OTA 暂存区（0xE00000）
 *   4. 验签 + CRC 校验
 *   5. 标记 NEED_COMMIT → 重启 → bootloader 升级
 */
#include "usb_msc_ota.h"
#include "ff.h"
#include "ota_storage.h"
#include "ota_security.h"
#include "ota_flag.h"
#include "ota_state.h"      /* v9.81cl: 统一 ota_mark_ready（KV+标志页），与串口/USB/HTTP 一致 */
#include "app_conf.h"         /* OTA_FW_VERSION */
#include "hc32f46_driver.h"
#include <string.h>
#include <stdio.h>

#define OTA_HEADER_LEN      82
#define OTA_HDR_VERSION_OFF 4
#define OTA_HDR_PAYLOAD_OFF 10
#define OTA_HDR_CRC_OFF     14
#define STAGE_IO_CHUNK      4096

static FATFS s_fs;
static FIL  s_file;

/* 读回调：从 fw.bin 文件读（ota_security_verify_ex 用） */
typedef struct {
    FIL *fp;
} fw_read_ctx_t;

static int fw_read(uint32_t off, uint8_t *buf, uint32_t len, void *arg)
{
    fw_read_ctx_t *ctx = (fw_read_ctx_t *)arg;
    UINT br = 0;
    if (f_lseek(ctx->fp, (FSIZE_t)off) != FR_OK) return -1;
    if (f_read(ctx->fp, buf, len, &br) != FR_OK) return -1;
    return (br == len) ? 0 : -1;
}

int usb_msc_ota_check(void)
{
    uint8_t hdr[OTA_HEADER_LEN];
    uint32_t fw_len, version, crc_expect;
    uint32_t payload_len;
    uint8_t *chunk;
    uint32_t off = 0;   /* v9.81ci: 从 0 开始，完整拷贝包头(82B)+载荷 */
    FRESULT fr;
    UINT br;
    fw_read_ctx_t ctx;
    int ret = 0;

    /* 1. 挂载 FAT 卷（卷 0 = QSPI 最后 8MB） */
    fr = f_mount(&s_fs, "", 1);
    if (fr != FR_OK) {
        TRACE("usb_msc_ota: mount fail %d\n", (int)fr);
        return 0;   /* 无 FAT 卷不视为错误 */
    }

    /* 2. 打开 fw.bin */
    fr = f_open(&s_file, "FW.BIN", FA_READ);
    if (fr != FR_OK) {
        TRACE("usb_msc_ota: open FW.BIN fail %d\n", (int)fr);
        f_mount(NULL, "", 0);   /* 卸载 */
        return 0;   /* 无升级文件 */
    }

    /* 3. 读包头校验 magic */
    if (f_read(&s_file, hdr, sizeof(hdr), &br) != FR_OK || br != sizeof(hdr)) {
        TRACE("usb_msc_ota: read hdr fail\n");
        goto fin_no_upgrade;
    }
    if (hdr[0] != 'O' || hdr[1] != 'T' || hdr[2] != 'A' || hdr[3] != '1') {
        TRACE("usb_msc_ota: bad magic\n");
        goto fin_no_upgrade;
    }

    version = (uint32_t)hdr[OTA_HDR_VERSION_OFF] |
              ((uint32_t)hdr[OTA_HDR_VERSION_OFF+1] << 8) |
              ((uint32_t)hdr[OTA_HDR_VERSION_OFF+2] << 16) |
              ((uint32_t)hdr[OTA_HDR_VERSION_OFF+3] << 24);
    payload_len = (uint32_t)hdr[OTA_HDR_PAYLOAD_OFF] |
              ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF+1] << 8) |
              ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF+2] << 16) |
              ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF+3] << 24);
    crc_expect = (uint32_t)hdr[OTA_HDR_CRC_OFF] |
              ((uint32_t)hdr[OTA_HDR_CRC_OFF+1] << 8) |
              ((uint32_t)hdr[OTA_HDR_CRC_OFF+2] << 16) |
              ((uint32_t)hdr[OTA_HDR_CRC_OFF+3] << 24);
    fw_len = OTA_HEADER_LEN + payload_len;

    TRACE("usb_msc_ota: fw.bin v0x%08X size %u crc %08X\n",
          (unsigned)version, (unsigned)fw_len, (unsigned)crc_expect);

    /* 版本检查：不高于当前则忽略 */
    if (version < OTA_FW_VERSION) {    /* v9.81cm: 同版本允许重刷（仅拒降级） */
        TRACE("usb_msc_ota: version not newer, skip\n");
        goto fin_no_upgrade;
    }

    /* 4. 准备暂存区（擦除） */
    if (ota_storage_prepare(fw_len) != 0) {
        TRACE("usb_msc_ota: stage prepare fail\n");
        ret = -1;
        goto fin;
    }

    /* 5. 流式搬移 fw.bin → OTA 暂存区 */
    chunk = (uint8_t *)malloc_hexp(STAGE_IO_CHUNK);
    if (chunk == NULL) { ret = -1; goto fin; }
    f_lseek(&s_file, 0);
    while (off < fw_len) {
        uint32_t want = (fw_len - off < STAGE_IO_CHUNK) ? (fw_len - off) : STAGE_IO_CHUNK;
        if (f_read(&s_file, chunk, want, &br) != FR_OK || br != want) {
            free_hexp(chunk);
            ret = -1;
            goto fin;
        }
        if (ota_storage_write_stage(chunk, want, off) != 0) {
            free_hexp(chunk);
            ret = -1;
            goto fin;
        }
        off += want;
    }
    free_hexp(chunk);

    /* 6. 验签 + CRC（读 fw.bin 文件） */
    ctx.fp = &s_file;
    if (ota_security_verify_ex(fw_read, &ctx) != 0) {
        TRACE("usb_msc_ota: security verify fail\n");
        ret = -1;
        goto fin;
    }

    /* 7. 标记升级就绪 → 重启（v9.81cl: 与其他三通道一致——ota_mark_ready 写 KV+标志页，size 传 payload 长度） */
    TRACE("usb_msc_ota: upgrade ready v0x%08X, reboot\n", (unsigned)version);
    if (ota_mark_ready(payload_len, version) != 0) {
        TRACE("usb_msc_ota: mark_ready FAIL\n");
        ret = -1;
        goto fin;
    }
    f_unlink("FW.BIN");   /* v9.81ci: ???????,??????? */
    ret = 1;
    goto fin;

fin_no_upgrade:
    ret = 0;
fin:
    f_close(&s_file);
    f_mount(NULL, "", 0);
    return ret;
}
