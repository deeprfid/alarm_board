/**
 *******************************************************************************
 * @file  usb_dev_winusb_class.c
 * @brief The windows usb device functions.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2024-05-31       CDT             First version
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
#include "usb_dev_winusb_class.h"
#include "usb_dev_driver.h"
#include "usb_dev_ctrleptrans.h"
#include "usb_dev_stdreq.h"
#include "usb_dev_desc.h"
#include "io_stream.h"   /* v1.10: WinUSB RX 喂 gUsbParams[2]（io_stream 接口） */
#include "cmsis_os2.h"   /* v9.82c: osKernelGetTickCount（保活点射） */

/**
 * @addtogroup LL_USB_LIB
 * @{
 */

/**
 * @addtogroup LL_USB_DEV_CLASS
 * @{
 */

/**
 * @addtogroup LL_USB_WINUSB USB Device WinUSB
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/

/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/
uint8_t *usb_dev_winusb_getcfgdesc(uint16_t *length);
void usb_dev_winusb_getdesc(usb_core_instance *pdev, USB_SETUP_REQ *req);

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
usb_dev_class_func  class_winusb_cbk = {
    &usb_dev_winusb_init,
    &usb_dev_winusb_deinit,
    &usb_dev_winusb_setup,
    NULL,
    NULL,
    &usb_dev_winusb_getcfgdesc,
    &usb_dev_winusb_sof,   /* v9.82c: SOF 保活点射 */
    &usb_dev_winusb_datain,
    &usb_dev_winusb_dataout,
    NULL,
    NULL,
};

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
__USB_ALIGN_BEGIN static uint8_t usb_rx_buffer[MAX_WINUSB_PACKET_SIZE];

/* USB IN transfer status, 0 means can start a IN transfer */
static uint8_t  USB_Tx_State   = 0U;

/* v9.82: 常驻 TX 保活（同 CDC）——EP5 发完自动重装，端点永不空闲 */
static volatile uint8_t  s_winusb_tx_pending = 0U;
static volatile uint8_t *s_winusb_tx_buf = NULL;
static volatile uint16_t s_winusb_tx_len = 0U;
static volatile uint8_t  s_winusb_tx_last_real = 0U;
static volatile uint32_t s_winusb_tx_seq  = 0U;
static volatile uint32_t s_winusb_tx_done = 0U;
static const uint8_t     s_winusb_keepalive = 0x00U;
/* v9.82b: 保活改为 1ms 定时器点射（原 ISR 无限续装 → 15K/s 洪泛饿死 OUT 接收路径） */
static volatile uint8_t  s_winusb_ep_busy = 0U;
static volatile uint32_t s_winusb_ka_tick = 0U;
static void             *s_winusb_pdev = NULL;

__USB_ALIGN_BEGIN static uint8_t usb_dev_winusb_cfgdesc[USB_WINUSB_CONFIG_DESC_SIZ] = {
    0x09,
    USB_CFG_DESCRIPTOR_TYPE,
    0x20,   /* wTotalLength:no of returned bytes */
    0x00,
    0x01,   /* bNumInterfaces: 1 interface for WinUSB */
    0x01,   /* bConfigurationValue: Configuration value */
    0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
    0xC0,   /* bmAttributes: self powered */
    0x32,   /* MaxPower 50*2 mA */

    /*---------------------------------------------------------------------------*/
    /*Data class interface descriptor*/
    0x09,   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: */
    0x00,   /* bInterfaceNumber: Number of Interface, zero based index of this interface */
    0x00,   /* bAlternateSetting: Alternate setting */
    0x02,   /* bNumEndpoints: Two endpoints used */
    0xff,   /* bInterfaceClass: vendor */
    0x00,   /* bInterfaceSubClass: */
    0x00,   /* bInterfaceProtocol: */
    0x00,   /* iInterface: */

    /*Endpoint OUT Descriptor*/
    0x07,   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
    WINUSB_OUT_EP,              /* bEndpointAddress */
    0x02,                       /* bmAttributes: Bulk */
    LOBYTE(MAX_WINUSB_PACKET_SIZE),/* wMaxPacketSize: */
    HIBYTE(MAX_WINUSB_PACKET_SIZE),
    0x00,                       /* bInterval: ignore for Bulk transfer */

    /*Endpoint IN Descriptor*/
    0x07,                       /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT,     /* bDescriptorType: Endpoint */
    WINUSB_IN_EP,               /* bEndpointAddress */
    0x02,                       /* bmAttributes: Bulk */
    LOBYTE(MAX_WINUSB_PACKET_SIZE),/* wMaxPacketSize: */
    HIBYTE(MAX_WINUSB_PACKET_SIZE),
    0x00                        /* bInterval: ignore for Bulk transfer */
} ;

