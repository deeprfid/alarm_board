/**
 * @file rdr_cfg_kv.c
 * @brief RFID 配置 FlashDB KV 存储实现（v9.81s）
 *        配置数据只存 QSPI FlashDB KV；hwport flash_bytes_* 拦截配置区地址路由到本模块
 */
#include <string.h>
#include "hc32f46_driver.h"   /* rfidcfg 类型、flash 读 */
#include "flashdb.h"
#include "rdr_cfg_kv.h"

extern struct fdb_kvdb AlarmDB;   /* flashdb.c 初始化，fdb_kvdb1 分区 */

static int s_kv_ready = 0;   /* v9.81s: flashdb() 后置位，hwport 拦截启用开关 */

typedef struct { uint32_t base; uint32_t len; const char *key; } rdr_cfg_map_t;

/* 配置区地址区间 → KV 键（区间匹配，支持偏移访问） */
static const rdr_cfg_map_t s_cfg_map[] = {
    { RDR_EASTAG_ADDR,    sizeof(rfidcfg), RDR_KV_EASTAG      },
    { RDR_NET_ADDR,       RDR_NET_LEN,     RDR_KV_NETCFG      },
    { RDR_WIFI_ADDR,      RDR_WIFI_LEN,    RDR_KV_WIFICFG     },
    { RDR_HWTYPE_ADDR,    RDR_HWTYPE_LEN,  RDR_KV_HWTYPE      },
    { RDR_MONET_ADDR,     RDR_MONET_LEN,   RDR_KV_MONETCFG    },
    { RDR_BT_ADDR,        RDR_BT_LEN,      RDR_KV_BTCFG       },
    { RDR_ACTIVE_ADDR,    RDR_ACTIVE_LEN,  RDR_KV_ACTIVEMODE  },
    { RDR_WORKMODE_ADDR,  RDR_WORKMODE_LEN,RDR_KV_WORKMODE    },
    { RDR_PASSIVE_ADDR,   RDR_PASSIVE_LEN, RDR_KV_PASSIVEMODE },
    { RDR_BOOT_ADDR,      RDR_BOOT_LEN,    RDR_KV_BOOTCFG     },
};

/* 配置区地址 → KV 键 + 块内偏移；非配置区返回 NULL */
const char *rdr_cfg_addr_to_key(uint32_t addr)
{
    int i;
    for (i = 0; i < (int)(sizeof(s_cfg_map)/sizeof(s_cfg_map[0])); i++) {
        if (addr >= s_cfg_map[i].base && addr < s_cfg_map[i].base + s_cfg_map[i].len)
            return s_cfg_map[i].key;
    }
    return NULL;
}

/* 配置区地址 → KV 键 + 块内偏移；非配置区返回 NULL（hwport 用） */
const char *rdr_cfg_addr_to_key_off(uint32_t addr, uint32_t *off)
{
    int i;
    for (i = 0; i < (int)(sizeof(s_cfg_map)/sizeof(s_cfg_map[0])); i++) {
        if (addr >= s_cfg_map[i].base && addr < s_cfg_map[i].base + s_cfg_map[i].len) {
            *off = addr - s_cfg_map[i].base;
            return s_cfg_map[i].key;
        }
    }
    return NULL;
}

int rdr_cfg_kv_get(const char *key, void *buf, size_t len)
{
    struct fdb_blob blob;
    size_t n = fdb_kv_get_blob(&AlarmDB, key, fdb_blob_make(&blob, buf, len));
    return (n == len) ? 0 : -1;
}

int rdr_cfg_kv_set(const char *key, const void *buf, size_t len)
{
    struct fdb_blob blob;
    fdb_err_t e = fdb_kv_set_blob(&AlarmDB, key, fdb_blob_make(&blob, buf, len));
    return (e == FDB_NO_ERR) ? 0 : -1;
}

/* 一次性迁移：片内旧配置(0xFA000/0xFC000 区) → KV。已迁移跳过。
 * flashdb() 初始化后调用；片内只读一次，之后不再写片内。 */
extern int flashdb(void);   /* app 工程 FlashDB 初始化 */

/* 直读片内 Flash（绕过 hwport 拦截，供迁移读取旧配置用） */
static void rdr_efm_read(uint32_t addr, void *buf, size_t len)
{
    size_t i;
    const volatile uint8_t *p = (const volatile uint8_t *)addr;
    for (i = 0; i < len; i++)
        ((uint8_t *)buf)[i] = p[i];
}

/* 初始化 FlashDB + 迁移（driver init_thread 早期调用，KV 提前就绪） */
void rdr_cfg_kv_init(void)
{
    (void)flashdb();
    rdr_cfg_migrate();
}

int rdr_cfg_kv_ready(void)
{
    return s_kv_ready;
}

