
#ifndef _timer_H_
#define _timer_H_

#include "hc32_ll.h"
#include  "type.h"

#define TMR2_UNIT                       (CM_TMR2_1)
#define TMR2_CH                         (TMR2_CH_A)
#define TMR2_PERIPH_CLK                 (FCG2_PERIPH_TMR2_1)

/**
 * Use hardware trigger if needed. None-zero to enable.
 * Hardware trigger conditions control. The conditions that can start Timer2, \
 * stop Timer2 or clear counter of Timer2.
 */
#define TMR2_USE_HW_TRIG                (0U)

/**
 * Clock source for this example.
 * In this example:
 *   1. System clock is 240MHz.
 *   2. PCLK1 is 120MHz.
 *   3. Use timer2 to count 1ms.
 *
 * A simple formula for calculating the compare value is:
 *   Tmr2CompareValue = (Tmr2Period(us) * [Tmr2ClockSource(MHz) / Tmr2ClockDiv]) - 1.
 */
#define TMR2_CLK_SRC                    (TMR2_CLK_PCLK1)
#define TMR2_CLK_DIV                    (TMR2_CLK_DIV1024)
#define TMR2_CMP_VAL                    (60000UL - 1U)           // 15000----->1ms div8   60000---->512ms  div1024        

/* Definitions about Timer2 interrupt for the example. */
#define TMR2_INT_TYPE                   (TMR2_INT_MATCH_CH_A)
#define TMR2_INT_PRIO                   (DDL_IRQ_PRIO_03)
#define TMR2_INT_SRC                    (INT_SRC_TMR2_1_CMP_A)
#define TMR2_INT_IRQn                   (INT050_IRQn)
#define TMR2_FLAG                       (TMR2_FLAG_MATCH_CH_A)


uint64_t  getSysTick(void);//获取系统时间，单位ms
void timer_ini(void);
void timer4_ini(void);
void  timer_Init(void);//定时计数器初始化函数，上电后的初始化配置函数,配置成功返回0，不成功返回1
void timer_Get_msctr(unsigned int* timestamp);////读取间隔必须大于1us,否则不准，该函数为获取模块时间值，获取时间值存放在timestamp，单位ms，第一次获取时为0，
                                              //后面获取时即是与第一次获取相隔的时间值，当timestamp=0xffff ffff后即重新从0开始
void timer_Get_lltimer(unsigned int* timestamp); //获取目前计时器的计数值，计数值放入timestamp
unsigned int timer_Diff_lltimer(unsigned int lltimer_stamp);//返回值为从计时器的计数值为lltimer_stamp开始到现在计时器又经历了多少计数值
															//计数值每加1代表经过多少时间要备注一下
//unsigned int timer_Diff_us(unsigned int lltimer_stamp);//返回值为计时器的计数值为lltimer_stamp时到现在所经历的时间，单位微妙
unsigned int timer_Diff_ms(unsigned int lltimer_stamp);//返回值为计时器的计数值为lltimer_stamp时到现在所经历的时间，单位毫妙

void timer_Delay_us(unsigned int us);	//微妙级延时，调用后等待延时结束才退出该函数
void timer_Delay_ms(unsigned int ms);	//毫秒级延时，调用后等待延时结束才退出该函数

int32_t Timer2_init(void);
void TMR2_Cmp_IrqCallback(void);

#endif

