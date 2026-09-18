/**
 *******************************************************************************
 * @file  flash.h
 * @brief 片内 Flash 擦/写/读原语 —— 与量产扫描板 bootloader(boot_iap) 的 flash.c/h 对齐
 *
 * 与 boot_iap 的唯一差别: 内部调用【共享 DDL(Rev3.3.0)】的 EFM 接口
 * （EFM_REG_Unlock/EFM_REG_Lock, 且每个擦写操作前要 EFM_FWMC_Cmd(ENABLE)）。
 *
 * 谁在用: ota_flash.c(App 与 Boot 共用的 OTA 契约) 的底层原语。
 * 【必须 RAM 驻留】擦/写期间取指不能来自 Flash —— 两个工程的 scatter 都要把它放进 RW_RAMCODE。
 *******************************************************************************
 */
#ifndef __FLASH_H__
#define __FLASH_H__

#include <stdint.h>
#include "hc32_ll_efm.h"

/* Flash 定义（与 boot_iap 一致；扇区 8KB = EFM 的 SECTOR_SIZE） */
#define FLASH_BASE                  (EFM_START_ADDR)
#define FLASH_SIZE                  (EFM_END_ADDR + 1U)
#define FLASH_SECTOR_SIZE           (0x2000UL)
#define FLASH_SECTOR_NUM            (64U)

/* SRAM 定义 */
#define SRAM_SIZE                   (0x02F000UL)
/* 向量表步长 */
#define VECT_TAB_STEP               (0x400UL)

int32_t FLASH_CheckAddrAlign(uint32_t u32Addr);
int32_t FLASH_EraseSector(uint32_t u32Addr, uint32_t u32Size);
int32_t FLASH_WriteData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len);
int32_t FLASH_ReadData(uint32_t u32Addr, uint8_t *pu8Buff, uint32_t u32Len);

#endif /* __FLASH_H__ */
