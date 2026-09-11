/*
*********************************************************************************************************
*
*	模块名称 : LED指示灯驱动模块
*	文件名称 : bsp_led.c
*	版    本 : V1.0
*	说    明 : 驱动LED指示灯
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2018-09-05 armfly  正式发布
*
*	Copyright (C), 2015-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


LED_T Port_1_LED;//Board_LED_Green CH1
LED_T Port_2_LED;//Board_LED_RED   CH2-3
LED_T Port_3_LED;//Board_LED_BLUE  CH4-5
LED_T Port_4_LED;//Board_LED_Green CH6-7
LED_T Port_5_LED;//Board_LED_WHITE CH8

LED_T R_tLED;
LED_T G_tLED;
LED_T B_tLED;

static volatile uint8_t mutex_led = 0;

void mutex_led_lock(void)
{

    mutex_led = 1;
    HAL_SuspendTick();
}


void mutex_led_unlock(void)
{
    mutex_led = 0;
    HAL_ResumeTick();
}
void bsp_InitLed(void)
{
    Led_Stop(&Port_1_LED, PORTLED_1);
    Led_Stop(&Port_2_LED, PORTLED_2);
    Led_Stop(&Port_3_LED, PORTLED_3);
    Led_Stop(&Port_4_LED, PORTLED_4);
    Led_Stop(&Port_5_LED, PORTLED_5);
    Led_Stop(&R_tLED, LED_RLED);
    Led_Stop(&G_tLED, LED_GLED);
    Led_Stop(&B_tLED, LED_BLED);

//    LED_Start(&Port_1_LED, PORTLED_1, 20, 50, 0);
//	  LED_Start(&Port_2_LED, PORTLED_1, 10, 50, 0);
//	  LED_Start(&Port_3_LED, PORTLED_1, 20, 50, 0);
//	  LED_Start(&Port_4_LED, PORTLED_1, 10, 50, 0);
//	  LED_Start(&Port_5_LED, PORTLED_1, 5 , 50, 0);
}

/*
*********************************************************************************************************
*	函 数 名: bsp_LedOn
*	功能说明: 点亮指定的LED指示灯。
*	形    参:  _no : 指示灯序号，范围 1 - 4
*	返 回 值: 无
*********************************************************************************************************
*/

void bsp_LedOn(uint8_t _no)
{
    if (_no == PORTLED_1)
    {
        HAL_GPIO_WritePin(MCULED_1_GPIO_Port, MCULED_1_Pin, GPIO_PIN_RESET);
    }

    if (_no == PORTLED_2)
    {
        HAL_GPIO_WritePin(MCULED_2_GPIO_Port, MCULED_2_Pin, GPIO_PIN_RESET);
    }

    if (_no == PORTLED_3)
    {
        HAL_GPIO_WritePin(MCULED_3_GPIO_Port, MCULED_3_Pin, GPIO_PIN_RESET);
    }

    if (_no == PORTLED_4)
    {
        HAL_GPIO_WritePin(MCULED_4_GPIO_Port, MCULED_4_Pin, GPIO_PIN_RESET);
    }

    if (_no == PORTLED_5)
    {
        HAL_GPIO_WritePin(MCULED_5_GPIO_Port, MCULED_5_Pin, GPIO_PIN_RESET);
    }

    if (_no == LED_RLED)
    {
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
    }

    if (_no == LED_GLED)
    {
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    }

    if (_no == LED_BLED)
    {
        HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);
    }

}

