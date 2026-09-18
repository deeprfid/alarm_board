/**
 *******************************************************************************
 * @file  efm/efm_otp/source/main.c
 * @brief Main program of EFM for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Modify the process
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
 * @addtogroup EFM_Otp
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define EFM_OTP_BLOCK_NUM       (22U)

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
 * @brief  Main function of EFM project
 * @param  None
 * @retval int32_t return value, if needed
 */
void EFM_Otp_init(void)
{
    en_int_status_t flag1;
    en_int_status_t flag2;
    uint32_t u32Data = 0x5A5A5A5AU;

  

    /* Wait flash0, flash1 ready. */
    do {
        flag1 = EFM_GetStatus(EFM_FLAG_RDY);
        flag2 = EFM_GetStatus(EFM_FLAG_RDY1);
    } while ((SET != flag1) || (SET != flag2));

    /* EFM_FWMC write enable */
    EFM_FWMC_Cmd(ENABLE);
    /* If otp enable, erase invalid except FLASH0. If locked, all otp area erase invalid. */
    (void)EFM_SectorErase(EFM_OTP_BLOCK22);
    /* Release OTP area write protect */
    EFM_OTP_WP_Unlock();
    /* Enable otp */
    EFM_OTP_Enable();
    /* Program to specified otp block */
    EFM_ProgramWord(EFM_OTP_BLOCK22, u32Data);
    if (u32Data == RW_MEM32(EFM_OTP_BLOCK22)) {
      //  BSP_LED_On(LED_BLUE);
    } else {
      //  BSP_LED_On(LED_YELLOW);
    }

    /* K1 */
    while (RESET == BSP_KEY_GetStatus(BSP_KEY_1)) {
        ;
    }

    /* Lock OTP block */
    (void)EFM_OTP_Lock(EFM_OTP_BLOCK_LOCKADDR(EFM_OTP_BLOCK_NUM));
    /* Program the lock otp block */
    EFM_ProgramWord(EFM_OTP_BLOCK22 + 4UL, u32Data);

    if (SET == EFM_GetStatus(EFM_FLAG_OTPWERR)) {
      //  BSP_LED_Off(LED_BLUE);
      //  BSP_LED_On(LED_RED);
        /* Clear Flag */
        EFM_ClearStatus(EFM_FLAG_OTPWERR);
    }

    EFM_FWMC_Cmd(DISABLE);
    EFM_OTP_WP_Lock();
    /* Register write protected for some required peripherals. */
  

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
