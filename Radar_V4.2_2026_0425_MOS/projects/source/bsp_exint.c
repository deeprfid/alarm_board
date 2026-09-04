
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

en_pin_state_t Radar_Led_update(void)
{

    en_pin_state_t aicamsingal = switch_decoder_pio_read(AI_CAMERA);



    if(R_tLED.ucEnalbe == 1 || G_tLED.ucEnalbe == 1 )
    {
        uint8_t	radarsingal = bsp_get_radar_singal();

        if(radarsingal || PIN_RESET == aicamsingal)
        {
            LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
        }

        return aicamsingal;
    }
   if(R_tLED.ucEnalbe == 0 && G_tLED.ucEnalbe == 0 )
    {

        en_pin_state_t p_Aicammode   = switch_decoder_pio_read(AICAM_MODE);
        en_pin_state_t p_Radarmode   = switch_decoder_pio_read(RADAR_MODE);
        en_pin_state_t p_Easmode     = switch_decoder_pio_read(EAS_MODE);
        en_pin_state_t p_light       = switch_decoder_pio_read(LIGHT_ON);

        uint8_t	radarsingal = bsp_get_radar_singal();


        if(p_Aicammode == PIN_RESET && p_Radarmode == PIN_RESET)	 //BLUE LED controled by CAM & Radar
        {
            if(radarsingal || PIN_RESET == aicamsingal)
            {
							// if(B_tLED.ucEnalbe ==0)
							 { 
                LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
                LED_Start(&B_tLED, LED_BLED   , 100, 1, 1);
							 }	 
            }
						else
						{
						    Led_Stop(&Radar_LED, RADARLED);
							  Led_Stop(&B_tLED   , LED_BLED);
						}	
            return aicamsingal;
        }

        if( p_Aicammode == PIN_RESET && p_Radarmode == PIN_SET)    //BLUE LED trig by AICAM ONLY
        {
            if(PIN_RESET == aicamsingal)
            {
							// if(B_tLED.ucEnalbe ==0)
							 { 
               LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
               LED_Start(&B_tLED, LED_BLED   , 100, 1, 1);
							 }	 
            }
						else
						{
						    Led_Stop(&Radar_LED, RADARLED);
							  Led_Stop(&B_tLED   , LED_BLED);
						}	
            return aicamsingal;
        }
        if( p_Radarmode == PIN_RESET && p_Aicammode == PIN_SET)   //BLUE LED trig by Radar ONLY
        {
            if(radarsingal)
            {
               // if(B_tLED.ucEnalbe ==0)
							 { 
                LED_Start(&Radar_LED, RADARLED, 300, 1, 1);
                LED_Start(&B_tLED, LED_BLED   , 300, 1, 1);
							 }	 
						}	

            return aicamsingal;
        }

        if((p_Easmode == PIN_RESET) &&  ((p_Aicammode == PIN_SET) && (p_Radarmode == PIN_SET))) // EAS ONLY,CAM & Radar NOT INSTALLED
        {
             LED_Start(&B_tLED, LED_BLED   , 100, 1, 1); //

            if(radarsingal || PIN_RESET == aicamsingal)
            {
             //  if(B_tLED.ucEnalbe ==0)
							 { 
                LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
                LED_Start(&B_tLED, LED_BLED   , 100, 1, 1);
							 }	 
						}	


            return aicamsingal;
        }

        if((p_light == PIN_RESET) &&  ((p_Aicammode == PIN_SET) && (p_Radarmode == PIN_SET)))
        {
           LED_Start(&B_tLED, LED_BLED   , 100, 1, 1);
					
           if(radarsingal || PIN_RESET == aicamsingal)
            {
              // if(B_tLED.ucEnalbe ==0)
							 { 
                LED_Start(&Radar_LED, RADARLED, 100, 1, 1);
                LED_Start(&B_tLED, LED_BLED   , 100, 1, 1);
							 }	 
						}	
						

            return aicamsingal;

        }

    }



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
