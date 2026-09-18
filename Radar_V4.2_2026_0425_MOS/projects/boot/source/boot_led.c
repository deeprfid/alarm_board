/**
 * @file  boot_led.c
 * @brief Boot 的 LED 指示实现（直接操作 DDL GPIO，不依赖 BSP）
 */
#include "hc32_ll.h"
#include "boot_led.h"

#define BOOT_LED_PORT   (GPIO_PORT_B)
#define BOOT_LED_PIN    (GPIO_PIN_03)   /* 板载绿灯，高有效 */

/* 粗略延时：Boot 跑在 SystemInit 配好的 200MHz 上，一次循环约数百 ns */
static void boot_wait(volatile uint32_t u32Loop)
{
    while (u32Loop-- != 0UL) {
        __NOP();
    }
}

void boot_led_init(void)
{
    stc_gpio_init_t stcInit;

    (void)GPIO_StructInit(&stcInit);
    stcInit.u16PinState = PIN_STAT_RST;   /* 先灭 */
    stcInit.u16PinDir   = PIN_DIR_OUT;
    stcInit.u16PinDrv   = PIN_MID_DRV;

    /* PB3 同时是 SWO：本板不用 SWO，关掉它才能当普通 IO 用 */
    GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);

    (void)GPIO_Init(BOOT_LED_PORT, BOOT_LED_PIN, &stcInit);
    GPIO_ResetPins(BOOT_LED_PORT, BOOT_LED_PIN);
}

void boot_led_blink(uint8_t cnt)
{
    uint8_t i;

    for (i = 0u; i < cnt; i++) {
        GPIO_SetPins(BOOT_LED_PORT, BOOT_LED_PIN);
        boot_wait(120000UL);            /* 约 100ms 亮 */
        GPIO_ResetPins(BOOT_LED_PORT, BOOT_LED_PIN);
        boot_wait(120000UL);            /* 约 100ms 灭 */
    }
}

void boot_led_code(uint8_t cnt)
{
    for (;;) {
        boot_led_blink(cnt);
        boot_wait(1500000UL);           /* 约 1.2s 长停，便于数数 */
    }
}
