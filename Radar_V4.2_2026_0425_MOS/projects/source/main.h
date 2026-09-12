/**
 *******************************************************************************
 * @file  usart/usart_uart_dma/source/main.h
 * @brief This file contains the including files of main routine.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2023, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */
#ifndef __MAIN_H__
#define __MAIN_H__

#include "hc32_ll.h"
#include "ev_hc32f460_lqfp100_v2_bsp.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"
#include "time.h"
#include "bsp_rs485.h"
#include "radar.h"
#include "hc32_ll_utility.h"
#include "ring_buf.h"
#include "bsp_beep.h"
#include "bsp_led.h"
#include "bsp_alarm.h"
#include "bsp_msg.h"
#include "bsp_pwm.h"
#include "bsp_key.h"
#include "bsp_exint.h"
#include "bma_ringbuffer.h"
#include "bsp_gpio.h"

#define LIGHT_ON           (0x1)
#define SYNC_MODE          (0x2)
#define RADAR_MODE         (0x3)
#define AICAM_MODE         (0x4)
#define EAS_MODE           (0x5)
#define AI_CAMERA          (11)
#define INput_RELAY        (12)

#define Custom_By_SZBMA             (0x00)
#define SYNC_WITH_LINUX_MAINBOARD   (0x00)
#define PDUHEAD                     (0xFF)
#define ALARM_G_CODE                (0xA5)
#define ALARM_R_CODE                (0x5A)
#define NONE_EAS_CODE               (0x64)
#define AUX_EAS_CODE                (0x32)
#define ALARM_RELAY_CODE            (0x55)
#define HASH_MSG_DIGEST_SIZE        (0x20)
#define HASH_TIMEOUT_VAL            (0x0A)

#define RING_BUF_SIZE                   (2048UL)



#define HASH_TIMEOUT_VAL            (0x0A)


#define MEM_ZERO_STRUCT(x)              do {                                   \
        memset((void*)&(x), 0L, (sizeof(x)));  \
    }while(0)



/* 雷达扫描数据(老 32B 确认帧字段)
 * 字段含义见 radar_proto.h 的 radar_report_t */
typedef struct {
    uint16_t target_state;
    uint16_t moving_target_distance;
    uint16_t moving_target_energy;
    uint16_t stationary_target_distance;
    uint16_t stationary_target_energy;
    uint16_t detection_distance;
    uint16_t pinout;
    uint16_t targeted;
} stc_radar_scan_data_t;

typedef struct
{
    unsigned char framehead;
    unsigned char deviceID;
    unsigned char alarm_done;
    unsigned char unused[5];
    stc_radar_scan_data_t radar;
    uint32_t  rngkey;
    uint16_t  uidkey;
    uint16_t  crc;
} alarm_confirm_package;


void Green_pass_Tag(void);
void Reguler_Tag(void);
void Relay_AM_EAS(void);
void Alarm_Off(void);
void Alarm_On(void);
void Alarm_SilenceCmd(void);
uint16_t Ucode_read(uint32_t *rngkey, uint16_t *uidkey);
void Radar_Led_update(void);
uint8_t bsp_get_radar_detection(void);
uint8_t bsp_get_radar_singal(void);
void bsp_RunPer10ms(void);
void ALARM_INT_Init(void);
en_pin_state_t switch_decoder_pio_read(uint8_t channel);
void switch_decoder_init(void);
void Relay_status_check(void);
void system_power_on(void);
void HashConfig(void);

#endif /* __MAIN_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
