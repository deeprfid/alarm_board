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

#define RADAR1_INT_PORT              (GPIO_PORT_E)    //雷达1目标输入中断信号
#define RADAR1_INT_PIN               (GPIO_PIN_09)
#define RADAR1_INT_CH                (EXTINT_CH09)
#define RADAR1_INT_SRC               (INT_SRC_PORT_EIRQ9)
#define RADAR1_INT_IRQn              (INT020_IRQn)
#define RADAR1_INT_PRIO              (DDL_IRQ_PRIO_DEFAULT)

#define RADAR2_INT_PORT              (GPIO_PORT_C)    //雷达2目标输入中断信号
#define RADAR2_INT_PIN               (GPIO_PIN_08)
#define RADAR2_INT_CH                (EXTINT_CH08)
#define RADAR2_INT_SRC               (INT_SRC_PORT_EIRQ8)
#define RADAR2_INT_IRQn              (INT010_IRQn)
#define RADAR2_INT_PRIO              (DDL_IRQ_PRIO_DEFAULT)


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


#define LED01(x)                 do { (x) ?                                \
                                    GPIO_SetPins (LED_PORT, LED_1_PIN):     \
                                    GPIO_ResetPins(LED_PORT, LED_1_PIN);   \
                                } while (0)

#define LED01_TOGGLE()           do { (GPIO_TogglePins(LED_PORT, LED_1_PIN)); } while (0)


#define LED02(x)                 do { (x) ?                                \
                                    GPIO_SetPins(LED_PORT, LED_2_PIN):     \
                                    GPIO_ResetPins(LED_PORT, LED_2_PIN);   \
                                } while (0)

#define LED02_TOGGLE()           do { (GPIO_TogglePins(LED_PORT, LED_2_PIN)); } while (0)


#define LED03(x)                 do { (x) ?                                \
                                    GPIO_SetPins(LED_PORT, LED_3_PIN):     \
                                    GPIO_ResetPins(LED_PORT, LED_3_PIN);   \
                                } while (0)

#define LED03_TOGGLE()           do { (GPIO_TogglePins(LED_PORT, LED_3_PIN)); } while (0)


#define LED04(x)                 do { (x) ?                                \
                                    GPIO_SetPins(LED_PORT  , LED_4_PIN):     \
                                    GPIO_ResetPins(LED_PORT, LED_4_PIN);   \
                                } while (0)

#define LED04_TOGGLE()           do { (GPIO_TogglePins(LED_PORT, LED_4_PIN)); } while (0)





#define LED_R_STATE(x)             do { (x) ?                                  \
                                    GPIO_SetPins(LED_RGB_PORT, LED_R_PIN):     \
                                    GPIO_ResetPins(LED_RGB_PORT, LED_R_PIN);   \
                                    } while (0)

//#define LED_R_TOGGLE()            do { GPIO_TogglePins(LED_RGB_PORT, LED_R_PIN); } while (0)




#define LED_G_STATE(x)             do { (x) ?                                  \
                                    GPIO_SetPins(LED_RGB_PORT, LED_G_PIN):     \
                                    GPIO_ResetPins(LED_RGB_PORT, LED_G_PIN);   \
                                    } while (0)

//#define LED_G_TOGGLE()            do { GPIO_TogglePins(LED_RGB_PORT, LED_G_PIN); } while (0)




#define LED_B_STATE(x)             do { (x) ?                                    \
                                    GPIO_SetPins(LED_RGB_PORT, LED_B_PIN):       \
                                    GPIO_ResetPins(LED_RGB_PORT, LED_B_PIN);     \
                                    } while (0)

#define BEEP_STATE(x)             do { (x) ?                                    \
                                    GPIO_SetPins  (BOARD_BEEP_PORT, BOARD_BUZZ_PIN):       \
                                    GPIO_ResetPins(BOARD_BEEP_PORT, BOARD_BUZZ_PIN);     \
                                    } while (0)
																		
#define RELAY_STATE(x)             do { (x) ?                                    \
                                    GPIO_SetPins(BOARD_GPO_PORT  , BOARD_RELAY_PIN):       \
                                    GPIO_ResetPins(BOARD_GPO_PORT, BOARD_RELAY_PIN);     \
                                    } while (0)

																		
#define PWRDC12V_STATE(x)          do { (x) ?                                    \
                                    GPIO_ResetPins(BOARD_GPO_PORT  , BOARD_DC12V_PIN):       \
                                    GPIO_SetPins(BOARD_GPO_PORT, BOARD_DC12V_PIN);     \
                                    } while (0)																		
//#define LED_B_TOGGLE()            do { GPIO_TogglePins(LED_RGB_PORT, LED_B_PIN); } while (0)





#define BEEP_BOARD(x)             do { (x) ?                                    \
                                    GPIO_SetPins(BOARD_BEEP_PORT , BOARD_BEEP_PIN)	:       \
                                    GPIO_ResetPins(BOARD_BEEP_PORT, BOARD_BEEP_PIN);     \
                                    } while (0)
																		
																	
																		
/* 供外部调用的函数声明 */
void Board_gpio_init(void);
void bsp_Init_gpio(void);
void bsp_gpio_On(uint8_t _no);
void bsp_gpio_Off(uint8_t _no);
void bsp_gpio_toggle(uint8_t _no);
void GPIO_Pro(LED_T *g_tled,uint8_t ledid);
void GPIO_Stop(LED_T *g_tled,uint8_t ledid);
void GPIO_status_update(void);
uint8_t Radar_AIcam_update(void);
uint8_t Check_alarm_status(void);
void mutex_gpio_lock(void);
void mutex_gpio_unlock(void);
void GPIO_pwr_init(LED_T *g_tled,uint8_t ledid);
void GPIO_Start(LED_T *g_tled,uint8_t ledid,uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle);
void Green_pass_Tag(void);
void Green_pass_Tag(void);
void Reguler_Tag(void);
void Relay_AM_EAS(void);
void Alarm_SilenceCmd(void);
void Alarm_Off(void);
void bsp_RunPer10ms(void);
void RADAR1_Ext_Init(void);
void RADAR2_Ext_Init(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
