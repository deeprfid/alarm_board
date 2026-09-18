/**
 * @file  boot_jump.h
 * @brief Boot 跳转到指定槽（向量表校验 + 交接前环境还原）
 *
 * 交接原则（借鉴 Decoder bootloader 的 fw_jump_helper.c run_app 做法）：
 *   **不把配好的 PLL 交出去** —— 跳转前把系统时钟退回默认 HRC 并关 PLL，
 *   让 App 从「近似复位态」自行初始化。这样避开了「Boot 配好的时钟 + App 再配一次」的耦合。
 */
#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

#include <stdint.h>

/* 槽向量表有效性：栈顶落在 SRAM 内 + 复位向量落在该槽内。返回 0 有效 */
int32_t boot_app_vector_valid(uint32_t slot);

/* 交接前还原运行环境（关中断 / 时钟退回 HRC / 关 PLL / 设 VTOR）。不返回 */
void    boot_prep_handoff(uint32_t slot);

/* 校验 + 交接 + 跳转；校验失败返回 -1，正常不返回 */
int32_t boot_jump_to(uint32_t slot);

#endif /* BOOT_JUMP_H */
