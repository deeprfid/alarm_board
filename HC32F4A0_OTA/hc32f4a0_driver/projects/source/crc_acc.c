/**
 *******************************************************************************
 * @file  crc/crc_hw_accumulate_check/source/main.c
 * @brief This example demonstrates CRC base function.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2023-09-30       CDT             First version
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
 * @addtogroup CRC_HW_Accumulate_Check
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
int32_t CRC_HW_Accumulate_init(void)
{
    uint32_t u32CrcErrorCount = 0UL;
    uint16_t u16CrcValue;
    uint32_t u32CrcValue;
    uint32_t i = 0U;
    en_flag_status_t enCrcMatch;
    stc_crc_init_t stcCrcInit;
    const uint8_t  au8SrcData[20U] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, \
                                      0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14
                                     };

    /* MCU Peripheral registers write unprotected */



    /* Enable CRC module clock. */
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_CRC, ENABLE);

    /* MCU Peripheral registers write protected */


    /******** Initialize CRC16. ***********************************************/
    stcCrcInit.u32Protocol = CRC_CRC16;
    stcCrcInit.u32InitValue = CRC16_INIT_VALUE;
    (void)CRC_Init(&stcCrcInit);

    /* Write data by frame */
    for (i = 0U; i < 20U; i += 5U) {
        (void)CRC_CRC16_AccumulateData(CRC_DATA_WIDTH_8BIT, &au8SrcData[i], 5U, NULL);
    }

    /* Get the CRC result */
    u16CrcValue = CRC_GetResult();

    /* Get The Accumulate Check Result */
    enCrcMatch = CRC_CRC16_GetCheckResult(u16CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /******** Initialize CRC32. ***********************************************/
    stcCrcInit.u32Protocol = CRC_CRC32;
    stcCrcInit.u32InitValue = CRC32_INIT_VALUE;
    (void)CRC_Init(&stcCrcInit);

    /* Write data by frame */
    for (i = 0U; i < 20U; i += 5U) {
        (void)CRC_CRC32_AccumulateData(CRC_DATA_WIDTH_8BIT, &au8SrcData[i], 5U, NULL);
    }

    /* Get the CRC result */
    u32CrcValue = CRC_GetResult();

    /* Get The Accumulate Check Result */
    enCrcMatch = CRC_CRC32_GetCheckResult(u32CrcValue);
    if (enCrcMatch != SET) {
        u32CrcErrorCount++;
    }

    /* Check test result */
    if (0UL == u32CrcErrorCount) {
       // BSP_LED_On(LED_BLUE);   /* Test result meets the expected. */
    } else {
       // BSP_LED_On(LED_RED);    /* Test result doesn't meet the expected. */
    }

    for (;;) {
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
