/* tag_csv.c - 白名单 TAG.CSV 生成（v9.81cj-k） */
#include "tag_csv.h"
#include "hc32f46_driver.h"
#include <flashdb.h>
#include "ff.h"
#include "List.h"   /* LTDataType */
#include "wl_dedup.h"
#include "task_monitor.h"   /* SoftWdtFed / FLASHDB_SWDT_ID */
#include <string.h>

extern struct fdb_tsdb whitelistDB;

extern volatile int gIsUsbAvailable;   /* usb_dev_user.c: USB 已枚举且未挂起（电脑在用） */

static volatile int s_csv_dirty = 0;
static int s_usb_defer_logged = 0;   /* v9.81cl: USB 占用提示只打一次 */

/* v9.81cl: FAT 卷互斥（tag_csv_task 生成 vs HTTP /tagcsv 下载，FatFs 非线程安全） */
static volatile int s_fat_busy = 0;

int tag_csv_fat_trylock(void)
{
    __disable_irq();
    if (s_fat_busy) { __enable_irq(); return -1; }
    s_fat_busy = 1;
    __enable_irq();
    return 0;
}

void tag_csv_fat_unlock(void)
{
    __disable_irq();
    s_fat_busy = 0;
    __enable_irq();
}

/* v9.81cl: f_mkfs 工作缓冲（FAT 卷损坏自愈用，仅挂载/打开失败时触发） */
static uint8_t s_mkfs_work[4096];

void tag_csv_mark_dirty(void)
{
    s_csv_dirty = 1;
}

static FRESULT tag_csv_mkfs(void)
{
    FRESULT fr = f_mkfs("", NULL, s_mkfs_work, sizeof(s_mkfs_work));
    TRACE("tag_csv: mkfs -> %d\n", (int)fr);
    return fr;
}

/* 位图去重计数回调（先数唯一数，可选） */
typedef struct {
    FIL *fp;
    uint32_t count;
    char line[64];
} csv_ctx_t;

static bool csv_write_cb(fdb_tsl_t tsl, void *arg)
{
    csv_ctx_t *ctx = (csv_ctx_t *)arg;
    struct fdb_blob blob;
    LTDataType epcID;
    UINT bw;

    if (tsl->status != FDB_TSL_WRITE)
        return false;   /* 跳过 DELETED/非写入 */

    fdb_blob_read((fdb_db_t)&whitelistDB,
                  fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &epcID, sizeof(epcID))));
    if (blob.saved.len <= 0 || epcID.Epclen <= 0 || epcID.Epclen > 16)
        return false;

    /* 位图去重 */
    if (!wl_dedup_check(epcID.epc, epcID.Epclen))
        return false;

    /* EPC -> hex 字符串 */
    ctx->line[0] = 0;
    {
        char hex[] = "0123456789ABCDEF";
        uint8_t i;
        char *p = ctx->line;
        for (i = 0; i < epcID.Epclen; i++) {
            *p++ = hex[(epcID.epc[i] >> 4) & 0xF];
            *p++ = hex[epcID.epc[i] & 0xF];
        }
        *p++ = '\r';
        *p++ = '\n';
        *p = 0;
    }

    /* v9.81cj-k: 每 500 条喂狗，防 47s 生成超时复位 */
    if ((ctx->count % 500) == 0)
        SoftWdtFed(FLASHDB_SWDT_ID);
    if (f_write(ctx->fp, ctx->line, (UINT)strlen(ctx->line), &bw) != FR_OK)
        return true;   /* 写失败停止 */
    ctx->count++;
    return false;
}

void tag_csv_task(void)
{
    FATFS fs;
    FIL fp;
    FRESULT fr;
    csv_ctx_t ctx;
    uint32_t start = (uint32_t)getSysTick();

    if (!s_csv_dirty)
        return;

    /* v9.81cl: USB 枚举激活期间不写 FAT 卷——电脑挂载着同一个 8MB 分区，
     * 设备侧写入会与电脑缓存/写回冲突（损坏卷 + 电脑显示旧内容）。
     * 保持 dirty 标记，等 USB 拔出（挂起/断开）后再生成。 */
    if (gIsUsbAvailable) {
        if (!s_usb_defer_logged) {
            TRACE("tag_csv: USB active, defer generation\n");
            s_usb_defer_logged = 1;
        }
        return;
    }
    s_usb_defer_logged = 0;
    s_csv_dirty = 0;

    /* v9.81cl: 与 HTTP /tagcsv 下载互斥；生成中下载方会收到 503 */
    if (tag_csv_fat_trylock() != 0) {
        s_csv_dirty = 1;   /* 下次再生成 */
        return;
    }

    /* 1. 挂载 FAT 卷（卷 0 = QSPI 最后 8MB）；失败则整卷重建一次再试 */
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        TRACE("tag_csv: mount fail %d, rebuild volume\n", (int)fr);
        tag_csv_mkfs();
        fr = f_mount(&fs, "", 1);
        if (fr != FR_OK) {
            TRACE("tag_csv: remount fail %d\n", (int)fr);
            tag_csv_fat_unlock();
            s_csv_dirty = 1;   /* 下次再试 */
            return;
        }
    }

    /* 2. 创建/覆盖 TAG.CSV；失败则重建卷重试一次 */
    fr = f_open(&fp, "TAG.CSV", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        TRACE("tag_csv: open fail %d, rebuild volume\n", (int)fr);
        f_mount(NULL, "", 0);
        tag_csv_mkfs();
        fr = f_mount(&fs, "", 1);
        if (fr == FR_OK)
            fr = f_open(&fp, "TAG.CSV", FA_WRITE | FA_CREATE_ALWAYS);
        if (fr != FR_OK) {
            TRACE("tag_csv: reopen fail %d\n", (int)fr);
            f_mount(NULL, "", 0);
            tag_csv_fat_unlock();
            s_csv_dirty = 1;
            return;
        }
    }

    /* 3. 遍历 TSDB 去重写文件 */
    wl_dedup_reset();
    memset(&ctx, 0, sizeof(ctx));
    ctx.fp = &fp;
    fdb_tsl_iter(&whitelistDB, csv_write_cb, &ctx);

    /* 4. 落盘 + 关闭 + 卸载 */
    f_sync(&fp);
    f_close(&fp);
    f_mount(NULL, "", 0);
    tag_csv_fat_unlock();

    TRACE("tag_csv: %u tags -> TAG.CSV (%u ms)\n",
          (unsigned)ctx.count, (unsigned)((uint32_t)getSysTick() - start));
}
