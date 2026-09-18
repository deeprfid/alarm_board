#include "usb_dev_user.h"
#include "usb_dev_driver.h"   /* v9.81r: usb_deveptx 澹版槑锛堟秷闅愬紡澹版槑 warning锛?*/
#include "usb_dev_desc.h"
#include "cdc_data_process.h"
#include "usb_bsp.h"
#include "usb_dev_hid_cdc_wrapper.h"
#include "usb_dev_cdc_msc_wrapper.h"   /* v9.81ci: usb_dev_composite_cbk (CDC+MSC) */
#include "usb_dev_custom_hid_class.h"
#include "usb_dev_keyboard_class.h"
#include "usb_dev_winusb_class.h"   /* v1.10: WinUSB 独立模式（OTA 快通道，绕过 usbser.sys 265ms） */
#include "usb_dev_cdc_class.h"      /* v9.82: usb_cdc_tx_arm（保活协作发送） */
#include "hc32f46_driver.h"
#include "io_stream.h"
#include "string.h"

usb_core_instance  usb_dev;

volatile int gIsUsbAvailable = 0;    /* set by USB user devcfg callback */
volatile int cus_hid_rxbuf_rindex = 0;
/* F460 compat symbols referenced by io_stream (midwares replaced class buffers) */
int gUsbCompType = 2;                    /* HidCdc: CDC used for OTA */
/* v9.82d: CDC/WinUSB 共用一个 16KB RX 环形缓冲（模式互斥，同一时刻只有一个在用），
 * 省 16KB RAM 给标签推送等业务。cus_hid_rx_buf 独立（custom HID 类用）。 */
uint8_t usb_compo_rx_buf[USB_COMPO_RXBUF_LEN];
uint8_t cus_hid_rx_buf[1024];            /* HID RX buffer (io_stream ref) */

volatile int g_usb_hid_mode = 0;         /* v9.81cm: 1=HID閿洏妯″紡锛堟弿杩扮 PID/浜у搧鍚嶅尯鍒嗭紝闃?Windows 椹卞姩缂撳瓨鍐茬獊锛?*/
volatile int g_usb_winusb_mode = 0;      /* v1.10: 1=WinUSB 独立模式（OTA 快通道） */
volatile int g_usb_winusb_xfer_done = 1; /* v1.10: WinUSB IN XFRC 完成标志 */
extern volatile int g_usb_cdc_xfer_done;   /* CDC IN XFRC 完成标志（usb_dev_cdc_datain 置位） */
static uint32_t s_cs_cnt = 0;   /* v1.22 诊断: CDC XFRC 等待计数 */


int IsUsbAvailable(void)
{
    return gIsUsbAvailable;
}

