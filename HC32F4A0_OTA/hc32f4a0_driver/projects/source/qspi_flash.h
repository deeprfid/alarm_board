/**
 *******************************************************************************
 * @file  qspi/qspi_base/source/qspi_flash.h
 * @brief This file contains all the functions prototypes that the QSPI accesses
 *        Flash.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2023, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#ifndef __QSPI_FLASH_H__
#define __QSPI_FLASH_H__

/* C binding of definitions if building with C++ compiler */
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "hc32_ll_qspi.h"
#include "hc32_ll_fcg.h"
#include "hc32_ll_gpio.h"

/**
 * @addtogroup HC32F4A0_DDL_Examples
 * @{
 */

/**
 * @addtogroup QSPI_Base
 * @{
 */

/*******************************************************************************
 * Global type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Global pre-processor symbols/macros ('#define')
 ******************************************************************************/
/**
 * @defgroup QSPI_FLASH_Global_Macros QSPI_FLASH Global Macros
 * @{
 */
/* QSPI read mode and W25Q64 dummy cycles */
/*          QSPI_Read_Mode              W25Q256_Dummy_Cycles
 *  QSPI_RD_MD_STD_RD               |       (Ignore)
 *  QSPI_RD_MD_FAST_RD              |   QSPI_DUMMY_CYCLE8
 *  QSPI_RD_MD_DUAL_OUTPUT_FAST_RD  |   QSPI_DUMMY_CYCLE8
 *  QSPI_RD_MD_DUAL_IO_FAST_RD      |   QSPI_DUMMY_CYCLE4
 *  QSPI_RD_MD_QUAD_OUTPUT_FAST_RD  |   QSPI_DUMMY_CYCLE8
 *  QSPI_RD_MD_QUAD_IO_FAST_RD      |   QSPI_DUMMY_CYCLE6
 */

/* QSPI configuration define */
#define QSPI_FLASH_CLK                  (FCG1_PERIPH_QSPI)
#define QSPI_FLASH_RD_MD                (QSPI_RD_MD_QUAD_IO_FAST_RD)
#define QSPI_FLASH_RD_DUMMY_CYCLE       (QSPI_DUMMY_CYCLE6)
#define QSPI_FLASH_ADDR_WIDTH           (QSPI_ADDR_WIDTH_32BIT_INSTR_24BIT)
/* QSSN */
#define QSPI_FLASH_CS_PORT              (GPIO_PORT_C)
#define QSPI_FLASH_CS_PIN               (GPIO_PIN_07)
#define QSPI_FLASH_CS_FUNC              (GPIO_FUNC_18)
/* QSCK */
#define QSPI_FLASH_SCK_PORT             (GPIO_PORT_C)
#define QSPI_FLASH_SCK_PIN              (GPIO_PIN_06)
#define QSPI_FLASH_SCK_FUNC             (GPIO_FUNC_18)
/* QSIO0 */
#define QSPI_FLASH_IO0_PORT             (GPIO_PORT_D)
#define QSPI_FLASH_IO0_PIN              (GPIO_PIN_08)
#define QSPI_FLASH_IO0_FUNC             (GPIO_FUNC_18)
/* QSIO1 */
#define QSPI_FLASH_IO1_PORT             (GPIO_PORT_D)
#define QSPI_FLASH_IO1_PIN              (GPIO_PIN_09)
#define QSPI_FLASH_IO1_FUNC             (GPIO_FUNC_18)
/* QSIO2 */
#define QSPI_FLASH_IO2_PORT             (GPIO_PORT_D)
#define QSPI_FLASH_IO2_PIN              (GPIO_PIN_10)
#define QSPI_FLASH_IO2_FUNC             (GPIO_FUNC_18)
/* QSIO3 */
#define QSPI_FLASH_IO3_PORT             (GPIO_PORT_D)
#define QSPI_FLASH_IO3_PIN              (GPIO_PIN_11)
#define QSPI_FLASH_IO3_FUNC             (GPIO_FUNC_18)

/**
 * @defgroup W25Q256_Standard_SPI_Instructions W25Q64 Standard SPI Instructions
 * @{
 */
#define W25Q256_WR_ENABLE                (0x06U)
#define W25Q256_SR_WR_ENABLE             (0x50U)
#define W25Q256_WR_DISABLE               (0x04U)
#define W25Q256_RELEASE_POWER_DOWN_ID    (0xABU)
#define W25Q256_MANUFACTURER_DEVICE_ID   (0x90U)
#define W25Q256_JEDEC_ID                 (0x9FU)
#define W25Q256_RD_UNIQUE_ID             (0x4BU)
#define W25Q256_RD_DATA                  (0x03U)
#define W25Q256_FAST_RD                  (0x0BU)
#define W25Q256_PAGE_PROGRAM             (0x02U)
#define W25Q256_SECTOR_ERASE             (0x20U)
#define W25Q256_BLK_ERASE_32KB           (0x52U)
#define W25Q256_BLK_ERASE_64KB           (0xD8U)
#define W25Q256_CHIP_ERASE               (0xC7U)
#define W25Q256_RD_STATUS_REG1           (0x05U)
#define W25Q256_WR_STATUS_REG1           (0x01U)
#define W25Q256_RD_STATUS_REG2           (0x35U)
#define W25Q256_WR_STATUS_REG2           (0x31U)
#define W25Q256_RD_STATUS_REG3           (0x15U)
#define W25Q256_WR_STATUS_REG3           (0x11U)
#define W25Q256_RD_SFDP_REG              (0x5AU)
#define W25Q256_ERASE_SECURITY_REG       (0x44U)
#define W25Q256_PROGRAM_SECURITY_REG     (0x42U)
#define W25Q256_RD_SECURITY_REG          (0x48U)
#define W25Q256_GLOBAL_BLK_LOCK          (0x7EU)
#define W25Q256_GLOBAL_BLK_UNLOCK        (0x98U)
#define W25Q256_RD_BLK_LOCK              (0x3DU)
#define W25Q256_PERSON_BLK_LOCK          (0x36U)
#define W25Q256_PERSON_BLK_UNLOCK        (0x39U)
#define W25Q256_ERASE_PROGRAM_SUSPEND    (0x75U)
#define W25Q256_ERASE_PROGRAM_RESUME     (0x7AU)
#define W25Q256_POWER_DOWN               (0xB9U)
#define W25Q256_ENABLE_RST               (0x66U)
#define W25Q256_RST_DEVICE               (0x99U)
/**
 * @}
 */

