/**
 *******************************************************************************
 * @file  ota_integration.h
 * @brief OTA 与业务代码的集成接口（v1.1：单分发线程）
 *
 * 目的：把散落在 user_main.c / Lan2Uart.c / reader_msg.c 的 OTA 调用收拢到本层，
 *       业务文件只保留一行接口调用，OTA 内部逻辑全在 ota_*.c。
 *       v1.1：OTA 线程合并为单个 ota_dispatch_task（USB + HTTP + 主动串口），栈省 16KB。
 *******************************************************************************
 */
#ifndef __OTA_INTEGRATION_H__
#define __OTA_INTEGRATION_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA 启动链（一次性）：暂存包版本检查 + 双bank确认 + USB 初始化 + 单分发线程。
 *        替代 user_main.c 内联的版本检查块 + ota_agent_boot/usb_msc_ota_check/ota_usb_start/ota_http_start。
 */
void ota_init_all(void);

/**
 * @brief 业务初始化完成后的新固件自检确认（防回滚）。
 *        替代 user_main.c 内联的 ota_agent_confirm()。
 */
void ota_confirm_after_init(void);

/**
 * @brief 被动模式：send_func 主循环 UART1 OTA 帧拦截（含 otabuf 大缓冲逻辑）。
 * @param fd     当前接口（COMMON_INTERFACE_UART1 才处理）
 * @param head3  已读到的前 3 字节（"OTA" magic 判断）
 * @return 1 = 已处理 OTA 帧（调用方应 continue）；0 = 非 OTA 帧
 */
int ota_serial_pump(int fd, const uint8_t *head3);

/**
 * @brief send_func 抢读到 USB1 "OTA" 帧头 → 转交 ota_usb_task 解析。
 * @return 1 = 已转交（调用方应 continue）；0 = 非 OTA 帧
 */
int ota_usb_feed_pump(int fd, const uint8_t *head3);

/**
 * @brief OTA 会话让路判断：USB1/UART1 在 OTA 会话中返回 1。
 *        替代 Lan2Uart.c / reader_msg.c 内联的 ota_channel_busy/ota_usb_stream_active 判断。
 */
int ota_channel_pending(int fd);

#ifdef __cplusplus
}
#endif

#endif /* __OTA_INTEGRATION_H__ */