int init_usb(int type)
{
    gUsbParams[0].isBlock = O_BLOCK;
    gUsbParams[0].timeout = -1;
    gUsbParams[0].recvbuf = cus_hid_rx_buf;
    gUsbParams[0].recvbufsize = sizeof(cus_hid_rx_buf);   /* v9.82d: 原误用 16KB，1KB 缓冲会溢出 */
    gUsbParams[0].usb_head = 0;
    gUsbParams[0].usb_tail = 0;

    gUsbParams[1].isBlock = O_BLOCK;
    gUsbParams[1].timeout = -1;
    gUsbParams[1].recvbuf = usb_compo_rx_buf;
    gUsbParams[1].recvbufsize = USB_COMPO_RXBUF_LEN;
    gUsbParams[1].usb_head = 0;
    gUsbParams[1].usb_tail = 0;

    /* midwares: usb_dev_init with port identify (USBHS + embedded PHY) */
    {
        stc_usb_port_identify stcPortIdentify;
        stcPortIdentify.u8CoreID = USBHS_CORE_ID;   /* v1.10: 引脚匹配（USBH_DM/DP），内部 PHY 全速 */
        stcPortIdentify.u8PhyType = USBHS_PHY_EMBED;
        /* v9.81cm: 鍙?USB 妯″紡鎸?usb_type 鍒嗘淳锛?         *   rdr_st_set_usb_type_KeyHid(1): HID 閿洏妯″紡锛堝厤椹卞姩锛屾爣绛剧粡閿洏杈撳嚭锛?         *   鍏跺畠/榛樿: CDC+MSC 澶嶅悎锛堜覆鍙?OTA + 铏氭嫙 U 鐩樺崌绾э級 */
        g_usb_hid_mode = (type == rdr_st_set_usb_type_KeyHid) ? 1 : 0;
        if (type == rdr_st_set_usb_type_WinUsb) {
            /* v1.10: WinUSB 独立模式——OTA 走批量端点(EP4/EP5)，Windows 经 MS OS 描述符自动绑 WinUSB.sys，
             * 绕过 usbser.sys 稀疏包 URB 265ms 限制（USB OTA 20s -> ~5s） */
            g_usb_winusb_mode = 1;
            gUsbParams[2].isBlock = O_BLOCK;
            gUsbParams[2].timeout = -1;
            gUsbParams[2].recvbuf = usb_compo_rx_buf;
            gUsbParams[2].recvbufsize = USB_COMPO_RXBUF_LEN;
            gUsbParams[2].usb_head = 0;
            gUsbParams[2].usb_tail = 0;
            TRACE("[usb] init WinUSB mode\r\n");
            usb_dev_init(&usb_dev, &stcPortIdentify, &user_desc, &class_winusb_cbk, &user_cb);
        } else if (g_usb_hid_mode) {
            TRACE("[usb] init HID Keyboard mode (hw_inf=%d)\r\n", g_usb_hid_mode);
            usb_dev_init(&usb_dev, &stcPortIdentify, &user_desc, &usb_dev_keyboard_cbk, &user_cb);
        } else {
            TRACE("[usb] init CDC+MSC mode\r\n");
            usb_dev_init(&usb_dev, &stcPortIdentify, &user_desc, &usb_dev_composite_cbk, &user_cb);
        }
    }
    /* v9.82c: 保活点射改走 USB SOF 中断（每 1ms 一次），不依赖 RTOS 定时器 */
    return 0;
}

static void kbd_wait_tx_done(void)
{
    extern volatile int g_kbd_tx_done;
    uint32_t timeout = 500000;
    g_kbd_tx_done = 0;
    while (g_kbd_tx_done == 0 && --timeout != 0U) {
        if ((timeout & 0x3FFU) == 0U) { osThreadYield(); }   /* v9.81cn: 定期让出 CPU，不阻塞其他任务 */
}
}

void send_key(uint8_t key, int isupper)
{
    static uint8_t report[8];   /* v9.81cn: 闈欐€佺紦鍐插尯鈥斺€擴SB 涓柇寮傛璇?xfer_buff锛屾爤鍙橀噺浼氬け鏁?*/

    if (gIsUsbAvailable == 0)
        return;

    memset(report, 0, 8);
    report[2] = key;

    if (key >= 0x04 && key <= 0x09 && isupper == 1)
        report[0] = 0x02;

    /* v9.81cn: 鎸変笅鍖?鈫?绛?XFER_COMPL 鈫?閲婃斁鍖?鈫?绛?XFER_COMPL銆?     * 淇濊瘉 Windows 渚濇璇诲埌"鎸変笅/閲婃斁"涓や釜鐙珛鎶ュ憡锛屽瓧绗︿笉涔卞簭涓嶄涪鍖呫€?*/
    usb_dev_mouse_txreport(&usb_dev, report, 8); /* 鎸変笅 */
    kbd_wait_tx_done();   /* v9.81cn: 按下包发完再发释放 */
    memset(report, 0, 8);
    usb_dev_mouse_txreport(&usb_dev, report, 8); /* 閲婃斁鎸夐敭锛坮eport 娓呴浂锛?*/
    kbd_wait_tx_done();   /* v9.81cn: 按下包发完再发释放 */
}

