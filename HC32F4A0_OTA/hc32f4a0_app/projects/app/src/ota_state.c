/**
 * @file ota_state.c
 * @brief 升级状态机实现：FlashDB KV（AlarmDB）持久化状态，掉电安全
 * @note 借鉴 EasyFlash IAP：env(iap_need_copy) 标志 + 掉电恢复
 */
#include <string.h>
#include "flashdb.h"
#include "ota_state.h"
#include "ota_flag.h"
#include "hc32f46_driver.h"      /* TRACE */

extern struct fdb_kvdb AlarmDB;   /* flashdb.c 初始化，fdb_kvdb1 分区 */

static fdb_err_t kv_set_u32(const char *key, uint32_t val)
{
    struct fdb_blob blob;
    return fdb_kv_set_blob(&AlarmDB, key, fdb_blob_make(&blob, &val, sizeof(val)));
}

static int kv_get_u32(const char *key, uint32_t *val)
{
    struct fdb_blob blob;
    size_t n = fdb_kv_get_blob(&AlarmDB, key, fdb_blob_make(&blob, val, sizeof(*val)));
    return (n == sizeof(*val)) ? 0 : -1;
}

void ota_state_init(void)
{
    /* 无特殊初始化；KV 在 flashdb() 中已初始化 */
}

ota_state_t ota_state_run(void)
{
    uint32_t need_copy = 0, progress = 0, size = 0, sign_ok = 0;

    kv_get_u32(OTA_KV_NEED_COPY, &need_copy);
    kv_get_u32(OTA_KV_PROGRESS, &progress);
    kv_get_u32(OTA_KV_COPY_SIZE, &size);
    kv_get_u32(OTA_KV_SIGN_OK, &sign_ok);

    if (need_copy) {
        if (sign_ok && progress >= size && size > 0)
            return OTA_STATE_READY;                 /* 待 commit（bootloader 执行） */
        return OTA_STATE_DOWNLOADING;               /* 下载未完成/未验签 → 续传或重下 */
    }
    return OTA_STATE_IDLE;
}

void ota_set_progress(uint32_t bytes)
{
    kv_set_u32(OTA_KV_PROGRESS, bytes);
}

uint32_t ota_get_progress(void)
{
    uint32_t p = 0;
    kv_get_u32(OTA_KV_PROGRESS, &p);
    return p;
}

uint32_t ota_get_copy_size(void)
{
    uint32_t n = 0;
    kv_get_u32(OTA_KV_COPY_SIZE, &n);
    return n;
}

int ota_mark_ready(uint32_t size, uint32_t version)
{
    fdb_err_t e;
    int fr;

    e = kv_set_u32(OTA_KV_COPY_SIZE, size);
    if (e != FDB_NO_ERR) return -1;
    e = kv_set_u32(OTA_KV_VERSION, version);
    if (e != FDB_NO_ERR) return -1;
    e = kv_set_u32(OTA_KV_SIGN_OK, 1);
    if (e != FDB_NO_ERR) return -1;
    /* 最后写 NEED_COPY（先数据后标志，防半写误判） */
    e = kv_set_u32(OTA_KV_NEED_COPY, 1);
    if (e != FDB_NO_ERR) return -1;
    /* Phase 4：标志页 NEED_COMMIT（bootloader 读，先数据后标志） */
    fr = ota_flag_mark_ready(size, version);
    TRACE("ota mark: flag=%d\n", fr);
    if (fr != 0) return -1;
    return 0;   /* 调用方随后 system_reset() 进 bootloader */
}

void ota_self_check_ok(void)
{
    kv_set_u32(OTA_KV_NEED_COPY, 0);
    kv_set_u32(OTA_KV_SIGN_OK, 0);
    kv_set_u32(OTA_KV_RESULT, 0);
    ota_boot_count_clear();
}

void ota_boot_confirm(void)
{
    /* 新固件自检通过：清标志 NEED_CONFIRM/boot_count + KV（bootloader 据此不回滚） */
    (void)ota_flag_confirm();
    kv_set_u32(OTA_KV_NEED_COPY, 0);
    kv_set_u32(OTA_KV_SIGN_OK, 0);
    kv_set_u32(OTA_KV_RESULT, 0);
    kv_set_u32(OTA_KV_COPY_SIZE, 0);   /* v9.80：一并清 copy_size/version，杜绝跨轮升级残留 */
    kv_set_u32(OTA_KV_VERSION, 0);
    ota_boot_count_clear();
    ota_set_progress(0);      /* 升级完成：下载进度归零，下次干净下载 */
}

void ota_fail(uint8_t result)
{
    kv_set_u32(OTA_KV_RESULT, result);
    if (result == 2) {
        /* rollback：清 need_copy，bootloader 负责恢复 */
        kv_set_u32(OTA_KV_NEED_COPY, 0);
    }
}

uint32_t ota_boot_count_get(void)
{
    uint32_t n = 0;
    kv_get_u32(OTA_KV_BOOT_COUNT, &n);
    return n;
}

void ota_boot_count_inc(void)
{
    uint32_t n = ota_boot_count_get() + 1;
    kv_set_u32(OTA_KV_BOOT_COUNT, n);
}

void ota_boot_count_clear(void)
{
    kv_set_u32(OTA_KV_BOOT_COUNT, 0);
}
