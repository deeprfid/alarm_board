/**
 *******************************************************************************
 * @file  ota_integration.c
 * @brief OTA ?????????????v1.1???????????
 *
 * v1.1??OTA ????????ota_usb_task / ota_http_server / ota_uart_active_task??
 *       ???????? ota_dispatch_task????? USB + HTTP + ???????????? 32KB -> 16KB??
 *       ???????????????????git diff ???????????????? + HTTP select ????????
 *******************************************************************************
 */
#include "qspi_flash.h"   /* ?????? hc32f46_driver.h ??(hc32f4a0.h:3630) */
#include <string.h>
#include "hc32f46_driver.h"
#include "mp_pool.h"      /* malloc_hexp */
#include "app_conf.h"     /* OTA_FW_VERSION */
#include "hw_api.h"       /* hw_netconf??listenPort?? */
#include "ota_integration.h"
#include "ota_agent.h"
#include "ota_http.h"     /* http_handle_conn */
#include "ota_state.h"
#include "ota_storage.h"      /* OTA_QSPI_STAGE_BASE / ota_channel_busy */
#include "ota_transport_uart.h"
#include "ota_usb.h"
#include "ota_usb_stream.h"
#include "usb_msc_ota.h"

/* USB 回环测试模式：定义 USB_ECHO_TEST 即回显收据（临时诊断用），默认关闭走 OTA */

extern int ispassive;   /* user_main.c ????1=??????0=????????????????????? UART1?? */
extern volatile int g_usb_winusb_mode;   /* v1.10: WinUSB 独立模式 */
extern int winusb_send(const void *buf, uint32_t len);   /* v1.10 */
void ota_dispatch_start(void);   /* forward decl: ota_init_all calls it (defined below) */

/* ---------- ????????? user_main.c L816-836?? ---------- */
void ota_init_all(void)
{
    /* ?????????????????? ?? ??????????????????FlashDB KV ??????? */
    {
        uint8_t hdr[82];
        if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE, hdr, sizeof(hdr)) == 0 &&
            hdr[0] == 'O' && hdr[1] == 'T' && hdr[2] == 'A' && hdr[3] == '1') {
            uint32_t sv = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
                          ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
            if (sv != OTA_FW_VERSION) {
                TRACE("OTA: stage pkg v0x%08X != fw 0x%08X, reset progress\n",
                      (unsigned)sv, (unsigned)OTA_FW_VERSION);
                ota_set_progress(0);
            }
        }
    }

    ota_agent_boot();   /* check staged fw (FlashDB KV ready) */
    if (usb_msc_ota_check() > 0)   /* USB ??? fw.bin ???????? */
        system_reset();            /* ?????? bootloader ??????? */
    ota_usb_start();    /* init_usb??CDC+MSC ?? HID??????????? */
    ota_dispatch_start();   /* ?? OTA ???????USB + HTTP + ???????? */
}

/* ---------- ?1?????????? user_main.c L433?? ---------- */
void ota_confirm_after_init(void)
{
    ota_agent_confirm();   /* ???????????????1???????????????? */
}

