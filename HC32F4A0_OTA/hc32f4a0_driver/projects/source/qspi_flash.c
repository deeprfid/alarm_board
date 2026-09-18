/**
 *******************************************************************************
 * @file  qspi/qspi_base/source/qspi_flash.c
 * @brief This file provides firmware functions to the QSPI accesses flash.
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

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "qspi_flash.h"
#include "hc32f46_driver.h"
#include "string.h"
/**
 * @addtogroup HC32F4A0_DDL_Examples
 * @{
 */

/**
 * @addtogroup QSPI_Base
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define QSPI_FLASH_ENTER_XIP_MD         (0x20U)
#define QSPI_FLASH_EXIT_XIP_MD          (0xFFU)

SFLASH_T g_tSF;

/* v9.81cl: QSPI 忙锁（任务级互斥）。
 * 原 QSPI_FLASH_Write 整个写循环 __disable_irq()——64KB 写关中断 300-500ms，
 * RFID 模块 UART(921600) 在关中断期间丢数据 -> TagInventory err:3。
 * 改为：指令发送短临界区关中断 + tPP/擦除等待期间中断放开；忙锁防任务并发指令序列。 */
static volatile uint32_t s_qspi_busy = 0;

static void QSPI_FLASH_BusyWait(void)
{
    uint32_t guard = 0;
    while (s_qspi_busy != 0U) {
        for (volatile uint32_t i = 0; i < 2000U; i++) {}   /* 让出窗口，中断照常工作 */
        if (++guard > 1000000U) break;                     /* 异常兜底，防死等 */
    }
}

static void QSPI_FLASH_Lock(void)
{
    uint32_t primask;
    QSPI_FLASH_BusyWait();
    primask = __get_PRIMASK();
    __disable_irq();
    s_qspi_busy = 1;
    if (primask == 0U) __enable_irq();
}

