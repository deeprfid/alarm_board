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
#include "stdint.h"
#include "bsp_msg.h"
/* LED_R Port/Pin definition */
#define BOARD_LED_PORT          (GPIO_PORT_B)
#define BOARD_LED_PIN           (GPIO_PIN_03)






/**
 * @addtogroup HC32F460_DDL_Examples
 * @{
 */

/**
 * @addtogroup USART_UART_DMA
 * @{
 */

/*******************************************************************************
 * Local type definitions ('typedef')
 ******************************************************************************/
#define LL_PERIPH_SEL                   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
        LL_PERIPH_EFM | LL_PERIPH_SRAM)
/*******************************************************************************
 * Local pre-processor symbols/macros ('#define')
 ******************************************************************************/
/* Peripheral register WE/WP selection */

/* DMA definition */
#define RX_DMA_UNIT                     (CM_DMA1)
#define RX_DMA_CH                       (DMA_CH0)
#define RX_DMA_FCG_ENABLE()             (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA1, ENABLE))
#define RX_DMA_TRIG_SEL                 (AOS_DMA1_0)
#define RX_DMA_TRIG_EVT_SRC             (EVT_SRC_USART4_RI)
#define RX_DMA_RECONF_TRIG_SEL          (AOS_DMA_RC)
#define RX_DMA_RECONF_TRIG_EVT_SRC      (EVT_SRC_AOS_STRG)
#define RX_DMA_TC_INT                   (DMA_INT_TC_CH0)
#define RX_DMA_TC_FLAG                  (DMA_FLAG_TC_CH0)
#define RX_DMA_TC_IRQn                  (INT000_IRQn)
#define RX_DMA_TC_INT_SRC               (INT_SRC_DMA1_TC0)

#define TX_DMA_UNIT                     (CM_DMA1)
#define TX_DMA_CH                       (DMA_CH1)
#define TX_DMA_FCG_ENABLE()             (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA1, ENABLE))
#define TX_DMA_TRIG_SEL                 (AOS_DMA1_1)
#define TX_DMA_TRIG_EVT_SRC             (EVT_SRC_USART4_TI)
#define TX_DMA_TC_INT                   (DMA_INT_TC_CH1)
#define TX_DMA_TC_FLAG                  (DMA_FLAG_TC_CH1)
#define TX_DMA_TC_IRQn                  (INT001_IRQn)
#define TX_DMA_TC_INT_SRC               (INT_SRC_DMA1_TC1)

/* Timer0 unit & channel definition */
#define TMR0_UNIT                       (CM_TMR0_2)
#define TMR0_CH                         (TMR0_CH_B)
#define TMR0_FCG_ENABLE()               (FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMR0_2, ENABLE))

/* USART RX/TX pin definition */
#define USART_RX_PORT                   (GPIO_PORT_B)   /* PB6: USART4_RX */
#define USART_RX_PIN                    (GPIO_PIN_06)
#define USART_RX_GPIO_FUNC              (GPIO_FUNC_37)

#define USART_TX_PORT                   (GPIO_PORT_B)   /* PB7: USART4_TX */
#define USART_TX_PIN                    (GPIO_PIN_07)
#define USART_TX_GPIO_FUNC              (GPIO_FUNC_36)

/* USART unit definition */
#define USART_UNIT                      (CM_USART4)
#define USART_FCG_ENABLE()              (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART4, ENABLE))

/* USART baudrate definition */
#define USART_BAUDRATE                  (460800UL)

/* USART timeout bits definition */
#define USART_TIMEOUT_BITS              (100U)

/* USART interrupt definition */
#define USART_TX_CPLT_IRQn              (INT002_IRQn)
#define USART_TX_CPLT_INT_SRC           (INT_SRC_USART4_TCI)

#define USART_RX_ERR_IRQn               (INT003_IRQn)
#define USART_RX_ERR_INT_SRC            (INT_SRC_USART4_EI)

#define USART_RX_TIMEOUT_IRQn           (INT004_IRQn)
#define USART_RX_TIMEOUT_INT_SRC        (INT_SRC_USART4_RTO)

/* Application frame length max definition */
#define APP_FRAME_LEN_MAX               (32U)
#define MAXANTCNT 16
#define MAXEMBDATALEN 128
#define MAXEPCBYTESCNT 62


#define MSG_CRC_INIT                    (0xFFFF)
#define MSG_CCITT_CRC_POLY              (0x1021)




typedef struct
{
    unsigned char FrameHead;
    unsigned char Pdu_len;
    unsigned char DeviceID;
    unsigned char AntID;
    unsigned char Alarm_Duration[6];
    uint16_t  Radarcfg[5];
    uint32_t  time_stamp;
    uint32_t  random_forest;
    uint16_t  reserved;
    uint16_t  crc;
} alarm_pdu;

void LED_GPIO_Init(void);
void Uart4_int(void);
void DMA_Config(void);
void TMR0_Config(uint16_t u16TimeoutBits);
void Alarm_On(void);
void Alarm_Off(void);
int8_t Get_pdu_data(uint8_t *pdubuff);
void Legal_Tag(void);
void Check_alarm_state(void);
void Check_Uart_Pdu(void);
void Uart4_DMA_Trans(const void *pvBuf, uint32_t u32Len);
uint16_t CalcCRC(uint8_t *msgbuf, uint8_t msglen);
void Check_UidKey(void);
void TrngConfig(void);
uint32_t trng_create(void);
void data_denoising(uint8_t *inbuf, uint8_t *outbuf, uint32_t noise, uint8_t len);
/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