/* ---------- ?? OTA ?????????? ota_usb_task + ota_http_server + ota_uart_active_task?? ---------- */
static void ota_dispatch_task(void *arg)
{
    TRACE("[ota] dispatch task started\n");
    uint8_t buf[512];   /* v9.82d: 512B 大缓冲排空（原 128B+阻塞5ms 排空极慢） */   /* v1.0: 128B read batch - short irq-disable in usb_recv (2048 disabled USB IRQ too long -> USB FIFO overrun dropped packets) */   /* v1.0: 2048-byte USB read batch - drain cdc_rx_buf faster (was 256, RX buffer overflow dropped OTA frames) */
    int rtimeout = 50;
    int hrtm = 5000;
    int n, fd;
    int nb = 1;              /* v9.82e-FIX: 非阻塞=O_NONBLOCK=1！0 是 O_BLOCK（阻塞，WinUSB 下 timeout=-1 永久卡死 HTTP） */
    int http_cnt = 0;   /* v1.0: HTTP poll throttle - avoid W5100S SPI every loop (was stalling USB/UART) */
    int uart_timeout = 10;   /* v1.0: active-mode UART1 short poll (was 50ms, stalled USB) */
    uint64 lastacttm = 0;
    commonUartPara otaPara;

    (void)arg;

    /* USB1??CDC ??????init_usb ???? ota_usb_start ??? */
    ioctl(COMMON_INTERFACE_USB1, COMMON_INTERFACE_SET_TIMEOUT, &rtimeout);
    ioctl(COMMON_INTERFACE_USB1, COMMON_INTERFACE_SET_ISBLOCK, &nb);
    ioctl(COMMON_INTERFACE_USB2, COMMON_INTERFACE_SET_ISBLOCK, &nb);
    ioctl(COMMON_INTERFACE_USB1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);

    /* HTTP OTA SOCKET2????? = listenPort+1?? */
    ioctl(COMMON_INTERFACE_SOCKET2, COMMON_INTERFACE_SET_TIMEOUT, &hrtm);

    /* ???????????? OTA ????????????????? UART1 ?? send_func ????????????????? */
    if (!ispassive) {
        memset(&otaPara, 0, sizeof(otaPara));
        otaPara.baudrate = 115200;
        otaPara.timeout  = 100;
        otaPara.isRdam   = 1;
        (void)uart_open(COMMON_INTERFACE_UART1, &otaPara);
        ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
        ioctl(COMMON_INTERFACE_UART1, COMMON_INTERFACE_SET_TIMEOUT, &uart_timeout);   /* v1.0: 10ms short poll */
        ota_transport_uart_init(COMMON_INTERFACE_UART1);
    }

#ifdef USB_ECHO_TEST
    /* ===== 回环测试模式：当前 USB 通道收什么回什么（高速验证 USB 双向通路，不喂 OTA） =====
     * v1.15: WinUSB(USB2) 也支持回显——验证 winusb 连续流是否也有 ~500ms 取走周期 */
    for (;;) {
        if (g_usb_winusb_mode) {
            n = read(COMMON_INTERFACE_USB2, buf, sizeof(buf));
            if (n > 0)
                (void)write(COMMON_INTERFACE_USB2, buf, (uint32_t)n);
            else
                sleep_ms(1);
        } else {
            n = read(COMMON_INTERFACE_USB1, buf, sizeof(buf));
            if (n > 0)
                (void)write(COMMON_INTERFACE_USB1, buf, (uint32_t)n);
            else
                sleep_ms(1);
        }
    }
#else
    /* v9.82e-diag: 循环入口打点（排查 HTTP/分发线程用，可放开） */
    // TRACE("[ota] dispatch loop enter, usb_winusb=%d\n", g_usb_winusb_mode);
    for (;;) {
        /* 1) USB OTA——CDC(USB1) 或 WinUSB(USB2, v1.10) */
        if (g_usb_winusb_mode) {
            while ((n = read(COMMON_INTERFACE_USB2, buf, sizeof(buf))) > 0) {
                if (ota_usb_stream_feed(COMMON_INTERFACE_USB2, buf, (uint32_t)n) > 0)
                    return;
            }
        } else {
            while ((n = read(COMMON_INTERFACE_USB1, buf, sizeof(buf))) > 0) {
                if (ota_usb_stream_feed(COMMON_INTERFACE_USB1, buf, (uint32_t)n) > 0)
                    return;   /* stream finished -> device resetting */
            }
            if (ota_usb_stream_idle_timeout())
                continue;
            (void)ota_transport_uart_idle_timeout();
        }

        /* 2) ?????????? OTA */
        if (!ispassive) {
            n = read(COMMON_INTERFACE_UART1, buf, sizeof(buf));
            if (n > 0)
                (void)ota_transport_uart_feed(COMMON_INTERFACE_UART1, buf, (uint32_t)n);
            else
                (void)ota_transport_uart_idle_timeout();
        }

        /* 3) HTTP OTA????? = listenPort+1?????????????? 10s??????? 100ms ?????????? */
        if (++http_cnt >= 10) {   /* v1.0: poll HTTP every 10 loops */
        fd = apt_single_select_nob(COMMON_INTERFACE_SOCKET2,   /* v1.0: non-blocking - do not stall USB/UART polling */
                               (unsigned short)(hw_netconf()->listenPort + 1),
                               &lastacttm, 100);
        /* v9.82e-diag: HTTP select 诊断（排查用，可放开）——fd>=0=收到连接 / -1=监听失败 / -2=无活动 */
        // if (http_cnt == 10 || fd >= 0 || (http_cnt % 200) == 0)
        //     TRACE("ota http: fd=%d port=%u\n", fd, (unsigned)(hw_netconf()->listenPort + 1));
        if (fd >= 0) {
            http_handle_conn(fd);
            disconnect(fd);
            close(fd);
        }
        /* fd==-2: no conn activity, keep polling USB/UART; fd==-1 socket err retry */
        }
        /* v9.82d-fix: 让出必须每轮执行（原在 http 块内 → 被动模式 9/10 轮忙转，
         * 饿死 W5100S 网络栈任务 → 升级重启后 HTTP 建连失败） */
        /* 真让出修复（同 F460）：sleep_ms(1)=osDelay(0) 不阻塞（tick 2ms，1/2=0），
         * 空载（无 USB/串口/网络的新板）时 High 忙转饿死业务/初始化线程 → 系统卡死；
         * 改 osDelay(1) = RTX 硬阻塞 1 tick(2ms)，每轮真正让出 */
        osDelay(1);
}
#endif   /* USB_ECHO_TEST */
}

