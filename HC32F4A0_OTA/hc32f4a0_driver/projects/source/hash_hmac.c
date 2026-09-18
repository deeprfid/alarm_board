/**
 *******************************************************************************
 * @file  hash/hash_hmac/source/main.c
 * @brief Main program HASH for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Set XTAL as system clock source
   2024-11-08       CDT             Delete interrupt mode
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
 * @addtogroup HASH_HMAC
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define HASH_MSG_DIGEST_SIZE        (32U)

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
static uint8_t m_au8MsgDigest[HASH_MSG_DIGEST_SIZE];

const static uint8_t m_au8ExpectMsgDigest[HASH_MSG_DIGEST_SIZE] = {
    0x07, 0xD5, 0xE6, 0xA2, 0xA0, 0x18, 0x45, 0xC0,
    0xEF, 0x34, 0x13, 0xE1, 0x52, 0xD4, 0x96, 0xFC,
    0x8B, 0x54, 0xAC, 0xFB, 0xB3, 0x82, 0x5B, 0x15,
    0x34, 0x78, 0x3D, 0xB2, 0xFC, 0x35, 0x52, 0x25,
};

const static char *m_s8SrcData =
"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\
abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\
abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\
01234567890123456789";

const static char *m_s8Key = "abcde";

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Main function of template project
 * @param  None
 * @retval int32_t return value, if needed
 */
int32_t HASH_HMAC_init(void)
{
    /* MCU Peripheral registers write unprotected. */
   
    HashConfig();
    /* MCU Peripheral registers write protected. */
  

    for (;;) {
        (void)HASH_HMAC_Calculate((uint8_t *)m_s8SrcData, strlen(m_s8SrcData), \
                                  (uint8_t *)m_s8Key, strlen(m_s8Key), \
                                  m_au8MsgDigest);
        if ((uint8_t)memcmp(m_au8MsgDigest, m_au8ExpectMsgDigest, HASH_MSG_DIGEST_SIZE) == 0U) {
            DDL_Printf("HASH HMAC calculation OK.\r\n");
        } else {
            DDL_Printf("HASH HMAC calculation FAIL.\r\n");
        }
        DDL_DelayMS(1000U);
    }
}


/**
 * @brief  HASH configuration.
 * @param  None
 * @retval None
 */
static void HashConfig(void)
{
    /* Enable HASH. */
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
