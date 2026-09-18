/**
 *******************************************************************************
 * @file  trng/trng_base/source/main.c
 * @brief Main program TRNG base for the Device Driver Library.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2023-09-30       CDT             Set XTAL as system clock source
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
#include "hc32_ll.h"
#include "hc32f46_driver.h"

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/


/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Main function of template project
 * @param  None
 * @retval int32_t return value, if needed
 */
 
uint32_t rng_get_random(void)
{
	 uint32_t trngbuf=0; 
   (void)TRNG_GenerateRandom((uint32_t*)&trngbuf, 1U);

	 return trngbuf;
}	
void trng_create(uint8_t *trngbuf, uint8_t u8Length)
{

    /* Unlock peripherals or registers */

    /* Lock peripherals or registers */

   // (void)TRNG_Generate((uint32_t*)trngbuf, u8Length / 4U, TIMEOUT_10MS);
     (void)TRNG_GenerateRandom((uint32_t*)trngbuf, u8Length / 4U);
    //memcpy(inbuf,(uint8_t*)&m_au32Random,8);

    return ;

}



/**
 * @brief  TRNG initialization configuration.
 * @param  None
 * @retval None
 */
void TrngInitConfig(void)
{
     FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_TRNG, ENABLE);
    /* TRNG initialization configuration. */
    TRNG_Init(TRNG_SHIFT_CNT64, TRNG_RELOAD_INIT_VAL_ENABLE);
    TRNG_Cmd(ENABLE);
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
