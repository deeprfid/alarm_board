/**
 * @file  ota_app.h
 * @brief App 侧 OTA 自检确认（A/B 无搬运：把本槽置 RUNNABLE）
 *
 * 为什么必须有：下载完成后 App 侧只做了「置 TRIAL + NEED_CONFIRM」。若 App 自己
 * 不在启动时确认，Boot 会每 3 次启动就把该槽判为 FAILED 并回退旧槽 ——
 * 功能上安全，但升完会被判失败。所以 App 走到「初始化完成」这一步就要确认。
 */
#ifndef OTA_APP_H
#define OTA_APP_H

/* ===== OTA 总开关 =====
 * 0 = 关闭：App 启动时【不】调用 ota_app_boot_confirm()，即【不擦写标志扇区】。
 *     此时 Boot 永远走兜底路径（按 A -> B 扫第一个可用槽），恒跳槽 A。
 *     用途：先单独验证「Boot 能否跳到 App」，把 OTA 这条链路的变量排除掉。
 * 1 = 打开：完整 OTA（App 自检后写标志把本槽置 RUNNABLE）。
 *
 * 为什么默认 0：这个调用会在启动早期擦写 0x7E000 标志扇区，是启动路径上唯一写 Flash 的动作；
 * 一旦它的执行代码没进 RAM，就会在擦写期间取指失败而把 CPU 卡死（已实测到）。
 * 所以先关掉它，把「跳转」这件事单独验证通过，再开回来。 */
#ifndef OTA_APP_ENABLE
#define OTA_APP_ENABLE   0
#endif

#include <stdint.h>
#include "ota_layout.h"

/* 自检通过：把本槽置 RUNNABLE、清 NEED_CONFIRM、boot_count 归零；active 指向本槽。
 * slot 传本槽号（可用 OTA_SLOT_OF_ADDR(SCB->VTOR) 取）。返回 0 成功 */
int32_t ota_app_boot_confirm(uint32_t slot);

#endif /* OTA_APP_H */
