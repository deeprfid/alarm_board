/**
 * @file ota_boot.h
 * @brief Bootloader 引导/升级接口：标志读写、commit、回滚、跳 App
 */
#ifndef OTA_BOOT_H
#define OTA_BOOT_H

#include "boot_cfg.h"

/* boot_qspi.c：精简 QSPI 初始化/读（bootloader 只读暂存区） */
void boot_qspi_init(void);
int  boot_qspi_read(uint32_t addr, uint8_t *buf, uint32_t size);

/* 初始化（时钟/外设/QSPI/EFM） */
void boot_hw_init(void);

/* 引导决策：返回后进入 App 或复位 */
void boot_run(void);

/* 读当前 bank 标志；无有效标志返回 0（清零） */
int  boot_flag_read(boot_flag_t *f);
/* 写当前 bank 标志（擦扇区+写） */
int  boot_flag_write(const boot_flag_t *f);
/* 写另一 bank 标志 */
int  boot_flag_write_other(const boot_flag_t *f);

/* 跳转到当前 bank 的 App（0x10000） */
void boot_jump_app(void);

#endif /* OTA_BOOT_H */
