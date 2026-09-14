/*******************************************************************************
 * radar_port.h -- 雷达串口硬件层(多口: 每口一个 USART + 逐字节中断接收 + 轮询发送)
 ******************************************************************************/
#ifndef __RADAR_PORT_H__
#define __RADAR_PORT_H__

#include "radar_cfg.h"

#define RADAR_PORT_CNT                  (3U)      /* 雷达口数量(与 RADARn_UART_* 对应) */

int32_t  radar_port_init(uint8_t port);
int32_t  radar_port_write(uint8_t port, const uint8_t *buf, uint16_t len);
uint8_t  radar_port_tx_busy(uint8_t port);
void     radar_port_tx_watchdog(uint8_t port, uint32_t now_ms);
void     radar_port_set_baud(uint8_t port, uint32_t baud);
void     radar_port_rx_flush(uint8_t port);
uint32_t radar_port_get_baud(uint8_t port);
uint8_t  radar_port_baud_ok(uint8_t port);   /* 0 = 该档分频/BRR 没设下(换档失败) */
uint32_t radar_port_brr(uint8_t port);       /* BRR 回读: 高字节=整数分频 */
uint32_t radar_port_baud_actual(uint8_t port);   /* 硬件实际波特率(反推) */
void     radar_port_set_rx_handler(uint8_t port, void (*handler)(const uint8_t *data, uint16_t len));
void     radar_port_poll(uint8_t port);      /* 主循环调用: 发送泵 + 环形缓冲交给上层 */
uint32_t radar_port_rx_drop(uint8_t port);
uint32_t radar_port_rx_bytes(uint8_t port);

#endif /* __RADAR_PORT_H__ */
