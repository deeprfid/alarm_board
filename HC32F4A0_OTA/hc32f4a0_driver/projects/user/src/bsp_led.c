/*
*********************************************************************************************************
*
*   Module Name : LED Indicator Driver Module
*   File Name   : bsp_led.c
*   Version     : V1.0
*   Description : Drive LED indicator
*
*   Modification Record :
*       Version     Date        Author      Description
*       V1.0        2018-09-05 armfly      Initial release
*
*   Copyright (C), 2015-2030, Armfly Electronics www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "hc32f46_driver.h"
#include "bsp_led.h"
#include "bsp_pwm.h"
LED_T R_tLED;
LED_T G_tLED;
LED_T B_tLED;
LED_T Relay_ctl;
LED_T Buzz_ctl;
LED_T Beep_ctl;
LED_T Pwrdc_ctl;
LED_T B_tled1;
LED_T B_tled2;
LED_T B_tled3;
LED_T B_tled4;


osMutexId_t  Alarm_MuxID;
osRtxMutex_t Alarm_Mux_cb;
extern rfidcfg mycfgdata;

uint8_t Get_Radar(void)
{
    return GPIO_ReadInputPins(BOARD_RADAR1_PORT, BOARD_RADAR1_PIN) |
           GPIO_ReadInputPins(BOARD_RADAR2_PORT, BOARD_RADAR2_PIN);
}

void bsp_Init_gpio(void)
{
    bsp_gpio_Off(1);
    bsp_gpio_Off(2);
    bsp_gpio_Off(3);
    bsp_gpio_Off(4);
    bsp_gpio_Off(5);
    bsp_gpio_Off(10);
    R_tLED.ucMute = 0;
    G_tLED.ucMute = 0;
    B_tLED.ucMute = 0;
    B_tled1.ucMute = 0;
    B_tled2.ucMute = 0;
    B_tled3.ucMute = 0;
    B_tled4.ucMute = 0;
    Pwrdc_ctl.ucMute = 0;
    Relay_ctl.ucMute = 0;
    Buzz_ctl.ucMute = 0;
    Beep_ctl.ucMute = 0;
    RADAR1_Ext_Init();
    RADAR2_Ext_Init();
    osMutexAttr_t mux_alarm_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &Alarm_Mux_cb,
        sizeof(Alarm_Mux_cb)
    };
    Alarm_MuxID = osMutexNew(&mux_alarm_attr);
}

void bsp_gpio_On(uint8_t _no)
{
    switch (_no)
    {
        case 1:
            LED01(0);
            break;

        case 2:
            LED02(0);
            break;

        case 3:
            LED03(0);
            break;

        case 4:
            LED04(0);
            break;

        case 5:
          //  BEEP_STATE(1);
				   BEEP_ENABLE();
            break;

        case 6:
            LED_R_STATE(1);
            break;

        case 7:
            LED_G_STATE(1);
            break;

        case 8:
            LED_B_STATE(1);
            break;

        case 9:
            RELAY_STATE(1);
            break;

        case 10:
            PWRDC12V_STATE(0);
            break;

        case 11:
            BEEP_BOARD(1);
            break;

        default:
            return;
    }
}

void bsp_gpio_Off(uint8_t _no)
{
    switch (_no)
    {
        case 1:
            LED01(1);
            break;

        case 2:
            LED02(1);
            break;

        case 3:
            LED03(1);
            break;

        case 4:
            LED04(1);
            break;

        case 5:
          //  BEEP_STATE(0);
				   BEEP_DISABLE();
            break;

        case 6:
            LED_R_STATE(0);
            break;

        case 7:
            LED_G_STATE(0);
            break;

        case 8:
            LED_B_STATE(0);
            break;

        case 9:
            RELAY_STATE(0);
            break;

        case 10:
            PWRDC12V_STATE(1);
            break;

        case 11:
            BEEP_BOARD(0);
            break;

        default:
            return;
    }
}

void bsp_gpio_toggle(uint8_t _no)
{
    switch (_no)
    {
        case 1:
            LED01_TOGGLE();
            break;

        case 2:
            LED02_TOGGLE();
            break;

        case 3:
            LED03_TOGGLE();
            break;

        case 4:
            LED04_TOGGLE();
            break;

        case 6:
            LED_R_TOGGLE();
            break;

        case 7:
            LED_G_TOGGLE();
            break;

        case 8:
            LED_B_TOGGLE();
            break;

        default:
            return;
    }
}

void GPIO_Pro(LED_T *g_tled, uint8_t ledid)
{
    if ((g_tled->ucEnalbe == 0) || (g_tled->usStopTime == 0) || (g_tled->ucMute == 1))
    {
        return;
    }

    if (g_tled->ucState == 0)
    {
        if (g_tled->usStopTime > 0)
        {
            if (++g_tled->usCount >= g_tled->usBeepTime)
            {
                bsp_gpio_Off(ledid);
                g_tled->usCount = 0;
                g_tled->ucState = 1;
            }
        }
    }
    else if (g_tled->ucState == 1)
    {
        if (++g_tled->usCount >= g_tled->usStopTime)
        {
            if (g_tled->usCycle > 0)
            {
                if (++g_tled->usCycleCount >= g_tled->usCycle)
                {
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
            bsp_gpio_On(ledid);
        }
    }
}

void GPIO_Start(LED_T *g_tled, uint8_t ledid, uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle)
{
    if (_usBeepTime == 0 || g_tled->ucMute == 1 || g_tled->ucEnalbe == 1)
    {
        return;
    }

    g_tled->usBeepTime = _usBeepTime;
    g_tled->usStopTime = _usStopTime;
    g_tled->usCycle = _usCycle;
    g_tled->usCount = 0;
    g_tled->usCycleCount = 0;
    g_tled->ucState = 0;
    g_tled->ucEnalbe = 1;
    bsp_gpio_On(ledid);
}

void mutex_gpio_lock(void)
{
    osMutexAcquire(Alarm_MuxID, osWaitForever);
}

void mutex_gpio_unlock(void)
{
    osMutexRelease(Alarm_MuxID);
}

void GPIO_Stop(LED_T *g_tled, uint8_t ledid)
{
    mutex_gpio_lock();
    g_tled->ucEnalbe = 0;
    GPIO_pwr_init(g_tled, ledid);
    mutex_gpio_unlock();
}

void GPIO_pwr_init(LED_T *g_tled, uint8_t ledid)
{
    g_tled->ucEnalbe = 0;
    g_tled->ucMute = 0;
    g_tled->ucState = 0;
    g_tled->usBeepTime = 0;
    g_tled->usCount = 0;
    g_tled->usCycle = 0;
    g_tled->usCycleCount = 0;
    g_tled->usStopTime = 0;
    bsp_gpio_Off(ledid);
}

void GPIO_status_update(void)
{
    mutex_gpio_lock();

    GPIO_Pro(&B_tled1, BOARD_LED1);
    GPIO_Pro(&B_tled2, BOARD_LED2);
    GPIO_Pro(&B_tled3, BOARD_LED3);
    GPIO_Pro(&B_tled4, BOARD_LED4);
    GPIO_Pro(&R_tLED, LED_RLED);
    GPIO_Pro(&G_tLED, LED_GLED);
    GPIO_Pro(&B_tLED, LED_BLED);
    GPIO_Pro(&Relay_ctl, RELAY_CTRL);
    GPIO_Pro(&Buzz_ctl, BUZZ_CTRL);
    GPIO_Pro(&Pwrdc_ctl, PWRDC_CTRL);
    GPIO_Pro(&Beep_ctl, BEEP_CTRL);
    mutex_gpio_unlock();
}

void OSTimeTickHook(void);

void bsp_RunPer10ms(void)
{
    static uint8_t s_count = 0;

    if (++s_count >= 10)
    {
        s_count = 0;
        GPIO_status_update();
        OSTimeTickHook();
    }
}

uint8_t Check_alarm_status(void)
{
    if (R_tLED.ucEnalbe == 0 && Buzz_ctl.ucEnalbe == 0 && G_tLED.ucEnalbe == 0)
    {
        Alarm_Off();
        return true;
    }
    else
    {
        return false;
    }
}
extern uint32_t buzz_duty;
void Alarm_On(uint8_t rgb_led_status, uint8_t easflag,uint32_t beeptime)
{
    uint8_t radar1  = switch_pio_read(BOARD_RADAR_1);
    uint8_t radar2  = switch_pio_read(BOARD_RADAR_2);
    uint8_t aicam   = switch_pio_read(BOARD_GPI1); //AI CAMERA
    uint8_t easmode = switch_pio_read(EAS_MODE);
    buzz_duty=beeptime;
    if(easmode == PIN_RESET || easflag == PIN_RESET)
    {
        goto FINOPT;
    }

    if( radar1 == PIN_SET || radar2 == PIN_SET || PIN_RESET == aicam)
    {
        goto FINOPT;
    }
    else
    {
        return;
    }

FINOPT:

    switch (rgb_led_status)
    {
        case ALARM_G_CODE:
            Green_pass_Tag();
            break;

        case ALARM_R_CODE:
            Reguler_Tag();
            break;

        case ALARM_RELAY_CODE:
            Relay_AM_EAS();
            break;

        default:
            break;
    }
}

void Alarm_Output(uint8_t gpoid, uint16_t _msONtime, uint16_t _msOFFtime, uint16_t Cycle)
{
    switch(gpoid)
    {
        case BOARD_LED1:
        {
            GPIO_Start(&B_tled1, BOARD_LED1, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case BOARD_LED2:
        {
            GPIO_Start(&B_tled2, BOARD_LED2, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case BOARD_LED3:
        {
            GPIO_Start(&B_tled3, BOARD_LED3, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case BOARD_LED4:
        {
            GPIO_Start(&B_tled4, BOARD_LED4, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case LED_RLED  :
        {
            GPIO_Start(&R_tLED, LED_RLED, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case LED_GLED  :
        {
            GPIO_Start(&G_tLED, LED_GLED, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case LED_BLED  :
        {
            GPIO_Start(&B_tLED, LED_BLED, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case PWRDC_CTRL:
        {
            GPIO_Start(&Pwrdc_ctl, PWRDC_CTRL, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case BUZZ_CTRL :
        {
            GPIO_Start(&Buzz_ctl, BUZZ_CTRL, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case BEEP_CTRL :
        {
            GPIO_Start(&Beep_ctl, BEEP_CTRL, _msONtime, _msOFFtime, Cycle);
            break;
        }

        case RELAY_CTRL:
        {
            GPIO_Start(&Relay_ctl, RELAY_CTRL, _msONtime, _msOFFtime, Cycle);
            break;
        }

        default:
            break;
    }

}

void Alarm_Disable(uint8_t gpoid)
{
    switch(gpoid)
    {
        case BOARD_LED1:
        {
            GPIO_Stop(&B_tled1, BOARD_LED1);
            break;
        }

        case BOARD_LED2:
        {
            GPIO_Stop(&B_tled2, BOARD_LED2);
            break;
        }

        case BOARD_LED3:
        {
            GPIO_Stop(&B_tled3, BOARD_LED3);
            break;
        }

        case BOARD_LED4:
        {
            GPIO_Stop(&B_tled4, BOARD_LED4);
            break;
        }

        case LED_RLED  :
        {
            GPIO_Stop(&R_tLED, LED_RLED  );
            break;
        }

        case LED_GLED  :
        {
            GPIO_Stop(&G_tLED, LED_GLED  );
            break;
        }

        case LED_BLED  :
        {
            GPIO_Stop(&B_tLED, LED_BLED  );
            break;
        }

        case PWRDC_CTRL:
        {
            GPIO_Stop(&Pwrdc_ctl, PWRDC_CTRL);
            break;
        }

        case BUZZ_CTRL :
        {
            GPIO_Stop(&Buzz_ctl, BUZZ_CTRL );
            break;
        }

        case BEEP_CTRL :
        {
            GPIO_Stop(&Beep_ctl, BEEP_CTRL );
            break;
        }

        case RELAY_CTRL:
        {
            GPIO_Stop(&Relay_ctl, RELAY_CTRL);
            break;
        }

        default:
            break;
    }

}
void Green_pass_Tag(void)
{
    GPIO_Stop(&R_tLED, LED_RLED);
    GPIO_Stop(&B_tLED, LED_BLED);
    GPIO_Start(&G_tLED, LED_GLED, mycfgdata.alarm_duration * 25, 1, 1);
    GPIO_Stop(&Relay_ctl, RELAY_CTRL);
    GPIO_Stop(&Buzz_ctl, BUZZ_CTRL);
}

void Reguler_Tag(void)
{
    GPIO_Stop(&G_tLED, LED_GLED);
    GPIO_Stop(&B_tLED, LED_BLED);
    GPIO_Start(&Relay_ctl, RELAY_CTRL, mycfgdata.alarm_duration * 25, 1, 1);
    GPIO_Start(&Pwrdc_ctl, PWRDC_CTRL, mycfgdata.alarm_duration * 25, 1, 1);
    GPIO_Start(&Buzz_ctl, BUZZ_CTRL, 15, 10, mycfgdata.alarm_duration);
    GPIO_Start(&B_tled4, BOARD_LED4, mycfgdata.alarm_duration * 25, 1, 1);
    GPIO_Start(&R_tLED, LED_RLED, mycfgdata.alarm_duration * 25, 1, 1);
}

void Relay_AM_EAS(void)
{
    GPIO_Stop(&G_tLED, LED_GLED);
    GPIO_Stop(&B_tLED, LED_BLED);
    GPIO_Start(&Buzz_ctl, BUZZ_CTRL, mycfgdata.alarm_duration * 25, 1, 1);
    GPIO_Start(&Pwrdc_ctl, PWRDC_CTRL, mycfgdata.alarm_duration * 25, 1, 1);
    GPIO_Start(&R_tLED, LED_RLED, mycfgdata.alarm_duration * 25, 1, 1);
}

void Alarm_SilenceCmd(void)
{
    GPIO_Stop(&Buzz_ctl, BUZZ_CTRL);
    GPIO_Stop(&Relay_ctl, RELAY_CTRL);
    GPIO_Stop(&Pwrdc_ctl, PWRDC_CTRL);
    GPIO_Stop(&R_tLED, LED_RLED);
    GPIO_Stop(&G_tLED, LED_GLED);
    GPIO_Stop(&B_tLED, LED_BLED);
}

void Alarm_Off(void)
{
    GPIO_Stop(&Buzz_ctl, BUZZ_CTRL);
    GPIO_Stop(&Relay_ctl, RELAY_CTRL);
    GPIO_Stop(&Pwrdc_ctl, PWRDC_CTRL);
    GPIO_Stop(&R_tLED, LED_RLED);
    GPIO_Stop(&G_tLED, LED_GLED);
}

uint8_t RGB_Lighting_update(void)
{

    if(PIN_RESET == switch_pio_read(LIGHT_ON))
    {
        if(R_tLED.ucEnalbe == 0 && G_tLED.ucEnalbe == 0)
            bsp_gpio_On(LED_BLED);
        else
            bsp_gpio_Off(LED_BLED);
				
    }

    else if((PIN_SET == switch_pio_read(BOARD_RADAR_1)) || (PIN_SET == switch_pio_read(BOARD_RADAR_2)))
    {
        if(R_tLED.ucEnalbe == 0 && G_tLED.ucEnalbe == 0 )
        {
            GPIO_Start(&B_tLED, LED_BLED, 100, 1, 1);
            return 1;
        }
       
    }


    return 0;
}


static void EXTINT_IrqCallback(void)
{
    if (SET == EXTINT_GetExtIntStatus(RADAR1_INT_CH))
    {
        if(R_tLED.ucEnalbe == 0 && G_tLED.ucEnalbe == 0)
        {
           
            GPIO_Start(&B_tled1, BOARD_LED1, 100, 1, 1);
        }

        EXTINT_ClearExtIntStatus(RADAR1_INT_CH);
    }
}


void RADAR1_Ext_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_OFF;
    (void)GPIO_Init(RADAR1_INT_PORT, RADAR1_INT_PIN, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV1;
    stcExtIntInit.u32Edge        = EXTINT_TRIG_BOTH;
    (void)EXTINT_Init(RADAR1_INT_CH, &stcExtIntInit);

    /* IRQ sign-in */
    stcIrqSignConfig.enIntSrc = RADAR1_INT_SRC;
    stcIrqSignConfig.enIRQn   = RADAR1_INT_IRQn;
    stcIrqSignConfig.pfnCallback = &EXTINT_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    /* NVIC config */
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, RADAR1_INT_PRIO);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
}

static void EXTINT_RADAR2_IrqCallback(void)
{
    if (SET == EXTINT_GetExtIntStatus(RADAR2_INT_CH))
    {
        if(R_tLED.ucEnalbe == 0 && G_tLED.ucEnalbe == 0)
        {
            
            GPIO_Start(&B_tled1, BOARD_LED1, 100, 1, 1);
        }

        EXTINT_ClearExtIntStatus(RADAR2_INT_CH);
    }
}


void RADAR2_Ext_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_OFF;
    (void)GPIO_Init(RADAR2_INT_PORT, RADAR2_INT_PIN, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV1;
    stcExtIntInit.u32Edge        = EXTINT_TRIG_BOTH;
    (void)EXTINT_Init(RADAR2_INT_CH, &stcExtIntInit);

    /* IRQ sign-in */
    stcIrqSignConfig.enIntSrc = RADAR2_INT_SRC;
    stcIrqSignConfig.enIRQn   = RADAR2_INT_IRQn;
    stcIrqSignConfig.pfnCallback = &EXTINT_RADAR2_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    /* NVIC config */
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, RADAR2_INT_PRIO);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
}

/***************************** Armfly Electronics (END OF FILE) *********************************/
