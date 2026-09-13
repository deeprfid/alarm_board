
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

int32_t main(void)
{
    LL_PERIPH_WE(LL_PERIPH_SEL);
    (void)BSP_CLK_Init();
    (void)Relay_gpio_init();
    (void)Board_LED_Init();
    (void)LED_GPIO_Init();
    (void)BEEP_InitHard();
    (void)bsp_InitKey();
    (void)DMA_Config();
    (void)TMR0_Config(USART_TIMEOUT_BITS);
    (void)Uart4_int();
    (void)HashConfig();
    (void)TrngConfig();
    (void)Alarm_Off();
    (void)switch_decoder_init();
    (void)SysTick_Init(1000U);
    LL_PERIPH_WP(LL_PERIPH_SEL);
    system_power_on();
    (void)radar_init();          /* 雷达串口: USART1+DMA(波特率自适应由 radar_poll 推进) */
    (void)WDT_Config();
    for (;;)
    {
        Check_Uart_Pdu();
        Check_alarm_state();
        radar_poll();            /* 雷达字节->分帧->解析(非阻塞) */
        radar_dbg_poll();        /* 雷达调试输出(临时, RADAR_DBG_EN=0 时为空实现) */
        Check_UidKey();

    }
}



/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
