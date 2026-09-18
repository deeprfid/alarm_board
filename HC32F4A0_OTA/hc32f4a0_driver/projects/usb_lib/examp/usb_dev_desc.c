/**
 *******************************************************************************
 * @file  usb/usb_dev_cdc/source/usb_dev_desc.c
 * @brief USB descriptor define and function for example
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
   2022-10-31       CDT             Modify DEV_MANUFACTURER_STRING
   2024-11-08       CDT             Support Microsoft OS descriptor
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

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "usb_dev_desc.h"
#include "usb_dev_stdreq.h"

/**
 * @addtogroup HC32F4A0_DDL_Applications
 * @{
 */

/**
 * @addtogroup USB_Dev_Cdc
 * @{
 */

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
#define DEV_VID                        (0x2E88)
#define DEV_PID                        (0x4603)   /* CDC+MSC 模式 */
#define DEV_PID_HID                    (0x4606)   /* v9.81cn: HID 键盘模式 PID（4604/4605 均已被 Windows 缓存为失败枚举，每次换新 PID 强制重新识别） */

#define DEV_PID_WINUSB                 (0x4608)   /* v1.10: WinUSB 独立模式 PID（4607 被 Windows 缓存失败枚举，换新 PID 强制重新识别） */
extern volatile int g_usb_hid_mode;
extern volatile int g_usb_winusb_mode;   /* v1.10: WinUSB 模式标志 */

#define DEV_LANGID_STRING              (0x409)
#define DEV_MANUFACTURER_STRING        ("XHSC")
#define DEV_PRODUCT_FS_STRING          ("Device of CDC")
#define DEV_SERIALNUMBER_FS_STRING     ("00000000050C")
#define DEV_CONFIGURATION_FS_STRING    ("CDC Config")
#define DEV_INTERFACE_FS_STRING        ("CDC Interface")

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
uint8_t *usb_dev_desc(uint16_t *length);
uint8_t *usb_dev_langiddesc(uint16_t *length);
uint8_t *usb_dev_manufacturerstr(uint16_t *length);
uint8_t *usb_dev_productdesc(uint16_t *length);
uint8_t *usb_dev_serialstr(uint16_t *length);
uint8_t *usb_dev_configstrdesc(uint16_t *length);
uint8_t *usb_dev_intfstrdesc(uint16_t *length);
uint8_t *usb_dev_winusbosstr(uint16_t *length);   /* v1.10: MS OS 字符串（Windows 自动绑 WinUSB.sys） */

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
usb_dev_desc_func user_desc = {
    &usb_dev_desc,
    &usb_dev_langiddesc,
    &usb_dev_manufacturerstr,
    &usb_dev_productdesc,
    &usb_dev_serialstr,
    &usb_dev_configstrdesc,
    &usb_dev_intfstrdesc,
    &usb_dev_winusbosstr,   /* v1.10: MS OS 字符串 */
};

/* USB Standard Device Descriptor */
__USB_ALIGN_BEGIN static uint8_t usb_dev_devicedesc[USB_SIZ_DEVICE_DESC] = {
    0x12,                       /* bLength */
    USB_DEVICE_DESCRIPTOR_TYPE, /* bDescriptorType */
    0x00,                       /* bcdUSB */
    0x02,
    0x00,                       /* bDeviceClass */
    0x00,                       /* bDeviceSubClass */
    0x00,                       /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
    LOBYTE(DEV_VID),            /* idVendor */
    HIBYTE(DEV_VID),            /* idVendor */
    LOBYTE(DEV_PID),            /* idVendor */
    HIBYTE(DEV_PID),            /* idVendor */
    0x00,                       /* bcdDevice rel. 2.00 */
    0x02,
    MFC_STR_IDX,                /* Index of manufacturer string */
    PRODUCT_STR_IDX,            /* Index of product string */
    SERIAL_STR_IDX,             /* Index of serial number string */
    DEV_MAX_CFG_NUM             /* bNumConfigurations */
} ;

/* USB Standard Device Descriptor */
__USB_ALIGN_BEGIN static uint8_t USB_DEV_LangIDDesc[USB_SIZ_STRING_LANGID] = {
    USB_SIZ_STRING_LANGID,
    USB_DESC_TYPE_STRING,
    LOBYTE(DEV_LANGID_STRING),
    HIBYTE(DEV_LANGID_STRING),
};

