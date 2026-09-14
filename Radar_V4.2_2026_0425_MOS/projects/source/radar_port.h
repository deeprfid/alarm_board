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

/* 波特率寄存器实际值(单值回读, 便于 Watch 手抄) */
extern volatile uint32_t g_radar_brr;

/* 单值诊断(Watch 里直接抄一个数字):
 *   g_radar_rx_bytes : 累计收到字节数(0 = DMA 一个字节都没搬进来)
 *   g_radar_pin_low  : RX 引脚采到低电平的次数(>0 = 线上确实有数据活动/起始位)
 *   g_radar_dma_left : RX DMA 通道剩余传输计数(等于窗口大小=没搬动; 递减=正在搬; 0=窗口满未重挂) */
extern volatile uint32_t g_radar_rx_bytes;
extern volatile uint32_t g_radar_pin_low;
extern volatile uint32_t g_radar_dma_left;
extern volatile uint32_t g_radar_rx_err;

/* 诊断计数(Keil Watch 里直接看这几个名字):
 *   g_radar_tx_dma_tc_cnt : TX DMA 传输完成次数    (0 = DMA 根本没跑/没触发)
 *   g_radar_tx_tci_cnt    : USART1 发送完成中断次数(0 = 上面有数但 TCI 没来 -> 中断映射问题)
 *   g_radar_tx_timeout_cnt: 兜底复位次数           (>0 说明完成中断一直没来, 已由看门狗放行) */
extern volatile uint32_t g_radar_tx_dma_tc_cnt;
extern volatile uint32_t g_radar_tx_tci_cnt;
extern volatile uint32_t g_radar_tx_timeout_cnt;

/* 修改波特率(用于自适应探测); 内部会先丢弃接收缓冲里旧波特率的残留字节 */
void radar_port_set_baud(uint32_t baud);

/* 丢弃接收缓冲与 DMA 窗口里的残留字节 */
void radar_port_rx_flush(void);
uint32_t radar_port_get_baud(void);

/* 收到字节时的回调(data 可能是 1 字节) */
void radar_port_set_rx_handler(void (*handler)(const uint8_t *data, uint16_t len));

/* 主循环调用: 把中断收到的字节交给上层回调 */
void radar_port_poll(void);

/* 统计: 接收缓冲丢弃字节数 / 接收到的总字节数 */
uint32_t radar_port_rx_drop(void);
uint32_t radar_port_rx_bytes(void);

#endif /* __RADAR_PORT_H__ */
