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

#include "main.h"

extern uint8_t  alarm_flag;
extern uint8_t  rgb_led_status;
uint8_t Relay_input_flag;

LED_T R_tLED;
LED_T G_tLED;
LED_T B_tLED;
LED_T OPA_BeepLED;
LED_T Radar_LED;
LED_T Board_LED_1;
LED_T Board_LED_2;
LED_T Relay_GPO;


static volatile uint8_t mutex_led = 0;

void mutex_led_lock(void)
{

    mutex_led = 1;
    SysTick_Suspend();
}


void mutex_led_unlock(void)
{
    mutex_led = 0;
    SysTick_Resume();
}


/*
*********************************************************************************************************
*	函 数 名: bsp_InitLed
*	功能说明: 配置LED指示灯相关的GPIO,  该函数被 bsp_Init() 调用。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void LED_GPIO_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    stcGpioInit.u16PinDrv = PIN_MID_DRV;
    //  stcGpioInit.u16PullUp = PIN_PU_ON;
    (void)GPIO_Init(LED_R_PORT, LED_R_PIN, &stcGpioInit);
    (void)GPIO_Init(LED_G_PORT, LED_G_PIN, &stcGpioInit);
    (void)GPIO_Init(LED_B_PORT, LED_B_PIN, &stcGpioInit);

    GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);
    (void)GPIO_Init(BOARD_LED_PORT, BOARD_LED_PIN, &stcGpioInit);


    R_tLED.ucMute = 0;
    G_tLED.ucMute = 0;
    B_tLED.ucMute = 0;
    OPA_BeepLED.ucMute = 0;
    Radar_LED.ucMute = 0;
    Relay_GPO.ucMute = 0;
    Board_LED_1.ucMute = 0;
    Board_LED_2.ucMute = 0;

}

void bsp_InitLed(void)
{
    Alarm_Off();
}

/*
*********************************************************************************************************
*    板载指示灯初始化(上电自检用): 绿指示灯 PB3 + BOARD_LED_1/2(PA11/PA12)
*********************************************************************************************************
*/
void Board_LED_Init(void)
{
    stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    stcGpioInit.u16PinDrv   = PIN_MID_DRV;
    GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);

    (void)GPIO_Init(RADAR_BOARD_LED_G_PORT, RADAR_BOARD_LED_G_PIN, &stcGpioInit);
    (void)GPIO_Init(BOARD_LED_1_PORT, BOARD_LED_1_PIN, &stcGpioInit);
    (void)GPIO_Init(BOARD_LED_1_PORT, BOARD_LED_2_PIN, &stcGpioInit);
}