/**
 * @defgroup W25Q256_Dual_Quad_SPI_Instruction W25Q64 Dual Quad SPI Instruction
 * @{
 */
#define W25Q256_FAST_RD_DUAL_OUTPUT      (0x3BU)
#define W25Q256_FAST_RD_DUAL_IO          (0xBBU)
#define W25Q256_MFTR_DEVICE_ID_DUAL_IO   (0x92U)
#define W25Q256_QUAD_INPUT_PAGE_PROGRAM  (0x32U)
#define W25Q256_FAST_RD_QUAD_OUTPUT      (0x6BU)
#define W25Q256_MFTR_DEVICE_ID_QUAD_IO   (0x94U)
#define W25Q256_FAST_RD_QUAD_IO          (0xEBU)
#define W25Q256_SET_BURST_WITH_WRAP      (0x77U)
#define W25Q256_4byte_addr_len           (0xB7U)
/**
 * @}
 */

/**
 * @defgroup W25Q256_Size W25Q64 Size
 * @{
 */
#define W25Q256_PAGE_SIZE                (256UL)
#define W25Q256_SECTOR_SIZE              (1024UL * 4UL)
#define W25Q256_BLK_SIZE                 (1024UL * 64UL)
#define W25Q256_PAGE_PER_SECTOR          (W25Q256_SECTOR_SIZE / W25Q256_PAGE_SIZE)
#define W25Q256_MAX_ADDR                 (0x2000000UL)
/**
 * @}
 */

/**
 * @defgroup W25Q256_Status_Flag W25Q64 Status Flag
 * @{
 */
#define W25Q256_FLAG_BUSY                (0x01U)
#define W25Q256_FLAG_SUSPEND             (0x80U)
/**
 * @}
 */

/**
 * @defgroup W25Q256_Miscellaneous_Macros W25Q64 Miscellaneous Macros
 * @{
 */
#define W25Q256_UNIQUE_ID_SIZE           (8U)
#define W25Q256_DUMMY_BYTE_VALUE         (0xFFU)
/**
 * @}
 */

enum
{
	SST25VF016B_ID = 0xBF2541,
	MX25L1606E_ID  = 0xC22015,
	W25Q64BV_ID    = 0xEF4017, /* 8M  byte BV, JV, FV */
	W25Q128_ID     = 0xEF4018, /* 16M byte BV, JV, FV */
	W25Q256_ID     = 0xEF4019  /* 32M byte BV, JV, FV */
};

typedef struct
{
	uint32_t ChipID;		/* 芯片ID */
	char     ChipName[16];		/* 芯片型号字符串，主要用于显示 */
	uint32_t TotalSize;		/* 总容量 */
	uint16_t SectorSize;		/* 扇区大小 */
}SFLASH_T;
 
 /* W25Q256JV */
#define WRITE_ENABLE_CMD      0x06            /* ????? */  
#define READ_ID_CMD2          0x9F            /* ??ID?? */  
#define READ_STATUS_REG_CMD   0x05            /* ?????? */ 
#define SUBSECTOR_ERASE_4_BYTE_ADDR_CMD      0x21    /* 32bit????????, 4KB */
#define QUAD_IN_FAST_PROG_4_BYTE_ADDR_CMD    0x34    /* 32bit???4??????? */
#define QUAD_INOUT_FAST_READ_4_BYTE_ADDR_CMD 0xEC    /* 32bit???4??????? */ 

/*******************************************************************************
 * Global variable definitions ('extern')
 ******************************************************************************/

/*******************************************************************************
  Global function prototypes (definition in C source)
 ******************************************************************************/
/**
 * @addtogroup QSPI_FLASH_Global_Functions
 * @{
 */
void QSPI_FLASH_DeInit(void);
void QSPI_FLASH_Init(void);
int32_t QSPI_FLASH_Write(uint32_t u32Addr, uint8_t *pu8WriteBuf, uint32_t u32Size);
int32_t QSPI_FLASH_Read(uint32_t u32Addr, uint8_t *pu8ReadBuf, uint32_t u32Size);
int32_t QSPI_FLASH_EraseSector(uint32_t u32SectorAddr);
int32_t QSPI_FLASH_EraseBlock64K(uint32_t u32BlockAddr);   /* v9.81o: 64KB 块擦除 0xD8 */
int32_t QSPI_FLASH_EraseChip(void);
void QSPI_FLASH_GetUniqueID(uint8_t *pu8UID,uint8_t *pu8mfrID,uint8_t *pu8JEDECID);
int32_t QSPI_Enter4_byte_addrlength(void);
void sf_ReadInfo(void);
extern SFLASH_T g_tSF;
/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __QSPI_FLASH_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
