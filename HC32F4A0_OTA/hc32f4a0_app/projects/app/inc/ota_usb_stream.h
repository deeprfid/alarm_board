/**
 * @file ota_usb_stream.h
 * @brief USB stream OTA (v9.74): 4KB frames, direct EFM write to other bank, swap.
 */
#ifndef OTA_USB_STREAM_H
#define OTA_USB_STREAM_H

#include <stdint.h>

/* feed USB1 RX bytes into stream state machine.
 * return 1 = finished (device resets), 0 = continue, <0 = error */
int ota_usb_stream_feed(int fd, const uint8_t *data, uint32_t len);

/* v9.81: 会话空闲超时检查（上位机掉线时释放通道锁）。返回 1 = 超时已释放 */
int ota_usb_stream_idle_timeout(void);
int ota_usb_stream_active(void);   /* v9.81cl: 最近有 OTA 数据（send_func 让路用） */

#endif /* OTA_USB_STREAM_H */
