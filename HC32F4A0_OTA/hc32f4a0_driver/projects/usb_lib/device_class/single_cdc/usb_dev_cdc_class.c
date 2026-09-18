/**
 *******************************************************************************
 * @file  usb_dev_cdc_class.c
 * @brief The CDC VCP core functions.
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

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "usb_dev_cdc_class.h"
#include "usb_dev_driver.h"
#include "usb_dev_ctrleptrans.h"
#include "usb_dev_stdreq.h"
#include "usb_dev_desc.h"
#include "cdc_data_process.h"
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
 * @addtogroup LL_USB_SINGLE_CDC USB Device CDC
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
uint8_t *usb_dev_cdc_getcfgdesc(uint16_t *length);
void process_asynchdata_uart2usb(void *pdev);

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/
usb_dev_class_func  class_cdc_cbk = {
    &usb_dev_cdc_init,
    &usb_dev_cdc_deinit,
    &usb_dev_cdc_setup,
    NULL,
    &usb_dev_cdc_ctrlep_rxready,
    &usb_dev_cdc_getcfgdesc,
    &usb_dev_cdc_sof,
    &usb_dev_cdc_datain,
    &usb_dev_cdc_dataout,
    NULL,
    NULL,
};

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
__USB_ALIGN_BEGIN static uint32_t  alternate_setting  = 0UL;
__USB_ALIGN_BEGIN static uint8_t usb_rx_buffer[MAX_CDC_PACKET_SIZE];
uint8_t uart_rx_buffer[APP_RX_DATA_SIZE];  /* used as a buffer for receiving data from uart port */
volatile int g_usb_cdc_xfer_done = 1;      /* CDC IN XFRC 完成标志（usb_send 等待用） */
__USB_ALIGN_BEGIN static uint8_t CmdBuff[CDC_CMD_PACKET_SIZE];
uint32_t APP_Rx_ptr_in  = 0UL;
uint32_t APP_Rx_ptr_out = 0UL;
static uint32_t APP_Rx_length  = 0UL;
static uint8_t  USB_Tx_State   = 0U;
static uint32_t cdcCmd  = 0xFFUL;
static uint32_t cdcLen  = 0UL;
static uint32_t LastPackLen = 0UL;

/* v9.82: 常驻 TX 保活——IN 端点发完自动重装（有待发数据发数据，否则发 1B 0x00 保活）。
 * 目的：端点永不空闲 → 主机不进入 NAK 降频轮询 → 稀疏 IN（OTA ACK/上传）延迟
 * 540ms → ~1ms。XFRC 节流：主机读多快发多快，主机不读 FIFO 满自然停。 */
static volatile uint8_t  s_cdc_tx_pending = 0U;   /* 1 = usb_send 有待发数据 */
static volatile uint8_t *s_cdc_tx_buf = NULL;      /* 待发数据指针（usb_send 设置） */
static volatile uint16_t s_cdc_tx_len = 0U;        /* 待发数据长度 */
static volatile uint8_t  s_cdc_tx_last_real = 0U;  /* 刚完成的包是真实数据 */
static volatile uint32_t s_cdc_tx_seq  = 0U;       /* ISR 装载真实数据次数 */
static volatile uint32_t s_cdc_tx_done = 0U;       /* ISR 完成真实数据次数 */
static const uint8_t     s_cdc_keepalive = 0x00U;  /* 保活填充字节（主机解析器跳过 0x00） */
/* v9.82b: 保活改为 1ms 定时器点射（原 ISR 无限续装 → 15K/s 洪泛饿死 OUT 接收路径） */
static volatile uint8_t  s_cdc_ep_busy = 0U;       /* 1 = 端点上有一个包在飞 */
static volatile uint32_t s_cdc_ka_tick = 0U;       /* 上次保活装载 tick */
static void             *s_cdc_pdev = NULL;        /* 类实例（pump 用 usb_deveptx） */

__USB_ALIGN_BEGIN static uint8_t usb_dev_cdc_cfgdesc[USB_CDC_CONFIG_DESC_SIZ]  = {
    0x09,
    USB_CFG_DESCRIPTOR_TYPE,
    USB_CDC_CONFIG_DESC_SIZ,
    0x00,
    0x02,
    0x01,
    0x00,
    0xC0,
    0x32,

    0x08,
    0x0B,
    0x00,
    0x02,
    0x02,
    0x02,
    0x01,
    0x04,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    0x01,
    0x02,
    0x02,
    0x01,
    0x00,

    0x05,
    0x24,
    0x00,
    0x10,
    0x01,

    0x05,
    0x24,
    0x01,
    0x00,
    0x01,

    0x04,
    0x24,
    0x02,
    0x02,

    0x05,
    0x24,
    0x06,
    0x00,
    0x01,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    CDC_CMD_EP,
    0x03,
    LOBYTE(CDC_CMD_PACKET_SIZE),
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0xFF,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
    0x00,
    0x02,
    0x0A,
    0x00,
    0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    CDC_OUT_EP,
    0x02,
    LOBYTE(MAX_CDC_PACKET_SIZE),
    HIBYTE(MAX_CDC_PACKET_SIZE),
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    CDC_IN_EP,
    0x02,
    LOBYTE(MAX_CDC_PACKET_SIZE),
    HIBYTE(MAX_CDC_PACKET_SIZE),
    0x00
} ;

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  Initialize the CDC application
 * @param  [in] pdev        Device instance
 * @retval None
 */