/*
*********************************************************************************************************
*	函 数 名: bsp_LedOn
*	功能说明: 点亮指定的LED指示灯。
*	形    参:  _no : 指示灯序号，范围 1 - 4
*	返 回 值: 无
*********************************************************************************************************
*/



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
    if (_no == LED_RLED)
    {
        LED_R_ON();
    }
    else if (_no == LED_GLED)
    {
        LED_G_ON();
    }
    else if (_no == LED_BLED)
    {
        LED_B_ON();
    }

    else if (_no == OPA_BUZZLED)
    {
        gpo_set(GPO2, 1);
        gpo_set(GPO3, 1);
    }
    else if (_no == RADARLED)
    {
        gpo_set(BOARD_GLED, 1);
    }
    else if (_no == RELAYGPO)
    {
        gpo_set(GPO1, 1);
    }
    else if (_no == BOARDLED1)
    {
        GPIO_ResetPins(BOARD_LED_1_PORT, BOARD_LED_1_PIN); //GPIO_SetPins
    }
    else if (_no == BOARDLED2)
    {
        GPIO_ResetPins(BOARD_LED_1_PORT, BOARD_LED_2_PIN);
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
    if (_no == LED_RLED)
    {
        LED_R_OFF();
    }
    else if (_no == LED_GLED)
    {
        LED_G_OFF();
    }
    else if (_no == LED_BLED)
    {
        LED_B_OFF();
    }
    else if (_no == OPA_BUZZLED)
    {
        gpo_set(GPO2, 0);
        gpo_set(GPO3, 0);
    }
    else if (_no == RADARLED)
    {
        gpo_set(BOARD_GLED, 0);
    }
    else if (_no == RELAYGPO)
    {
        gpo_set(GPO1, 0);
    }
    else if (_no == BOARDLED1)
    {
        GPIO_SetPins(BOARD_LED_1_PORT, BOARD_LED_1_PIN); //GPIO_SetPins
    }
    else if (_no == BOARDLED2)
    {
        GPIO_SetPins(BOARD_LED_1_PORT, BOARD_LED_2_PIN);
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
    g_tled->ucEnalbe = 1;	/* 设置完全局参数后再使能发声标志 */
    bsp_LedOn(ledid);			/* 开始发声 */
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
    LED_Pro(&R_tLED, LED_RLED);
    LED_Pro(&G_tLED, LED_GLED);
    LED_Pro(&B_tLED, LED_BLED);
    LED_Pro(&OPA_BeepLED, OPA_BUZZLED);
    LED_Pro(&Radar_LED, RADARLED);
    LED_Pro(&Relay_GPO, RELAYGPO);
	  LED_Pro(&Board_LED_1, BOARDLED1);
	  LED_Pro(&Board_LED_2, BOARDLED2);
}






extern BEEP_T g_tBeep;
uint8_t Check_alarm_status(void)
{
    if(R_tLED.ucEnalbe == 0 && G_tLED.ucEnalbe == 0 && g_tBeep.ucEnalbe == 0)
    {
        Alarm_Off();
        return true;
    }
    else
    {
        return false;

    }


}


void Alarm_On(void)
{

    if(rgb_led_status == ALARM_R_CODE)
    {
       Reguler_Tag();

    }
    else if(rgb_led_status == ALARM_G_CODE)
    {
        Green_pass_Tag(); 

    }
    else if(rgb_led_status == ALARM_RELAY_CODE)
    {
        Relay_AM_EAS();

    }
    else
    {
        Alarm_Off();
    }
}

uint8_t alarm_duration = 5;

void  Alarm_BeeP_LED_Mode(uint8_t beepmode, uint8_t syncmode)
{
    en_pin_state_t p_Syncmode    = switch_decoder_pio_read(SYNC_MODE);

    {
        if(p_Syncmode)
        {
            BEEP_Start(21, 12, alarm_duration * 3);
            LED_Start(&R_tLED, LED_RLED, 21, 12, alarm_duration * 3);
					  LED_Start(&Board_LED_2, BOARDLED2, 21, 12, alarm_duration * 3);
            LED_Start(&OPA_BeepLED, OPA_BUZZLED, alarm_duration * 100, 1, 1);
            LED_Start(&Relay_GPO, RELAYGPO, alarm_duration * 100, 1, 1);
        }
        else
        {
            BEEP_Start(15, 10, alarm_duration);
            LED_Start(&R_tLED, LED_RLED, alarm_duration * 25, 1, 1);
					  LED_Start(&Board_LED_2, BOARDLED2,alarm_duration * 25, 1, 1);
            LED_Start(&OPA_BeepLED, OPA_BUZZLED, alarm_duration * 25, 1, 1);
            LED_Start(&Relay_GPO, RELAYGPO, alarm_duration * 25, 1, 1);
        }
    }




}

void GPIO_LED_test(void)
{
    Led_Stop(&R_tLED, LED_RLED);
	  Led_Stop(&G_tLED, LED_GLED);
    Led_Stop(&B_tLED, LED_BLED);

    LED_Start(&G_tLED, LED_GLED, 15, 15, 2);
    DDL_DelayMS(300);
    LED_Start(&B_tLED, LED_BLED, 15, 15, 2);
    DDL_DelayMS(300);
    LED_Start(&R_tLED, LED_RLED, 15, 15, 2);
    DDL_DelayMS(300);

}

void Green_pass_Tag(void)
{
	  if(R_tLED.ucEnalbe==1)  return;
	
    en_pin_state_t p_Syncmode    = switch_decoder_pio_read(SYNC_MODE);
    Led_Stop(&R_tLED, LED_RLED);
    Led_Stop(&B_tLED, LED_BLED);

    if(p_Syncmode)
    {
        LED_Start(&G_tLED, LED_GLED, alarm_duration * 100, 1, 1);
    }
    else
    {
        LED_Start(&G_tLED, LED_GLED, alarm_duration * 25, 1, 1);
    }

    BEEP_Stop();
}

void Reguler_Tag(void)
{
    Led_Stop(&G_tLED, LED_GLED);
    Led_Stop(&B_tLED, LED_BLED);
    Alarm_BeeP_LED_Mode(1, 1);

}

void Relay_AM_EAS(void)
{
    Led_Stop(&G_tLED, LED_GLED);
    Led_Stop(&B_tLED, LED_BLED);
    Alarm_BeeP_LED_Mode(1, 1);
}

void Network_offline(void)
{
    extern uint8_t offline_flag;
    LED_Start(&B_tLED, LED_BLED, 100, 100, 12);
    BEEP_Stop();
    Led_Stop(&R_tLED, LED_RLED);
    Led_Stop(&G_tLED, LED_GLED);
}

void Alarm_SilenceCmd(void)
{
    BEEP_Stop();
    Led_Stop(&R_tLED, LED_RLED);
    Led_Stop(&G_tLED, LED_GLED);
    Led_Stop(&B_tLED, LED_BLED);

}

void system_power_on(void)
{
    extern __align(64) alarm_confirm_package  HC32_RS485_corfirm_PDU;
    uint16_t  idkey = 0;
    idkey = Ucode_read(&HC32_RS485_corfirm_PDU.rngkey, &HC32_RS485_corfirm_PDU.uidkey);

    if(idkey)
    {

        LED_Start(&Radar_LED , RADARLED  , 15, 10, 2);
			  LED_Start(&Board_LED_1, BOARDLED1, 15, 10, 2);
			  LED_Start(&Board_LED_2, BOARDLED2, 15, 10, 2);
        BEEP_Start(15, 10, 2);
        GPIO_LED_test();
    }
    else
    {
        LED_Start(&Radar_LED  , RADARLED , 15, 10, 3);
			  LED_Start(&Board_LED_1, BOARDLED1, 15, 10, 3);
			  LED_Start(&Board_LED_2, BOARDLED2, 15, 10, 3);
        BEEP_Start(15, 10, 3);
    }
   
}

void Alarm_Off(void)
{
    BEEP_Stop();
    Led_Stop(&R_tLED, LED_RLED);
    Led_Stop(&G_tLED, LED_GLED);
    Led_Stop(&OPA_BeepLED, OPA_BUZZLED);
    Led_Stop(&Relay_GPO, RELAYGPO);
    Relay_input_flag = PIN_SET;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
