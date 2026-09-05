/*
*********************************************************************************************************
*
*	模块名称 : BSP模块(For STM32H7)
*	文件名称 : bsp.h
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

#ifndef _BSP_H_
#define _BSP_H_

#define STM32_V7    


/* 检查是否定义了开发板型号 */
#if !defined (STM32_V7)
	#error "Please define the board model : STM32_V7"
#endif

/* 定义 BSP 版本号 */
#define __STM32H7_BSP_VERSION		"1.10"

#define  USE_RTX    1

/* CPU空闲时执行的函数 */
//#define CPU_IDLE()		bsp_Idle()

/* 开关全局中断的宏 */
#define ENABLE_INT()	__set_PRIMASK(0)	/* 使能全局中断 */
#define DISABLE_INT()	__set_PRIMASK(1)	/* 禁止全局中断 */

/* 这个宏仅用于调试阶段排错 */
#define BSP_Printf		printf
//#define BSP_Printf(...)

#define EXTI9_5_ISR_MOVE_OUT		/* bsp.h 中定义此行，表示本函数移到 stam32f4xx_it.c。 避免重复定义 */

#define ERROR_HANDLER()		 Error_Handler();

/* 默认是关闭状态 */


#include "stm32f0xx_hal.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define GPI_IN1_Pin               GPIO_PIN_13
#define GPI_IN1_GPIO_Port         GPIOC
#define GPI_IN2_Pin               GPIO_PIN_14
#define GPI_IN2_GPIO_Port         GPIOC
#define GPI_IN3_Pin               GPIO_PIN_15
#define GPI_IN3_GPIO_Port         GPIOC
#define GPI_IN4_Pin               GPIO_PIN_1
#define GPI_IN4_GPIO_Port         GPIOB
#define GPO_BZ3V3_Pin             GPIO_PIN_2
#define GPO_BZ3V3_GPIO_Port       GPIOB
#define LED_B_Pin                 GPIO_PIN_12
#define LED_B_GPIO_Port           GPIOB
#define LED_R_Pin                 GPIO_PIN_13
#define LED_R_GPIO_Port           GPIOB
#define LED_G_Pin                 GPIO_PIN_14
#define LED_G_GPIO_Port           GPIOB
#define GPO_BZ_Pin                GPIO_PIN_15
#define GPO_BZ_GPIO_Port          GPIOB
#define MCULED_1_Pin              GPIO_PIN_6
#define MCULED_1_GPIO_Port        GPIOA
#define MCULED_2_Pin              GPIO_PIN_7
#define MCULED_2_GPIO_Port        GPIOA
#define MCULED_3_Pin              GPIO_PIN_11
#define MCULED_3_GPIO_Port        GPIOA
#define MCULED_4_Pin              GPIO_PIN_15
#define MCULED_4_GPIO_Port        GPIOA
#define MCULED_5_Pin              GPIO_PIN_7
#define MCULED_5_GPIO_Port        GPIOB
#define GPO1_Pin                  GPIO_PIN_6
#define GPO1_GPIO_Port            GPIOB
#define GPO2_Pin                  GPIO_PIN_9
#define GPO2_GPIO_Port            GPIOB
#define CMRESET_Pin              GPIO_PIN_8
#define CMRESET_GPIO_Port        GPIOB
#define Host_IRQ_Pin            GPIO_PIN_12
#define Host_IRQ_GPIO_Port      GPIOA

#define STM32F0_IWDG_ENABLE   (0U)
#define GET_RADAR_ENABLE      (1U)

#ifndef TRUE
  #define TRUE  1
#endif

#ifndef FALSE
	#define FALSE 0
#endif

#include "bsp_timer.h"
#include "bsp_uart_fifo.h"
#include "bsp_tim_pwm.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_gpio.h"
#include "app.h"
/* 提供给其他C文件调用的函数 */
void bsp_Init(void);
void bsp_Idle(void);
void System_Init(void);
void bsp_GetCpuID(uint32_t *_id);
void Error_Handler(void);
void STM32F030_delay(__IO uint32_t nCount);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