void ota_dispatch_start(void)
{
    osThreadAttr_t thAttr_t;

    init_osThreadAttr_t(&thAttr_t, 1024 * 16, osPriorityNormal);   /* v1.35: 回退 v1.32 High——新板无连接时 dispatch 空转抢占 main 致初始化无法完成 */
    osThreadNew(ota_dispatch_task, NULL, &thAttr_t);
}

/* ---------- ????????send_func UART1 OTA ??????? Lan2Uart.c L577-606?? ---------- */
int ota_serial_pump(int fd, const uint8_t *head3)
{
    static unsigned char *otabuf = NULL;

    if (fd != COMMON_INTERFACE_UART1 ||
        head3[0] != 'O' || head3[1] != 'T' || head3[2] != 'A')
        return 0;

    /* OTA ????????????????d??????? 4KB ??? RAM??
     * ??????????????????? / ???????????? reboot ??????????????? */
    if (otabuf == NULL) {
        otabuf = (unsigned char *)malloc_hexp(OTA_FRAME_MAX_LEN + 16);
        if (otabuf == NULL) {
            TRACE("ota: no mem for otabuf\n");
            return 1;   /* ?????????????????????? continue ???? */
        }
    }
    memcpy(otabuf, head3, 3);
    if (read_n(COMMON_INTERFACE_UART1, otabuf + 3, 6) == 6 && otabuf[3] == '1') {
        uint16_t otalen = (uint16_t)(otabuf[7] | (otabuf[8] << 8));
        if (otalen <= OTA_FRAME_MAX_PAYLOAD &&
            read_n(COMMON_INTERFACE_UART1, otabuf + 9, otalen + 2) == (int)(otalen + 2)) {
            ota_transport_uart_handle_frame(otabuf,
                                            OTA_FRAME_HDR_LEN + otalen + OTA_FRAME_CRC_LEN);
        }
    }
    return 1;
}

/* ---------- send_func ?????? USB1 OTA ???????? Lan2Uart.c L564-571?? ---------- */
int ota_usb_feed_pump(int fd, const uint8_t *head3)
{
    if (fd != COMMON_INTERFACE_USB1 ||
        head3[0] != 'O' || head3[1] != 'T' || head3[2] != 'A')
        return 0;
    (void)ota_usb_stream_feed(COMMON_INTERFACE_USB1, head3, 3);
    return 1;
}

/* ---------- OTA ???????????? Lan2Uart.c L549-554 / reader_msg.c L1182-1188?? ---------- */
int ota_channel_pending(int fd)
{
    if ((fd == COMMON_INTERFACE_USB1 || fd == COMMON_INTERFACE_UART1) &&
        (ota_channel_busy() || ota_usb_stream_active()))
        return 1;
    return 0;
}
