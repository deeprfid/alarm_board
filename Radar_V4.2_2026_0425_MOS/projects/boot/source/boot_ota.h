/**
 *******************************************************************************
 * @file  boot_ota.h
 * @brief F460 Bootloader 引导（A/B 双槽、无搬运、选择器标志选槽）
 *
 * 布局与标志/槽镜像结构直接复用 App 侧 projects/source/ota_layout.h —— 全工程一套契约，
 * 不复制第二份（改动只需改那一个头文件）。
 *******************************************************************************
 */
#ifndef __BOOT_OTA_H__
#define __BOOT_OTA_H__

#include <stdint.h>
#include "../../source/ota_layout.h"   /* 布局 / ota_flag_t / 槽镜像头：单一真值 */

/* 引导入口（不返回） */
void BOOT_OTA_Run(void);

#endif /* __BOOT_OTA_H__ */
