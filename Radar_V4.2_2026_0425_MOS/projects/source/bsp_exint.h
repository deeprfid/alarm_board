/*
*********************************************************************************************************
*
*	模块名称 : 定时器模块
*	文件名称 : bsp_timer.h
*	版    本 : V1.3
*	说    明 : 头文件
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "stdint.h"


extern stc_ring_buf_t g_AlarmRing;

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define EXINT_PORT              (GPIO_PORT_C)    //雷达目标输入中断信号
#define EXINT_PIN               (GPIO_PIN_14)
#define EXTINT_CH               (EXTINT_CH14)
#define EXINT_SRC               (INT_SRC_PORT_EIRQ14)
#define EXINT_IRQn              (INT010_IRQn)
#define EXINT_PRIO              (DDL_IRQ_PRIO_DEFAULT)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
#define AICAM_PORT               (GPIO_PORT_B)    //报警输入中断信号
#define AICAM_PIN1               (GPIO_PIN_00)
#define AICAM_CH1                (EXTINT_CH00)
#define AICAM_SRC1               (INT_SRC_PORT_EIRQ0)
#define AICAM_IRQn1              (INT011_IRQn)
#define AICAM_EXINT_PRIO         (DDL_IRQ_PRIO_DEFAULT)

//#define ALARM_PORT               (GPIO_PORT_B)    //报警输入中断信号
//#define ALARM_PIN2               (GPIO_PIN_01)
//#define ALARM_CH2                (EXTINT_CH01)
//#define ALARM_SRC2               (INT_SRC_PORT_EIRQ1)
//#define ALARM_IRQn2              (INT012_IRQn)
//#define ALARM_EXINT_PRIO         (DDL_IRQ_PRIO_DEFAULT)

//#define ALARM_PORT               (GPIO_PORT_B)    //报警输入中断信号
//#define ALARM_PIN3               (GPIO_PIN_02)
//#define ALARM_CH3                (EXTINT_CH02)
//#define ALARM_SRC3               (INT_SRC_PORT_EIRQ2)
//#define ALARM_IRQn3              (INT011_IRQn)
//#define ALARM_EXINT_PRIO         (DDL_IRQ_PRIO_DEFAULT)

//#define ALARM_PORT               (GPIO_PORT_B)    //报警输入中断信号
//#define ALARM_PIN4               (GPIO_PIN_10)
//#define ALARM_CH4                (EXTINT_CH10)
//#define ALARM_SRC4               (INT_SRC_PORT_EIRQ10)
//#define ALARM_IRQn4              (INT011_IRQn)
//#define ALARM_EXINT_PRIO         (DDL_IRQ_PRIO_DEFAULT)

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
