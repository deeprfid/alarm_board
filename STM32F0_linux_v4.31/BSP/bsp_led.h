/*
*********************************************************************************************************
*
*	模块名称 : LED指示灯驱动模块
*	文件名称 : bsp_led.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_LED_H
#define __BSP_LED_H
#include "stdint.h"
typedef struct _LED_T
{
	uint8_t  ucEnalbe;
	uint8_t  ucState;
	uint16_t usBeepTime;
	uint16_t usStopTime;
	uint16_t usCycle;
	uint16_t usCount;
	uint16_t usCycleCount;
	uint8_t  ucMute;		/* 1表示静音 */	
}LED_T;

#define PORTLED_1  (1UL)
#define PORTLED_2  (2UL)
#define PORTLED_3  (3UL)
#define PORTLED_4  (4UL)
#define PORTLED_5  (5UL)
#define LED_RLED   (6UL)
#define LED_GLED   (7UL)
#define LED_BLED   (8UL)

/* 供外部调用的函数声明 */
void bsp_InitLed(void);
void bsp_LedOn(uint8_t _no);
void bsp_LedOff(uint8_t _no);
void LED_Pro(LED_T *g_tled,uint8_t ledid);
void Led_Stop(LED_T *g_tled,uint8_t ledid);
void Led_status_update(void);
/* LED_T 访问临界区: 内部用 PRIMASK 保存/恢复, 必须成对调用且不可嵌套(见 bsp_led.c) */
void mutex_led_lock(void);
void mutex_led_unlock(void);
void Led_pwr_init(LED_T *g_tled,uint8_t ledid);
void LED_Start(LED_T *g_tled,uint8_t ledid,uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