void usb_dev_cdc_init(void *pdev)
{
    usb_opendevep(pdev, CDC_IN_EP, MAX_CDC_IN_PACKET_SIZE, EP_TYPE_BULK);
    usb_opendevep(pdev, CDC_OUT_EP, MAX_CDC_OUT_PACKET_SIZE, EP_TYPE_BULK);
    usb_opendevep(pdev, CDC_CMD_EP, CDC_CMD_PACKET_SIZE, EP_TYPE_INTR);
    vcp_init();
    usb_readytorx(pdev, CDC_OUT_EP, (uint8_t *)(usb_rx_buffer), MAX_CDC_OUT_PACKET_SIZE);
    /* v9.82b: 预装首包保活；后续由 1ms 定时器 pump 续装（usb_cdc_keepalive_pump） */
    s_cdc_pdev = pdev;
    s_cdc_ep_busy = 1U;
    s_cdc_ka_tick = (uint32_t)osKernelGetTickCount();
    usb_deveptx(pdev, CDC_IN_EP, (uint8_t *)&s_cdc_keepalive, 1U);
}

/**
 * @brief  Deinitialize the CDC application
 * @param  [in] pdev        Device instance
 * @retval None
 */
void usb_dev_cdc_deinit(void *pdev)
{
    usb_shutdevep(pdev, CDC_IN_EP);
    usb_shutdevep(pdev, CDC_OUT_EP);
    usb_shutdevep(pdev, CDC_CMD_EP);
    vcp_deinit();
}

/**
 * @brief  Handle the setup requests
 * @param  [in] pdev        Device instance
 * @param  [in] req         usb requests
 * @retval status
 */
uint8_t usb_dev_cdc_setup(void *pdev, USB_SETUP_REQ *req)
{
    uint16_t len = USB_CDC_DESC_SIZ;
    uint8_t  *pbuf = usb_dev_cdc_cfgdesc + 9;
    uint8_t  u8Res = USB_DEV_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
        case USB_REQ_TYPE_CLASS :
            if (req->wLength != 0U) {
                if ((req->bmRequest & 0x80U) != 0U) {
                    vcp_ctrlpare(req->bRequest, CmdBuff, req->wLength);
                    usb_ctrldatatx(pdev, CmdBuff, req->wLength);
                } else {
                    cdcCmd = req->bRequest;
                    cdcLen = req->wLength;
                    usb_ctrldatarx(pdev, CmdBuff, req->wLength);
                }
            } else {
                vcp_ctrlpare(req->bRequest, NULL, 0UL);
            }
            break;
        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest) {
                case USB_REQ_GET_DESCRIPTOR:
                    if ((req->wValue >> 8) == CDC_DESCRIPTOR_TYPE) {
                        pbuf = usb_dev_cdc_cfgdesc + 9U + (9U * USBD_ITF_MAX_NUM);
                        len  = LL_MIN(USB_CDC_DESC_SIZ, req->wLength);
                    }
                    usb_ctrldatatx(pdev, pbuf, len);
                    break;

                case USB_REQ_GET_INTERFACE :
                    usb_ctrldatatx(pdev, (uint8_t *)&alternate_setting, 1U);
                    break;

                case USB_REQ_SET_INTERFACE :
                    if ((uint8_t)(req->wValue) < USBD_ITF_MAX_NUM) {
                        alternate_setting = (uint8_t)(req->wValue);
                    } else {
                        usb_ctrlerr(pdev);
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            usb_ctrlerr(pdev);
            u8Res = USB_DEV_FAIL;
            break;
    }
    return u8Res;
}

/**
 * @brief  Data received on control endpoint
 * @param  [in] pdev        device device instance
 * @retval None
 */
void usb_dev_cdc_ctrlep_rxready(void *pdev)
{
    if (cdcCmd != NO_CMD) {
        vcp_ctrlpare(cdcCmd, CmdBuff, cdcLen);
        cdcCmd = NO_CMD;
    }
}

