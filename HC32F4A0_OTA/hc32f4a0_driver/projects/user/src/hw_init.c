/* hw_init.c - hardware layer unified init entry (v9.81ci)
 * 从 main()/init_thread() 聚合硬件初始化，业务代码只调统一入口。
 * 纯函数聚合：逻辑与原有调用顺序完全一致。
 */
#include "hw_init.h"
#include "bsp.h"             /* 最前: CMSIS/__IO 就绪 + LL_PERIPH_WE/SWDT_FeedDog/Timera_init */
#include "hc32f46_driver.h"
#include "bsp_led.h"
#include "bsp_i2c_gpio.h"
#include "qspi_flash.h"
#include "rdr_cfg_kv.h"      /* v9.81s: KV 提前初始化 */
#include "timer.h"
#include "common.h"
#include "board.h"           /* BSP_CLK_Init */

/* ============ driver_hw_init: 上电硬件初始化 (main 内, RTOS 启动前) ============ */
void driver_hw_init(void)
{
    LL_PERIPH_WE(LL_PERIPH_ALL);
    SWDT_FeedDog();
    BSP_CLK_Init();
    GPIO_Configuration();
    Board_Rtc_init();
    bsp_Init_gpio();
    Timer2_init();
    Timera_init();
    TrngInitConfig();
    init_mem_sta();
}

/* ============ driver_hw_init_late: 堆初始化后的外设 (main 内, _init_alloc 之后) ============ */
void driver_hw_init_late(void)
{
    DDL_PrintfInit(BSP_PRINTF_DEVICE, BSP_PRINTF_BAUDRATE, BSP_PRINTF_Preinit);
    QSPI_FLASH_Init();
}

/* ============ driver_hw_rtos_init: RTOS 环境就绪后的硬件配置 (init_thread 内) ============
 * 返回: 0=网络就绪 / 1=网络失败(板卡无以太网) */
int driver_hw_rtos_init(void)
{
    /* v9.81s: FlashDB/KV 提前初始化（QSPI 已就绪）——配置读写从此走 KV，OTA 保配置 */
    rdr_cfg_kv_init();

    /* 板卡探测：外设类型探测失败则复位重试 */
    if (get_board_compos() != 0)
    {
        Spi_Ex_Code spiex = detect_spi_ex_dev();
        set_board_compos(spiex, Uart_Ex_None);
        sleep_ms(300);
        system_reset();
        return 1;   /* 不复位则返回失败 */
    }

    /* 网络初始化（若板卡有以太网） */
    if (get_spi_ex_dev() == Spi_Ex_Ethernet)
    {
        if (network_init(-1) != 0)
        {
            return 1;
        }
        /* DHCP 由调用方（init_thread）按 gIsConfDhcp 决定是否启动线程 */
    }
    return 0;
}
