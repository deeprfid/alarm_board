/**
 *******************************************************************************
 * @file  main.c
 * @brief HC32F460 报警板 Bootloader 入口
 *
 * 【回到量产路径】本文件的 SystemClockConfig() 与扫描板量产 bootloader(boot_iap) 逐行一致,
 * 只做了一处改名: EFM_CacheRamReset -> EFM_DataCacheResetCmd(共享 DDL Rev3.3.0 的命名)。
 * 8MHz 晶振(PH0/PH1)在本板确认已焊接, App 也一直靠它跑 PLL —— 故此处按量产路径配置:
 *     XTAL 8MHz -> MPLL 200MHz(8/1*50/2),
 *     并配套设置 SRAM 等待周期、Flash 等待周期(EFM_WAIT_CYCLE5)、GPIO 读等待(GPIO_RD_WAIT3)、
 *     切驱动能力(PWC_HighSpeedToHighPerformance)、切时钟源到 PLL、复位并开启 Cache。
 *
 * 不做的事情(有意):
 *   1) 不开 printf —— 本板没有可接 printf 的调试口(唯一串口是 RS485 业务口),
 *      日志发出去也没人收, 还会占用业务线。一切诊断走 LED(boot_led.c)。
 *   2) 不初始化任何业务外设 —— 把外设状态原样留给 App。
 *******************************************************************************
 */
#include "hc32_ll.h"
#include "boot_ota.h"

extern void SWDT_FeedDog(void);

#define BOOT_PERIPH_WE_SEL   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                              LL_PERIPH_EFM | LL_PERIPH_SRAM)

/* XTAL pins (PH0/PH1) */
#define BSP_XTAL_PORT        (GPIO_PORT_H)
#define BSP_XTAL_PIN         (GPIO_PIN_00 | GPIO_PIN_01)

/**
 * @brief  系统时钟：XTAL 8MHz -> MPLL 200MHz（与量产 boot_iap 完全一致）
 */
static void SystemClockConfig(void)
{
    stc_clock_xtal_init_t stcXtalInit;
    stc_clock_pll_init_t  stcMpllInit;

    GPIO_AnalogCmd(BSP_XTAL_PORT, BSP_XTAL_PIN, ENABLE);
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

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M): 8M/1*50/2 = 200M */
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
    SWDT_FeedDog();
    while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL)) {
        ;
    }
    SWDT_FeedDog();

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

    /* Reset cache ram */
    EFM_DataCacheResetCmd(ENABLE);
    EFM_DataCacheResetCmd(DISABLE);
    /* Enable cache */
    EFM_CacheCmd(ENABLE);
}

int32_t main(void)
{
    /* MCU Peripheral registers write unprotected */
    LL_PERIPH_WE(BOOT_PERIPH_WE_SEL);

    /* System clock: XTAL 8MHz -> MPLL 200MHz（与量产 boot_iap 一致） */
    SystemClockConfig();

    /* MCU Peripheral registers write protected */
    LL_PERIPH_WP(BOOT_PERIPH_WE_SEL);

    SWDT_FeedDog();

    /* 引导（不返回）：LED 指示 + 读标志 + 校验槽 + 跳转 / 停在 Boot */
    BOOT_OTA_Run();

    for (;;) {
        ;   /* 不应到达 */
    }
}

/*******************************************************************************
 * EOF
 ******************************************************************************/
