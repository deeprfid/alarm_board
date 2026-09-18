
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "bsp.h"
#include "hc32f46_driver.h"
#include "bsp_pwm.h"

uint32_t buzz_duty = 2;

void Bsp_pwm_init(void)
{
    if(buzz_duty == 0)
    {
        return;
    }

     LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU);

    /* MCU Peripheral registers write protected. */
   
    TMRA_DeInit(TMRA_PWM);
    TMRA_Stop(TMRA_PWM);
    TMRA_PWM_OutputCmd(TMRA_PWM, TMRA_PWM_CH, DISABLE);
    if(buzz_duty > 0 && buzz_duty <= 10)
    {
        TmrA4_CH4Config(buzz_duty * 300 - 1);
    }
    else
    {
        TmrA4_CH4Config(1500);
    }
    TMRA_Start(TMRA_PWM);
   
}

/**
 * @brief  TimerA configuration.
 * @param  None
 * @retval None
 */
void TmrA4_CH4Config(uint32_t duty)
{
    stc_tmra_init_t stcTmraInit;
    stc_tmra_pwm_init_t stcPwmInit;

    /* 1. Enable TimerA peripheral clock. */
    FCG_Fcg2PeriphClockCmd(TMRA_PERIPH_CLK, ENABLE);

    /* 2. Set a default initialization value for stcTmraInit. */
    (void)TMRA_StructInit(&stcTmraInit);

    /* 3. Modifies the initialization values depends on the application. */
    stcTmraInit.sw_count.u8CountMode = TMRA_MD;
    stcTmraInit.sw_count.u8CountDir  = TMRA_DIR;
	  stcTmraInit.sw_count.u8ClockDiv  = TMRA_CLK_DIV;
    stcTmraInit.u32PeriodValue = TMRA_PERIOD_VAL;
    (void)TMRA_Init(TMRA_PWM, &stcTmraInit);

    /* 4. Set the comparison reference value. */
    #if (APP_FUNC == APP_FUNC_NORMAL_SINGLE_PWM)
    (void)TMRA_PWM_StructInit(&stcPwmInit);
    stcPwmInit.u32CompareValue = duty;
    stcPwmInit.u16StartPolarity       = TMRA_PWM_HIGH;
    stcPwmInit.u16StopPolarity        = TMRA_PWM_LOW;
    stcPwmInit.u16PeriodMatchPolarity = TMRA_PWM_HIGH;
    GPIO_SetFunc(TMRA_PWM_PORT, TMRA_PWM_PIN, TMRA_PWM_PIN_FUNC);
    (void)TMRA_PWM_Init(TMRA_PWM, TMRA_PWM_CH, &stcPwmInit);
    TMRA_PWM_OutputCmd (TMRA_PWM, TMRA_PWM_CH, ENABLE);

    #elif (APP_FUNC == APP_FUNC_SINGLE_EDGE_ALIGNED_PWM)
    (void)TMRA_PWM_StructInit(&stcPwmInit);
    stcPwmInit.u32CompareValue = TMRA_PWMX_CMP_VAL;
    GPIO_SetFunc(TMRA_PWMX_PORT, TMRA_PWMX_PIN, TMRA_PWMX_PIN_FUNC);
    (void)TMRA_PWM_Init(TMRA_UNIT, TMRA_PWMX_CH, &stcPwmInit);
    TMRA_PWM_OutputCmd(TMRA_UNIT, TMRA_PWMX_CH, ENABLE);

    (void)TMRA_PWM_StructInit(&stcPwmInit);
    stcPwmInit.u32CompareValue = TMRA_PWMY_CMP_VAL;
    GPIO_SetFunc(TMRA_PWMY_PORT, TMRA_PWMY_PIN, TMRA_PWMY_PIN_FUNC);
    (void)TMRA_PWM_Init(TMRA_UNIT, TMRA_PWMY_CH, &stcPwmInit);
    TMRA_PWM_OutputCmd(TMRA_UNIT, TMRA_PWMY_CH, ENABLE);

    #elif (APP_FUNC == APP_FUNC_TOW_EDGE_SYMMETRIC_PWM)
    (void)TMRA_PWM_StructInit(&stcPwmInit);
    stcPwmInit.u32CompareValue        = TMRA_PWMX_CMP_VAL;
    stcPwmInit.u16StartPolarity       = TMRA_PWM_HIGH;
    stcPwmInit.u16StopPolarity        = TMRA_PWM_HIGH;
    stcPwmInit.u16PeriodMatchPolarity = TMRA_PWM_HOLD;
    GPIO_SetFunc(TMRA_PWMX_PORT, TMRA_PWMX_PIN, TMRA_PWMX_PIN_FUNC);
    (void)TMRA_PWM_Init(TMRA_UNIT, TMRA_PWMX_CH, &stcPwmInit);
    TMRA_PWM_OutputCmd(TMRA_UNIT, TMRA_PWMX_CH, ENABLE);

    stcPwmInit.u32CompareValue  = TMRA_PWMY_CMP_VAL;
    stcPwmInit.u16StartPolarity = TMRA_PWM_LOW;
    stcPwmInit.u16StopPolarity  = TMRA_PWM_LOW;
    GPIO_SetFunc(TMRA_PWMY_PORT, TMRA_PWMY_PIN, TMRA_PWMY_PIN_FUNC);
    (void)TMRA_PWM_Init(TMRA_UNIT, TMRA_PWMY_CH, &stcPwmInit);
    TMRA_PWM_OutputCmd(TMRA_UNIT, TMRA_PWMY_CH, ENABLE);
    #endif
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
