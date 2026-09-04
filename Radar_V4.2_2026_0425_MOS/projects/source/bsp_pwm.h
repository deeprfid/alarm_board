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

/* Function of this example. */
#define APP_FUNC_NORMAL_SINGLE_PWM          (0U)
#define APP_FUNC_SINGLE_EDGE_ALIGNED_PWM    (1U)
#define APP_FUNC_TOW_EDGE_SYMMETRIC_PWM     (2U)

/* Specify the function of the example. */
#define APP_FUNC                            (APP_FUNC_NORMAL_SINGLE_PWM)

/**
 * Define the configurations of PWM according to the function that selected.
 * In this example:
 *   1. System clock is 8MHz.
 *   2. Clock source of TimerA is PCLK(8MHz by default) and divided by 1.
 *   3. About PWM:
 *      APP_FUNC_NORMAL_SINGLE_PWM: frequency 200KHz, high duty 30%
 *      APP_FUNC_SINGLE_EDGE_ALIGNED_PWM: frequency 100KH, high duty 30%, 55%
 *      APP_FUNC_TOW_EDGE_SYMMETRIC_PWM: frequency 50KHz, high duty 50%, 40%
 *
 * Sawtooth mode:
 *   Calculate the period value according to the frequency:
 *     PeriodVal = (TimerAClockFrequency(Hz) / PWMFreq) - 1
 *   Calculate the compare value according to the duty ratio:
 *     CmpVal = ((PeriodVal + 1) * Duty) - 1
 *
 * Triangle mode:
 *   Calculate the period value according to the frequency:
 *     PeriodVal = (TimerAClockFrequency(Hz) / (PWMFreq * 2))
 *   Calculate the compare value according to the duty ratio:
 *     CmpVal = (PeriodVal * Duty)
 */
//#if (APP_FUNC == APP_FUNC_NORMAL_SINGLE_PWM)
#define TMRA_UNIT                       (CM_TMRA_1)
#define TMRA_PERIPH_CLK                 (FCG2_PERIPH_TMRA_1)
#define TMRA_PWM_CH                     (TMRA_CH1)

#define TMRA_PERIOD_VAL                 (4000U - 1U)
#define TMRA_PWM_CMP_VAL                (2000U  - 1U)
#define TMRA_PWM_PORT                   (GPIO_PORT_A)
#define TMRA_PWM_PIN                    (GPIO_PIN_08)
#define TMRA_PWM_PIN_FUNC               (GPIO_FUNC_4)

#define TMRA_MD                         (TMRA_MD_SAWTOOTH)
#define TMRA_DIR                        (TMRA_DIR_UP)


//#elif (APP_FUNC == APP_FUNC_SINGLE_EDGE_ALIGNED_PWM)
//#define TMRA_UNIT                       (CM_TMRA_1)
//#define TMRA_PERIPH_CLK                 (FCG2_PERIPH_TMRA_1)
//#define TMRA_PWMX_CH                    (TMRA_CH1)
//#define TMRA_PWMY_CH                    (TMRA_CH2)

//#define TMRA_PWMX_PORT                  (GPIO_PORT_A)
//#define TMRA_PWMX_PIN                   (GPIO_PIN_08)
//#define TMRA_PWMX_PIN_FUNC              (GPIO_FUNC_4)
//#define TMRA_PWMY_PORT                  (GPIO_PORT_A)
//#define TMRA_PWMY_PIN                   (GPIO_PIN_09)
//#define TMRA_PWMY_PIN_FUNC              (GPIO_FUNC_4)

//#define TMRA_MD                         (TMRA_MD_SAWTOOTH)
//#define TMRA_DIR                        (TMRA_DIR_UP)
//#define TMRA_PERIOD_VAL                 (80U - 1U)
//#define TMRA_PWMX_CMP_VAL               (24U - 1U)
//#define TMRA_PWMY_CMP_VAL               (44U - 1U)

//#elif (APP_FUNC == APP_FUNC_TOW_EDGE_SYMMETRIC_PWM)
//#define TMRA_UNIT                       (CM_TMRA_1)
//#define TMRA_PERIPH_CLK                 (FCG2_PERIPH_TMRA_1)
//#define TMRA_PWMX_CH                    (TMRA_CH1)
//#define TMRA_PWMY_CH                    (TMRA_CH2)

//#define TMRA_PWMX_PORT                  (GPIO_PORT_A)
//#define TMRA_PWMX_PIN                   (GPIO_PIN_08)
//#define TMRA_PWMX_PIN_FUNC              (GPIO_FUNC_4)
//#define TMRA_PWMY_PORT                  (GPIO_PORT_A)
//#define TMRA_PWMY_PIN                   (GPIO_PIN_09)
//#define TMRA_PWMY_PIN_FUNC              (GPIO_FUNC_4)

//#define TMRA_MD                         (TMRA_MD_TRIANGLE)
//#define TMRA_DIR                        (TMRA_DIR_UP)
//#define TMRA_PERIOD_VAL                 (80U)
//#define TMRA_PWMX_CMP_VAL               (40U)
//#define TMRA_PWMY_CMP_VAL               (48U)

//#else
//#error "The function is NOT supported."
//#endif

void Bsp_pwm_init(void);
void TmrAConfig(uint32_t duty);

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
