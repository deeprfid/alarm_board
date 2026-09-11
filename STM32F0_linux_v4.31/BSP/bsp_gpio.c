/*
*********************************************************************************************************
*
*	模块名称 : BSP模块(For STM32H7)
*	文件名称 : bsp.c
*	版    本 : V1.0
*	说    明 : 这是硬件底层驱动程序的主文件。每个c文件可以 #include "bsp.h" 来包含所有的外设驱动模块。
*			   bsp = Borad surport packet 板级支持包
*	修改记录 :
*		版本号  日期         作者       说明
*		V1.0    2018-07-29  Eric2013   正式发布
*
*	Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"

int PIO_GPIOInit(void)
{
		HAL_GPIO_WritePin(GPO1_GPIO_Port,GPO1_Pin,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPO2_GPIO_Port,GPO2_Pin,GPIO_PIN_RESET);
    return 0;
}

void EXTI4_15_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(Host_IRQ_Pin);
}

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == Host_IRQ_Pin)
  {

		BEEP_Start(20,10,2);
  }
}
/***************************** www.autobma.com (END OF FILE) *********************************/
