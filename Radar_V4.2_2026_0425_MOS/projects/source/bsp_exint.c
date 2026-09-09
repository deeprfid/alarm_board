
#include "main.h"

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

static void EXTINT_IrqCallback(void);
static void AICAM_INT_IrqCallback(void);




extern LED_T R_tLED;
extern LED_T G_tLED;
extern LED_T B_tLED;
extern LED_T Radar_LED;
extern LED_T Board_LED_1;
extern LED_T Board_LED_2;

#if 0 /* old timer-blink helper, kept with disabled legacy logic */
static void led_blink_update(LED_T *led, uint8_t id, uint8_t active, uint16_t cadence)
{
    if (active) { LED_Start(led, id, cadence, 1, 1); }
    else        { Led_Stop(led, id); }
}
#endif

en_pin_state_t Radar_Led_update(void)
{
    en_pin_state_t aicamsingal = switch_decoder_pio_read(AI_CAMERA);
    uint8_t  radar_on  = bsp_get_radar_singal();
    uint8_t  aicam_on  = (PIN_RESET == aicamsingal) ? 1u : 0u;
    uint8_t  presence  = (radar_on || aicam_on) ? 1u : 0u;

#if 0 /* ---- OLD: software-timer blink via LED_Start/Led_Stop (kept, disabled) ---- */

    /* Alarm active (R/G LED running): reflect presence on Radar_LED */
    if ((R_tLED.ucEnalbe == 1) || (G_tLED.ucEnalbe == 1))
    {
        if (presence) { LED_Start(&Radar_LED, RADARLED, 100, 1, 1); }
        return aicamsingal;
    }

    /* Normal mode: behavior selected by installation switches */
    {
        en_pin_state_t p_Aicammode = switch_decoder_pio_read(AICAM_MODE);
        en_pin_state_t p_Radarmode = switch_decoder_pio_read(RADAR_MODE);
        en_pin_state_t p_Easmode   = switch_decoder_pio_read(EAS_MODE);
        en_pin_state_t p_light     = switch_decoder_pio_read(LIGHT_ON);

        if ((p_Aicammode == PIN_RESET) && (p_Radarmode == PIN_RESET))
        {
            led_blink_update(&Radar_LED, RADARLED, presence, 100);
            led_blink_update(&B_tLED, LED_BLED, presence, 100);
            return aicamsingal;
        }

        if ((p_Aicammode == PIN_RESET) && (p_Radarmode == PIN_SET))
        {
            led_blink_update(&Radar_LED, RADARLED, aicam_on, 100);
            led_blink_update(&B_tLED, LED_BLED, aicam_on, 100);
            return aicamsingal;
        }

        if ((p_Radarmode == PIN_RESET) && (p_Aicammode == PIN_SET))
        {
            led_blink_update(&Radar_LED, RADARLED, radar_on, 300);
            led_blink_update(&B_tLED, LED_BLED, radar_on, 300);
            return aicamsingal;
        }

        if ((p_Easmode == PIN_RESET) && (p_Aicammode == PIN_SET) && (p_Radarmode == PIN_SET))
        {
            LED_Start(&B_tLED, LED_BLED, 100, 1, 1);
            if (presence)
            {
                LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
                LED_Start(&B_tLED, LED_BLED, 100, 1, 1);
            }
            return aicamsingal;
        }

        if ((p_light == PIN_RESET) && (p_Aicammode == PIN_SET) && (p_Radarmode == PIN_SET))
        {
            LED_Start(&B_tLED, LED_BLED, 100, 1, 1);
            if (presence)
            {
                LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
                LED_Start(&B_tLED, LED_BLED, 100, 1, 1);
            }
            return aicamsingal;
        }
    }

    return aicamsingal;

#else /* ---- NEW: direct IO control (no LED_Start/Led_Stop) ---- */

    /* R/G alarm LEDs have priority: during alarm the Alarm_* funcs own the LEDs */
    if ((R_tLED.ucEnalbe == 1) || (G_tLED.ucEnalbe == 1))
    {
        return aicamsingal;
    }

    /* stop any leftover software blink on the two indicator LEDs so LED_Pro cannot fight us */
    if (B_tLED.ucEnalbe != 0u)     { Led_Stop(&B_tLED, LED_BLED); }
    if (Radar_LED.ucEnalbe != 0u)  { Led_Stop(&Radar_LED, RADARLED); }

    /* Blue LED: LIGHT_ON -> always on; else follow presence (radar/camera/EAS) */
    if (PIN_RESET == switch_decoder_pio_read(LIGHT_ON))
    {
        LED_B_ON();
    }
    else
    {
        if (presence) { LED_B_ON(); }
        else          { LED_B_OFF(); }
    }

    /* Board small Radar_LED: follow presence */
    if (presence) { bsp_LedOn(RADARLED); }
    else          { bsp_LedOff(RADARLED); }

#endif

    return aicamsingal;
}

static void EXTINT_IrqCallback(void)
{
    if (SET == EXTINT_GetExtIntStatus(EXTINT_CH))
    {

        if(PIN_SET == GPIO_ReadInputPins(EXINT_PORT, EXINT_PIN))
        {

        }
        else
        {

        }

        EXTINT_ClearExtIntStatus(EXTINT_CH);
    }
}


void Ext_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_OFF;
    (void)GPIO_Init(EXINT_PORT, EXINT_PIN, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV1;
    stcExtIntInit.u32Edge        = EXTINT_TRIG_BOTH;
    (void)EXTINT_Init(EXTINT_CH, &stcExtIntInit);

    /* IRQ sign-in */
    stcIrqSignConfig.enIntSrc = EXINT_SRC;
    stcIrqSignConfig.enIRQn   = EXINT_IRQn;
    stcIrqSignConfig.pfnCallback = &EXTINT_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    /* NVIC config */
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, EXINT_PRIO);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
}


void ALARM_INT_Init(void)
{
    stc_extint_init_t stcExtIntInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    stc_gpio_init_t stcGpioInit;

    /* GPIO config */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16ExtInt = PIN_EXTINT_ON;
    stcGpioInit.u16PullUp = PIN_PU_ON;
    (void)GPIO_Init(AICAM_PORT, AICAM_PIN1, &stcGpioInit);

    /* ExtInt config */
    (void)EXTINT_StructInit(&stcExtIntInit);
    stcExtIntInit.u32Filter      = EXTINT_FILTER_ON;
    stcExtIntInit.u32FilterClock = EXTINT_FCLK_DIV1;
    stcExtIntInit.u32Edge        = EXTINT_TRIG_FALLING;
    (void)EXTINT_Init(AICAM_CH1, &stcExtIntInit);

    /* IRQ sign-in */
    stcIrqSignConfig.enIntSrc = AICAM_SRC1;
    stcIrqSignConfig.enIRQn   = AICAM_IRQn1;
    stcIrqSignConfig.pfnCallback = &AICAM_INT_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);

    /* NVIC config */
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, EXINT_PRIO);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);
}


static void AICAM_INT_IrqCallback(void)
{

    EXTINT_ClearExtIntStatus(AICAM_CH1);
}

extern uint32_t    m_u32Tickms;
void SysTick_Handler(void)
{

    m_u32Tickms++;
    bsp_RunPer10ms();
    bsp_KeyScan10ms();


    __DSB();  /* Arm Errata 838869 */
}

void bsp_RunPer10ms(void)
{
    static uint8_t s_count = 0;

    if (++s_count >= 10)
    {
        s_count = 0;
        BEEP_Pro();
        Led_status_update();
        Relay_status_check();
    }
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
