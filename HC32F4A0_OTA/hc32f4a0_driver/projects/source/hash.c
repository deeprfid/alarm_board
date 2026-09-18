/**
 *******************************************************************************
 * @file  hash/hash_base/source/main.c
 * @brief Main program HASH base for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Set XTAL as system clock source
                                    Modify printf baudrate
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
#include <string.h>

#include "bsp.h"

/**
 * @addtogroup HC32F4A0_DDL_Examples
 * @{
 */

/**
 * @addtogroup HASH_Base
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define HASH_MSG_DIGEST_SIZE        (32U)
#define HASH_TIMEOUT_VAL            (10U)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/


static void HashConfig(void);

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
const static uint8_t m_au8MsgDigest[HASH_MSG_DIGEST_SIZE] = {
    0x36, 0xBB, 0xE5, 0x0E, 0xD9, 0x68, 0x41, 0xD1,
    0x04, 0x43, 0xBC, 0xB6, 0x70, 0xD6, 0x55, 0x4F,
    0x0A, 0x34, 0xB7, 0x61, 0xBE, 0x67, 0xEC, 0x9C,
    0x4A, 0x8A, 0xD2, 0xC0, 0xC4, 0x4C, 0xA4, 0x2C,
};
const static char *m_s8SrcData = "abcde";

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

/**
 * @brief  Main function of HASH base project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t HASH_Base_init(void)
{
    uint32_t i;
    uint8_t au8MsgDigest[HASH_MSG_DIGEST_SIZE];


    /* HASH configuration. */
    HashConfig();
    /* MCU Peripheral registers write protected. */
  

    /***************** Configuration end, application start **************/

    for (;;) {
        HASH_Calculate((uint8_t *)m_s8SrcData, strlen(m_s8SrcData), au8MsgDigest);

        for (i = 0UL; i < HASH_MSG_DIGEST_SIZE; i++) {
            if (au8MsgDigest[i] != m_au8MsgDigest[i]) {
                DDL_Printf("HASH basic calculation FAIL.\r\n");
                for (;;) {
                    /* rsvd */
                }
            }
        }
        DDL_Printf("HASH basic calculation OK.\r\n");

        DDL_DelayMS(500U);
    }
}



/**
 * @brief  HASH configuration.
 * @param  None
 * @retval None
 */
static void HashConfig(void)
{
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_HASH, ENABLE);
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