#define USB_LEN_CAMPATID_DESC (0x28U)
__USB_ALIGN_BEGIN static uint8_t usb_dev_winusb_campatiddesc[USB_LEN_CAMPATID_DESC] = {
    USB_LEN_CAMPATID_DESC, 0, 0, 0, /* dwLength */
    0, 1,          /* bcdVersion 1.0 */
    4, 0,          /* wIndex: extended compat ID descriptor */
    1,             /* bCount: Number of function sections */
    0, 0, 0, 0, 0, 0, 0, /* reserve 7 bytes */
    /* function */
    0,             /* bFirstInterfaceNumber */
    1,             /* reserved */
    'W', 'I', 'N', 'U', 'S', 'B', 0, 0, /* compatibleID */
    0,   0,   0,   0,   0,   0, 0, 0,   /* subCompatibleID */
    0,   0,   0,   0,   0,   0          /* reserved 6 bytes */
};

#define USB_LEN_OS_PROPERTY_DESC (0x8EU)
__USB_ALIGN_BEGIN static uint8_t usb_dev_winusb_propertydesc[USB_LEN_OS_PROPERTY_DESC] = {
    USB_LEN_OS_PROPERTY_DESC, 0, 0, 0,    /* dwLength */
    0x00, 0x01,       /* bcdVersion 1.0 */
    0x05, 0x00,       /* wIndex: Extended Property Descriptor Index(5) */
    0x01, 0x00,       /* wCount: number of section (1) */
    /* property section */
    0x84, 0x00, 0x00, 0x00,   /* dwSize */
    0x1, 0, 0, 0,     /* dwPropertyDataType (1： A NULL-terminated Unicode String) */
    0x28, 0,          /* wPropertyNameLength (42) */
    /* bPropertyName */
    'D', 0,
    'e', 0,
    'v', 0,
    'i', 0,
    'c', 0,
    'e', 0,
    'I', 0,
    'n', 0,
    't', 0,
    'e', 0,
    'r', 0,
    'f', 0,
    'a', 0,
    'c', 0,
    'e', 0,
    'G', 0,
    'U', 0,
    'I', 0,
    'D', 0,
    0, 0,

    0x4E, 0, 0, 0,  /* dwPropertyDataLength */
    /* bPropertyData */
    '{', 0,
    '1', 0,
    '3', 0,
    'E', 0,
    'B', 0,
    '3', 0,
    '6', 0,
    '0', 0,
    'B', 0,
    '-', 0,
    'B', 0,
    'C', 0,
    '1', 0,
    'E', 0,
    '-', 0,
    '4', 0,
    '6', 0,
    'C', 0,
    'B', 0,
    '-', 0,
    'A', 0,
    'C', 0,
    '8', 0,
    'B', 0,
    '-', 0,
    'E', 0,
    'F', 0,
    '3', 0,
    'D', 0,
    'A', 0,
    '4', 0,
    '7', 0,
    'B', 0,
    '4', 0,
    '0', 0,
    '6', 0,
    '2', 0,
    '}', 0,
    0, 0,
};

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Initialize the WinUSB application
 * @param  [in] pdev        Device instance
 * @retval None
 */
void usb_dev_winusb_init(void *pdev)
{
    usb_opendevep(pdev, WINUSB_IN_EP, MAX_WINUSB_IN_PACKET_SIZE, EP_TYPE_BULK);
    usb_opendevep(pdev, WINUSB_OUT_EP, MAX_WINUSB_OUT_PACKET_SIZE, EP_TYPE_BULK);
    usb_readytorx(pdev, WINUSB_OUT_EP, (uint8_t *)(usb_rx_buffer), MAX_WINUSB_OUT_PACKET_SIZE);
    /* v9.82b: 预装首包保活；后续由 1ms 定时器 pump 续装（usb_winusb_keepalive_pump） */
    s_winusb_pdev = pdev;
    s_winusb_ep_busy = 1U;
    s_winusb_ka_tick = (uint32_t)osKernelGetTickCount();
    usb_deveptx(pdev, WINUSB_IN_EP, (uint8_t *)&s_winusb_keepalive, 1U);
}

/**
 * @brief  Deinitialize the WinUSB application
 * @param  [in] pdev        Device instance
 * @retval None
 */