/* v9.81cm: HID 閿洏妯″紡杈撳嚭 EPC锛坔ex 澶у啓 + 鍥炶溅锛屾壂鐮佹灙琛屼负锛?*/
void hid_kbd_type_epc(const uint8_t *epc, uint8_t len)
{
    static const uint8_t hexkey[16] = {
        0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,   /* v9.81cn: 0-9锛圲SB HID 鎵弿鐮侊細0=0x27, 1=0x1E...9=0x26锛屽師琛ㄩ敊浣嶈嚧鏁板瓧+1锛?*/
        0x04, 0x05, 0x06, 0x07, 0x08, 0x09,                           /* A-F */
    };
    uint8_t i, hi, lo;

    if (len == 0 || len > 16)
        return;
    for (i = 0; i < len; i++) {
        hi = (epc[i] >> 4) & 0xF;
        lo = epc[i] & 0xF;
        send_key(hexkey[hi], hi >= 10);
        send_key(hexkey[lo], lo >= 10);
    }
    send_key(0x28, 0);   /* Enter */
}

/*
void deinitUSB()
{
	hd_usb_gintdis(&usb_dev);
	CLK_UpllCmd(Disable);
}
*/
/*
int usb_send(const void *buf, uint32_t len)
{
	int i;
	int scnt = len / 64;
	int lastcnt = len % 64;
	if (gIsUsbAvailable == 0)
		return -1;
	for (i = 0; i < scnt; ++i)
    	usb_dev_hid_txreport(&usb_dev, (uint8_t *)buf+i*64, 64);
	if (lastcnt != 0)
    	usb_dev_hid_txreport(&usb_dev, (uint8_t *)buf+i*64, lastcnt);
	return len;
}
*/

int usb_send(int uid, const void *buf, uint32_t len)
{
    int i;
    int j;
    int scnt;
    int lastcnt;
    uint16_t crc;

    if (gIsUsbAvailable == 0) {
        return -1;
    }
    /* v9.81cm: HID 閿洏妯″紡鏃?CDC 鎺ュ彛 鈫?涓㈠純涓婁綅鏈哄崗璁啓锛堟爣绛剧粡閿洏 send_key 杈撳嚭锛?*/
    if (uid == 1 && g_usb_hid_mode) {
        return 0;
    }

//	printf("usb_send start ---------------------------\n");
    if (uid == 0)
    {
        uint8_t report[64];
        scnt = len / 61;
        lastcnt = len % 61;
        crc = 0;

        for (i = 0; i < scnt; ++i)
        {
            crc = 0;
            report[0] = 61;
            memcpy(report + 1, (uint8_t *)buf + i * 61, 61);

            for (j = 0; j < 61; ++j)
                crc += ((uint8_t *)buf + i * 61)[j];

            report[62] = (crc >> 8) & 0xff;
            report[63] = (crc >> 0) & 0xff;
    /* usb_dev_hid_txreport(&usb_dev, report, 64); */
            /*
            printf("usb send:");
            for (j = 0; j < 64;++j)
            	printf("%02X ", report[j]);
            printf("\n");
            */
        }

        if (lastcnt != 0)
        {
            crc = 0;
            report[0] = lastcnt;
            memcpy(report + 1, (uint8_t *)buf + i * 61, lastcnt);

            for (j = 0; j < lastcnt; ++j)
                crc += ((uint8_t *)buf + i * 61)[j];

            report[lastcnt + 1] = (crc >> 8) & 0xff;
            report[lastcnt + 2] = (crc >> 0) & 0xff;
    /* usb_dev_hid_txreport(&usb_dev, report, 64); */
            /*
            printf("usb send:");
            for (j = 0; j < lastcnt+3;++j)
            	printf("%02X ", report[j]);
            printf("\n");
            */
        }
    }
    else if (uid == 1)
    {
        uint32_t t_cs0 = (uint32_t)osKernelGetTickCount();   /* v1.22 诊断 */
        scnt = len / 62;
        lastcnt = len % 62;

        /* root-fix: usb_deveptx 异步（仅写寄存器，TXFEMP ISR 填 FIFO，XFRC 才算完成）。
         * 连续两次 usb_deveptx 时第二次发生在第一次传输中（EPENA 仍置位）→ epntransbegin
         * 重复置 EPENA 无效且覆盖 DIEPTSIZ → 第二段被吞。每段发送后等 XFRC 完成。 */
        for (i = 0; i < scnt; ++i)
            usb_cdc_tx_arm((const uint8_t *)buf + i * 62, 62);

        if (lastcnt != 0)
            usb_cdc_tx_arm((const uint8_t *)buf + i * 62, (uint32_t)lastcnt);
        /* v9.82d: CDC XFRC 等待诊断，每 10 条一打（原每 5 条） */
        if (++s_cs_cnt % 10 == 0)
            TRACE("cs: len=%lu %lums\n", (unsigned long)len,
                  (unsigned long)((uint32_t)osKernelGetTickCount() - t_cs0));

//		usb_deveptx(&usb_dev, CDC_IN_EP, (uint8 *)buf, len);
    }
    else
        return -1;

    return len;
}

