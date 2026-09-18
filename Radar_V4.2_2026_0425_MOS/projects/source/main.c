
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"
#include "ota_layout.h"   /* OTA_SLOT_SIZE（槽对齐尺寸） */
#include "ota_flash.h"      /* ota_app_boot_confirm / OTA_APP_ENABLE */

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

int32_t main(void)
{
    /* OTA 槽化：向量表指向本槽基址。
     * 用 per-target 编译宏 OTA_APP_BASE（槽A=0x8000 / 槽B=0x28000，见各 target 的 Define）。
     *
     * 【踩坑记录 · 上板实测抓到】原先写的是 ((uint32_t)&main) & ~(OTA_SLOT_SIZE-1)，
     * 前提是「槽按 128KB 对齐」—— 但槽 A=0x8000、槽 B=0x28000 都【不是】128KB 的倍数，
     * 掩码算出来是 0x0 / 0x20000：等于把向量表指到 Boot 自己或槽 A 内部，App 一启动就死。
     * 教训：槽基址必须显式给，绝不要用对齐掩码去「推导」。 */
    SCB->VTOR = (uint32_t)OTA_APP_BASE;
    __DSB();
    __ISB();


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
    (void)system_power_on();
    (void)radar_init();          /* 雷达三口(USART1/2/3, 逐字节RI中断收+轮询TXE发); 自适应由 radar_poll 推进 */
    /* ===== OTA 自检确认（A/B 无搬运）=====
     * 走到这里说明初始化全部完成 = 自检通过：把本槽置 RUNNABLE、清 NEED_CONFIRM、boot_count 归零,
     * 否则 Boot 会每 3 次启动就把本槽判 FAILED 并回退旧槽（升完会被判失败）。
     * 放在 WDT_Config()【之前】：ota_flag_write 要擦 8KB 标志扇区(约 20-30ms), 此时看门狗还没开,
     * 避免在擦写窗口里被狗咬。
     * 本槽号由 SCB->VTOR 反推 —— 与上面 VTOR 的设置同源, 不会不一致。 */
#if (OTA_APP_ENABLE != 0)
    (void)ota_app_boot_confirm(OTA_SLOT_OF_ADDR(SCB->VTOR));
#endif

    (void)WDT_Config();
	  LL_PERIPH_WP(LL_PERIPH_SEL);
    for (;;)
    {
        Check_Uart_Pdu();
        Check_alarm_state();
        radar_poll();            /* 雷达字节->分帧->解析(非阻塞) */
        Check_UidKey();

    }
}



/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
