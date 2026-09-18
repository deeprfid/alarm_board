/**
 * @file boot_main.c
 * @brief F4A0 Bootloader 入口：硬件初始化 -> 引导决策（升级/自检/跳 App）
 * @note 时钟配置参考官方 IAP 例子（PLLH 240MHz）；commit/回滚逻辑见 ota_boot.c
 */
#include <string.h>
#include "hc32f4xx.h"
#include "hc32_ll.h"
#include "hc32_ll_clk.h"
#include "hc32_ll_efm.h"
#include "hc32_ll_gpio.h"
#include "hc32_ll_pwc.h"
#include "hc32_ll_sram.h"
#include "hc32_ll_utility.h"
#include "boot_cfg.h"
#include "ota_boot.h"
#include "hc32_ll_usart.h"
#include <stdarg.h>
#include <stdio.h>

/* ---- boot 简易 UART1 打印（USART1 PB0/PB1 @115200，与 App 命令口一致） ---- */
static void boot_uart_init(void)
{
    stc_gpio_init_t stcGpioInit;
    stc_usart_uart_init_t stcInitCfg;
    USART_DeInit(CM_USART1);
    (void)GPIO_StructInit(&stcGpioInit);
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_00, &stcGpioInit);
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_01, &stcGpioInit);
    GPIO_SetFunc(GPIO_PORT_B, GPIO_PIN_00, GPIO_FUNC_32);   /* USART1_TX */
    GPIO_SetFunc(GPIO_PORT_B, GPIO_PIN_01, GPIO_FUNC_33);   /* USART1_RX */
    FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART1, ENABLE);
    (void)USART_UART_StructInit(&stcInitCfg);
    stcInitCfg.u32ClockDiv = USART_CLK_DIV1;   /* 与 App uart2.c 一致（StructInit 默认 DIV64 会导致波特率错误） */
    stcInitCfg.u32Baudrate = 115200UL;   /* v9.81al: boot 打印与 App 命令口统一 115200 */
    (void)USART_UART_Init(CM_USART1, &stcInitCfg, NULL);
    USART_FuncCmd(CM_USART1, (USART_TX | USART_RX), ENABLE);
}

void boot_putc(char c)
{
    volatile uint32_t i;
    while (SET != USART_GetStatus(CM_USART1, USART_FLAG_TX_EMPTY)) {
    }
    USART_WriteData(CM_USART1, (uint8_t)c);
    /* 等最后字节移位完成：115200 一字节 ~87us，TX_EMPTY 在移位开始即置位，
     * 连续写会溢出丢字。延时 40000 NOP @240MHz ≈ 167us > 87us，确保完整发出 */
    DDL_DelayUS(100);
}

void boot_uart_flush(void)
{
    volatile uint32_t i;
    while (SET != USART_GetStatus(CM_USART1, USART_FLAG_TX_EMPTY)) {
    }
    for (i = 0; i < 2000U; i++) {   /* 等最后一个字节移位完成 */
        __NOP();
    }
}

void boot_printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    char *p;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (p = buf; *p != '\0'; p++) {
        boot_putc(*p);
    }
}

/* ---- 时钟初始化（参考官方 IAP_CLK_Init：PLLH @240MHz） ---- */
static void boot_clk_init(void)
{
    stc_clock_xtal_init_t stcXtalInit;
    stc_clock_pll_init_t  stcPLLHInit;

    CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | CLK_PCLK2_DIV4 |
                                      CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2 | CLK_EXCLK_DIV2 | CLK_HCLK_DIV1));
    (void)CLK_XtalStructInit(&stcXtalInit);
    stcXtalInit.u8Mode       = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv        = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8State      = CLK_XTAL_ON;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;
    GPIO_AnalogCmd(GPIO_PORT_H, (GPIO_PIN_00 | GPIO_PIN_01), ENABLE);   /* XTAL 引脚 */
    (void)CLK_XtalInit(&stcXtalInit);

    (void)CLK_PLLStructInit(&stcPLLHInit);
    stcPLLHInit.u8PLLState  = CLK_PLL_ON;
    stcPLLHInit.PLLCFGR     = 0UL;
    stcPLLHInit.PLLCFGR_f.PLLM   = 1UL - 1UL;      /* 8MHz / 1 */
    stcPLLHInit.PLLCFGR_f.PLLN   = 120UL - 1UL;    /* x120 = 960MHz VCO */
    stcPLLHInit.PLLCFGR_f.PLLP   = 4UL - 1UL;      /* /4 = 240MHz */
    stcPLLHInit.PLLCFGR_f.PLLQ   = 4UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLR   = 4UL - 1UL;
    stcPLLHInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&stcPLLHInit);

    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM123 | SRAM_SRAM4 | SRAM_SRAMB), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);
    EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT4);
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
}

/* ---- 硬件初始化 ---- */
void boot_hw_init(void)
{
    /* 外设寄存器写保护解锁（EFM/FCG/GPIO/PWC/SRAM） */
    LL_PERIPH_WE(LL_PERIPH_EFM | LL_PERIPH_FCG | LL_PERIPH_GPIO |
                 LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_SRAM);
    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);

    boot_clk_init();
    boot_uart_init();
    boot_qspi_init();
	  DDL_DelayMS(10);
	  boot_printf("BOOT: start\n");
}

int main(void)
{
    boot_hw_init();
    boot_run();            /* 不返回（跳 App 或复位） */
    for (;;) {
    }
}
