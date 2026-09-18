#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"
#include "hc32f46_driver.h"
#include "dhcp.h"
#include "driverconfig.h"
#include "port.h"
#include "bsp.h"
#include "uart.h"
#include "board.h"
#include <rt_heap.h>
#include "bsp_led.h"
#include "qspi_flash.h"
#include "rdr_cfg_kv.h"   /* v9.81s: KV 提前初始化 */
#include "bsp_i2c_gpio.h"
#include "hw_init.h"   /* v9.81ci: driver_hw_init/driver_hw_rtos_init */

#define MY_MAX_DHCP_RETRY 3
volatile int gIsDhcpIpSet = 1;
extern volatile int gIsRunDhcpTimeHandler;

int dhcp_wait()
{
    uint8_t res = 0;
    int my_dhcp_retry = 0;
    TRACE("enter dhcp_func\n");
    gIsRunDhcpTimeHandler = 1;

    wiz_wait_link_on();

    while (1)
    {
        res = DHCP_run();

        switch (res)
        {
            case DHCP_IP_ASSIGN:
                break;

            case DHCP_IP_CHANGED:
                break;

            case DHCP_IP_LEASED:
                gIsDhcpIpSet = 1;
                return 0;

            case DHCP_FAILED:
                my_dhcp_retry++;

                if (my_dhcp_retry > MY_MAX_DHCP_RETRY)
                {
                    TRACE(">> DHCP %d Failed\r\n", my_dhcp_retry);
                    my_dhcp_retry = 0;
                    DHCP_stop();
                    set_default_ip();
                    gIsDhcpIpSet = 1;
                    return 0;
                }
                break;

            default:
                break;
        }

        sleep_ms(200);
    }
}

#if IS_RTOS2_SUPPORT
extern unsigned char Image$$RW_IRAM1$$ZI$$Limit;
extern int user_main_thstk_size;
extern osPriority_t user_main_priority;

void user_main(void *arg);

void dhcp_func(void *arg)
{
    uint8_t res = 0;
    int my_dhcp_retry = 0;
    TRACE("enter dhcp_func\n");
    gIsRunDhcpTimeHandler = 1;

    while (1)
    {
        res = DHCP_run();

        switch (res)
        {
            case DHCP_IP_ASSIGN:
                break;

            case DHCP_IP_CHANGED:
                break;

            case DHCP_IP_LEASED:
                gIsDhcpIpSet = 1;
                break;

            case DHCP_FAILED:
                my_dhcp_retry++;

                if (my_dhcp_retry > MY_MAX_DHCP_RETRY)
                {
                    TRACE(">> DHCP %d Failed\r\n", my_dhcp_retry);
                    my_dhcp_retry = 0;
                    DHCP_stop();
                    set_default_ip();
                    gIsDhcpIpSet = 1;
                    goto FIN;
                }
                break;

            default:
                break;
        }

        sleep_ms(200);
    }

FIN:
    free_hexp(arg);
}

const int dhcp_thstk_size = 1024;
extern int is_enable_fwupdate;
void firmware_upgrade_process(void *arg);
void broadcast_process(void* arg);
extern int gIsConfDhcp;
extern BoardComponents_ST gBoardCompos;

void init_thread(void *arg)
{
    WorkMode_Code wmode;
    osThreadAttr_t thAttr_t;
    osThreadId_t thid;

    /* v9.81ci: RTOS ????????????(KV/????/??) */
    if (driver_hw_rtos_init() != 0)
    {
        gBoardCompos.spi_ex = Spi_Ex_None;
    }
    TRACE("[fw] v0x%08X build, spi_ex:%d, gIsConfDhcp:%d\n", (unsigned)FW_VERSION_NUM, get_spi_ex_dev(), gIsConfDhcp);

    if (get_spi_ex_dev() == Spi_Ex_Ethernet && gIsConfDhcp == 1)
    {
        wiz_wait_link_on();
        gIsDhcpIpSet = 0;
        init_osThreadAttr_t(&thAttr_t, dhcp_thstk_size, osPriorityNormal);
        osThreadNew(dhcp_func, NULL, &thAttr_t);
    }

    init_osThreadAttr_t(&thAttr_t, user_main_thstk_size, user_main_priority);
    osThreadNew(user_main, NULL, &thAttr_t);

    if (get_spi_ex_dev() == Spi_Ex_Ethernet)
    {
        wmode = TestFwType_ex();
        TRACE("is_enable_fwupdate:%d,wmode:%d\n", is_enable_fwupdate, wmode);

        if (is_enable_fwupdate == 1 || (is_enable_fwupdate == 2 && wmode > WorkMode_Passive))
        {
            init_osThreadAttr_t(&thAttr_t, 8192, osPriorityNormal);   /* v9.82j: 1536B too small for HTTP+FlashDB deep chain */
            osThreadNew(firmware_upgrade_process, NULL, &thAttr_t);
            TRACE("firmware_upgrade_process thid:%p\n", thid);
        }

        TRACE("is_enable_fwupdate:%d\n", is_enable_fwupdate);
    }

    broadcast_process(NULL);
}

osRtxThread_t initth_tcb;
const osTimerAttr_t timer_One_Shot_Attr = {.name = "One_Shot_timer"};
osTimerId_t timerID_One_Shot = NULL;

void Softtimer_init(void)
{
    timerID_One_Shot = osTimerNew(timer_One_Shot,
                                  osTimerOnce,
                                  (void *)0,
                                  &timer_One_Shot_Attr);
}

void Uart_port_test(void);

int main(void)
{
    /* R3(2026-08-14): heap_base 动态跟随静态区末尾(Image$$RW_IRAM1$$ZI$$Limit),
       利用 SRAMH 高速区剩余 ~31KB；heap_top 保持 0x20060000(512KB SRAM 末端) */
    int heap_base_address = (int)(&Image$$RW_IRAM1$$ZI$$Limit);
    int heap_top_address  = (int)0x20060000;
    osThreadAttr_t thAttr_t;
    int init_thstk_size = 4096;   /* v9.81s-b: 1536→4096——init_thread 现含 FlashDB 初始化(flashdb+TSDB)+配置迁移，原 1.5KB 栈溢出 */
    
    /* v9.81ci: 硬件初始化统一入口 */
    driver_hw_init();

    heap_base_address += 64 - heap_base_address % 64;
    _init_alloc(heap_base_address, heap_top_address);
    /* v9.81ci: ????????????(???? + QSPI) */
    driver_hw_init_late();
    TRACE("app heap_base_address:%x, init_thstk_size:%p\n", heap_base_address, &init_thstk_size);
   // Uart_port_test();
    osKernelInitialize();
    Softtimer_init();
    init_osThreadAttr_t(&thAttr_t, init_thstk_size, osPriorityNormal);
    osThreadNew(init_thread, NULL, &thAttr_t);
    osKernelStart();

    return 0;
}

#endif