void rdr_cfg_migrate(void)
{
    uint32_t ver = 0;
    static uint8_t buf[RDR_ACTIVE_LEN];   /* v9.81s-b: static——migrate 在 1.5KB init_thread 栈跑，2048B 局部数组爆栈 */

    s_kv_ready = 1;   /* v9.81s: flashdb() 已初始化，KV 就绪（hwport 拦截从此启用） */

    if (rdr_cfg_kv_get(RDR_KV_CFGVER, &ver, sizeof(ver)) == 0 && ver == 1U)
        return;   /* 已迁移过，KV 就绪标志已置 */

    /* EastagPage（rfidcfg） */
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_EASTAG_ADDR, buf, sizeof(rfidcfg));   /* 直读片内（不经 hwport 拦截） */
    if (!rdr_cfg_is_all_ff(buf, sizeof(rfidcfg)))
        rdr_cfg_kv_set(RDR_KV_EASTAG, buf, sizeof(rfidcfg));

    /* 9 项配置块 */
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_NET_ADDR, buf, RDR_NET_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_NET_LEN)) rdr_cfg_kv_set(RDR_KV_NETCFG, buf, RDR_NET_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_WIFI_ADDR, buf, RDR_WIFI_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_WIFI_LEN)) rdr_cfg_kv_set(RDR_KV_WIFICFG, buf, RDR_WIFI_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_HWTYPE_ADDR, buf, RDR_HWTYPE_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_HWTYPE_LEN)) rdr_cfg_kv_set(RDR_KV_HWTYPE, buf, RDR_HWTYPE_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_MONET_ADDR, buf, RDR_MONET_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_MONET_LEN)) rdr_cfg_kv_set(RDR_KV_MONETCFG, buf, RDR_MONET_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_BT_ADDR, buf, RDR_BT_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_BT_LEN)) rdr_cfg_kv_set(RDR_KV_BTCFG, buf, RDR_BT_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_ACTIVE_ADDR, buf, RDR_ACTIVE_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_ACTIVE_LEN)) rdr_cfg_kv_set(RDR_KV_ACTIVEMODE, buf, RDR_ACTIVE_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_WORKMODE_ADDR, buf, RDR_WORKMODE_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_WORKMODE_LEN)) rdr_cfg_kv_set(RDR_KV_WORKMODE, buf, RDR_WORKMODE_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_PASSIVE_ADDR, buf, RDR_PASSIVE_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_PASSIVE_LEN)) rdr_cfg_kv_set(RDR_KV_PASSIVEMODE, buf, RDR_PASSIVE_LEN);
    memset(buf, 0xFF, sizeof(buf));
    rdr_efm_read(RDR_BOOT_ADDR, buf, RDR_BOOT_LEN);
    if (!rdr_cfg_is_all_ff(buf, RDR_BOOT_LEN)) rdr_cfg_kv_set(RDR_KV_BOOTCFG, buf, RDR_BOOT_LEN);

    ver = 1U;
    rdr_cfg_kv_set(RDR_KV_CFGVER, &ver, sizeof(ver));

}

int rdr_cfg_is_all_ff(const uint8_t *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        if (buf[i] != 0xFF)
            return 0;
    return 1;
}

/* 配置块长度（按映射表） */
size_t rdr_cfg_kv_len(const char *key)
{
    int i;
    for (i = 0; i < (int)(sizeof(s_cfg_map)/sizeof(s_cfg_map[0])); i++)
        if (s_cfg_map[i].key == key || strcmp(s_cfg_map[i].key, key) == 0)
            return s_cfg_map[i].len;
    return 0;
}

/* KV 带偏移读：读整块到临时缓冲，取偏移段 */
int rdr_cfg_kv_get_at(const char *key, size_t off, void *buf, size_t len)
{
    size_t blklen = rdr_cfg_kv_len(key);
    uint8_t *tmp;   /* v9.81t: 2048B 栈数组→堆，防保存线程栈溢出 */
    size_t n;
    struct fdb_blob blob;

    if (blklen == 0 || blklen > RDR_ACTIVE_LEN || off + len > blklen)
        return -1;
    tmp = malloc_hexp(blklen);
    if (tmp == NULL)
        return -1;
    n = fdb_kv_get_blob(&AlarmDB, key, fdb_blob_make(&blob, tmp, blklen));
    if (n < off + len)
    {
        free_hexp(tmp);
        return -1;
    }
    memcpy(buf, tmp + off, len);
    free_hexp(tmp);
    return 0;
}

/* KV 带偏移写：读整块→改偏移段→写回（RMW） */
int rdr_cfg_kv_set_at(const char *key, size_t off, const void *buf, size_t len)
{
    size_t blklen = rdr_cfg_kv_len(key);
    uint8_t *tmp;   /* v9.81t: 2048B 栈数组→堆，防保存线程栈溢出 */
    size_t n;
    struct fdb_blob blob;
    fdb_err_t e;

    if (blklen == 0 || blklen > RDR_ACTIVE_LEN || off + len > blklen)
        return -1;
    tmp = malloc_hexp(blklen);
    if (tmp == NULL)
        return -1;
    n = fdb_kv_get_blob(&AlarmDB, key, fdb_blob_make(&blob, tmp, blklen));
    if (n != blklen) memset(tmp, 0xFF, blklen);   /* 无旧数据 → 全 FF */
    memcpy(tmp + off, buf, len);
    e = fdb_kv_set_blob(&AlarmDB, key, fdb_blob_make(&blob, tmp, blklen));
    free_hexp(tmp);
    return (e == FDB_NO_ERR) ? 0 : -1;
}
