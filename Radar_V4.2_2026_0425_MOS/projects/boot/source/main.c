/**
 *******************************************************************************
 * @file  main.c
 * @brief HC32F460 报警板 Bootloader 入口
 *
 * 【时钟：XTAL 8MHz -> MPLL 200MHz，与量产 boot_iap 逐行一致】
 *   来源：Scanner_20260901/boot_iap/source/main.c:43-96 的 SystemClockConfig()。
 *   本工程与它的差异只有两处【平台适配】，不涉及任何参数/顺序：
 *     ① 新 DDL 里 EFM_CacheRamReset(ENABLE/DISABLE) -> EFM_DataCacheResetCmd(ENABLE/DISABLE)；
 *     ② F460 的晶振是 IN/OUT 两个脚：GPIO_AnalogCmd 用 BOOT_XTAL_IN_PIN | BOOT_XTAL_OUT_PIN
 *        （取值同 BSP_XTAL_IN_PIN/OUT_PIN；boot_iap 那颗芯片只有单个 BSP_XTAL_PIN）。
 *   其余（含等 PLL 稳定前后的 SWDT_FeedDog()、总线分频、PLL 参数 8M/1*50/2=200M、
 *   SRAM/EFM/GPIO 等待周期、PWC 切换、cache 复位与使能）一字未改。
 *
 *   跳转前 boot_jump() 会调 boot_clock_deinit() 把时钟退回近复位态再交接，
 *   所以 200MHz 只存在于 Boot 自己这一程，App 仍从干净状态自行 BSP_CLK_Init()。
 *
 * 【开机时机】SystemClockConfig() 必须在 LL_PERIPH_WE 之后调用 —— 配 CMU/PWC 需要先解写保护
 *   （BOOT_PERIPH_WE_SEL 里的 LL_PERIPH_PWC_CLK_RMU 就是干这个的）。
 *
 * 【诊断手段】本板没有可接 printf 的调试口（唯一串口是 RS485 业务口），
 *   日志发出去也没人收、还占业务线。一切诊断走 LED（boot_led.c）。
 *******************************************************************************
 */
#include "hc32_ll.h"
#include "hc32_ll_clk.h"
#include "hc32_ll_sram.h"
#include "hc32_ll_pwc.h"
#include "hc32_ll_efm.h"
#include "boot_ota.h"

/* 晶振引脚 —— 取值与 BSP 头文件 ev_hc32f460_lqfp100_v2.h:238-240 完全一致
 *   （BSP_XTAL_PORT = GPIO_PORT_H / BSP_XTAL_IN_PIN = GPIO_PIN_01 / BSP_XTAL_OUT_PIN = GPIO_PIN_00）。
 * 为什么不直接 #include 那个头：它的全部内容被 #if (BSP_EV_HC32F460_LQFP100_V2 == BSP_EV_HC32F4XX)
 *   罩着，Boot 工程没有定义该宏；而用 Keil 的 <Define> 写 NAME=VALUE 会被汇编器拒绝
 *   （armasm 也吃这份 Define，报 A1137E），所以此处直接落值，避免为一个宏给工程加隐患。 */
#define BOOT_XTAL_PORT       (GPIO_PORT_H)
#define BOOT_XTAL_IN_PIN     (GPIO_PIN_01)
#define BOOT_XTAL_OUT_PIN    (GPIO_PIN_00)

#define BOOT_PERIPH_WE_SEL   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                              LL_PERIPH_EFM | LL_PERIPH_SRAM)

extern void SWDT_FeedDog(void);

/**
 * @brief  系统时钟：XTAL 8MHz -> MPLL 200MHz
 * @note   与量产 boot_iap 的 SystemClockConfig() 逐行一致，见文件头说明的两处平台适配。
 */
static void SystemClockConfig(void)
{
    stc_clock_xtal_init_t stcXtalInit;
    stc_clock_pll_init_t  stcMpllInit;

    GPIO_AnalogCmd(BOOT_XTAL_PORT, (BOOT_XTAL_IN_PIN | BOOT_XTAL_OUT_PIN), ENABLE);
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

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M): 8M/1*50/2=200M */
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
    /* 解外设寄存器写保护（配时钟要写 CMU/PWC，必须在这一步之后） */
    LL_PERIPH_WE(BOOT_PERIPH_WE_SEL);

    /* System clock: MPLL@200MHz（与量产 boot_iap 一致） */
    SystemClockConfig();

    /* 引导（不返回）：LED 指示 + 读标志 + 校验槽 + 跳转 / 停在 Boot */
    BOOT_OTA_Run();

    for (;;) {
        ;   /* 不应到达 */
    }
}

/*******************************************************************************
 * EOF
 ******************************************************************************/
