/**
 * @file ota_storage.h
 * @brief MCU 升级存储后端（QSPI 暂存 + 片内 dual-bank commit），F4A0 首个实现
 * @version V0.2  2026-08-14  Phase 3 起点
 *
 * 布局：
 *   QSPI 16MB：0x00400000 起 4MB = 固件暂存区（下载目标，不碰片内运行区）
 *   片内 2MB dual-bank：Bank A=0x00000000（当前运行），Bank B=0x00100000（commit 目标）
 *   commit = 暂存→Bank B + EFM_SwapCmd（硬件引导交换，复位后从 B 启动）
 */
#ifndef OTA_STORAGE_H
#define OTA_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#define OTA_QSPI_STAGE_BASE   0x00E00000UL   /* QSPI 固件暂存区基址（4MB 偏移） */
#define OTA_QSPI_STAGE_SIZE   (1UL * 1024 * 1024)
#define OTA_BANK_B_BASE       0x00100000UL   /* 片内 dual-bank Bank B 基址 */
#define OTA_OTHER_APP_BASE    (OTA_BANK_B_BASE + 0x10000UL)  /* 另一 bank App 区（与 boot_cfg.h BOOT_OTHER_APP_BASE 一致） */
#define OTA_STAGE_BLOCK       1024           /* 块大小（QSPI<->片内搬运） */

/* 存储后端（供状态机/Agent 调用） */
int ota_storage_prepare(uint32_t total_size);            /* 擦 QSPI 暂存区 */
int ota_storage_write_stage(const uint8_t *data, uint32_t len, uint32_t offset);
int ota_storage_verify_stage(uint32_t size);             /* 读回 CRC 校验 */
int ota_storage_verify_payload(uint32_t payload_off, uint32_t payload_len, uint32_t expected_crc32); /* 载荷区 CRC32 比对（0=通过，-2=不匹配） */
int ota_storage_commit_from(uint32_t qspi_off, uint32_t size); /* QSPI(偏移)→Bank B + SwapCmd */
int ota_storage_rollback(void);                          /* 清 Swap（回 Bank A） */
int ota_switch_bank(void);                               /* A/B 手动切换：另一 bank App 有效时 swap+复位（0=OK） */

/* 通道互斥（v9.80：串口/UART 与 USB CDC 共用暂存区，禁止并发下载写冲突） */
int  ota_channel_try_acquire(void);   /* 0=获取成功；-1=另一通道进行中 */
void ota_channel_release(void);
int  ota_channel_busy(void);           /* v9.81b: 通道占用只读查询（send_func 跳过 USB1 用） */

#endif /* OTA_STORAGE_H */
