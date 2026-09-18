/**
 * @file  boot_led.c
 * @brief Boot 的 LED 指示实现（直接操作 DDL GPIO，不依赖 BSP）
 */
#include "hc32_ll.h"
#include "hc32_ll_utility.h"   /* DDL_DelayMS：按实测 SystemCoreClock 计时，不依赖猜时钟频率 */
#include "boot_led.h"

#define BOOT_LED_PORT   (GPIO_PORT_B)
#define BOOT_LED_PIN    (GPIO_PIN_03)   /* 板载绿灯，高有效 */

/* 另外两个板载 LED（bsp_led.h: BOARD_LED_RED=PA12 / BOARD_LED_BLUE=PA11）。
 *
 * 【极性】三个板载 LED 并不一致 —— 依据 bsp_led.c 的 bsp_LedOn() 映射:
 *     BOARDLED_RED   -> GPIO_ResetPins  => 【低 = 亮】
 *     BOARDLED_BLUE  -> GPIO_ResetPins  => 【低 = 亮】
 *     BOARDLED_GREEN -> GPIO_SetPins    => 【高 = 亮】(PB3, 也就是本模块的诊断灯)
 * 故「压灭」红/蓝要拉【高】，压灭绿灯要拉【低】。搞反会把红蓝点亮 —— 我第一版就踩了这个坑。 */
#define BOOT_LED2_PORT  (GPIO_PORT_A)
#define BOOT_LED2_RED   (GPIO_PIN_12)   /* 低=亮, 故拉高为灭 */
#define BOOT_LED2_BLUE  (GPIO_PIN_11)   /* 低=亮, 故拉高为灭 */

/* 【不要再用忙等循环拍时间】
 * 原先这里是一个按「200MHz」估的循环计数，但事实是：
 *   system_hc32f460.c 的 SystemInit() 【并不配置时钟】，它只调用 SystemCoreClockUpdate()
 *   按当前 CMU 寄存器【测出】频率；而本 Boot 不配时钟，跑在 ICG 决定的 HRC(~20MHz) 上。
 * 于是那个循环快了约 10 倍 -> 闪得又快又糊, 现场根本数不清。
 *
 * 改用 DDL_DelayMS()：它基于实测的 SystemCoreClock 计时，
 * 因此无论 Boot 跑在什么频率上，灯的节拍都是设计值。 */
#define BOOT_LED_ON_MS     200u   /* 亮 200ms */
#define BOOT_LED_OFF_MS    200u   /* 灭 200ms */
#define BOOT_LED_GAP_MS   1500u   /* 轮间长灭 1.5s，便于分段数数 */

void boot_led_init(void)
{
    stc_gpio_init_t stcInit;

    (void)GPIO_StructInit(&stcInit);
    stcInit.u16PinState = PIN_STAT_RST;   /* 先灭 */
    stcInit.u16PinDir   = PIN_DIR_OUT;
    stcInit.u16PinDrv   = PIN_MID_DRV;

    /* PB3 同时是 SWO：本板不用 SWO，关掉它才能当普通 IO 用 */
    GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);

    /* 三个板载 LED 全部先压灭 —— 现场要能一眼数清绿灯的编码。
     * 注意极性不同: 绿灯拉低为灭, 红/蓝拉高为灭。 */
    (void)GPIO_Init(BOOT_LED_PORT,  BOOT_LED_PIN,   &stcInit);   /* PB3  绿 */
    (void)GPIO_Init(BOOT_LED2_PORT, BOOT_LED2_RED,  &stcInit);   /* PA12 红 */
    (void)GPIO_Init(BOOT_LED2_PORT, BOOT_LED2_BLUE, &stcInit);   /* PA11 蓝 */
    GPIO_ResetPins(BOOT_LED_PORT,  BOOT_LED_PIN);     /* 绿: 低 = 灭 */
    GPIO_SetPins(BOOT_LED2_PORT,   BOOT_LED2_RED);    /* 红: 高 = 灭 */
    GPIO_SetPins(BOOT_LED2_PORT,   BOOT_LED2_BLUE);   /* 蓝: 高 = 灭 */

    /* 蜂鸣器不处理：它由 TMRA 的 PWM 驱动(bsp_beep.c: BEEP_DISABLE = TMRA_Stop)，
     * 复位后 TMRA 是停的、Boot 也没启动它，故本来就是静音。 */
}

void boot_led_blink(uint8_t cnt)
{
    uint8_t i;

    for (i = 0u; i < cnt; i++) {
        GPIO_SetPins(BOOT_LED_PORT, BOOT_LED_PIN);
        DDL_DelayMS(BOOT_LED_ON_MS);
        GPIO_ResetPins(BOOT_LED_PORT, BOOT_LED_PIN);
        DDL_DelayMS(BOOT_LED_OFF_MS);
    }
}

/* 可读的槽号指示：闪 cnt 次 + 长停，重复 rounds 轮后返回。
 * 为什么需要：跳转后 App 会立刻接管这些灯，现场就没机会看清 Boot 给的编码了。 */
void boot_led_signal(uint8_t cnt, uint8_t rounds)
{
    uint8_t i;

    for (i = 0u; i < rounds; i++) {
        boot_led_blink(cnt);
        DDL_DelayMS(BOOT_LED_GAP_MS);
    }
}

void boot_led_code(uint8_t cnt)
{
    for (;;) {
        boot_led_blink(cnt);
        DDL_DelayMS(BOOT_LED_GAP_MS);
    }
}
