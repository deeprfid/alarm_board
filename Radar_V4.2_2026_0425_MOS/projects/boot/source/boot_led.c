/**
 * @file  boot_led.c
 * @brief Boot 的 LED 指示实现（三灯，用「哪个灯亮」表示结果）
 */
#include "hc32_ll.h"
#include "hc32_ll_utility.h"   /* DDL_DelayMS：按实测 SystemCoreClock 计时 */
#include "boot_led.h"

extern void SWDT_FeedDog(void);

#define LED_G_PORT   (GPIO_PORT_B)   /* 绿 PB3  高=亮 */
#define LED_G_PIN    (GPIO_PIN_03)
#define LED_R_PORT   (GPIO_PORT_A)   /* 红 PA12 低=亮 */
#define LED_R_PIN    (GPIO_PIN_12)
#define LED_B_PORT   (GPIO_PORT_A)   /* 蓝 PA11 低=亮 */
#define LED_B_PIN    (GPIO_PIN_11)

#define BOOT_SLOT_HOLD_MS   2000u    /* 槽指示常亮时长 */
#define BOOT_ERR_ON_MS      1000u
#define BOOT_ERR_OFF_MS     1000u

/* 分片喂狗延时：凡是长于一个分片的等待都走这里。
 *
 * 【为什么必须这样】ICG 里同时配了 WDT/SWDT（hc32_ll_icg.h: ICG_REG_CFG0_CONST 含
 * ICG_REG_WDT_CONFIG | ICG_REG_SWDT_CONFIG），复位后可能已按 ICG 使能。原先 boot_led_slot()
 * 用一整段 DDL_DelayMS(2000) 干等 —— 这 2 秒里一次狗都没喂：只要 ICG 的 SWDT 溢出周期短于 2s，
 * 就会在亮灯中途被咬复位、循环重启，现场表现与「上电不运行」几乎一样，极难定位。
 * 改成每 100ms 喂一次，狗的溢出周期只要 > 100ms 就都安全。 */
#define BOOT_DELAY_SLICE_MS 100u

static void delay_fed_ms(uint32_t u32Ms)
{
    while (u32Ms != 0u) {
        uint32_t u32Slice = (u32Ms > BOOT_DELAY_SLICE_MS) ? BOOT_DELAY_SLICE_MS : u32Ms;

        SWDT_FeedDog();
        DDL_DelayMS(u32Slice);
        u32Ms -= u32Slice;
    }
}

/* 亮/灭各灯：极性不同，集中在这里，别处一律用这两个函数 */
static void led_on(uint8_t id)
{
    if (id == BOOT_SLOT_A)      { GPIO_SetPins(LED_G_PORT, LED_G_PIN); }   /* 绿：高=亮 */
    else if (id == 2u)          { GPIO_ResetPins(LED_R_PORT, LED_R_PIN); } /* 红：低=亮 */
    else                        { GPIO_ResetPins(LED_B_PORT, LED_B_PIN); } /* 蓝：低=亮 */
}

static void led_off(uint8_t id)
{
    if (id == BOOT_SLOT_A)      { GPIO_ResetPins(LED_G_PORT, LED_G_PIN); }
    else if (id == 2u)          { GPIO_SetPins(LED_R_PORT, LED_R_PIN); }
    else                        { GPIO_SetPins(LED_B_PORT, LED_B_PIN); }
}

void boot_led_init(void)
{
    stc_gpio_init_t stcInit;

    (void)GPIO_StructInit(&stcInit);
    stcInit.u16PinState = PIN_STAT_RST;
    stcInit.u16PinDir   = PIN_DIR_OUT;
    stcInit.u16PinDrv   = PIN_MID_DRV;

    /* PB3 同时是 SWO：本板不用 SWO，关掉才能当普通 IO */
    GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);

    (void)GPIO_Init(LED_G_PORT, LED_G_PIN, &stcInit);
    (void)GPIO_Init(LED_R_PORT, LED_R_PIN, &stcInit);
    (void)GPIO_Init(LED_B_PORT, LED_B_PIN, &stcInit);

    led_off(BOOT_SLOT_A);   /* 绿灭 */
    led_off(2u);            /* 红灭 */
    led_off(3u);            /* 蓝灭 */
}

void boot_led_slot(uint32_t slot)
{
    uint8_t id = (slot == BOOT_SLOT_B) ? 3u : BOOT_SLOT_A;   /* B->蓝(3), A->绿 */

    led_on(id);
    delay_fed_ms(BOOT_SLOT_HOLD_MS);
    led_off(id);
}

void boot_led_error(void)
{
    /* ICG 里同时配了 WDT/SWDT（hc32_ll_icg.h: ICG_REG_CFG0_CONST 含 ICG_REG_WDT_CONFIG | ICG_REG_SWDT_CONFIG），
     * 复位后可能已按 ICG 使能 —— 所以这个死循环里必须喂狗，否则会被狗咬复位、红灯闪到一半重启。 */
    for (;;) {
        led_on(2u);                     /* 红亮 */
        delay_fed_ms(BOOT_ERR_ON_MS);
        led_off(2u);                    /* 红灭 */
        delay_fed_ms(BOOT_ERR_OFF_MS);
    }
}