/*
*********************************************************************************************************
*	函 数 名: bsp_LedOff
*	功能说明: 熄灭指定的LED指示灯。
*	形    参:  _no : 指示灯序号，范围 1 - 4
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_LedOff(uint8_t _no)
{
    if (_no == PORTLED_1)
    {
        HAL_GPIO_WritePin(MCULED_1_GPIO_Port, MCULED_1_Pin, GPIO_PIN_SET);
    }

    if (_no == PORTLED_2)
    {
        HAL_GPIO_WritePin(MCULED_2_GPIO_Port, MCULED_2_Pin, GPIO_PIN_SET);
    }

    if (_no == PORTLED_3)
    {
        HAL_GPIO_WritePin(MCULED_3_GPIO_Port, MCULED_3_Pin, GPIO_PIN_SET);
    }

    if (_no == PORTLED_4)
    {
        HAL_GPIO_WritePin(MCULED_4_GPIO_Port, MCULED_4_Pin, GPIO_PIN_SET);
    }

    if (_no == PORTLED_5)
    {
        HAL_GPIO_WritePin(MCULED_5_GPIO_Port, MCULED_5_Pin, GPIO_PIN_SET);
    }

    if (_no == LED_RLED)
    {
        HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
    }

    if (_no == LED_GLED)
    {
        HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
    }

    if (_no == LED_BLED)
    {
        HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);
    }

}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Pro
*	功能说明: 每隔10ms调用1次该函数，用于控制蜂鸣器发声。该函数在 bsp_timer.c 中被调用。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void LED_Pro(LED_T *g_tled, uint8_t ledid)
{

    if ((g_tled->ucEnalbe == 0) || (g_tled->usStopTime == 0) || (g_tled->ucMute == 1))
    {
        return;
    }

    if (g_tled->ucState == 0)
    {
        if (g_tled->usStopTime > 0)	    /* 间断发声 */
        {
            if (++g_tled->usCount >= g_tled->usBeepTime)
            {
                bsp_LedOff(ledid);		/* 停止发声 */
                g_tled->usCount = 0;
                g_tled->ucState = 1;
            }
        }
        else
        {
            ;	/* 不做任何处理，连续发声 */
        }
    }
    else if (g_tled->ucState == 1)
    {
        if (++g_tled->usCount >= g_tled->usStopTime)
        {
            /* 连续发声时，直到调用stop停止为止 */
            if (g_tled->usCycle > 0)
            {
                if (++g_tled->usCycleCount >= g_tled->usCycle)
                {
                    /* 循环次数到，停止发声 */
                    g_tled->ucEnalbe = 0;
                }

                if (g_tled->ucEnalbe == 0)
                {
                    g_tled->usStopTime = 0;
                    return;
                }
            }

            g_tled->usCount = 0;
            g_tled->ucState = 0;

            bsp_LedOn(ledid);			/* 开始发声 */
        }
    }
}


void LED_Start(LED_T *g_tled, uint8_t ledid, uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle)
{
    if (_usBeepTime == 0 || g_tled->ucMute == 1)
    {
        return;
    }

    g_tled->usBeepTime = _usBeepTime;
    g_tled->usStopTime = _usStopTime;
    g_tled->usCycle = _usCycle;
    g_tled->usCount = 0;
    g_tled->usCycleCount = 0;
    g_tled->ucState = 0;
    g_tled->ucEnalbe = 1;	    /* 设置完全局参数后再使能发声标志 */
    bsp_LedOn(ledid);			    /* 开始发声 */
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Stop
*	功能说明: 停止蜂鸣音。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Led_Stop(LED_T *g_tled, uint8_t ledid)
{
    mutex_led_lock();
    g_tled->ucEnalbe = 0;
    Led_pwr_init(g_tled, ledid);
    mutex_led_unlock();
}


void Led_pwr_init(LED_T *g_tled, uint8_t ledid)
{
    g_tled->ucEnalbe = 0;
    g_tled->ucMute = 0;
    g_tled->ucState = 0;
    g_tled->usBeepTime = 0;
    g_tled->usCount = 0;
    g_tled->usCycle = 0;
    g_tled->usCycleCount = 0;
    g_tled->usStopTime = 0;
    bsp_LedOff(ledid);

}




void Led_status_update(void)
{
    LED_Pro(&Port_1_LED, PORTLED_1);
    LED_Pro(&Port_2_LED, PORTLED_2);
    LED_Pro(&Port_3_LED, PORTLED_3);
    LED_Pro(&Port_4_LED, PORTLED_4);
    LED_Pro(&Port_5_LED, PORTLED_5);
    LED_Pro(&R_tLED, LED_RLED);
    LED_Pro(&G_tLED, LED_GLED);
    LED_Pro(&B_tLED, LED_BLED);

}


void bsp_RunPer10ms(void)
{
    static uint8_t s_count = 0;

    if (++s_count >= 10)
    {
        s_count = 0;
        Led_status_update();
        BEEP_Pro();
    }
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
