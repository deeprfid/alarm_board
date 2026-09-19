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

/* LED 编号（原代码里是裸的 2u/3u 魔法数字，极易看错，这里命名化）。
 * 【注意】绿灯的编号与 BOOT_SLOT_A 同值（0）—— 这是刻意的：led_on/led_off 直接用编号选灯。
 * 蓝灯目前已不用于任何状态，保留编号只为初始化时把它压灭。 */
#define BOOT_LED_GREEN       (0u)    /* PB3  高=亮（= BOOT_SLOT_A） */
#define BOOT_LED_RED         (2u)    /* PA12 低=亮 —— 槽B 指示 + Boot 卡住告警 */
#define BOOT_LED_BLUE        (3u)    /* PA11 低=亮 —— 当前未使用 */

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
    if (id == BOOT_LED_GREEN)   { GPIO_SetPins(LED_G_PORT, LED_G_PIN); }   /* 绿：高=亮 */
    else if (id == BOOT_LED_RED){ GPIO_ResetPins(LED_R_PORT, LED_R_PIN); } /* 红：低=亮 */
    else                        { GPIO_ResetPins(LED_B_PORT, LED_B_PIN); } /* 蓝：低=亮 */
}

static void led_off(uint8_t id)
{
    if (id == BOOT_LED_GREEN)   { GPIO_ResetPins(LED_G_PORT, LED_G_PIN); }
    else if (id == BOOT_LED_RED){ GPIO_SetPins(LED_R_PORT, LED_R_PIN); }
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

    led_off(BOOT_LED_GREEN);   /* 绿灭 */
    led_off(BOOT_LED_RED);     /* 红灭 */
    led_off(BOOT_LED_BLUE);    /* 蓝灭（当前不用，但必须压灭） */
}

void boot_led_slot(uint32_t slot)
{
    /* 槽A -> 绿灯；槽B -> 红灯（原为蓝灯，现场反馈蓝灯与 App 的雷达信号指示混淆，已改）。
     * 与 boot_led_error() 的区分见 boot_led.h：槽指示是「常亮 2 秒后跳走」，
     * 错误是「1s 亮/1s 灭无限循环、永不跳转」—— 形态不同，不会认错。 */
    uint8_t id = (slot == BOOT_SLOT_B) ? BOOT_LED_RED : BOOT_LED_GREEN;

    led_on(id);
    delay_fed_ms(BOOT_SLOT_HOLD_MS);
    led_off(id);
}

void boot_led_error(void)
{
    /* ICG 里同时配了 WDT/SWDT（hc32_ll_icg.h: ICG_REG_CFG0_CONST 含 ICG_REG_WDT_CONFIG | ICG_REG_SWDT_CONFIG），
     * 复位后可能已按 ICG 使能 —— 所以这个死循环里必须喂狗，否则会被狗咬复位、红灯闪到一半重启。 */
    for (;;) {
        led_on(BOOT_LED_RED);           /* 红亮 */
        delay_fed_ms(BOOT_ERR_ON_MS);
        led_off(BOOT_LED_RED);          /* 红灭 */
        delay_fed_ms(BOOT_ERR_OFF_MS);
    }
}
