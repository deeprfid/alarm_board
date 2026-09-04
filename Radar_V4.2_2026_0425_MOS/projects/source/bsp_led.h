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



#define LED_RLED     (1UL)
#define LED_GLED     (2UL)
#define LED_BLED     (3UL)
#define OPA_BUZZLED  (4UL)
#define RADARLED     (5UL)
#define RELAYGPO     (6UL)
#define BOARDLED1   (7UL)
#define BOARDLED2   (8UL)



#define LED_G_PORT          (GPIO_PORT_B)
#define LED_G_PIN           (GPIO_PIN_08)
/* LED_G Port/Pin definition */
#define LED_R_PORT          (GPIO_PORT_B)
#define LED_R_PIN           (GPIO_PIN_05)
/* LED_Y Port/Pin definition */
#define LED_B_PORT          (GPIO_PORT_C)
#define LED_B_PIN           (GPIO_PIN_15)
/* LED_B Port/Pin definition */

/* LED toggle definition */
#define LED_R_ON()           (GPIO_SetPins(LED_R_PORT, LED_R_PIN)) 
#define LED_R_OFF()          (GPIO_ResetPins(LED_R_PORT, LED_R_PIN)) 

#define LED_G_ON()           (GPIO_SetPins(LED_G_PORT, LED_G_PIN))
#define LED_G_OFF()          (GPIO_ResetPins(LED_G_PORT, LED_G_PIN))

#define LED_B_ON()           (GPIO_SetPins(LED_B_PORT, LED_B_PIN))
#define LED_B_OFF()          (GPIO_ResetPins(LED_B_PORT, LED_B_PIN))

/* 供外部调用的函数声明 */
void bsp_InitLed(void);
void bsp_LedOn(uint8_t _no);
void bsp_LedOff(uint8_t _no);
void bsp_LedToggle(uint8_t _no);
void LED_Pro(LED_T *g_tled,uint8_t ledid);
void Led_Stop(LED_T *g_tled,uint8_t ledid);
void Led_status_update(void);
uint8_t Check_alarm_status(void);
void mutex_led_lock(void);
void mutex_led_unlock(void);
void Led_pwr_init(LED_T *g_tled,uint8_t ledid);
void LED_Start(LED_T *g_tled,uint8_t ledid,uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle);
void GPIO_LED_test(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