__USB_ALIGN_BEGIN static uint8_t usb_dev_strdesc[USB_MAX_STR_DESC_SIZ];

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  return the device descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_desc(uint16_t *length)
{
    /* v9.81cm: HID 模式用独立 PID（idProduct @[10:11]），避免 Windows 用缓存的 CDC 驱动 */
    if (g_usb_winusb_mode) {
        usb_dev_devicedesc[10] = LOBYTE(DEV_PID_WINUSB);
        usb_dev_devicedesc[11] = HIBYTE(DEV_PID_WINUSB);
    } else if (g_usb_hid_mode) {
        usb_dev_devicedesc[10] = LOBYTE(DEV_PID_HID);
        usb_dev_devicedesc[11] = HIBYTE(DEV_PID_HID);
    } else {
        usb_dev_devicedesc[10] = LOBYTE(DEV_PID);
        usb_dev_devicedesc[11] = HIBYTE(DEV_PID);
    }
    *length = (uint16_t)sizeof(usb_dev_devicedesc);
    return usb_dev_devicedesc;
}

/**
 * @brief  return the LangID string descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_langiddesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USB_DEV_LangIDDesc);
    return USB_DEV_LangIDDesc;
}

/**
 * @brief  return the product string descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_productdesc(uint16_t *length)
{
    /* v9.81cm: 产品名按模式区分（HID 键盘 / CDC+MSC）；v1.10: WinUSB */
    usb_getstring((uint8_t *)(g_usb_winusb_mode ? "Device of WinUSB" :
                              (g_usb_hid_mode ? "HID Keyboard" : DEV_PRODUCT_FS_STRING)),
                  usb_dev_strdesc, length);
    return usb_dev_strdesc;
}

/**
 * @brief  return the manufacturer string descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_manufacturerstr(uint16_t *length)
{
    usb_getstring((uint8_t *)DEV_MANUFACTURER_STRING, usb_dev_strdesc, length);
    return usb_dev_strdesc;
}

/**
 * @brief  return the serial number string descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_serialstr(uint16_t *length)
{
    usb_getstring((uint8_t *)DEV_SERIALNUMBER_FS_STRING, usb_dev_strdesc, length);
    return usb_dev_strdesc;
}

/**
 * @brief  return the configuration string descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_configstrdesc(uint16_t *length)
{
    usb_getstring((uint8_t *)DEV_CONFIGURATION_FS_STRING, usb_dev_strdesc, length);
    return usb_dev_strdesc;
}

/**
 * @brief  return the interface string descriptor
 * @param  [in] length      pointer to data length variable
 * @retval pointer to descriptor buffer
 */
uint8_t *usb_dev_intfstrdesc(uint16_t *length)
{
    usb_getstring((uint8_t *)DEV_INTERFACE_FS_STRING, usb_dev_strdesc, length);
    return usb_dev_strdesc;
}

/* v1.10: MS OS 字符串描述符 "MSFT100"+VendorCode 0xA0——Windows 8+ 枚举时请求 0xEE，
 * 收到 MSFT100 后以 0xA0 vendor code 请求 OS 描述符 → 自动绑定 WinUSB.sys（免 INF）。
 * 参考 SDK usb_dev_winusb 例程。 */
#define USB_LEN_OS_DESC (0x12U)
__USB_ALIGN_BEGIN static uint8_t USBD_OS_STRING[USB_LEN_OS_DESC] = {
    USB_LEN_OS_DESC, /* Length of the descriptor */
    0x03,            /* Descriptor type (STRING) */
    'M', 0,          /* Signature field "MSFT100" */
    'S', 0,
    'F', 0,
    'T', 0,
    '1', 0,
    '0', 0,
    '0', 0,
    0xA0,            /* Vendor code（OS 描述符请求用） */
    0x00,            /* Pad field */
};

uint8_t *usb_dev_winusbosstr(uint16_t *length)
{
    /* v1.25: MS OS 字符串只在 WinUSB 模式响应——CDC/HID 模式返回空（长度 0）。
     * 根因：user_desc 无条件注册 MSFT100 后，Windows 对所有模式（含 CDC）都当 MS OS 设备处理，
     * usbser 枚举行为改变 → CDC 取走周期 110ms->531ms（8.2s->33s 回归）。 */
    if (!g_usb_winusb_mode) {
        *length = 0;
        return NULL;
    }
    *length = USB_LEN_OS_DESC;
    return USBD_OS_STRING;
}

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