void usb_dev_winusb_deinit(void *pdev)
{
    usb_shutdevep(pdev, WINUSB_IN_EP);
    usb_shutdevep(pdev, WINUSB_OUT_EP);
}

/**
 * @brief  Handle the setup requests
 * @param  [in] pdev        Device instance
 * @param  [in] req         usb requests
 * @retval status
 */
uint8_t usb_dev_winusb_setup(void *pdev, USB_SETUP_REQ *req)
{
    uint8_t  u8Res = USB_DEV_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS :
            break;
        case USB_REQ_TYPE_STANDARD:
            break;
        case USB_REQ_TYPE_VENDOR:
            usb_dev_winusb_getdesc(pdev, req);
            break;
        default:
            usb_ctrlerr(pdev);
            u8Res = USB_DEV_FAIL;
            break;
    }
    return u8Res;
}

/**
 * @brief  v9.82c: SOF 事件——每 1ms 点射保活，防主机 NAK 降频轮询
 * @param  [in] pdev        Device instance
 * @retval None
 */
uint8_t usb_dev_winusb_sof(void *pdev)
{
    usb_winusb_keepalive_pump();
    return USB_DEV_OK;
}

/* v9.82c: 与回调表/头文件签名一致（uint8_t） */

/**
 * @brief  Data sent on non-control IN endpoint
 * @param  [in] pdev        Device instance
 * @param  [in] epnum       endpoint index
 * @retval None
 */
extern volatile int g_usb_winusb_xfer_done;   /* v1.10: WinUSB IN XFRC 完成标志 */
void usb_dev_winusb_datain(void *pdev, uint8_t epnum)
{
    /* In transfer finished */
    USB_Tx_State = 0U;
    g_usb_winusb_xfer_done = 1;

    /* v9.82b: 刚完成的是真实数据则计数；保活由 1ms 定时器 pump 续装 */
    if (s_winusb_tx_last_real != 0U) {
        s_winusb_tx_done++;
        s_winusb_tx_last_real = 0U;
    }
    s_winusb_ep_busy = 0U;
    if (s_winusb_tx_pending != 0U && s_winusb_tx_buf != NULL) {
        s_winusb_tx_pending = 0U;
        s_winusb_tx_last_real = 1U;
        s_winusb_tx_seq++;
        s_winusb_ep_busy = 1U;
        usb_deveptx(pdev, WINUSB_IN_EP, (uint8_t *)s_winusb_tx_buf, (uint32_t)s_winusb_tx_len);
    }
}

/**
 * @brief  Data received on non-control Out endpoint
 * @param  [in] pdev        device instance
 * @param  [in] epnum       endpoint index
 * @retval None
 */
void usb_dev_winusb_dataout(void *pdev, uint8_t epnum)
{
    uint16_t usb_rx_cnt;
    uint32_t avail, fbcnt, sbcnt;
    commonUsbParaLocal *pUsb = &gUsbParams[2];   /* v1.10: WinUSB RX 缓冲 */

    usb_rx_cnt = (uint16_t)((usb_core_instance *)pdev)->dev.out_ep[epnum].xfer_count;
    usb_readytorx(pdev, WINUSB_OUT_EP, (uint8_t *)(usb_rx_buffer), MAX_WINUSB_OUT_PACKET_SIZE);

    /* v1.10: 收到的数据存入 gUsbParams[2] 环形缓冲（io_stream read(COMMON_INTERFACE_USB2) 消费），
     * 不再 echo 回显——OTA 帧/上传命令由此进入应用层 */
    if (usb_rx_cnt == 0U)
        return;
    avail = (uint32_t)pUsb->recvbufsize -
            (uint32_t)((pUsb->usb_tail - pUsb->usb_head + pUsb->recvbufsize) % pUsb->recvbufsize);
    if (usb_rx_cnt > avail)
        return;   /* 满：丢弃（由上层重传） */
    if (((uint32_t)pUsb->usb_tail + usb_rx_cnt) <= (uint32_t)pUsb->recvbufsize) {
        memcpy(pUsb->recvbuf + pUsb->usb_tail, usb_rx_buffer, usb_rx_cnt);
        pUsb->usb_tail += usb_rx_cnt;
        if (pUsb->usb_tail >= pUsb->recvbufsize)
            pUsb->usb_tail = 0U;
    } else {
        fbcnt = (uint32_t)pUsb->recvbufsize - (uint32_t)pUsb->usb_tail;
        sbcnt = (uint32_t)usb_rx_cnt - fbcnt;
        memcpy(pUsb->recvbuf + pUsb->usb_tail, usb_rx_buffer, fbcnt);
        memcpy(pUsb->recvbuf, usb_rx_buffer + fbcnt, sbcnt);
        pUsb->usb_tail = (uint16_t)sbcnt;
    }
}

