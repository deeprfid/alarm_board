/*
*********************************************************************************************************
*
*	模块名称 : 串口中断+FIFO驱动模块
*	文件名称 : bsp_uart_fifo.h
*	说    明 : 头文件
*
*	Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_GPIO_H_
#define _BSP_GPIO_H_
#include <stdint.h>

int  PIO_GPIOInit(void);	
void beep_on(void);
void beep_off(void);
void gpo_set(uint8_t gpoid, uint8_t state);
void PIO_GpioRead(uint8_t *vals);
void PIO_GpioSet(uint8_t mask,uint8_t vals);
uint8_t gpi_get(uint8_t gpoid);
uint8_t gpi_get_all(void);
void EXTI4_15_IRQHandler_Config(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
