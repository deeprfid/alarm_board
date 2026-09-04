/**
 *******************************************************************************
 * @file  usart/usart_uart_int/source/ring_buf.h
 * @brief This file contains all the functions prototypes of the ring buffer.
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



/*******************************************************************************
 * Include files
 ******************************************************************************/
#define	MSG_NONE               (0)
#define	MSG_485_TAG_RTU        (1)    /* 接收到RS485 MODBUS RTU数据包*/
#define	MSG_485_TAG_CRC_FAULT  (2)    /* 接收到RS485数据包，CRC未过 */
#define	MSG_NETWORK_OFFLINE    (3)		/* Receive from bridge pdu*/
#define	MSG_BRIDGE_TX          (4)    /* send bridge pdu to alarm board without sound-light output*/
#define	MSG_Relay_RX           (5)    /* Receive from relay input GPI*/
#define MSG_Alarm_OFF          (6)
#define	MSG_LEDTEST            (7)

void alarm_thread(void);
void Network_offline(void);
uint16_t switch_pio_readall(uint16_t channel);
/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
