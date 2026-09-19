/**
 * @file  radar_ota.c
 * @brief F4A0 -> F460 报警板固件分发实现
 *
 * 流程：挂载 FAT 卷 -> 打开 RADAR.BIN -> 整体读进堆 -> 校验包头/长度 -> 卸载 FAT
 *       -> ota_dist_start(fd, pkg, len) 发起 -> 由 radar_ota_poll() 周期推进
 *       -> 成功后删除 RADAR.BIN（避免每次上电重复分发同一个包）
 *
 * 【为什么先把整包读进内存再卸载 FAT】分发是长过程（30KB 包在 460800bps 上要好几秒），
 * 期间不该把 FAT 卷一直挂着 —— USB MSC 那边可能也要访问；而且读文件比发帧快得多，
 * 一次性读完反而简单（包只有几十 KB，堆放得下）。
 */
#include <string.h>
#include "radar_ota.h"
#include "ff.h"
#include "hc32f46_driver.h"   /* COMMON_INTERFACE_RS485_x / malloc_hexp / free_hexp */
#include "ota_dist.h"
#include "ota_host.h"

#define RADAR_PKG_NAME        "RADAR.BIN"
#define RADAR_PKG_HDR_LEN     82u
#define RADAR_PKG_MAX         (256UL * 1024UL)   /* 防呆上限：超出直接拒，别把堆吃光 */

/* 目标 RS485 通道。
 * 依据 alarm.c 的 ipc_hpm_message() 分发关系：antid 0x1->RS485_3、0x2/0x3->RS485_2、0x4/0x0->RS485_1。
 * 默认取 RS485_1（antid 0x4；广播 0x0 也含它）。要换口改这一行即可。 */
#define RADAR_OTA_IFACE       COMMON_INTERFACE_RS485_1

static uint8_t *s_pkg  = NULL;    /* 整包（含 82B 包头） */
static uint32_t s_len  = 0;
static FATFS    s_fs;
static int      s_fs_mounted = 0;

static uint32_t rd_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void radar_ota_release(void)
{
    if (s_pkg != NULL) { free_hexp(s_pkg); s_pkg = NULL; }
    s_len = 0;
}

static void radar_ota_unmount(void)
{
    if (s_fs_mounted) { f_mount(NULL, "", 0); s_fs_mounted = 0; }
}

/* 校验并读入整包。返回 0 成功 */
static int radar_ota_load(void)
{
    FIL     file;
    FRESULT fr;
    UINT    br = 0;
    uint8_t hdr[RADAR_PKG_HDR_LEN];
    uint32_t payload_len;
    uint32_t want;

    fr = f_mount(&s_fs, "", 1);
    if (fr != FR_OK) {
        TRACE("radar_ota: mount fail %d\n", (int)fr);
        return 0;                       /* 无 FAT 卷不算错误 */
    }
    s_fs_mounted = 1;

    fr = f_open(&file, RADAR_PKG_NAME, FA_READ);
    if (fr != FR_OK) {
        TRACE("radar_ota: no %s\n", RADAR_PKG_NAME);
        radar_ota_unmount();
        return 0;                       /* 没有分发文件：正常情况 */
    }

    want = (uint32_t)f_size(&file);
    if ((want < RADAR_PKG_HDR_LEN) || (want > RADAR_PKG_MAX)) {
        TRACE("radar_ota: bad size %lu\n", (unsigned long)want);
        goto bad;
    }

    if ((f_read(&file, hdr, RADAR_PKG_HDR_LEN, &br) != FR_OK) || (br != RADAR_PKG_HDR_LEN)) {
        TRACE("radar_ota: read hdr fail\n");
        goto bad;
    }
    if ((hdr[0] != 'O') || (hdr[1] != 'T') || (hdr[2] != 'A') || (hdr[3] != '1')) {
        TRACE("radar_ota: bad magic\n");
        goto bad;
    }

    /* 长度自洽：文件大小必须等于 82 + payload_len（与 tools/ota_pack_f460.py 的产出一致） */
    payload_len = rd_u32le(&hdr[10]);
    if ((RADAR_PKG_HDR_LEN + payload_len) != want) {
        TRACE("radar_ota: len mismatch hdr=%lu file=%lu\n",
              (unsigned long)(RADAR_PKG_HDR_LEN + payload_len), (unsigned long)want);
        goto bad;
    }

    s_pkg = (uint8_t *)malloc_hexp(want);
    if (s_pkg == NULL) {
        TRACE("radar_ota: oom %lu\n", (unsigned long)want);
        goto bad;
    }

    if (f_lseek(&file, 0) != FR_OK) { goto bad; }
    if ((f_read(&file, s_pkg, want, &br) != FR_OK) || (br != want)) {
        TRACE("radar_ota: read body fail\n");
        goto bad;
    }

    s_len = want;
    f_close(&file);
    radar_ota_unmount();               /* 读完就卸载：分发期间不占着 FAT */
    TRACE("radar_ota: loaded %s %lu bytes, ver 0x%08lX\n",
          RADAR_PKG_NAME, (unsigned long)s_len, (unsigned long)rd_u32le(&hdr[4]));
    return 1;                          /* 1 = 载入成功待发起 */

bad:
    f_close(&file);
    radar_ota_unmount();
    radar_ota_release();
    return -1;
}

int radar_ota_check(void)
{
    int r;

    if (radar_ota_busy()) { return 0; }

    r = radar_ota_load();
    if (r <= 0) { return (r == 0) ? 0 : -1; }

    if (ota_dist_start(RADAR_OTA_IFACE, s_pkg, s_len) != 0) {
        TRACE("radar_ota: dist_start FAIL\n");
        radar_ota_release();
        return -1;
    }

    TRACE("radar_ota: distributing %lu bytes on RS485 iface %d\n",
          (unsigned long)s_len, (int)RADAR_OTA_IFACE);
    return 1;
}

void radar_ota_poll(void)
{
    int r;

    if (!radar_ota_busy()) { return; }

    r = ota_dist_poll();
    if (r != 0) { return; }            /* 仍在进行 */

    /* 结束：ota_dist_poll 返回 0 时结果已固化在 ota_dist_result() */
    r = ota_dist_result();
    TRACE("radar_ota: done, result=%d, progress=%lu/%lu\n",
          r, (unsigned long)ota_dist_progress(), (unsigned long)ota_dist_total());

    if (r == 1) {
        /* 成功：删掉 RADAR.BIN，避免每次上电重复分发同一个包。
         * 失败【不删】—— 留着方便复位后重试/排查。 */
        if (f_mount(&s_fs, "", 1) == FR_OK) {
            if (f_unlink(RADAR_PKG_NAME) == FR_OK) {
                TRACE("radar_ota: %s removed\n", RADAR_PKG_NAME);
            }
            f_mount(NULL, "", 0);
        }
    }

    radar_ota_release();
}

int radar_ota_busy(void)
{
    return ota_dist_busy();
}
