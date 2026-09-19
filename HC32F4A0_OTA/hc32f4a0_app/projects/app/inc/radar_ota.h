/**
 * @file  radar_ota.h
 * @brief F4A0 -> F460 报警板固件分发：从 FAT 卷读 RADAR.BIN，经 RS485 下发
 *
 * 【与 usb_msc_ota.c 的分工 —— 别搞混】两者都从 MSC 的 FAT 卷取文件，但目标完全不同：
 *     FW.BIN    -> F4A0 【自己】升级：暂存到 QSPI -> 写标志 -> 重启 -> bootloader 搬移
 *     RADAR.BIN -> F460 【报警板】升级：本文件，直接把 OTA1 帧写到 RS485 上，不碰本机 Flash
 *   用不同文件名区分是刻意的：同一个文件被两边抢会导致「插上盘就重启、板子却升不上去」。
 *
 * 口径（docs/ota_boot_design.md v0.2 §0）：
 *   分发期间关闭全部业务帧、链路独占、只跑固件下载。闸门是【全局】的
 *   （ota_dist_busy()），业务发送路径见 alarm.c / mqtt_interface.c 的 ipc_hpm_message。
 */
#ifndef RADAR_OTA_H
#define RADAR_OTA_H

#include <stdint.h>

/* 启动时调一次：FAT 卷里有 RADAR.BIN 就发起一次分发。
 * 返回 1 = 已发起；0 = 无文件/无需分发；负数 = 出错。 */
int radar_ota_check(void);

/* 周期推进（建议 5~20ms 一次）。返回 1 = 仍在分发，0 = 空闲 */
void radar_ota_poll(void);

/* 是否正在分发（= 业务帧闸门，等价于 ota_dist_busy） */
int radar_ota_busy(void);

#endif /* RADAR_OTA_H */
