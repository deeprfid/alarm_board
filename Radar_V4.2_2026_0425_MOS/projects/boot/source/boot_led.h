/**
 * @file  boot_led.h
 * @brief Boot 的 LED 指示 —— 极简：**用哪个灯亮表示结果，不数数**
 *
 * 硬件（bsp_led.h）：板载三灯，极性不同
 *     BOARD_LED_GREEN = PB3  高=亮
 *     BOARD_LED_RED   = PA12 低=亮
 *     BOARD_LED_BLUE  = PA11 低=亮
 * PB3 同时是 SWO 脚，初始化里要 GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE)。
 *
 * ===== 判读规则（一句话）=====
 *   跳转前会有一个灯「稳稳亮 2 秒」：  亮【绿】= 跳槽 A      亮【红】= 跳槽 B
 *   若看到【红灯】1 秒亮/1 秒灭一直闪  = 两槽都不可用，卡在 Boot
 *   三个灯都不动                      = Boot 没跑到 main
 *
 * 【槽B 为什么用红灯而不是蓝灯】现场反馈：蓝灯与 App 的雷达信号指示混在一起看不懂。
 *   红灯虽然与「卡在 Boot」同色，但两者形态完全不同、不会认错：
 *     槽指示   = 常亮 2 秒 -> 熄灭 -> 立刻跳转（App 随之起来，灯态交给 App）
 *     Boot 卡住 = 1 秒亮 / 1 秒灭【无限循环】，永不跳转
 *   即「亮 2 秒就走」vs「一直闪」。
 *   蓝灯现在不再用于任何状态，但初始化仍把它压灭（避免悬空/残留点亮）。
 *
 * 为什么不用「闪 N 次」的编码：那要现场数数、还要数轮次，极易看错 —— 本模块就是为此重写的。
 */
#ifndef BOOT_LED_H
#define BOOT_LED_H

#include <stdint.h>

#define BOOT_SLOT_A         0u
#define BOOT_SLOT_B         1u

/* 三灯初始化：全部配为输出并压灭 */
void boot_led_init(void);

/* 显示即将跳转的槽：槽A=绿灯常亮 2s；槽B=红灯常亮 2s。显示完自动灭 */
void boot_led_slot(uint32_t slot);

/* 卡住告警：红灯 1s 亮 / 1s 灭，**不返回**（内部喂狗） */
void boot_led_error(void);

#endif /* BOOT_LED_H */
