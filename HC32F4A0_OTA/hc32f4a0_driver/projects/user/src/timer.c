#include <stdio.h>
#include "timer.h"
#include "hc32_ll.h"
#include "irq.h"
uint8    Timerfirstreadmtime;
uint32	 Timermsvalue;
uint32	 Timermslastva;

uint64 gGetSysTickVal = 0;

uint64_t  getSysTick(void)//获取系统时间，单位ms
{
    uint64 ret;
    uint32 tcount = TMR2_GetCountValue(CM_TMR2_1,TMR2_CH_A) / 120;
    ret = (uint64)512 * (uint64)gSysTickCnt + tcount;

    if (ret < gGetSysTickVal)
    {
        tcount = TMR2_GetCountValue(CM_TMR2_1,TMR2_CH_A) / 120;
        ret = (uint64)512 * (uint64)gSysTickCnt + tcount;
    }

    gGetSysTickVal = ret;

    return ret;
}


//误差1ms,返回TIMESTAMP为所经历的时间，最长是14小时后重新复位，但是必须读取一次后下次的读取时间不超过14小时
//读取间隔必须大于1us,否则不准
void timer_Get_msctr(unsigned int* timestamp)
{

}


void timer_Get_lltimer(unsigned int* timestamp)//获取目前计时器的计数值，计数值放入timestamp
{

}



//返回值为从计时器的计数值为lltimer_stamp开始到现在计时器又经历了多少计数值,82个计数值表示1ms
unsigned int timer_Diff_lltimer(unsigned int lltimer_stamp)
{

    return 0;
}

unsigned int timer_Diff_ms(unsigned int lltimer_stamp)//返回值为计时器的计数值为lltimer_stamp时到现在所经历的时间，单位毫妙
{

    return 0;
}


void timer_Delay_us(unsigned int us)	//微妙级延时，调用后等待延时结束才退出该函数,最长0xffffffff us
{

}


void timer_Delay_ms(unsigned int ms)	//毫秒级延时，调用后等待延时结束才退出该函数,最长0xffffffff ms
{

}




void timer_ini(void)
{
    //timera1,timera2用在ms计时，timera3用于us延时


}

void timer4_ini(void)//512ms中断一次，用于ms 长时间计时
{

}

void timer_Init(void)
{

    unsigned int timecfirstread;
    Timerfirstreadmtime = 1;
    Timermsvalue = 0;
    Timermslastva = 0;
    timer_ini();
    timer4_ini();
    timer_Get_msctr(&timecfirstread); //第一次获取时间
}

