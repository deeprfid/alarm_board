/**
 * @file  boot_led.c
 * @brief Boot 的 LED 指示实现（直接操作 DDL GPIO，不依赖 BSP）
 */
#include "hc32_ll.h"
#include "boot_led.h"

#define BOOT_LED_PORT   (GPIO_PORT_B)
#define BOOT_LED_PIN    (GPIO_PIN_03)   /* 板载绿灯，高有效 */

/* 另外两个 LED（bsp_led.h 的 BOARD_LED_RED/BLUE_PORT/PIN）： Boot 要把它们一起压灭，
 * 否则 App 的 Board_LED_Init() 留下的「红+蓝常亮」会让绿灯编码数不清。
 * 极性口径同 bsp_led.h: LED_x_ON = GPIO_SetPins -> 高有效，故【低 = 灭】。 */
#define BOOT_LED2_PORT  (GPIO_PORT_A)
#define BOOT_LED2_RED   (GPIO_PIN_12)
#define BOOT_LED2_BLUE  (GPIO_PIN_11)

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

    /* 三个 LED 全部先压灭 —— 现场要能一眼数清绿灯的编码 */
    (void)GPIO_Init(BOOT_LED_PORT,  BOOT_LED_PIN,   &stcInit);   /* PB3  绿 */
    (void)GPIO_Init(BOOT_LED2_PORT, BOOT_LED2_RED,  &stcInit);   /* PA12 红 */
    (void)GPIO_Init(BOOT_LED2_PORT, BOOT_LED2_BLUE, &stcInit);   /* PA11 蓝 */
    GPIO_ResetPins(BOOT_LED_PORT,  BOOT_LED_PIN);
    GPIO_ResetPins(BOOT_LED2_PORT, BOOT_LED2_RED);
    GPIO_ResetPins(BOOT_LED2_PORT, BOOT_LED2_BLUE);

    /* 蜂鸣器不处理：它由 TMRA 的 PWM 驱动(bsp_beep.c: BEEP_DISABLE = TMRA_Stop)，
     * 复位后 TMRA 是停的、Boot 也没启动它，故本来就是静音。 */
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
