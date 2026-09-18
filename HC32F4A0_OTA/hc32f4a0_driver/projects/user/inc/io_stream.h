#ifndef _IO_STREAM_H_
#define _IO_STREAM_H_
#include "type.h"
#include "timer.h"
#include "hc32f46_driver.h"


typedef struct
{
	uint8 *recvbuf;
	uint16 recvbufsize;
volatile	uint16 usb_head;
volatile	uint16 usb_tail;
	uint8	isBlock;
	int	timeout;
} commonUsbParaLocal;

extern commonUsbParaLocal gUsbParams[3];   /* v1.10: [2]=WinUSB */

#define USB_COMPO_RXBUF_LEN 16384   /* v1.0: 16384 = 4x4107 frame - bigger RX headroom for dispatch-thread polling (was 9216) */    /* v9.81cm: 9216 = 2脳4107(甯ч暱9+4096+2) 鏈変綑閲忊€斺€斿師 8192 瑁呬笉涓嬫壒 2 甯?8214B)锛岀 2 甯у熬閮ㄦ孩鍑鸿嚧 CRC MISMATCH/閲嶄紶鍣煶 */
extern uint8 usb_compo_rx_buf[];   /* v9.82d: CDC/WinUSB 共享 RX 环形缓冲 */
extern uint8 cus_hid_rx_buf[];

#endif




