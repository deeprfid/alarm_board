/*******************************************************************************
 * radar_port.h -- 雷达串口硬件层(USART1 + DMA2 + TMR0 空闲超时)
 * 只负责"搬运字节", 不含任何协议语义。
 ******************************************************************************/
#ifndef __RADAR_PORT_H__
#define __RADAR_PORT_H__

#include "radar_cfg.h"

/* 初始化: 引脚/时钟/USART/DMA/TMR0/中断/缓冲 */
void radar_port_init(void);

/* 非阻塞发送: 0 = 已启动, <0 = 忙或参数错误 */
int32_t radar_port_write(const uint8_t *buf, uint16_t len);
uint8_t radar_port_tx_busy(void);

/* 发送完成中断没来时的兜底: 主循环调用; 超过 RADAR_TX_TIMEOUT_MS 未完成 -> 复位 TX 通路并放行 */
void radar_port_tx_watchdog(uint32_t now_ms);


/* 修改波特率(用于自适应探测); 内部会先丢弃接收缓冲里旧波特率的残留字节 */
void radar_port_set_baud(uint32_t baud);

/* 丢弃接收缓冲与 DMA 窗口里的残留字节 */
void radar_port_rx_flush(void);
uint32_t radar_port_get_baud(void);
/* 当前波特率是否表示得出来(0 = 分频/BRR 设不下, 探测时应跳过该档) */
uint8_t radar_port_baud_ok(void);

/* 收到字节时的回调(data 可能是 1 字节) */
void radar_port_set_rx_handler(void (*handler)(const uint8_t *data, uint16_t len));

/* 主循环调用: 把中断收到的字节交给上层回调 */
void radar_port_poll(void);

/* 统计: 接收缓冲丢弃字节数 / 接收到的总字节数 */
uint32_t radar_port_rx_drop(void);
uint32_t radar_port_rx_bytes(void);

#endif /* __RADAR_PORT_H__ */
