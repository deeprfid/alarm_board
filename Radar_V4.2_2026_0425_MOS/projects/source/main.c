
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"
#include "ota_layout.h"     /* OTA_SLOT_SIZE（槽对齐尺寸）/ SCB->VTOR 反推槽号 */
#include "ota_flash.h"      /* ota_app_boot_confirm / OTA_APP_ENABLE */
#include "ota_recv.h"       /* ota_recv_result / ota_recv_exit */

/**
 * @brief  OTA 收完后的落地动作（每轮主循环调一次，开销仅读一个变量）
 *
 *   ota_recv_finish() 已把标志写成「active = 新槽 + TRIAL + NEED_CONFIRM」并阻塞发完最后一个 ACK，
 *   但设备【不会自己重启】—— 复位必须在这里做，否则新槽永远不会被 Boot 拉起。
 *   ACK 是 ota_recv_finish() 里用阻塞发送发完的（字节已经在线上了），所以这里【不等】直接复位：
 *   DDL_DelayMS 在 DDL 里是用 SysTick 实现的，而本 App 自己也拿 SysTick 做 1ms 心跳
 *   （bsp_timer.c），两者会打架 —— 为一次复位前的等待去动心跳计时不值得。
 *
 *   失败则清结果并【退出升级态】，业务通信才回得来。
 *
 * 【不受 OTA_APP_ENABLE 门控】下载链路本身是无条件接线的（见 common.c 的 ota_recv_sniff/feed），
 *   所以落地动作也必须无条件在，否则一次下载会卡在 DONE 状态、既不激活也不回业务。
 */
static void OTA_Housekeeping(void)
{
    switch (ota_recv_result())
    {
        case OTA_RX_RESULT_OK:
            NVIC_SystemReset();     /* 复位 -> Boot 读标志 -> 拉起新槽（TRIAL） */
            break;

        case OTA_RX_RESULT_FAIL:
            ota_recv_result_clear();
            ota_recv_exit();
            break;

        default:
            break;
    }
}

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
        OTA_Housekeeping();      /* OTA 收完 -> 复位激活；失败 -> 退回业务态 */

    }
}



/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
