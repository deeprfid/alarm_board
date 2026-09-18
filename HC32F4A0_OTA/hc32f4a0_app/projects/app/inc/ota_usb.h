/**
 * @file ota_usb.h
 * @brief USB1(CDC) 本地 OTA 通道：轮询线程 + 流式组帧（复用 OTA1 帧协议）
 */
#ifndef OTA_USB_H
#define OTA_USB_H

/* 启动 USB1 OTA 轮询线程（user_main 调用一次） */
void ota_usb_start(void);

#endif /* OTA_USB_H */