static void QSPI_FLASH_Unlock(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_qspi_busy = 0;
    if (primask == 0U) __enable_irq();
}

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @defgroup QSPI_FLASH_Global_Functions QSPI_FLASH Global Functions
 * @{
 */
/**
 * @brief  Convert word to bytes.
 * @param  [in] u32Word                 The word value.
 * @param  [in] pu8Byte                 Pointer to the byte buffer.
 * @retval None
 */
static void QSPI_FLASH_WordToByte(uint32_t u32Word, uint8_t *pu8Byte)
{
    uint32_t u32ByteNum;
    uint8_t u8Count = 0U;

    u32ByteNum = QSPI_FLASH_ADDR_WIDTH;

    do
    {
        pu8Byte[u8Count++] = (uint8_t)(u32Word >> (u32ByteNum * 8U)) & 0xFFU;
    }
    while ((u32ByteNum--) != 0UL);
}

/**
 * @brief  QSPI write instruction.
 * @param  [in] u8Instr                 The instruction code.
 * @param  [in] pu8Addr                 Pointer to the address buffer.
 * @param  [in] u32AddrLen              Size of address buffer.
 * @param  [in] pu8WriteBuf             Pointer to the write buffer.
 * @param  [in] u32BufLen               Size of write buffer.
 * @retval None
 */
static void QSPI_FLASH_WriteInstr(uint8_t u8Instr, uint8_t *pu8Addr, uint32_t u32AddrLen,
                                  uint8_t *pu8WriteBuf, uint32_t u32BufLen)
{
    uint32_t u32Count;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(u8Instr);

    if ((NULL != pu8Addr) && (0UL != u32AddrLen))
    {
        for (u32Count = 0UL; u32Count < u32AddrLen; u32Count++)
        {
            QSPI_WriteDirectCommValue(pu8Addr[u32Count]);
        }
    }

    if ((NULL != pu8WriteBuf) && (0UL != u32BufLen))
    {
        for (u32Count = 0UL; u32Count < u32BufLen; u32Count++)
        {
            QSPI_WriteDirectCommValue(pu8WriteBuf[u32Count]);
        }
    }

    QSPI_ExitDirectCommMode();
}

/**
 * @brief  QSPI read instruction.
 * @param  [in] u8Instr                 The instruction code.
 * @param  [in] pu8Addr                 Pointer to the address buffer.
 * @param  [in] u32AddrLen              Size of address buffer.
 * @param  [out] pu8ReadBuf             Pointer to the read buffer.
 * @param  [in] u32BufLen               Size of read buffer.
 * @retval None
 */
static void QSPI_FLASH_ReadInstr(uint8_t u8Instr, uint8_t *pu8Addr, uint32_t u32AddrLen,
                                 uint8_t *pu8ReadBuf, uint32_t u32BufLen)
{
    uint32_t u32Count;

    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(u8Instr);

    if ((NULL != pu8Addr) && (0UL != u32AddrLen))
    {
        for (u32Count = 0UL; u32Count < u32AddrLen; u32Count++)
        {
            QSPI_WriteDirectCommValue(pu8Addr[u32Count]);
        }
    }

    if ((NULL != pu8ReadBuf) && (0UL != u32BufLen))
    {
        for (u32Count = 0UL; u32Count < u32BufLen; u32Count++)
        {
            pu8ReadBuf[u32Count] = QSPI_ReadDirectCommValue();
        }
    }

    QSPI_ExitDirectCommMode();
}

/**
 * @brief  QSPI check process done.
 * @param  u32Timeout                   The timeout times (ms).
 * @retval int32_t:
 *           - LL_OK: No errors occurred.
 *           - LL_ERR_TIMEOUT: Works timeout.
 */
static int32_t QSPI_FLASH_CheckProcessDone(uint32_t u32Timeout)
{
    uint8_t u8Status;
    uint32_t u32Count;
    int32_t i32Ret = LL_ERR_TIMEOUT;

    u32Count = u32Timeout * (HCLK_VALUE / 20000UL);
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(W25Q256_RD_STATUS_REG1);

    while ((u32Count--) != 0UL)
    {
        u8Status = QSPI_ReadDirectCommValue();

        if (0U == (u8Status & W25Q256_FLAG_BUSY))
        {
            i32Ret = LL_OK;
            break;
        }
    }

    QSPI_ExitDirectCommMode();

    return i32Ret;
}

/**
 * @brief  De-initializes QSPI.
 * @param  None
 * @retval None
 */
void QSPI_FLASH_DeInit(void)
{
    (void)QSPI_DeInit();
}

/**
 * @brief  Initialize the QSPI Flash.
 * @param  None
 * @retval int32_t:
 *           - LL_OK: Initialize success
 *           - LL_ERR_INVD_PARAM: Invalid parameter
 */
void QSPI_FLASH_Init(void)
{
    stc_qspi_init_t stcQspiInit;
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(QSPI_FLASH_CS_PORT,  QSPI_FLASH_CS_PIN,  &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_SCK_PORT, QSPI_FLASH_SCK_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO0_PORT, QSPI_FLASH_IO0_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO1_PORT, QSPI_FLASH_IO1_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO2_PORT, QSPI_FLASH_IO2_PIN, &stcGpioInit);
    (void)GPIO_Init(QSPI_FLASH_IO3_PORT, QSPI_FLASH_IO3_PIN, &stcGpioInit);
    GPIO_SetFunc(QSPI_FLASH_CS_PORT,  QSPI_FLASH_CS_PIN,  QSPI_FLASH_CS_FUNC);
    GPIO_SetFunc(QSPI_FLASH_SCK_PORT, QSPI_FLASH_SCK_PIN, QSPI_FLASH_SCK_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO0_PORT, QSPI_FLASH_IO0_PIN, QSPI_FLASH_IO0_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO1_PORT, QSPI_FLASH_IO1_PIN, QSPI_FLASH_IO1_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO2_PORT, QSPI_FLASH_IO2_PIN, QSPI_FLASH_IO2_FUNC);
    GPIO_SetFunc(QSPI_FLASH_IO3_PORT, QSPI_FLASH_IO3_PIN, QSPI_FLASH_IO3_FUNC);

    FCG_Fcg1PeriphClockCmd(QSPI_FLASH_CLK, ENABLE);
    (void)QSPI_StructInit(&stcQspiInit);
    stcQspiInit.u32ClockDiv       = QSPI_CLK_DIV3;   /* v9.81cl: 60MHz->80MHz（规格内上限） */
    stcQspiInit.u32ReadMode       = QSPI_FLASH_RD_MD;
    stcQspiInit.u32PrefetchMode   = QSPI_PREFETCH_MD_EDGE_STOP;
    stcQspiInit.u32DummyCycle     = QSPI_FLASH_RD_DUMMY_CYCLE;
    stcQspiInit.u32AddrWidth      = QSPI_FLASH_ADDR_WIDTH;
    stcQspiInit.u32SetupTime      = QSPI_QSSN_SETUP_ADVANCE_QSCK1P5;
    stcQspiInit.u32ReleaseTime    = QSPI_QSSN_RELEASE_DELAY_QSCK1P5;
    stcQspiInit.u32IntervalTime   = QSPI_QSSN_INTERVAL_QSCK2;
    (void)QSPI_Init(&stcQspiInit);
    QSPI_Enter4_byte_addrlength();
    sf_ReadInfo();

}

/**
 * @brief  Reads data from the QSPI memory.
 * @param  [in] u32Addr                 Read start address.
 * @param  [out] pu8ReadBuf             Pointer to the read buffer.
 * @param  [in] u32Size                 Size of the read buffer.
 * @retval int32_t:
 *           - LL_OK: Read succeeded
 *           - LL_ERR_INVD_PARAM: pu8ReadBuf == NULL or u32Size == 0U
 */
int32_t QSPI_FLASH_Read(uint32_t u32Addr, uint8_t *pu8ReadBuf, uint32_t u32Size)
{
    uint32_t u32Count = 0U;
    int32_t i32Ret = LL_OK;
    __IO uint8_t *pu8Read;

    u32Addr += QSPI_ROM_BASE;

    if ((NULL == pu8ReadBuf) || (0UL == u32Size) || ((u32Addr + u32Size) > QSPI_ROM_END))
    {
        i32Ret = LL_ERR_INVD_PARAM;
    }
    else
    {
        if (s_qspi_busy != 0U) {
            return LL_ERR_TIMEOUT;   /* v9.81cl: 写/擦进行中，XIP 读让上层重试 */
        }
        #if (QSPI_XIP_FUNC_ENABLE == DDL_ON)
        QSPI_XipModeCmd(QSPI_FLASH_ENTER_XIP_MD, ENABLE);
        #endif
        pu8Read = (__IO uint8_t *)u32Addr;

        while (u32Count < u32Size)
        {
            pu8ReadBuf[u32Count++] = *pu8Read++;
            #if (QSPI_XIP_FUNC_ENABLE == DDL_ON)

            if (u32Count == (u32Size - 1U))
            {
                QSPI_XipModeCmd(QSPI_FLASH_EXIT_XIP_MD, DISABLE);
            }

            #endif
        }
    }

    return i32Ret;
}

/**
 * @brief  Writes data to the QSPI memory.
 * @param  [in] u32Addr                 Write start address.
 * @param  [in] pu8WriteBuf             Pointer to the write buffer.
 * @param  [in] u32Size                 Size of the write buffer.
 * @retval int32_t:
 *           - LL_OK: Write succeeded
 *           - LL_ERR_INVD_PARAM: pu8WriteBuf == NULL or u32Size == 0U
 */

int32_t W25Q256_PageProgram(uint32_t u32Addr, uint8_t *pu8WriteBuf, uint32_t u32Size)
{
    int32_t i32Ret = LL_OK;
     uint8_t u8AddrBuf[4U];
    QSPI_FLASH_WriteInstr(W25Q256_WR_ENABLE, NULL, 0U, NULL, 0U);
    QSPI_FLASH_WordToByte(u32Addr, u8AddrBuf);
    QSPI_FLASH_WriteInstr(W25Q256_PAGE_PROGRAM, u8AddrBuf, (QSPI_FLASH_ADDR_WIDTH + 1U),
                          (uint8_t *)pu8WriteBuf, u32Size);
    i32Ret = QSPI_FLASH_CheckProcessDone(500U);

    return  i32Ret;
}

int32_t QSPI_FLASH_Write(uint32_t u32Addr, uint8_t *pu8WriteBuf, uint32_t u32Size)
{
    int32_t i32Ret = LL_OK;
    uint32_t remaining = u32Size;
    uint16_t offset = 0;
    uint16_t chunk_size;
    uint32_t addr = u32Addr;
    uint8_t u8AddrBuf[4U];

    QSPI_FLASH_Lock();

    while (remaining > 0) {
        /* v9.80 定案修复：chunk_size 必须基于循环变量 addr（当前写地址）而非参数 u32Addr。
         * 原代码用常量 u32Addr → 非 256 对齐起点(如 82)时每块都=256-(82%256)=174B，
         * 174B 块跨越 256B 页边界 → W25Q256 页编程地址回卷覆盖已写数据（CRC 校验必败）。
         * 512B 帧时代偏移 256 对齐（chunk=256 恒等）故未触发。 */
        chunk_size = 256 - (addr % 256);
        if (chunk_size > remaining) {
            chunk_size = remaining;
        }

        /* v9.81cl: 只对指令发送短关中断（微秒级）；tPP 等待期间中断放开，
         * UART/看门狗正常响应，不再长时间屏蔽中断丢 RFID 模块数据 */
        __disable_irq();
        QSPI_FLASH_WriteInstr(W25Q256_WR_ENABLE, NULL, 0U, NULL, 0U);
        QSPI_FLASH_WordToByte(addr, u8AddrBuf);
        QSPI_FLASH_WriteInstr(W25Q256_PAGE_PROGRAM, u8AddrBuf,
                              (QSPI_FLASH_ADDR_WIDTH + 1U), pu8WriteBuf + offset, chunk_size);
        __enable_irq();

        i32Ret = QSPI_FLASH_CheckProcessDone(500U);   /* 轮询 tPP，中断已放开 */
        if (i32Ret != LL_OK)
            break;

        addr += chunk_size;
        offset += chunk_size;
        remaining -= chunk_size;
    }

    QSPI_FLASH_Unlock();
    return i32Ret;
}

/**
 * @brief  Erase sector of the QSPI memory.
 * @param  [in] u32SectorAddr           The start address of the target sector.
 * @retval int32_t:
 *           - LL_OK: No errors occurred
 *           - LL_ERR_TIMEOUT: Erase sector timeout
 */
int32_t QSPI_FLASH_EraseSector(uint32_t u32SectorAddr)
{
    uint8_t u8AddrBuf[4U];
    int32_t i32Ret;
    QSPI_FLASH_Lock();
    __disable_irq();
    QSPI_FLASH_WriteInstr(W25Q256_WR_ENABLE, NULL, 0U, NULL, 0U);
    QSPI_FLASH_WordToByte(u32SectorAddr, u8AddrBuf);
    QSPI_FLASH_WriteInstr(W25Q256_SECTOR_ERASE, u8AddrBuf, (QSPI_FLASH_ADDR_WIDTH + 1U), NULL, 0U);
    __enable_irq();
    i32Ret = QSPI_FLASH_CheckProcessDone(500U);   /* 擦除等待期间中断放开 */
    QSPI_FLASH_Unlock();
    return i32Ret;
}

/**
 * @brief  Erase chip of the QSPI memory.
 * @param  None
 * @retval int32_t:
 *           - LL_OK: No errors occurred
 *           - LL_ERR_TIMEOUT: Erase sector timeout
 */
/* v9.81o: 64KB 块擦除（0xD8）——tBE 典型 120ms/64KB，等效 16 次 4KB 扇区擦（~600ms），提速约 5 倍 */
int32_t QSPI_FLASH_EraseBlock64K(uint32_t u32BlockAddr)
{
    uint8_t u8AddrBuf[4U];
    int32_t i32Ret;
    QSPI_FLASH_Lock();
    __disable_irq();
    QSPI_FLASH_WriteInstr(W25Q256_WR_ENABLE, NULL, 0U, NULL, 0U);
    QSPI_FLASH_WordToByte(u32BlockAddr, u8AddrBuf);
    QSPI_FLASH_WriteInstr(W25Q256_BLK_ERASE_64KB, u8AddrBuf, (QSPI_FLASH_ADDR_WIDTH + 1U), NULL, 0U);
    __enable_irq();
    i32Ret = QSPI_FLASH_CheckProcessDone(500U);   /* 擦除等待期间中断放开 */
    QSPI_FLASH_Unlock();
    return i32Ret;
}

int32_t QSPI_FLASH_EraseChip(void)
{
    QSPI_FLASH_WriteInstr(W25Q256_WR_ENABLE, NULL, 0U, NULL, 0U);
    QSPI_FLASH_WriteInstr(W25Q256_CHIP_ERASE, NULL, 0U, NULL, 0U);
    return QSPI_FLASH_CheckProcessDone(5000U);
}

int32_t QSPI_Enter4_byte_addrlength(void)
{
    QSPI_FLASH_WriteInstr(W25Q256_WR_ENABLE, NULL, 0U, NULL, 0U);
    QSPI_FLASH_WriteInstr(W25Q256_4byte_addr_len, NULL, 0U, NULL, 0U);
    return QSPI_FLASH_CheckProcessDone(5000U);
}

/**
 * @brief  Get the UID of the QSPI memory.
 * @param  [in] pu8UID                  The UID of the QSPI memory.
 * @retval None
 */
void QSPI_FLASH_GetUniqueID(uint8_t *pu8UID, uint8_t *pu8mfrID, uint8_t *pu8JEDECID)
{
    uint32_t i;
    uint8_t u8Dummy[4U];

    /* Fill the dummy values */
    for (i = 0UL; i < 4UL; i++)
    {
        u8Dummy[i] = 0xFFU;
    }

    QSPI_FLASH_ReadInstr(W25Q256_RD_UNIQUE_ID, u8Dummy, 4U, pu8UID, W25Q256_UNIQUE_ID_SIZE);
    QSPI_FLASH_ReadInstr(W25Q256_JEDEC_ID, NULL, 0U, pu8JEDECID, 3);
    QSPI_FLASH_ReadInstr(W25Q256_MANUFACTURER_DEVICE_ID, u8Dummy, 4U, pu8mfrID, 4);
}

void sf_ReadInfo(void)
{
    /* 自动识别串行Flash型号 */
    uint8_t  u8UID[W25Q256_UNIQUE_ID_SIZE] = {0U};
    uint8_t  u8mfrID[8] = {0};
    uint8_t  u8jedecID[4] = {0};

    QSPI_FLASH_GetUniqueID(u8UID, u8mfrID, u8jedecID);
    TRACE("UID     value: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\r\n",
          u8UID[0], u8UID[1], u8UID[2], u8UID[3], u8UID[4], u8UID[5], u8UID[6], u8UID[7]);
    TRACE("MFRID   value: %02x-%02x-%02x-%02x\r\n", u8mfrID[0], u8mfrID[1], u8mfrID[2], u8mfrID[3]);
    TRACE("JEDECID value: %02x-%02x-%02x\r\n", u8jedecID[0], u8jedecID[1], u8jedecID[2]);
		
    g_tSF.ChipID = (u8jedecID[0] << 16) | (u8jedecID[1] << 8) | (u8jedecID[2]);	/* 芯片ID */
    TRACE("JEDECID=%04x\r\n",g_tSF.ChipID);
    switch (g_tSF.ChipID)
    {
        case SST25VF016B_ID:
            strcpy(g_tSF.ChipName, "SST25VF016B");
            g_tSF.TotalSize  = 2 * 1024 * 1024;	/* 总容量 = 2M */
            g_tSF.SectorSize = 4 * 1024;		/* 扇区大小 = 4K */
            break;

        case MX25L1606E_ID:
            strcpy(g_tSF.ChipName, "MX25L1606E");
            g_tSF.TotalSize  = 2 * 1024 * 1024;	/* 总容量 = 2M */
            g_tSF.SectorSize = 4 * 1024;		/* 扇区大小 = 4K */
            break;

        case W25Q64BV_ID:
            strcpy(g_tSF.ChipName, "W25Q64");
            g_tSF.TotalSize  = 8 * 1024 * 1024;	/* 总容量 = 8M */
            g_tSF.SectorSize = 4 * 1024;		/* 扇区大小 = 4K */
            break;

        case W25Q128_ID:
            strcpy(g_tSF.ChipName, "W25Q128");
            g_tSF.TotalSize  = 16 * 1024 * 1024;	/* 总容量 = 16M */
            g_tSF.SectorSize = 4 * 1024;		/* 扇区大小 = 4K */
            break;

        case W25Q256_ID:
            strcpy(g_tSF.ChipName, "W25Q256JVEQ");
            g_tSF.TotalSize  = 32 * 1024 * 1024;	/* 总容量 = 32M */
            g_tSF.SectorSize = 4 * 1024;		/* 扇区大小 = 4K */
            break;

        default:
            strcpy(g_tSF.ChipName, "Unknow Flash");
            g_tSF.TotalSize  = 2 * 1024 * 1024;
            g_tSF.SectorSize = 4 * 1024;
            break;
    }

}




/******************************************************************************
 * EOF (not truncated)
 *****************************************************************************/
