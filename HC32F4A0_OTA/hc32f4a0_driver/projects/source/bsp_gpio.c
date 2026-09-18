/**
 *******************************************************************************
 * @file  gpio/gpio_output/source/main.c
 * @brief Main program of GPIO for the Device Driver Library.
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
#include "bsp.h"

/**
 * @addtogroup HC32F4A0_DDL_Examples
 * @{
 */

/**
 * @addtogroup GPIO_OUTPUT
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* LED_G Port/Pin definition */



#define DLY_MS              (500UL)

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
 * @brief  LED Init
 * @param  None
 * @retval None
 */
static void GPO_IO_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
	  GPIO_SetDebugPort(GPIO_PIN_TDI,DISABLE);
	
   (void)GPIO_Init(BOARD_BEEP_PORT, BOARD_BEEP_PIN, &stcGpioInit);
	// (void)GPIO_Init(BOARD_BEEP_PORT, BOARD_BUZZ_PIN, &stcGpioInit);
	
	 (void)GPIO_Init(LED_PORT, LED_1_PIN, &stcGpioInit);
	 (void)GPIO_Init(LED_PORT, LED_2_PIN, &stcGpioInit);
	 (void)GPIO_Init(LED_PORT, LED_3_PIN, &stcGpioInit);
	 (void)GPIO_Init(LED_PORT, LED_4_PIN, &stcGpioInit);
	
	 (void)GPIO_Init(LED_RGB_PORT, LED_R_PIN, &stcGpioInit);
	 (void)GPIO_Init(LED_RGB_PORT, LED_G_PIN, &stcGpioInit);
	 (void)GPIO_Init(LED_RGB_PORT, LED_B_PIN, &stcGpioInit);
	
	 (void)GPIO_Init(BOARD_GPO_PORT, BOARD_RELAY_PIN, &stcGpioInit);
	 (void)GPIO_Init(BOARD_GPO_PORT, BOARD_DC12V_PIN, &stcGpioInit);
	
	 (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_08, &stcGpioInit);  //RFID POWER SWITCH
	 
	 
	 (void)GPIO_Init(GPIO_PORT_D, GPIO_PIN_00, &stcGpioInit);  // BM8563_RTC
	 (void)GPIO_Init(GPIO_PORT_D, GPIO_PIN_01, &stcGpioInit);  // BM8563_RTC

}

static void GPI_IO_Init(void)
{
	  stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir = PIN_DIR_IN;
	
	
	  (void)GPIO_Init(BOARD_GPI_PORT, BOARD_GPI1_PIN, &stcGpioInit);
	  (void)GPIO_Init(BOARD_GPI_PORT, BOARD_GPI2_PIN, &stcGpioInit);
	  (void)GPIO_Init(BOARD_GPI_PORT, BOARD_GPI3_PIN, &stcGpioInit);
	
	  (void)GPIO_Init(BOARD_RADAR1_PORT, BOARD_RADAR1_PIN, &stcGpioInit);
	  (void)GPIO_Init(BOARD_RADAR2_PORT, BOARD_RADAR2_PIN, &stcGpioInit);
	
	  (void)GPIO_Init(BOARD_KEY_PORT, BOARD_KEY1_PIN, &stcGpioInit);
	  (void)GPIO_Init(BOARD_KEY_PORT, BOARD_KEY2_PIN, &stcGpioInit);
	
	  (void)GPIO_Init(IPRESET_PORT, IPRESET_PIN, &stcGpioInit);
	
	  (void)GPIO_Init(GPIO_PORT_E, GPIO_PIN_00, &stcGpioInit);
  	(void)GPIO_Init(GPIO_PORT_E, GPIO_PIN_01, &stcGpioInit);
	  (void)GPIO_Init(GPIO_PORT_E, GPIO_PIN_06, &stcGpioInit);
	  (void)GPIO_Init(GPIO_PORT_C, GPIO_PIN_13, &stcGpioInit);
	  (void)GPIO_Init(GPIO_PORT_C, GPIO_PIN_02, &stcGpioInit);
	  
	
	
}

void Board_LED_toggle(void)
{
  LED_1_TOGGLE();
	LED_2_TOGGLE();
	LED_3_TOGGLE();
	LED_4_TOGGLE();



}	
/**
 * @brief  Main function of GPIO project
 * @param  None
 * @retval int32_t return value, if needed
 */
void Board_GPIO_Init(void)
{

	  GPO_IO_Init();
	  GPI_IO_Init();

	 // Timer2_init();

	
}


void Board_GPIO_Scan(void)
{
 

			  if(PIN_RESET==GPIO_ReadInputPins(BOARD_KEY_PORT,BOARD_KEY1_PIN))
				{	GPIO_SetPins  (LED_RGB_PORT,LED_R_PIN);
				 
				}
				else
				{
				  GPIO_ResetPins  (LED_RGB_PORT,LED_R_PIN);
				
				}	
				 if(PIN_RESET==GPIO_ReadInputPins(BOARD_KEY_PORT,BOARD_KEY2_PIN))
				{	
				  GPIO_SetPins(LED_RGB_PORT,LED_G_PIN);
					
				}
				else
				{
				   GPIO_ResetPins(LED_RGB_PORT,LED_G_PIN);
				
				}
				//en_pin_state_t p_radar1,p_radar1
				if(PIN_SET==GPIO_ReadInputPins(BOARD_RADAR1_PORT, BOARD_RADAR1_PIN) || \
				   PIN_SET==GPIO_ReadInputPins(BOARD_RADAR2_PORT, BOARD_RADAR2_PIN)	
				)
				{//	GPIO_ResetPins(LED_RGB_PORT,LED_R_PIN);
				 // GPIO_ResetPins(LED_RGB_PORT,LED_G_PIN);
					GPIO_SetPins(LED_RGB_PORT,LED_B_PIN);
				}
				else
				{
				
				   GPIO_ResetPins(LED_RGB_PORT,LED_B_PIN);
				} 	
       // DDL_DelayMS(DLY_MS);
        /* De-init port if necessary */
        // GPIO_DeInit();
   
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