/**
 * @brief  v9.82b: 保活点射——1ms 定时器回调调用；端点空闲且距上次 ≥1ms 时
 *         装载 1B 0x00 保活包，防主机 NAK 降频轮询（频率 ~1K/s，不饿死 OUT）。
 * @retval None
 */
void usb_winusb_keepalive_pump(void)
{
    uint32_t now;

    if (s_winusb_pdev == NULL)
        return;
    now = (uint32_t)osKernelGetTickCount();
    if ((uint32_t)(now - s_winusb_ka_tick) < 1U)
        return;
    __disable_irq();
    if (s_winusb_ep_busy == 0U && s_winusb_tx_pending == 0U) {
        s_winusb_ep_busy = 1U;
        s_winusb_ka_tick = now;
        usb_deveptx((usb_core_instance *)s_winusb_pdev, WINUSB_IN_EP,
                    (uint8_t *)&s_winusb_keepalive, 1U);
    }
    __enable_irq();
}

/**
 * @brief  v9.82: WinUSB 发送一个包（与常驻保活协作，无竞争）。
 * @param  [in] buf  数据缓冲（必须存活到传输完成）
 * @param  [in] len  数据长度（>0）
 * @retval None
 */
void usb_winusb_tx_arm(const uint8_t *buf, uint32_t len)
{
    uint32_t done0;
    uint32_t tmo;

    if (buf == NULL || len == 0U)
        return;

    s_winusb_tx_buf = (volatile uint8_t *)buf;
    s_winusb_tx_len = (uint16_t)len;
    __disable_irq();
    s_winusb_tx_pending = 1U;
    __enable_irq();

    /* v9.82d: 先给 ISR 协作路径 ~0.3ms 窗口（保活链活着时 ISR 会接管） */
    done0 = s_winusb_tx_done;
    tmo = 10000U;
    while (s_winusb_tx_done == done0 && --tmo != 0U) { }

    if (s_winusb_tx_done == done0) {
        /* 协作路径未生效（保活链断/端点空闲）→ 直接装载，确保数据发出 */
        __disable_irq();
        if (s_winusb_ep_busy == 0U && s_winusb_tx_pending != 0U) {
            s_winusb_tx_pending = 0U;
            s_winusb_tx_last_real = 1U;
            s_winusb_tx_seq++;
            s_winusb_ep_busy = 1U;
            usb_deveptx((usb_core_instance *)s_winusb_pdev, WINUSB_IN_EP,
                        (uint8_t *)s_winusb_tx_buf, (uint32_t)s_winusb_tx_len);
        }
        __enable_irq();
        /* 等其 XFRC 完成（ISR 会 last_real→done++） */
        done0 = s_winusb_tx_done;
        tmo = 300000U;
        while (s_winusb_tx_done == done0 && --tmo != 0U) { }
    }
}

/**
 * @brief  get the configuration descriptor
 * @param  [in] length          length of configuration descriptor in bytes
 * @retval the pointer to configuration descriptor buffer
 */
uint8_t *usb_dev_winusb_getcfgdesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(usb_dev_winusb_cfgdesc);
    return usb_dev_winusb_cfgdesc;
}

/**
 * @brief  get the WinUSB feature descriptor
 * @param  [in] pdev        device instance
 * @param  [in] req         usb request
 * @retval None
 */
void usb_dev_winusb_getdesc(usb_core_instance *pdev, USB_SETUP_REQ *req)
{
    uint16_t len;
    uint8_t *pbuf;

    switch (req->wIndex) {
        case 0x04U:  /* Extended compat ID */
            pbuf = usb_dev_winusb_campatiddesc;
            len = USB_LEN_CAMPATID_DESC;
            break;
        case 0x05U:  /* Extended properties */
            pbuf = usb_dev_winusb_propertydesc;
            len = USB_LEN_OS_PROPERTY_DESC;
            break;

        default:
            usb_ctrlerr(pdev);
            return;
    }
    if ((len != 0U) && (req->wLength != 0U))  {
        len = LL_MIN(len, req->wLength);
        usb_ctrldatatx(pdev, pbuf, len);
    }
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
