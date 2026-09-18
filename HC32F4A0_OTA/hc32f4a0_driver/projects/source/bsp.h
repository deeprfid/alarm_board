/**
 *******************************************************************************
 * @file  qspi/qspi_base/source/main.h
 * @brief This file contains the including files of main routine.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2025, Xiaohua Semiconductor Co., Ltd. All rights reserved.
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
#include "ev_hc32f4a0_lqfp176_bsp.h"
#include "qspi_flash.h"
#include "ring_buf.h"





#define IPRESET_PORT          (GPIO_PORT_B)
#define IPRESET_PIN           (GPIO_PIN_10)

#define BOARD_RADAR1_PORT     (GPIO_PORT_E)
#define BOARD_RADAR2_PORT     (GPIO_PORT_C)

#define BOARD_RADAR1_PIN      (GPIO_PIN_09)
#define BOARD_RADAR2_PIN      (GPIO_PIN_08)

#define BOARD_KEY_PORT     (GPIO_PORT_C)
#define BOARD_KEY1_PIN     (GPIO_PIN_04)
#define BOARD_KEY2_PIN     (GPIO_PIN_05)

#define BOARD_GPI_PORT     (GPIO_PORT_C)

#define BOARD_GPI1_PIN     (GPIO_PIN_10)
#define BOARD_GPI2_PIN     (GPIO_PIN_11)
#define BOARD_GPI3_PIN     (GPIO_PIN_12)

#define BOARD_BEEP_PORT     (GPIO_PORT_B)

#define BOARD_BEEP_PIN      (GPIO_PIN_08)
#define BOARD_BUZZ_PIN      (GPIO_PIN_09)

#define BOARD_GPO_PORT      (GPIO_PORT_A)
#define BOARD_RELAY_PIN     (GPIO_PIN_10)
#define BOARD_DC12V_PIN     (GPIO_PIN_15)

#define LED_RGB_PORT        (GPIO_PORT_A)
#define LED_R_PIN           (GPIO_PIN_05)
#define LED_G_PIN           (GPIO_PIN_06)
#define LED_B_PIN           (GPIO_PIN_07)
#define LED_PORT            (GPIO_PORT_E)
#define LED_1_PIN           (GPIO_PIN_02)
#define LED_2_PIN           (GPIO_PIN_03)
#define LED_3_PIN           (GPIO_PIN_04)
#define LED_4_PIN           (GPIO_PIN_05)
/* LED toggle definition */
#define BOARD_BEEP_TOGGLE()      (GPIO_TogglePins(BOARD_BEEP_PORT, BOARD_BEEP_PIN))
#define BOARD_BUZZ_TOGGLE()      (GPIO_TogglePins(BOARD_BEEP_PORT, BOARD_BUZZ_PIN))

#define BOARD_RELAY_TOGGLE()      (GPIO_TogglePins(BOARD_GPO_PORT, BOARD_RELAY_PIN))
#define BOARD_DC12V_TOGGLE()      (GPIO_TogglePins(BOARD_GPO_PORT, BOARD_DC12V_PIN))


#define LED_R_TOGGLE()      (GPIO_TogglePins(LED_RGB_PORT, LED_R_PIN))
#define LED_G_TOGGLE()      (GPIO_TogglePins(LED_RGB_PORT, LED_G_PIN))
#define LED_B_TOGGLE()      (GPIO_TogglePins(LED_RGB_PORT, LED_B_PIN))

#define LED_1_TOGGLE()      (GPIO_TogglePins(LED_PORT, LED_1_PIN))
#define LED_2_TOGGLE()      (GPIO_TogglePins(LED_PORT, LED_2_PIN))
#define LED_3_TOGGLE()      (GPIO_TogglePins(LED_PORT, LED_3_PIN))
#define LED_4_TOGGLE()      (GPIO_TogglePins(LED_PORT, LED_4_PIN))


void Board_GPIO_Init(void);
void Board_GPIO_Scan(void);
void Usart_init(void);
void Timera_init(void);
void Board_LED_toggle(void);
#endif /* __MAIN_H__ */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
