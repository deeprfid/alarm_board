/*******************************************************************************
 * bsp_report.h
 * 0xAA Cmd 0x10 查询应答 payload 组装（3 字节）:
 *   Byte0 GPIO_IN  位图: bit0..2 = 雷达1/2/3 (PC14/PC13/PH2, 高=1)
 *                        bit3    = GPIO_IN1 摄像头 (PB0, 低有效->有信号=1)
 *                        bit4    = GPIO_IN2 继电器 (PB1, 低有效->有信号=1)
 *   Byte1 workmode 位图: bit0=LIGHT_ON bit1=SYNC_MODE bit2=RADAR_MODE
 *                        bit3=AICAM_MODE bit4=EAS_MODE
 *                        (各 0/1, 经 switch_decoder_pio_read 获取, 单通道独立读取)
 *   Byte2 alarm_done:    执行应答时固定上报 1
 * 本文件只做 payload 组装, 只调用 switch_decoder_pio_read, 不与其它业务混用。
 ******************************************************************************/
#ifndef __BSP_REPORT_H__
#define __BSP_REPORT_H__

#include <stdint.h>

/* 应答 payload 固定长度 */
#define BSP_REPORT_LEN      (3u)

/* Byte0 bit 定义 */
#define BSP_REPORT_BIT_RADAR1   (0x01u)   /* bit0: PC14 */
#define BSP_REPORT_BIT_RADAR2   (0x02u)   /* bit1: PC13 */
#define BSP_REPORT_BIT_RADAR3   (0x04u)   /* bit2: PH2  */
#define BSP_REPORT_BIT_GPIO_IN1 (0x08u)   /* bit3: PB0  摄像头(低有效) */
#define BSP_REPORT_BIT_GPIO_IN2 (0x10u)   /* bit4: PB1  继电器(低有效) */

/* Byte1 bit 定义 (workmode 开关位图) */
#define BSP_REPORT_BIT_LIGHT_ON   (0x01u) /* bit0: LIGHT_ON  (ch1) */
#define BSP_REPORT_BIT_SYNC_MODE  (0x02u) /* bit1: SYNC_MODE (ch2) */
#define BSP_REPORT_BIT_RADAR_MODE (0x04u) /* bit2: RADAR_MODE(ch3) */
#define BSP_REPORT_BIT_AICAM_MODE (0x08u) /* bit3: AICAM_MODE(ch4) */
#define BSP_REPORT_BIT_EAS_MODE   (0x10u) /* bit4: EAS_MODE  (ch5) */

/*
 * bsp_report_build - 组装 3 字节查询应答 payload
 *   out[0] = GPIO_IN 位图
 *   out[1] = workmode 位图
 *   out[2] = alarm_done (固定 1)
 * 返回 BSP_REPORT_LEN (3)
 */
uint8_t bsp_report_build(uint8_t *out);

#endif /* __BSP_REPORT_H__ */
