/**
 *******************************************************************************
 * @file  crc/crc_hw_encode_hw_check/source/main.c
 * @brief This example demonstrates CRC base function.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Modify for reconstructed API
   2024-11-08       CDT             Modify code due to CRC interface change
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
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
#include "bsp.h"

/**
 * @addtogroup HC32F4A0_DDL_Examples
 * @{
 */

/**
 * @addtogroup CRC_HW_Encode_HW_Check
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define CRC16_INIT_VALUE                (0xFFFFUL)
#define CRC32_INIT_VALUE                (0xFFFFFFFFUL)

/* Peripheral register WE/WP selection */
#define LL_PERIPH_SEL                   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU)

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
 * @brief  Main function of CRC base project
 * @param  None
 * @retval int32_t return value, if needed
 */
void CRC_HW_Encode_HW_Init(void)
{
    uint16_t u16CrcValue;
    uint32_t u32CrcValue;
    uint32_t u32CrcErrorCount = 0UL;
    en_flag_status_t enCrcMatch;
    stc_crc_init_t stcCrcInit;
    const uint8_t  au8SrcData[5U]  = {0x12U, 0x34, 0x56, 0x78, 0x90};
    const uint16_t au16SrcData[5U] = {0x1234U, 0x3456U, 0x7890U, 0x90AB, 0xABCD};
    const uint32_t au32SrcData[5U] = {0x12345678UL, 0x34567890UL, 0x567890ABUL, 0x7890ABCDUL, 0x90ABCDEFUL};

    /* MCU Peripheral registers write unprotected */


    /* Enable CRC module clock. */
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_CRC, ENABLE);

    /* MCU Peripheral registers write protected */


    /******** Initialize CRC16. ***********************************************/
    stcCrcInit.u32Protocol = CRC_CRC16;
    stcCrcInit.u32InitValue = CRC16_INIT_VALUE;
    (void)CRC_Init(&stcCrcInit);

    /******** Calculates CRC16 checksum by writing data in byte. **************/
    (void)CRC_CRC16_AccumulateData(CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData), &u16CrcValue);
    enCrcMatch = CRC_CRC16_CheckData(CRC16_INIT_VALUE, CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData), u16CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC16 checksum by writing data in half-word. *********/
    (void)CRC_CRC16_Calculate(CRC16_INIT_VALUE, CRC_DATA_WIDTH_16BIT, au16SrcData, ARRAY_SZ(au16SrcData), &u16CrcValue);
    enCrcMatch = CRC_CRC16_CheckData(CRC16_INIT_VALUE, CRC_DATA_WIDTH_16BIT, au16SrcData, ARRAY_SZ(au16SrcData), u16CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC16 checksum by writing data in word. **************/
    (void)CRC_CRC16_Calculate(CRC16_INIT_VALUE, CRC_DATA_WIDTH_32BIT, au32SrcData, ARRAY_SZ(au32SrcData), &u16CrcValue);
    enCrcMatch = CRC_CRC16_CheckData(CRC16_INIT_VALUE, CRC_DATA_WIDTH_32BIT, au32SrcData, ARRAY_SZ(au32SrcData), u16CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /******** Initialize CRC32. ***********************************************/
    stcCrcInit.u32Protocol = CRC_CRC32;
    stcCrcInit.u32InitValue = CRC32_INIT_VALUE;
    (void)CRC_Init(&stcCrcInit);

    /******** Calculates CRC32 checksum by writing data in byte. **************/
    (void)CRC_CRC32_AccumulateData(CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData), &u32CrcValue);
    enCrcMatch = CRC_CRC32_CheckData(CRC32_INIT_VALUE, CRC_DATA_WIDTH_8BIT, au8SrcData, ARRAY_SZ(au8SrcData), u32CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC32 checksum by writing data in half-word. *********/
    (void)CRC_CRC32_Calculate(CRC32_INIT_VALUE, CRC_DATA_WIDTH_16BIT, au16SrcData, ARRAY_SZ(au16SrcData), &u32CrcValue);
    enCrcMatch = CRC_CRC32_CheckData(CRC32_INIT_VALUE, CRC_DATA_WIDTH_16BIT, au16SrcData, ARRAY_SZ(au16SrcData), u32CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /******** Calculates CRC32 checksum by writing data in word. **************/
    (void)CRC_CRC32_Calculate(CRC32_INIT_VALUE, CRC_DATA_WIDTH_32BIT, au32SrcData, ARRAY_SZ(au32SrcData), &u32CrcValue);
    enCrcMatch = CRC_CRC32_CheckData(CRC32_INIT_VALUE, CRC_DATA_WIDTH_32BIT, au32SrcData, ARRAY_SZ(au32SrcData), u32CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /* Check test result */
    if (0UL == u32CrcErrorCount) {
      //  BSP_LED_On(LED_BLUE);   /* Test result meets the expected. */
    } else {
      //  BSP_LED_On(LED_RED);    /* Test result doesn't meet the expected. */
    }

}

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
