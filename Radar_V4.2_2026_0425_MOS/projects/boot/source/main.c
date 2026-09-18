/**
 *******************************************************************************
 * @file  main.c
 * @brief HC32F460 报警板 Bootloader 入口
 *
 * 【为什么这里不配时钟】
 *   曾按扫描板量产 boot_iap 的做法加上 SystemClockConfig()（XTAL 8MHz -> MPLL 200MHz），
 *   但那版在本板【上电不运行】（绿灯都不亮）。已逐项核对过：
 *     - XTAL 引脚一致（GPIO_PORT_H / PIN_00|PIN_01，见 BSP 头文件）
 *     - 配置顺序与参数与 App 的 BSP_CLK_Init() 实质完全相同（同一套 CLK_XtalInit/CLK_PLLInit 参数、
 *       同样等 CLK_STB_FLAG_PLL 稳定、同样 SRAM/EFM/GPIO 等待周期与 PWC 切换）
 *   仍无法从代码上看出一致性之外的差异 —— 故【回退】到本版本（不配时钟，跑复位默认 HRC）。
 *   该版本在现场实测可用：绿灯亮 2 秒 -> 跳槽 A -> App 正常起来。
 *   要再尝试量产时钟配置时，应【分步加回并每步上板验证】，不要一次全加。
 *
 * 【本版本口径】
 *   1) 不配时钟：跑 ICG 决定的复位默认时钟（HRC，约 20MHz）。
 *      App 自己会 BSP_CLK_Init() 配到 PLL 200MHz；跳转前 boot_jump() 会把时钟退回默认态再交接。
 *   2) 不开 printf：本板没有可接 printf 的调试口（唯一串口是 RS485 业务口），
 *      日志发出去也没人收、还占业务线。一切诊断走 LED（boot_led.c）。
 *   3) 不初始化任何业务外设：把外设状态原样留给 App。
 *******************************************************************************
 */
#include "hc32_ll.h"
#include "boot_ota.h"

#define BOOT_PERIPH_WE_SEL   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                              LL_PERIPH_EFM | LL_PERIPH_SRAM)

int32_t main(void)
{
    /* 只解外设寄存器写保护（GPIO/EFM 要用），不做任何时钟/串口配置 */
    LL_PERIPH_WE(BOOT_PERIPH_WE_SEL);

    /* 引导（不返回）：LED 指示 + 读标志 + 校验槽 + 跳转 / 停在 Boot */
    BOOT_OTA_Run();

    for (;;) {
        ;   /* 不应到达 */
    }
}

/*******************************************************************************
 * EOF
 ******************************************************************************/
