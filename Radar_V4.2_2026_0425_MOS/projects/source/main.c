
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"
#include "ota_layout.h"   /* OTA_SLOT_SIZE（槽对齐尺寸） */
#include "ota_app.h"        /* ota_app_boot_confirm */

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

    /* 防呆：编译宏必须与真实链接基址一致（不一致 = 该 target 的 Define 配错） */
    if (((uint32_t)(uint32_t)&main < (uint32_t)OTA_APP_BASE) ||
        ((uint32_t)(uint32_t)&main >= ((uint32_t)OTA_APP_BASE + OTA_SLOT_SIZE)))
    {
        for (;;) { }   /* 起不来比带病运行安全；用调试器看 PC 即可定位 */
    }

    LL_PERIPH_WE(LL_PERIPH_SEL);
    (void)BSP_CLK_Init();
    (void)Relay_gpio_init();
    (void)Board_LED_Init();
    (void)LED_GPIO_Init();
    (void)BEEP_InitHard();
    /* ===== 上电即把所有输出拉到【已知状态】=====
     * 三个板载 LED 全灭 + 三色 LED 全灭 + 蜂鸣器静音。
     *
     * 为什么必须做：现场要靠 LED 判读「Boot 的编码」和「App 的运行状态」，
     * 若上电初值不确定（残留电平/随机），灯就是乱的、根本数不清。
     *
     * 极性注意：三个板载 LED 并不一致 —— bsp_led.c 的 bsp_LedOn() 映射为
     *   BOARDLED_RED  -> GPIO_ResetPins  => 低=亮
     *   BOARDLED_BLUE -> GPIO_ResetPins  => 低=亮
     *   BOARDLED_GREEN-> GPIO_SetPins    => 高=亮
     * 所以这里【一律走既有接口】，由它们封装极性，不在此处裸写 GPIO（裸写必错）。 */
    bsp_LedOff(BOARDLED_RED);
    bsp_LedOff(BOARDLED_BLUE);
    bsp_LedOff(BOARDLED_GREEN);
    LED_R_OFF();
    LED_G_OFF();
    LED_B_OFF();
    BEEP_Stop();
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
    (void)ota_app_boot_confirm(OTA_SLOT_OF_ADDR(SCB->VTOR));

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
