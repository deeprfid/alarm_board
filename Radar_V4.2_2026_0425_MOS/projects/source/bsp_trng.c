
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"
uint32_t    m_u32Tickms;

uint32_t trng_create(void)
{
    uint32_t m_au32Random;
    /* Unlock peripherals or registers */

    /* Lock peripherals or registers */

    (void)TRNG_GenerateRandom(&m_au32Random, 1U);

    return m_au32Random;

}



void TrngConfig(void)
{


    /* Enable TRNG. */
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_TRNG, ENABLE);
    /* TRNG initialization configuration. */
    TRNG_Init(TRNG_SHIFT_CNT64, TRNG_RELOAD_INIT_VAL_ENABLE);

}

void data_denoising(uint8_t *inbuf, uint8_t *outbuf, uint32_t noise, uint8_t len)
{
    for(uint8_t i = 0; i < len; i++)
    {
        outbuf[i] = (noise ^ inbuf[i]) & 0xFF;
    }


}



void Check_UidKey(void)
{
    static uint32_t flag = 0, timecnt = 0;

    if(flag > m_u32Tickms || timecnt > m_u32Tickms)
    {
        flag = 0;
        timecnt = 0;
    }

    if(m_u32Tickms % 300 == 0 && m_u32Tickms > timecnt)
    {
        timecnt = m_u32Tickms;

    }

    if(m_u32Tickms % 50 == 0)
    {
        Radar_Led_update();
    }

    if(m_u32Tickms % 1000 == 0 && m_u32Tickms > flag)
    {
        uint32_t rngkey;
			  uint16_t uidkey;
        flag = m_u32Tickms;
        Check_alarm_status();
        Ucode_read(&rngkey, &uidkey);
    }

    WDT_FeedDog();
}


__WEAKDEF void BSP_CLK_Init(void)
{
    stc_clock_xtal_init_t     stcXtalInit;
    stc_clock_pll_init_t      stcMpllInit;

    GPIO_AnalogCmd(BSP_XTAL_PORT, BSP_XTAL_IN_PIN | BSP_XTAL_OUT_PIN, ENABLE);
    (void)CLK_XtalStructInit(&stcXtalInit);
    (void)CLK_PLLStructInit(&stcMpllInit);

    /* Set bus clk div. */
    CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | \
                                      CLK_PCLK2_DIV4 | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2));

    /* Config Xtal and enable Xtal */
    stcXtalInit.u8Mode = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8State = CLK_XTAL_ON;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;
    (void)CLK_XtalInit(&stcXtalInit);

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M). */
    stcMpllInit.PLLCFGR = 0UL;
    stcMpllInit.PLLCFGR_f.PLLM = 1UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLN = 50UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLP = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLQ = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLR = 2UL - 1UL;
    stcMpllInit.u8PLLState = CLK_PLL_ON;
    stcMpllInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&stcMpllInit);

    /* Wait MPLL ready. */
    while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL))
    {
        ;
    }

    /* sram init include read/write wait cycle setting */
    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM12 | SRAM_SRAM3 | SRAM_SRAMR), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);

    /* flash read wait cycle setting */
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    /* 3 cycles for 126MHz ~ 200MHz */
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);
    /* Switch driver ability */
    (void)PWC_HighSpeedToHighPerformance();
    /* Switch system clock source to MPLL. */
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
}

/**
 * @brief  BSP Xtal32 initialize.
 * @param  None
 * @retval int32_t:
 *         - LL_OK: XTAL32 enable successfully
 *         - LL_ERR_TIMEOUT: XTAL32 enable timeout.
 */
__WEAKDEF int32_t BSP_XTAL32_Init(void)
{
    stc_clock_xtal32_init_t stcXtal32Init;
    stc_fcm_init_t stcFcmInit;
    uint32_t u32TimeOut = 0UL;
    uint32_t u32Time = HCLK_VALUE / 5UL;

    if (CLK_XTAL32_ON == READ_REG8(CM_CMU->XTAL32CR))
    {
        /* Disable xtal32 */
        (void)CLK_Xtal32Cmd(DISABLE);
        /* Wait 5 * xtal32 cycle */
        DDL_DelayUS(160U);
    }

    /* Xtal32 config */
    (void)CLK_Xtal32StructInit(&stcXtal32Init);
    stcXtal32Init.u8State  = CLK_XTAL32_ON;
    stcXtal32Init.u8Drv    = CLK_XTAL32_DRV_MID;
    stcXtal32Init.u8Filter = CLK_XTAL32_FILTER_ALL_MD;
    GPIO_AnalogCmd(BSP_XTAL32_PORT, BSP_XTAL32_IN_PIN | BSP_XTAL32_OUT_PIN, ENABLE);
    (void)CLK_Xtal32Init(&stcXtal32Init);

    /* FCM config */
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_FCM, ENABLE);
    (void)FCM_StructInit(&stcFcmInit);
    stcFcmInit.u32RefClock       = FCM_REF_CLK_MRC;
    stcFcmInit.u32RefClockDiv    = FCM_REF_CLK_DIV8192;
    stcFcmInit.u32RefClockEdge   = FCM_REF_CLK_RISING;
    stcFcmInit.u32TargetClock    = FCM_TARGET_CLK_XTAL32;
    stcFcmInit.u32TargetClockDiv = FCM_TARGET_CLK_DIV1;
    stcFcmInit.u16LowerLimit     = (uint16_t)((XTAL32_VALUE / (MRC_VALUE / 8192U)) * 96UL / 100UL);
    stcFcmInit.u16UpperLimit     = (uint16_t)((XTAL32_VALUE / (MRC_VALUE / 8192U)) * 104UL / 100UL);
    (void)FCM_Init(&stcFcmInit);
    /* Enable FCM, to ensure xtal32 stable */
    FCM_Cmd(ENABLE);

    for (;;)
    {
        if (SET == FCM_GetStatus(FCM_FLAG_END))
        {
            FCM_ClearStatus(FCM_FLAG_END);

            if ((SET == FCM_GetStatus(FCM_FLAG_ERR)) || (SET == FCM_GetStatus(FCM_FLAG_OVF)))
            {
                FCM_ClearStatus(FCM_FLAG_ERR | FCM_FLAG_OVF);
            }
            else
            {
                (void)FCM_DeInit();
                FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_FCM, DISABLE);
                return LL_OK;
            }
        }

        u32TimeOut++;

        if (u32TimeOut > u32Time)
        {
            (void)FCM_DeInit();
            FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_FCM, DISABLE);
            return LL_ERR_TIMEOUT;
        }
    }
}


/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
