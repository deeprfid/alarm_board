/**
 * @file ota_usb.c
 * @brief USB1(CDC) 本地 OTA 通道实现：线程轮??USB1 ??流式 feed
 *        ACK/RESUME 通过 write(USB1) 回发（与 UART0 通道互不干扰?? */
#include <string.h>
#include "hc32f46_driver.h"
#include "ota_transport_uart.h"
#include "ota_usb_stream.h"
#include "ota_usb.h"
#include "app_conf.h"      /* v9.81cm: gPRdrStaSet（usb_type 配置??*/

/* extern from driver_lib (hc32_ll.h not includable due to header conflict) */
extern void LL_PERIPH_WE(uint32_t u32Peripheral);
#define LL_PERIPH_PWC_CLK_RMU  (1UL << 6U)
#define LL_PERIPH_FCG          (1UL << 1U)

void ota_usb_start(void)
{

    /* v9.81cm: USB 初始化模式按运行参数"上传接口"判定??     *   HID键盘(hw_inf==3) ??只初始化 HID 键盘，不启动 ota_usb_task（无 CDC 可读）；
     *   其它/被动模式 ??CDC+MSC：初始化 + 启动 ota_usb_task（USB-CDC OTA 通道??*/
    if (gRtSetting != NULL && gRtSetting->upload.hw_inf == Upload_Inf_HidKb) {
        (void)init_usb(rdr_st_set_usb_type_KeyHid);
        TRACE("[usb] HID Keyboard mode, CDC task NOT started\n");
        return;
    }
    if (gRtSetting != NULL && gRtSetting->upload.hw_inf == Upload_Inf_WinUsb) {
        /* v1.10: WinUSB 独立模式 - OTA+上传走批量端点(EP4/EP5), 绕过 usbser.sys 265ms */
        (void)init_usb(rdr_st_set_usb_type_WinUsb);
        TRACE("[usb] WinUSB mode\n");

        return;
    }
    (void)init_usb(rdr_st_set_usb_type_HidCdc);
    TRACE("[usb] CDC+MSC mode\n");
    /* v9.81cn: ??? USB OTA ?????????????????????CDC ?? OTA ??????裩 */
    /* v1.0: USB polling by ota_dispatch_task (ota_integration.c); this fn only init_usb */
}
