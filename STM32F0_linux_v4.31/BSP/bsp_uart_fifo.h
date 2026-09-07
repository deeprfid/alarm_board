/*
*********************************************************************************************************
*
*	模块名称 : 串口中断+FIFO驱动模块
*	文件名称 : bsp_uart_fifo.h
*	说    明 : 头文件
*
*	Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_USART_FIFO_H_
#define _BSP_USART_FIFO_H_
#include "stdint.h"
#include "stm32f0xx_hal.h"

#define	UART1_FIFO_EN	1
#define	UART2_FIFO_EN	1
#define	UART3_FIFO_EN	1
#define	UART4_FIFO_EN	1
#define	UART5_FIFO_EN	1
#define	UART6_FIFO_EN	1




/* 定义端口号 */
typedef enum
{
	COM1 = 0,	/* USART1 */
	COM2 = 1,	/* USART2 */
	COM3 = 2,	/* USART3 */
	COM4 = 3,	/* UART4 */
	COM5 = 4,	/* UART5 */
	COM6 = 5,	/* USART6 */

}COM_PORT_E;

/* 定义串口波特率和FIFO缓冲区大小，分为发送缓冲区和接收缓冲区, 支持全双工 */
#if UART1_FIFO_EN == 1
	#define UART1_BAUD			115200
	#define UART1_TX_BUF_SIZE	1*1024
	#define UART1_RX_BUF_SIZE	1*8192
#endif

#if UART2_FIFO_EN == 1
	#define UART2_BAUD			460800
	#define UART2_TX_BUF_SIZE	1*512
	#define UART2_RX_BUF_SIZE	1*512
#endif

#if UART3_FIFO_EN == 1
	#define UART3_BAUD			460800
	#define UART3_TX_BUF_SIZE	1*512
	#define UART3_RX_BUF_SIZE	1*512
	#define UART3_DMA_RX		1	/* 1=UART3 RX via DMA1 CH3 circular + IDLE */
#endif

#if UART4_FIFO_EN == 1
	#define UART4_BAUD			460800
	#define UART4_TX_BUF_SIZE	1*512
	#define UART4_RX_BUF_SIZE	1*512
	#define UART4_DMA_RX		1	/* 1=UART4 RX via DMA1 CH1 circular + IDLE */
#endif

#if UART5_FIFO_EN == 1
	#define UART5_BAUD			460800
	#define UART5_TX_BUF_SIZE	1*512
	#define UART5_RX_BUF_SIZE	1*512
	#define UART5_DMA_RX		1	/* 1=UART5 RX via DMA1 CH5 circular + IDLE */
#endif

#if UART6_FIFO_EN == 1
	#define UART6_BAUD			460800
	#define UART6_TX_BUF_SIZE	1*512
	#define UART6_RX_BUF_SIZE	1*512
#endif



/* 串口设备结构体 */
typedef struct
{
	USART_TypeDef *uart;		/* STM32内部串口设备指针 */
	uint8_t *pTxBuf;			  /* 发送缓冲区 */
	uint8_t *pRxBuf;			  /* 接收缓冲区 */
	uint16_t usTxBufSize;		/* 发送缓冲区大小 */
	uint16_t usRxBufSize;		/* 接收缓冲区大小 */
	__IO uint16_t usTxWrite;	/* 发送缓冲区写指针 */
	__IO uint16_t usTxRead;		/* 发送缓冲区读指针 */
	__IO uint16_t usTxCount;	/* 等待发送的数据个数 */
	__IO uint16_t usRxWrite;	/* 接收缓冲区写指针 */
	__IO uint16_t usRxRead;		/* 接收缓冲区读指针 */
	__IO uint16_t usRxCount;	/* 还未读取的新数据个数 */

	void (*SendBefor)(void); 	/* 开始发送之前的回调函数指针（主要用于RS485切换到发送模式） */
	void (*SendOver)(void); 	/* 发送完毕的回调函数指针（主要用于RS485将发送模式切换为接收模式） */
	void (*ReciveNew)(uint8_t _byte);	/* 串口收到数据的回调函数指针 */
	uint8_t Sending;			/* 正在发送中 */
	IRQn_Type uartirq;
}UART_T;




void bsp_InitUart(void);
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen);
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte);
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte);
void comClearTxFifo(COM_PORT_E _ucPort);
void comClearRxFifo(COM_PORT_E _ucPort);
void comSetBaud(COM_PORT_E _ucPort, uint32_t _BaudRate);
void bsp_SetUartParam(USART_TypeDef *Instance,  uint32_t BaudRate, uint32_t Parity, uint32_t Mode);
uint16_t uart_recv(COM_PORT_E _ucPorts, void *buf, uint32_t len);
uint8_t  UartTxEmpty(COM_PORT_E _ucPort);
uint16_t UartGetRxcnt(COM_PORT_E _ucPort);
  
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
