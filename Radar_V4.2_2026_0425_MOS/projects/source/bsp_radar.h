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


#include "hc32_ll.h"


#define RADARFRAMEHEAD  0XF1F2F3F4
#define RADARFRAMEEND   0XF5F6F7F8

#define BOARD_LED_1_PORT          (GPIO_PORT_A)
#define BOARD_LED_1_PIN           (GPIO_PIN_11)
#define BOARD_LED_2_PIN           (GPIO_PIN_12)

#define RADAR_BOARD_LED_G_PORT          (GPIO_PORT_B)
#define RADAR_BOARD_LED_G_PIN           (GPIO_PIN_03)

#define RADAR_PORT0                     (GPIO_PORT_C)
#define RADAR_PIN0                      (GPIO_PIN_14)

#define RADAR_PORT1                     (GPIO_PORT_C)
#define RADAR_PIN1                      (GPIO_PIN_13)

#define RADAR_PORT2                     (GPIO_PORT_H)
#define RADAR_PIN2                      (GPIO_PIN_02)


typedef struct {
    uint8_t  framehead[4];
    uint8_t  datalen[2];
    uint8_t  target_data;
    uint8_t  datafieldhead;
    uint8_t  target_state;
    uint8_t  moving_distance[2];
    uint8_t  moving_energy;
    uint8_t  sta_distance[2];
    uint8_t  sta_energy;
    uint8_t  detection_range[2];
    uint8_t  datafielend;
    uint8_t  check;
    uint8_t  frameend[4];
    } stc_radar_frame_t; // radar detection distance unit:cm

    
typedef struct {
    uint8_t  framehead[4];
    uint8_t  datalen[2];
    uint8_t  target_data;
    uint8_t  datafieldhead;
    uint8_t  target_state;
    uint8_t  moving_distance[2];
    uint8_t  moving_energy;
    uint8_t  sta_distance[2];
    uint8_t  sta_energy;
    uint8_t  detection_range[2];
    uint8_t  movedoor_cnt;
    uint8_t  stadoor_cnt;
    uint8_t  movedoor_energy_each[9];
    uint8_t  sta_energy_each[9];
    uint8_t  lightsensor;
    uint8_t  outpin;
    uint8_t  datafielend;
    uint8_t  check;
    uint8_t  frameend[4];
    } engineer_frame_t; // engineering mode radar detection distance unit:cm
    
    
    
typedef struct {
    uint16_t target_state;
    uint16_t moving_target_distance;
    uint16_t moving_target_energy;
    uint16_t stationary_target_distance;
    uint16_t stationary_target_energy;
    uint16_t detection_distance;
    uint16_t pinout;
    uint16_t targeted;
}stc_radar_scan_data_t;
void Rardar_init(void);
int32_t get_pdu_len(uint8_t *pdulen,uint8_t flag);
void config_frame_init(void);
void Board_LED_Init(void);
void Board_LED_On(void);
void Board_LED_Off(void); 
void Board_LED_Toggle(void);
void Check_Radar_state(void);
unsigned int GetNumU32(uint8_t *p);
void HashConfig(void);
void Ext_Init(void);
void Radar_singal_output(void);
en_pin_state_t Radar_singal_input(void);
void Alarm_Out_Enable(void);
void radar_init_by_uart(void);
void Radar_DMA_Trans(const void *pvBuf, uint32_t u32Len);
/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
