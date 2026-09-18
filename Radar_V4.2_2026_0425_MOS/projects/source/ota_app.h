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

#include <stdint.h>
#include "ota_layout.h"

/* 自检通过：把本槽置 RUNNABLE、清 NEED_CONFIRM、boot_count 归零；active 指向本槽。
 * slot 传本槽号（可用 OTA_SLOT_OF_ADDR(SCB->VTOR) 取）。返回 0 成功 */
int32_t ota_app_boot_confirm(uint32_t slot);

#endif /* OTA_APP_H */
