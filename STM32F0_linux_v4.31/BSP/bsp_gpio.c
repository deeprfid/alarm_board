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

void beep_on(void)    
{
   HAL_GPIO_WritePin(GPO_BZ3V3_GPIO_Port, GPO_BZ3V3_Pin, GPIO_PIN_SET);
}
void beep_off(void)
{
   HAL_GPIO_WritePin(GPO_BZ3V3_GPIO_Port, GPO_BZ3V3_Pin, GPIO_PIN_RESET);
}
void gpo_set(uint8_t gpoid, uint8_t state)
{
    if(gpoid == 1)
    {
        if(state == 1)	          
			 HAL_GPIO_WritePin(GPO1_GPIO_Port,GPO1_Pin,GPIO_PIN_SET);
        else      				      
			 HAL_GPIO_WritePin(GPO1_GPIO_Port,GPO1_Pin,GPIO_PIN_RESET);
    }
    if(gpoid == 2)
    {
        if (state == 1)	  
			  HAL_GPIO_WritePin(GPO2_GPIO_Port,GPO2_Pin,GPIO_PIN_SET);
        else      				      
			  HAL_GPIO_WritePin(GPO2_GPIO_Port,GPO2_Pin,GPIO_PIN_RESET);
    }
   
	 else if(gpoid == 5)
	 {
		 if (state == 0)
			beep_off();
		 else
			beep_on();
	 }
}




void PIO_GpioRead(uint8_t *vals) //读输入IO口状态 ，IN1的值存放在vals的bit0位，IN2的值存放在vals的bit1位,IN3的值存放在vals的bit2位,IN4的值存放在vals的bit3位,
{
    *vals=( HAL_GPIO_ReadPin(GPI_IN4_GPIO_Port,GPI_IN4_Pin)| (HAL_GPIO_ReadPin(GPI_IN3_GPIO_Port,GPI_IN3_Pin)<<1) | (HAL_GPIO_ReadPin(GPI_IN2_GPIO_Port,GPI_IN2_Pin)<<2) | (HAL_GPIO_ReadPin(GPI_IN1_GPIO_Port,GPI_IN1_Pin)<<3));
}

void PIO_GpioSet(uint8_t mask,uint8_t vals)//设置输出IO口状态，mask的bit0位表示输出IO1,bit1表示输出IO2，
{   //当bit0或bit1值为1时才表示要设置对应的IO口，设置的值为对应的vals的bit0与bit1的值

    if((mask&0x01)==1)
    {
        if((vals&0x01)==1)	         HAL_GPIO_WritePin(GPO1_GPIO_Port,GPO1_Pin,GPIO_PIN_SET);
        else      				           HAL_GPIO_WritePin(GPO1_GPIO_Port,GPO1_Pin,GPIO_PIN_RESET);
    }
    if(((mask&0x02)>>1)==1)
    {
        if (((vals&0x02)>>1)==1)	  HAL_GPIO_WritePin(GPO2_GPIO_Port,GPO2_Pin,GPIO_PIN_SET);
        else      				          HAL_GPIO_WritePin(GPO2_GPIO_Port,GPO2_Pin,GPIO_PIN_RESET);
    }

}

uint8_t gpi_get(uint8_t gpoid)
{
	uint8_t state;
	PIO_GpioRead(&state);
	return (state >> (gpoid-1)) & 0x01;
}

uint8_t gpi_get_all(void)
{
	uint8_t state;
	PIO_GpioRead(&state);
	return state;
}

void EXTI4_15_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(Host_IRQ_Pin);
}

void EXTI4_15_IRQHandler_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOA clock */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Configure PA.00 pin as input floating */
  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Pin  = Host_IRQ_Pin;
  HAL_GPIO_Init(Host_IRQ_GPIO_Port, &GPIO_InitStructure);

  /* Enable and set EXTI line 0 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
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
