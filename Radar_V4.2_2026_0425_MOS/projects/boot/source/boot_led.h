/**
 * @file  boot_led.h
 * @brief Boot 的 LED 指示（本板没有可接 printf 的调试口，LED 是唯一的输出手段）
 *
 * 硬件：PB3 = 板载绿灯（bsp_led.h 的 BOARD_LED_GREEN_PORT/PIN），**高有效**：
 *       LED_G_ON() = GPIO_SetPins()、LED_G_OFF() = GPIO_ResetPins()。
 *       PB3 同时是 SWO 脚，所以初始化里要 GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE)。
 *
 * 编码（快闪 N 次表示一件事；长停用于分隔）：
 *       ┌ Boot 启动        : 闪 1 次
 *       ├ 跳转【槽 A】      : 闪 2 次  -> 随即跳转
 *       ├ 跳转【槽 B】      : 闪 3 次  -> 随即跳转
 *       └ 找不到可启动槽 halt: 反复「闪 4 次 + 停 1.2s」（读到 4 就是卡在 Boot）
 *
 * 现场判读：
 *       完全不见闪       -> Boot 自己没跑起来（烧录地址/启动文件问题）
 *       闪1 -> 闪2/3     -> 正常，已跳 App
 *       闪1 -> 循环闪4   -> Boot 跑了但两槽都不可用（卡在 boot_halt）
 */
#ifndef BOOT_LED_H
#define BOOT_LED_H

#include <stdint.h>

/* 初始化：把三个 LED(R PB3 / PA12 红 / PA11 蓝)全部配成输出并压灭，只留绿灯做编码。
 * 蜂鸣器由 TMRA 的 PWM 驱动(bsp_beep.c)，复位后本就静音，Boot 不处理。 */
void boot_led_init(void);
void boot_led_blink(uint8_t cnt);  /* 快闪 cnt 次（约 100ms 亮/灭） */
/* 闪 cnt 次 + 长停，重复 rounds 轮后返回；用在跳转前，让现场看得清槽号 */
void boot_led_signal(uint8_t cnt, uint8_t rounds);
void boot_led_code(uint8_t cnt);   /* 死循环：闪 cnt 次 + 停 1.2s，永不返回 */

/* 编码常量 */
#define BOOT_LED_BOOT      1u   /* Boot 已启动 */
#define BOOT_LED_JUMP_A    2u   /* 即将跳槽 A */
#define BOOT_LED_JUMP_B    3u   /* 即将跳槽 B */
#define BOOT_LED_HALT      4u   /* 找不到可启动槽 */

#endif /* BOOT_LED_H */