/* v1.10: WinUSB 发送（EP5 IN）——与 usb_send(uid=1) 同款：等 XFRC 完成防连续覆盖 */
static uint32_t s_ws_cnt = 0;   /* v9.82d: ws TRACE 节流计数 */
int winusb_send(const void *buf, uint32_t len)
{
    int i;
    int scnt;
    int lastcnt;
    uint32_t t_w0;

    if (gIsUsbAvailable == 0 || !g_usb_winusb_mode)
        return -1;

    scnt = (int)(len / 62);
    lastcnt = (int)(len % 62);

    t_w0 = (uint32_t)osKernelGetTickCount();
    for (i = 0; i < scnt; ++i)
        usb_winusb_tx_arm((const uint8_t *)buf + i * 62, 62);

    if (lastcnt != 0)
        usb_winusb_tx_arm((const uint8_t *)buf + i * 62, (uint32_t)lastcnt);
    if (len > 1 && (++s_ws_cnt % 10) == 0)   /* v9.82d: 每 10 条一打（原每条，50 条刷屏） */
        TRACE("ws: len=%lu %lums\n", (unsigned long)len,
              (unsigned long)((uint32_t)osKernelGetTickCount() - t_w0));
    return (int)len;
}


int usb_recv(int uid, void *buf, uint32_t len)
{
    int recvLen = 0;
    uint16 usb_tail_now;

    if(gIsUsbAvailable == 0)
        return 0;

    commonUsbParaLocal *ubpara = &gUsbParams[uid];

    usb_tail_now = ubpara->usb_tail;   /* v1.0: no irq-disable - ACK flow control prevents concurrent RX; irq-disable caused FIFO overrun with big batches */

    if (usb_tail_now == ubpara->usb_head)
        return 0;
    else
    {
//		printf("1111 windex:%d, rindex:%d, len:%d\n", ubpara->usb_tail,
//			ubpara->usb_head, len);
        if (usb_tail_now > ubpara->usb_head)
        {
            recvLen = usb_tail_now - ubpara->usb_head;

            if(recvLen > len)
                recvLen = len;

            memcpy_byb(buf, ubpara->recvbuf + ubpara->usb_head, recvLen);
            ubpara->usb_head += recvLen;
//			printf("00000  recvLen:%d\n", recvLen);
        }
        else
        {
            recvLen = ubpara->recvbufsize - ubpara->usb_head;

            if(recvLen > len)
                recvLen = len;

            memcpy_byb(buf, ubpara->recvbuf + ubpara->usb_head, recvLen);
            ubpara->usb_head += recvLen;

//			printf("11111  recvLen:%d\n", recvLen);
            if (recvLen < len)
            {
                int band2len = len - recvLen;

                if (usb_tail_now <= band2len)
                    band2len = usb_tail_now;

                memcpy_byb((char *)buf + recvLen, ubpara->recvbuf, band2len);
                recvLen += band2len;
                ubpara->usb_head = band2len;
//				printf("11111  band2len:%d\n", band2len);
            }
        }
    }

//	printf("2222 windex:%d, rindex:%d, len:%d\n", ubpara->usb_tail,
//		ubpara->usb_head, len);
    /*
    for (i = 0; i < recvLen; ++i)
    	printf("%02X ", ((char*)buf)[i]);
    printf("\n");
    */
    if (ubpara->usb_head >= ubpara->recvbufsize)
        ubpara->usb_head = 0;

    return recvLen;
}

