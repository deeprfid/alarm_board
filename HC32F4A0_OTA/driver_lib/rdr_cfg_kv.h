/**
 * @file rdr_cfg_kv.h
 * @brief RFID 配置 FlashDB KV 存储（v9.81s）——配置数据 ONLY 存 QSPI FlashDB，片内不存
 *
 * 背景：OTA dual-bank swap 后片内配置区丢失（P0）。
 * 方案：hwport flash_bytes_read/write/erase 拦截配置区地址 → 路由到 KV。
 *       配置数据只存 QSPI（OTA 天然保留），片内配置区不再写入。
 */
#ifndef RDR_CFG_KV_H
#define RDR_CFG_KV_H

#include <stdint.h>
#include <stddef.h>

/* 配置区地址范围（片内，仅用于识别，数据不存片内） */
#define RDR_CFG_AREA_START    0x000FA000UL   /* EastagPage */
#define RDR_CFG_AREA_END      0x000FE000UL   /* 9 项配置区末 */

/* KV 键（fdb_kvdb1 分区，AlarmDB） */
#define RDR_KV_EASTAG       "rdr.eastag"
#define RDR_KV_NETCFG       "rdr.netcfg"
#define RDR_KV_WIFICFG      "rdr.wificfg"
#define RDR_KV_HWTYPE       "rdr.hwtype"
#define RDR_KV_MONETCFG     "rdr.monetcfg"
#define RDR_KV_BTCFG        "rdr.btcfg"
#define RDR_KV_ACTIVEMODE   "rdr.activemode"
#define RDR_KV_WORKMODE     "rdr.workmode"
#define RDR_KV_PASSIVEMODE  "rdr.passivemode"
#define RDR_KV_BOOTCFG      "rdr.bootcfg"
#define RDR_KV_CFGVER       "rdr.cfgver"

/* 配置块地址与大小（与 driverconfig.h / ipc.h 一致） */
#define RDR_EASTAG_ADDR      0x000FA000UL
#define RDR_NET_ADDR         0x000FC000UL
#define RDR_WIFI_ADDR        0x000FC080UL
#define RDR_HWTYPE_ADDR      0x000FC200UL
#define RDR_MONET_ADDR       0x000FC220UL
#define RDR_BT_ADDR          0x000FC300UL
#define RDR_ACTIVE_ADDR      0x000FC400UL
#define RDR_WORKMODE_ADDR    0x000FCF00UL
#define RDR_PASSIVE_ADDR     0x000FD000UL
#define RDR_BOOT_ADDR        0x000FDC00UL

#define RDR_NET_LEN          100
#define RDR_WIFI_LEN         256
#define RDR_HWTYPE_LEN       32
#define RDR_MONET_LEN        224
#define RDR_BT_LEN           128
#define RDR_ACTIVE_LEN       2048
#define RDR_WORKMODE_LEN     64
#define RDR_PASSIVE_LEN      2048
#define RDR_BOOT_LEN         320

/* 配置区地址 → KV 键（供 hwport 拦截用）；非配置区返回 NULL */
const char *rdr_cfg_addr_to_key(uint32_t addr);
/* 配置区地址 → KV 键 + 块内偏移；非配置区返回 NULL（hwport 偏移访问用） */
const char *rdr_cfg_addr_to_key_off(uint32_t addr, uint32_t *off);

/* 配置块长度（hwport 用） */
size_t rdr_cfg_kv_len(const char *key);

/* KV 带偏移读写（块内偏移，0=成功） */
int rdr_cfg_kv_get_at(const char *key, size_t off, void *buf, size_t len);
int rdr_cfg_kv_set_at(const char *key, size_t off, const void *buf, size_t len);

/* KV 读写（0=成功） */
int rdr_cfg_kv_get(const char *key, void *buf, size_t len);
int rdr_cfg_kv_set(const char *key, const void *buf, size_t len);

/* 判断缓冲是否全 0xFF */
int rdr_cfg_is_all_ff(const uint8_t *buf, size_t len);

/* 初始化 FlashDB + 配置迁移（driver init_thread 早期调用；幂等可重复） */
void rdr_cfg_kv_init(void);

/* KV 就绪标志：flashdb() 初始化后置 1（hwport 拦截只在就绪后启用） */
int rdr_cfg_kv_ready(void);

/* 一次性迁移：片内旧配置 → KV（启动时调用一次，之后片内不再写） */
void rdr_cfg_migrate(void);

#endif /* RDR_CFG_KV_H */