/**
 * @brief  Data sent on non-control IN endpoint
 * @param  [in] pdev        Device instance
 * @param  [in] epnum       endpoint index
 * @retval None
 */
void usb_dev_cdc_datain(void *pdev, uint8_t epnum)
{
    uint16_t tx2usb_ptr;
    uint16_t tx2usb_length;

    if (epnum == (uint8_t)(CDC_IN_EP & 0x7FU)) {
        g_usb_cdc_xfer_done = 1;
        /* v9.82b: 刚完成的是真实数据 → 计数完成 */
        if (s_cdc_tx_last_real != 0U) {
            s_cdc_tx_done++;
            s_cdc_tx_last_real = 0U;
        }
        /* 本包已发完（FIFO 空）——保活由 1ms 定时器 pump 续装，避免洪泛 */
        s_cdc_ep_busy = 0U;
        /* 有待发数据 → 立即装载真实数据（实时性优先） */
        if (s_cdc_tx_pending != 0U && s_cdc_tx_buf != NULL) {
            s_cdc_tx_pending = 0U;
            s_cdc_tx_last_real = 1U;
            s_cdc_tx_seq++;
            s_cdc_ep_busy = 1U;
            usb_deveptx(pdev, CDC_IN_EP, (uint8_t *)s_cdc_tx_buf, (uint32_t)s_cdc_tx_len);
        }
    }

    if (USB_Tx_State == 1U) {
        if (APP_Rx_length == 0U) {
            if (LastPackLen == MAX_CDC_IN_PACKET_SIZE) {
                usb_deveptx(pdev, CDC_IN_EP, NULL, 0UL);
                LastPackLen = 0UL;
            } else {
                USB_Tx_State = 0U;
            }
        } else {
            if (APP_Rx_length >= MAX_CDC_IN_PACKET_SIZE) {
                tx2usb_ptr      = (uint16_t)APP_Rx_ptr_out;
                tx2usb_length   = (uint16_t)MAX_CDC_IN_PACKET_SIZE - 1U;
                APP_Rx_ptr_out += MAX_CDC_IN_PACKET_SIZE - 1U;
                APP_Rx_length  -= MAX_CDC_IN_PACKET_SIZE - 1U;
            } else {
                tx2usb_ptr      = (uint16_t)APP_Rx_ptr_out;
                tx2usb_length   = (uint16_t)APP_Rx_length;
                APP_Rx_ptr_out += APP_Rx_length;
                APP_Rx_length   = 0U;
            }
            usb_deveptx(pdev,
                        CDC_IN_EP,
                        (uint8_t *)&uart_rx_buffer[tx2usb_ptr],
                        (uint32_t)tx2usb_length);
            LastPackLen = (uint32_t)tx2usb_length;
        }
    }
}

/**
 * @brief  Data received on non-control Out endpoint
 * @param  [in] pdev        device instance
 * @param  [in] epnum       endpoint index
 * @retval None
 */
void usb_dev_cdc_dataout(void *pdev, uint8_t epnum)
{
    uint16_t usb_rx_cnt;

    usb_rx_cnt = (uint16_t)((usb_core_instance *)pdev)->dev.out_ep[epnum].xfer_count;
    vcp_rxdata(usb_rx_buffer, usb_rx_cnt);
    usb_readytorx(pdev, CDC_OUT_EP, (uint8_t *)(usb_rx_buffer), MAX_CDC_OUT_PACKET_SIZE);
}

/**
 * @brief  Start Of Frame event management
 * @param  [in] pdev        Device instance
 * @retval status
 */
uint8_t usb_dev_cdc_sof(void *pdev)
{
    static uint32_t FrameCount = 0UL;

    /* v9.82c: 每 SOF（1ms）点射保活——端点空闲且 ≥1ms 装 1B 0x00，防 NAK 降频 */
    usb_cdc_keepalive_pump();

    if (FrameCount++ == CDC_IN_FRAME_INTERVAL) {
        FrameCount = 0UL;
        process_asynchdata_uart2usb(pdev);
    }
    return USB_DEV_OK;
}

/**
 * @brief  process the data received from usart and send through USB to host
 * @param  [in] pdev        device instance
 * @retval None
 */
