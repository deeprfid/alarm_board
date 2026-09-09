/*******************************************************************************
 * bsp_report.h
 * 0xAA Cmd 0x10 query reply payload (3 bytes):
 *   Byte0 GPIO_IN  bitmap: bit0..2 = radar1/2/3 (PC14/PC13/PH2, high=1)
 *                        bit3    = GPIO_IN1 camera (PB0, low-active -> active=1)
 *                        bit4    = GPIO_IN2 relay (PB1, low-active -> active=1)
 *   Byte1 workmode bitmap: bit0=LIGHT_ON bit1=SYNC_MODE bit2=RADAR_MODE
 *                          bit3=AICAM_MODE bit4=EAS_MODE
 *                          (each 0/1 via switch_decoder_pio_read, single channel)
 *   Byte2 alarm_done:    fixed 1 while answering
 * This file only assembles payload and only calls switch_decoder_pio_read.
 ******************************************************************************/
#ifndef __BSP_REPORT_H__
#define __BSP_REPORT_H__

#include <stdint.h>

/* reply payload fixed length */
#define BSP_REPORT_LEN      (3u)

/* Byte0 bit definitions */
#define BSP_REPORT_BIT_RADAR1   (0x01u)   /* bit0: PC14 */
#define BSP_REPORT_BIT_RADAR2   (0x02u)   /* bit1: PC13 */
#define BSP_REPORT_BIT_RADAR3   (0x04u)   /* bit2: PH2  */
#define BSP_REPORT_BIT_GPIO_IN1 (0x08u)   /* bit3: PB0  camera (low-active) */
#define BSP_REPORT_BIT_GPIO_IN2 (0x10u)   /* bit4: PB1  relay (low-active) */

/* Byte1 bit definitions (workmode switch bitmap) */
#define BSP_REPORT_BIT_LIGHT_ON   (0x01u) /* bit0: LIGHT_ON  (ch1) */
#define BSP_REPORT_BIT_SYNC_MODE  (0x02u) /* bit1: SYNC_MODE (ch2) */
#define BSP_REPORT_BIT_RADAR_MODE (0x04u) /* bit2: RADAR_MODE(ch3) */
#define BSP_REPORT_BIT_AICAM_MODE (0x08u) /* bit3: AICAM_MODE(ch4) */
#define BSP_REPORT_BIT_EAS_MODE   (0x10u) /* bit4: EAS_MODE  (ch5) */

/*
 * bsp_report_build - build 3-byte query reply payload
 *   out[0] = GPIO_IN bitmap
 *   out[1] = workmode bitmap
 *   out[2] = alarm_done (fixed 1)
 * returns BSP_REPORT_LEN (3)
 */
uint8_t bsp_report_build(uint8_t *out);

#endif /* __BSP_REPORT_H__ */
