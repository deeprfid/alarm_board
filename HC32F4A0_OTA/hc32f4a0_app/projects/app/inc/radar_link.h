/**
 * @file  radar_link.h
 * @brief F4A0 <-> HC32F460 报警板 业务链路（项目 2：F4A0 直接当轮询主机，没有 STM32）
 *
 * 端口 -> 天线映射（2026-09-19 现场确认）：
 *     COMMON_INTERFACE_RS485_1 (104, CM_USART3) -> 天线1
 *     COMMON_INTERFACE_RS485_2 (105, CM_USART8) -> 天线2,3
 *     COMMON_INTERFACE_RS485_3 (106, CM_USART5) -> 天线4
 *
 * 帧格式（与 F460 侧 common.c 的 0xAA 变长帧一致，逐字节对齐）：
 *     AA | lenv | cmd | addr | payload[lenv-2] | crc16_lo | crc16_hi
 *     lenv = plen + 2 ；crc16 = CCITT(0x1021) / init 0xFFFF，覆盖除两个 CRC 字节外的全部
 *     命令 0x10 = 查询 -> 板子回 3B: [gpioIn][workMode][alarmDone]
 *          gpioIn: bit0..2 = 雷达1..3, bit3 = GPIO_IN1, bit4 = GPIO_IN2
 *          radarVal = (gpioIn & 0x07) != 0
 *     命令 0x81 = 报警上报
 *
 * 【收发机制】不自己写 ISR：
 *   RX —— 驱动已在中断里填环形缓冲，这里只用 read(fd) 取（等价于"用中断收"）；
 *   TX —— Uart_RS485_send() 是逐字节阻塞轮询（TX_EMPTY 自旋），【不丢帧】，但它
 *         (a) 没有互斥锁 (b) 没有超时。故本模块自行加按口互斥，并用本文件的
 *         radar_send_frame() 走 DDL 直写 + 自旋超时，避免串口异常时死等。
 */
#ifndef RADAR_LINK_H
#define RADAR_LINK_H

#include <stdint.h>

#define RADAR_LINK_NUM      3u      /* 3 条 RS485 总线 */

typedef struct {
    uint8_t  online;        /* 1 = 本口最近有合法应答（fresh） */
    uint8_t  gpio_in;       /* 0x10 应答 Byte0 */
    uint8_t  work_mode;     /* 0x10 应答 Byte1 */
    uint8_t  alarm_done;    /* 0x10 应答 Byte2 */
    uint8_t  radar_val;     /* (gpio_in & 0x07) != 0 */
    uint8_t  bad_frames;    /* 线上出现过帧头但校验/长度不对被丢的帧数（饱和 255） */
    uint8_t  tx_err;        /* 发送失败计数（超时） */
    uint32_t last_rx_ms;    /* 最近一次合法应答时刻 */
    uint32_t tx_frames;     /* 已发出的查询帧数 */
    uint32_t rx_frames;     /* 已收到的合法帧数 */

    /* ---- 老定长帧(0xFF, 32B alarm_confirm_package)专用 ---- */
    uint8_t  leg_device_id;     /* 定长帧 deviceID */
    uint8_t  leg_alarm_done;    /* 定长帧 alarm_done */
    uint8_t  leg_got;           /* 至少收到过一帧合法的定长帧 */
    uint32_t leg_frames;        /* 合法定长帧计数 */
} radar_link_t;

/* 按链路下标（0/1/2 对应 RS485_1/2/3）取状态；越界返回 NULL */
const radar_link_t *radar_link_get(uint8_t idx);

/* 天线号(1..4) -> 链路下标(0/1/2)；无此天线返回 -1 */
int radar_link_of_antenna(uint8_t ant);

/* 天线号(1..4) 当前是否有人（radarVal）。无此天线返回 0 */
uint8_t radar_link_antenna_alarm(uint8_t ant);

void radar_link_init(void);     /* 开三个口、配非阻塞。启动时调一次 */
void radar_link_poll(void);     /* 周期推进（建议 5~20ms）。放独立线程里循环调 */

/* 诊断：本口最近是否 fresh（200ms 内有应答） */
#define RADAR_LINK_FRESH_MS   200UL

#endif /* RADAR_LINK_H */
