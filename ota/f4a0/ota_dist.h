/**
 * @file  ota_dist.h
 * @brief F4A0 侧固件分发集成层（把 ota_host 发送器接到 RS485 上）
 *
 * 分工：
 *   ota_host.c  纯逻辑发送器（帧/握手/分块/探测/卡死保护），零硬件依赖
 *   ota_dist.c  本文件：提供 IO 回调（read/write(fd) + osKernelGetTickCount）、
 *               分发状态、以及【业务帧闸门】——升级期间全系统停发业务帧
 *
 * 口径（docs/ota_boot_design.md v0.2 §0）：
 *   升级期间关闭全部业务帧、链路独占、只跑固件下载。故闸门是【全局】的：
 *   ota_dist_busy() 为真时，业务发送路径直接丢弃（见 alarm.c/mqtt_interface.c 的 ipc_hpm_message）。
 */
#ifndef OTA_DIST_H
#define OTA_DIST_H

#include <stdint.h>

/* 开始一次分发：fd 为目标 RS485 通道，pkg 为 .otapkg 内容（含 82B 包头），len 总长。
 * 返回 0 已启动；非 0 失败（参数/已在忙/通道不可写） */
int  ota_dist_start(int fd, const uint8_t *pkg, uint32_t len);

/* 周期推进（建议每 5~20ms 调一次，可放在独立任务里）；返回 1 = 仍在进行，0 = 已结束 */
int  ota_dist_poll(void);

/* 业务帧闸门：1 = 正在升级，业务发送路径必须丢弃 */
int  ota_dist_busy(void);

/* 进度：已确认偏移 / 包总长（字节，含 82B 包头） */
uint32_t ota_dist_progress(void);
uint32_t ota_dist_total(void);

/* 结果：0 = 无/进行中；1 = 成功；负数 = 失败码（见 ota_host.h 的 OTA_HOST_ERR_*） */
int  ota_dist_result(void);

/* 中止并释放闸门（异常/上层取消） */
void ota_dist_abort(void);

#endif /* OTA_DIST_H */
