/**
 *******************************************************************************
 * @file  usb/usb_dev_cdc/source/usb_app_conf.h
 * @brief low level driver configuration
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#ifndef __USB_APP_CONF_H__
#define __USB_APP_CONF_H__

/* C binding of definitions if building with C++ compiler */
#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * Include files
 ******************************************************************************/

/* USB MODE CONFIGURATION */
/*
USB_FS_MODE, USB_HS_MODE, USB_HS_EXTERNAL_PHY defined comment
(1) If only defined USB_FS_MODE:
    MCU USBFS core work in full speed using internal PHY.
(2) If only defined USB_HS_MODE:
    MCU USBHS core work in full speed using internal PHY.
(3) If both defined USB_HS_MODE && USB_HS_EXTERNAL_PHY
    MCU USBHS core work in high speed using external PHY.
(4) Other combination:
    Not support, forbid!!
*/

#define USB_HS_MODE        /* v1.10: HS core 引脚匹配（内部 PHY 全速 12M，非真 HS） */

#define USE_DEVICE_MODE

#ifdef USB_HS_MODE
/* v9.81ci: 板载 USBHS + 内部 PHY（全速）——勿定义 USB_HS_EXTERNAL_PHY（代码用 USBHS_PHY_EMBED） */
#endif

#ifndef USB_HS_MODE
#ifndef USB_FS_MODE
#error  "USB_HS_MODE or USB_FS_MODE should be defined"
#endif
#endif

#ifndef USE_DEVICE_MODE
#ifndef USE_HOST_MODE
#error  "USE_DEVICE_MODE or USE_HOST_MODE should be defined"
#endif
#endif

/* USB DEVICE ENDPOINT CONFIGURATION */
/* v9.81ci: CDC+MSC 复合（端点唯一性，对齐 SDK cdc_msc 示例） */
#define MSC_IN_EP               (0x81U)
#define MSC_OUT_EP              (0x01U)
#define CDC_IN_EP               (0x82U)
#define CDC_OUT_EP              (0x02U)
#define CDC_CMD_EP              (0x83U)
/* v9.81cm: HID 端点对齐 SDK HID 例程（usb_dev_hid_custom: IN=EP3, OUT=EP1） */
#define HID_IN_EP               (0x83U)
#define HID_OUT_EP              (0x01U)
#define HID_IN_PACKET           (64U)
#define HID_OUT_PACKET          (64U)
#define MSC_MEDIA_PACKET        (12UL * 1024UL)
#define MSC_MAX_PACKET          (64U)
#define WINUSB_IN_EP            (0x85U)
#define WINUSB_OUT_EP           (0x04U)
#define MAX_WINUSB_PACKET_SIZE  (64U)

/* USB FIFO CONFIGURATION */
#ifdef USB_FS_MODE
#define RX_FIFO_FS_SIZE         (128U)
#define TX0_FIFO_FS_SIZE        (64U)
#define TX1_FIFO_FS_SIZE        (0U)
#define TX2_FIFO_FS_SIZE        (32U)
#define TX3_FIFO_FS_SIZE        (0U)
#define TX4_FIFO_FS_SIZE        (0U)
#define TX5_FIFO_FS_SIZE        (32U)
#define TX6_FIFO_FS_SIZE        (0U)
#define TX7_FIFO_FS_SIZE        (0U)
#define TX8_FIFO_FS_SIZE        (0U)
#define TX9_FIFO_FS_SIZE        (0U)
#define TX10_FIFO_FS_SIZE       (0U)
#define TX11_FIFO_FS_SIZE       (0U)
#define TX12_FIFO_FS_SIZE       (0U)
#define TX13_FIFO_FS_SIZE       (0U)
#define TX14_FIFO_FS_SIZE       (0U)
#define TX15_FIFO_FS_SIZE       (0U)

#if ((RX_FIFO_FS_SIZE + \
      TX0_FIFO_FS_SIZE + TX1_FIFO_FS_SIZE + TX2_FIFO_FS_SIZE + TX3_FIFO_FS_SIZE + TX4_FIFO_FS_SIZE + \
      TX5_FIFO_FS_SIZE + TX6_FIFO_FS_SIZE + TX7_FIFO_FS_SIZE + TX8_FIFO_FS_SIZE + TX9_FIFO_FS_SIZE + \
      TX10_FIFO_FS_SIZE + TX11_FIFO_FS_SIZE + TX12_FIFO_FS_SIZE + TX13_FIFO_FS_SIZE + TX14_FIFO_FS_SIZE + \
      TX15_FIFO_FS_SIZE) > 640U)
#error  "The USB max FIFO size is 640 x 4 Bytes!"
#endif
#endif

#ifdef USB_HS_MODE
#define RX_FIFO_HS_SIZE         (512U)
#define TX0_FIFO_HS_SIZE        (64U)
#define TX1_FIFO_HS_SIZE        (64U)
#define TX2_FIFO_HS_SIZE        (64U)
#define TX3_FIFO_HS_SIZE        (32U)
#define TX4_FIFO_HS_SIZE        (32U)
#define TX5_FIFO_HS_SIZE        (64U)   /* v1.10: WinUSB EP5 IN 发送 FIFO（原 0 致 ACK 发不出） */
#define TX6_FIFO_HS_SIZE        (0U)
#define TX7_FIFO_HS_SIZE        (0U)
#define TX8_FIFO_HS_SIZE        (0U)
#define TX9_FIFO_HS_SIZE        (0U)
#define TX10_FIFO_HS_SIZE       (0U)
#define TX11_FIFO_HS_SIZE       (0U)
#define TX12_FIFO_HS_SIZE       (0U)
#define TX13_FIFO_HS_SIZE       (0U)
#define TX14_FIFO_HS_SIZE       (0U)
#define TX15_FIFO_HS_SIZE       (0U)

#if ((RX_FIFO_HS_SIZE + \
      TX0_FIFO_HS_SIZE + TX1_FIFO_HS_SIZE + TX2_FIFO_HS_SIZE + TX3_FIFO_HS_SIZE + TX4_FIFO_HS_SIZE + \
      TX5_FIFO_HS_SIZE + TX6_FIFO_HS_SIZE + TX7_FIFO_HS_SIZE + TX8_FIFO_HS_SIZE + TX9_FIFO_HS_SIZE + \
      TX10_FIFO_HS_SIZE + TX11_FIFO_HS_SIZE + TX12_FIFO_HS_SIZE + TX13_FIFO_HS_SIZE + TX14_FIFO_HS_SIZE + \
      TX15_FIFO_HS_SIZE) > 2048U)
#error  "The USB max FIFO size is 2048 x 4 Bytes!"
#endif
#endif

/* FUNCTION CONFIGURATION */
#define DEV_MAX_CFG_NUM         (1U)
#define USBD_ITF_MAX_NUM        (2U)   /* v9.81ci: CDC+MSC 复合（SDK 对齐） */
#define USB_MAX_STR_DESC_SIZ    (128U)

#define VBUS_SENSING_ENABLED
//#define SELF_POWER

/* CONFIGURATION FOR CDC */
#define MAX_CDC_PACKET_SIZE     (64U)      /* IN & OUT Endpoint Packet size */
#define CDC_CMD_PACKET_SIZE     (8U)       /* Control Endpoint Packet size */

#define CDC_IN_FRAME_INTERVAL   (5U)       /* Number of frames between IN transfers */
#define APP_RX_DATA_SIZE        (2048U)    /* Total size of IN buffer*/

#ifdef __cplusplus
}
#endif

#endif /* __USB_APP_CONF_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