void process_asynchdata_uart2usb(void *pdev)
{
    uint16_t tx2usb_ptr;     /* the location of the pointer in buffer that would be sent to USB */
    uint16_t tx2usb_length;  /* the length in bytes that would be sent to USB */

    if (USB_Tx_State != 1U) {
        if (APP_Rx_ptr_out == APP_RX_DATA_SIZE) {
            APP_Rx_ptr_out = 0UL;
        }
        if (APP_Rx_ptr_out == APP_Rx_ptr_in) {
            USB_Tx_State = 0U;
        } else {
            if (APP_Rx_ptr_out > APP_Rx_ptr_in) {
                APP_Rx_length = APP_RX_DATA_SIZE - APP_Rx_ptr_out;
            } else {
                APP_Rx_length = APP_Rx_ptr_in - APP_Rx_ptr_out;
            }

            if (APP_Rx_length >= MAX_CDC_IN_PACKET_SIZE) {
                tx2usb_ptr = (uint16_t)APP_Rx_ptr_out;
                tx2usb_length = MAX_CDC_IN_PACKET_SIZE - 1U;

                APP_Rx_ptr_out += MAX_CDC_IN_PACKET_SIZE - 1UL;
                APP_Rx_length -= MAX_CDC_IN_PACKET_SIZE - 1UL;
            } else {
                tx2usb_ptr = (uint16_t)APP_Rx_ptr_out;
                tx2usb_length = (uint16_t)APP_Rx_length;

                APP_Rx_ptr_out += APP_Rx_length;
                APP_Rx_length = 0UL;
            }
            USB_Tx_State = 1U;

            usb_deveptx(pdev,
                        CDC_IN_EP,
                        (uint8_t *)&uart_rx_buffer[tx2usb_ptr],
                        (uint32_t)tx2usb_length);
            LastPackLen = (uint32_t)tx2usb_length;
        }
    }
}

/**
 * @brief  v9.82b: 保活点射——1ms 定时器回调调用；端点空闲且距上次 ≥1ms 时
 *         装载 1B 0x00 保活包，防主机 NAK 降频轮询（频率 ~1K/s，不饿死 OUT）。
 * @retval None
 */
void usb_cdc_keepalive_pump(void)
{
    uint32_t now;

    if (s_cdc_pdev == NULL)
        return;
    now = (uint32_t)osKernelGetTickCount();
    if ((uint32_t)(now - s_cdc_ka_tick) < 1U)
        return;
    __disable_irq();
    if (s_cdc_ep_busy == 0U && s_cdc_tx_pending == 0U) {
        s_cdc_ep_busy = 1U;
        s_cdc_ka_tick = now;
        usb_deveptx((usb_core_instance *)s_cdc_pdev, CDC_IN_EP,
                    (uint8_t *)&s_cdc_keepalive, 1U);
    }
    __enable_irq();
}

/**
 * @brief  v9.82: CDC 发送一个包（与常驻保活协作，无竞争）。
 *         置待发标志 → datain ISR 在下一个 XFRC 装载该包 → 等其 XFRC 完成。
 * @param  [in] buf  数据缓冲（必须存活到传输完成）
 * @param  [in] len  数据长度（>0）
 * @retval None
 */
void usb_cdc_tx_arm(const uint8_t *buf, uint32_t len)
{
    uint32_t done0;
    uint32_t tmo;

    if (buf == NULL || len == 0U)
        return;

    s_cdc_tx_buf = (volatile uint8_t *)buf;
    s_cdc_tx_len = (uint16_t)len;
    __disable_irq();
    s_cdc_tx_pending = 1U;
    __enable_irq();

    /* v9.82d: 先给 ISR 协作路径 ~0.3ms 窗口（保活链活着时 ISR 会接管） */
    done0 = s_cdc_tx_done;
    tmo = 10000U;
    while (s_cdc_tx_done == done0 && --tmo != 0U) { }

    if (s_cdc_tx_done == done0) {
        /* 协作路径未生效（保活链断/端点空闲）→ 直接装载，确保数据发出 */
        __disable_irq();
        if (s_cdc_ep_busy == 0U && s_cdc_tx_pending != 0U) {
            s_cdc_tx_pending = 0U;
            s_cdc_tx_last_real = 1U;
            s_cdc_tx_seq++;
            s_cdc_ep_busy = 1U;
            usb_deveptx((usb_core_instance *)s_cdc_pdev, CDC_IN_EP,
                        (uint8_t *)s_cdc_tx_buf, (uint32_t)s_cdc_tx_len);
        }
        __enable_irq();
        /* 等其 XFRC 完成（ISR 会 last_real→done++） */
        done0 = s_cdc_tx_done;
        tmo = 300000U;
        while (s_cdc_tx_done == done0 && --tmo != 0U) { }
    }
}

/**
 * @brief  get the configuration descriptor
 * @param  [in] length          length of configuration descriptor in bytes
 * @retval the pointer to configuration descriptor buffer
 */
uint8_t *usb_dev_cdc_getcfgdesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(usb_dev_cdc_cfgdesc);
    return usb_dev_cdc_cfgdesc;
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
