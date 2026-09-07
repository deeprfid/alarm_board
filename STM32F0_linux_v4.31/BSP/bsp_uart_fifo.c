/*
*********************************************************************************************************
*
*	模块名称 : 串口中断+FIFO驱动模块
*	文件名称 : bsp_uart_fifo.c
*	版    本 : V1.8
*	说    明 : 采用串口中断+FIFO模式实现多个串口的同时访问
*	修改记录 :
*		版本号  日期       作者    说明
*
*	Copyright (C), 2015-2030,www.autobma.com
*
*********************************************************************************************************
*/

#include "bsp.h"


/* 定义每个串口结构体变量 */
#if UART1_FIFO_EN == 1
    static UART_T g_tUart1;
    static uint8_t g_TxBuf1[UART1_TX_BUF_SIZE];		/* 发送缓冲区 */
    static uint8_t g_RxBuf1[UART1_RX_BUF_SIZE];		/* 接收缓冲区 */
#endif

#if UART2_FIFO_EN == 1
    static UART_T g_tUart2;
    static uint8_t g_TxBuf2[UART2_TX_BUF_SIZE];		/* 发送缓冲区 */
    static uint8_t g_RxBuf2[UART2_RX_BUF_SIZE];		/* 接收缓冲区 */
#endif

#if UART3_FIFO_EN == 1
    static UART_T g_tUart3;
    static uint8_t g_TxBuf3[UART3_TX_BUF_SIZE];		/* 发送缓冲区 */
    static uint8_t g_RxBuf3[UART3_RX_BUF_SIZE];		/* 接收缓冲区 */
#endif

#if UART4_FIFO_EN == 1
    static UART_T g_tUart4;
    static uint8_t g_TxBuf4[UART4_TX_BUF_SIZE];		/* 发送缓冲区 */
    static uint8_t g_RxBuf4[UART4_RX_BUF_SIZE];		/* 接收缓冲区 */
#endif

#if UART5_FIFO_EN == 1
    static UART_T g_tUart5;
    static uint8_t g_TxBuf5[UART5_TX_BUF_SIZE];		/* 发送缓冲区 */
    static uint8_t g_RxBuf5[UART5_RX_BUF_SIZE];		/* 接收缓冲区 */
#endif

#if UART6_FIFO_EN == 1
    static UART_T g_tUart6;
    static uint8_t g_TxBuf6[UART6_TX_BUF_SIZE];		/* 发送缓冲区 */
    static uint8_t g_RxBuf6[UART6_RX_BUF_SIZE];		/* 接收缓冲区 */
#endif

UART_HandleTypeDef IPC_huart1;// IPCOM
UART_HandleTypeDef CH2_huart2;// CHANNEL 2
UART_HandleTypeDef CH3_huart3;// CHANNEL 3
UART_HandleTypeDef CH4_huart4;// CHANNEL 4
UART_HandleTypeDef CH5_huart5;// CHANNEL 5
UART_HandleTypeDef CH1_huart6;// CHANNEL 1

/* --- UART6 DMA TX (CH2) persistent buffer --- */
#define UART6_TX_DMA_BUF (600u)
static uint8_t  g_u6txbuf[UART6_TX_DMA_BUF];
static volatile uint8_t g_u6txbusy = 0u;

static void UartVarInit(void);
static void InitHardUart(void);
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen);
static void UartSendBlocking(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen);
static void uart6_dma_tx_start(const uint8_t *src, uint16_t len);
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte);
static void UartIRQ(UART_T *_pUart);



static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART4_UART_Init(void);
static void MX_USART5_UART_Init(void);
static void MX_USART6_UART_Init(void);
/*
*********************************************************************************************************
*	函 数 名: bsp_InitUart
*	功能说明: 初始化串口硬件，并对全局变量赋初值.
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart(void)
{

    UartVarInit();		/* 必须先初始化全局变量,再配置硬件 */

    InitHardUart();		/* 配置串口的硬件参数(波特率等) */

}

/*
*********************************************************************************************************
*	函 数 名: ComToUart
*	功能说明: 将COM端口号转换为UART指针
*	形    参: _ucPort: 端口号(COM1 - COM8)
*	返 回 值: uart指针
*********************************************************************************************************
*/
UART_T *ComToUart(COM_PORT_E _ucPort)
{
    if (_ucPort == COM1)
    {
        #if UART1_FIFO_EN == 1
        return &g_tUart1;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM2)
    {
        #if UART2_FIFO_EN == 1
        return &g_tUart2;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM3)
    {
        #if UART3_FIFO_EN == 1
        return &g_tUart3;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM4)
    {
        #if UART4_FIFO_EN == 1
        return &g_tUart4;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM5)
    {
        #if UART5_FIFO_EN == 1
        return &g_tUart5;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM6)
    {
        #if UART6_FIFO_EN == 1
        return &g_tUart6;
        #else
        return 0;
        #endif
    }
    else
    {
        Error_Handler();
        return 0;
    }
}

/*
*********************************************************************************************************
*	函 数 名: ComToUart
*	功能说明: 将COM端口号转换为 USART_TypeDef* USARTx
*	形    参: _ucPort: 端口号(COM1 - COM8)
*	返 回 值: USART_TypeDef*,  USART1, USART2, USART3, UART4, UART5，USART6，UART7，UART8。
*********************************************************************************************************
*/
USART_TypeDef *ComToUSARTx(COM_PORT_E _ucPort)
{
    if (_ucPort == COM1)
    {
        #if UART1_FIFO_EN == 1
        return USART1;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM2)
    {
        #if UART2_FIFO_EN == 1
        return USART2;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM3)
    {
        #if UART3_FIFO_EN == 1
        return USART3;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM4)
    {
        #if UART4_FIFO_EN == 1
        return USART4;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM5)
    {
        #if UART5_FIFO_EN == 1
        return USART5;
        #else
        return 0;
        #endif
    }
    else if (_ucPort == COM6)
    {
        #if UART6_FIFO_EN == 1
        return USART6;
        #else
        return 0;
        #endif
    }

    else
    {
        /* 不做任何处理 */
        return 0;
    }
}

/*
*********************************************************************************************************
*	函 数 名: comSendBuf
*	功能说明: 向串口发送一组数据。数据放到发送缓冲区后立即返回，由中断服务程序在后台完成发送
*	形    参: _ucPort: 端口号(COM1 - COM8)
*			  _ucaBuf: 待发送的数据缓冲区
*			  _usLen : 数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);

    if (pUart == 0)
    {
        return;
    }

#if UART6_FIFO_EN == 1
    if (pUart->uart == USART6)
    {
        uart6_dma_tx_start(_ucaBuf, _usLen);
        return;
    }
#endif
#if (UART3_FIFO_EN == 1 && UART3_DMA_RX == 1) || (UART4_FIFO_EN == 1 && UART4_DMA_RX == 1) || (UART5_FIFO_EN == 1 && UART5_DMA_RX == 1)
    if ((pUart->uart == USART3 && UART3_DMA_RX == 1) ||
        (pUart->uart == USART4 && UART4_DMA_RX == 1) ||
        (pUart->uart == USART5 && UART5_DMA_RX == 1))
    {
        UartSendBlocking(pUart, _ucaBuf, _usLen);
        return;
    }
#endif

//	if (pUart->SendBefor != 0)
//	{
//		pUart->SendBefor();		/* 如果是RS485通信，可以在这个函数中将RS485设置为发送模式 */
//	}

    UartSend(pUart, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*	函 数 名: comSendChar
*	功能说明: 向串口发送1个字节。数据放到发送缓冲区后立即返回，由中断服务程序在后台完成发送
*	形    参: _ucPort: 端口号(COM1 - COM8)
*			  _ucByte: 待发送的数据
*	返 回 值: 无
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte)
{
    comSendBuf(_ucPort, &_ucByte, 1);
}

/*
*********************************************************************************************************
*	函 数 名: comGetChar
*	功能说明: 从接收缓冲区读取1字节，非阻塞。无论有无数据均立即返回。
*	形    参: _ucPort: 端口号(COM1 - COM8)
*			  _pByte: 接收到的数据存放在这个地址
*	返 回 值: 0 表示无数据, 1 表示读取到有效字节
*********************************************************************************************************
*/
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);

    if (pUart == 0)
    {
        return 0;
    }

    return UartGetChar(pUart, _pByte);
}

/*
*********************************************************************************************************
*	函 数 名: comClearTxFifo
*	功能说明: 清零串口发送缓冲区
*	形    参: _ucPort: 端口号(COM1 - COM8)
*	返 回 值: 无
*********************************************************************************************************
*/
void comClearTxFifo(COM_PORT_E _ucPort)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);

    if (pUart == 0)
    {
        return;
    }

    pUart->usTxWrite = 0;
    pUart->usTxRead = 0;
    pUart->usTxCount = 0;
}

/*
*********************************************************************************************************
*	函 数 名: comClearRxFifo
*	功能说明: 清零串口接收缓冲区
*	形    参: _ucPort: 端口号(COM1 - COM8)
*	返 回 值: 无
*********************************************************************************************************
*/
void comClearRxFifo(COM_PORT_E _ucPort)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);

    if (pUart == 0)
    {
        return;
    }

    pUart->usRxWrite = 0;
    pUart->usRxRead = 0;
    pUart->usRxCount = 0;
}

/*
*********************************************************************************************************
*	函 数 名: comSetBaud
*	功能说明: 设置串口的波特率. 本函数固定设置为无校验，收发都使能模式
*	形    参: _ucPort: 端口号(COM1 - COM8)
*			  _BaudRate: 波特率，8倍过采样  波特率.0-12.5Mbps
*                                16倍过采样 波特率.0-6.25Mbps
*	返 回 值: 无
*********************************************************************************************************
*/
void comSetBaud(COM_PORT_E _ucPort, uint32_t _BaudRate)
{
    USART_TypeDef* USARTx;

    USARTx = ComToUSARTx(_ucPort);

    if (USARTx == 0)
    {
        return;
    }

    bsp_SetUartParam(USARTx,  _BaudRate, UART_PARITY_NONE, UART_MODE_TX_RX);
}


/*
*********************************************************************************************************
*	函 数 名: UartVarInit
*	功能说明: 初始化串口相关的变量
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void UartVarInit(void)
{
    #if UART1_FIFO_EN == 1
    g_tUart1.uart = USART1;						/* STM32 串口设备 */
    g_tUart1.pTxBuf = g_TxBuf1;					/* 发送缓冲区指针 */
    g_tUart1.pRxBuf = g_RxBuf1;					/* 接收缓冲区指针 */
    g_tUart1.usTxBufSize = UART1_TX_BUF_SIZE;	/* 发送缓冲区大小 */
    g_tUart1.usRxBufSize = UART1_RX_BUF_SIZE;	/* 接收缓冲区大小 */
    g_tUart1.usTxWrite = 0;						/* 发送FIFO写索引 */
    g_tUart1.usTxRead = 0;						/* 发送FIFO读索引 */
    g_tUart1.usRxWrite = 0;						/* 接收FIFO写索引 */
    g_tUart1.usRxRead = 0;						/* 接收FIFO读索引 */
    g_tUart1.usRxCount = 0;						/* 接收到的新数据个数 */
    g_tUart1.usTxCount = 0;						/* 待发送的数据个数 */
    g_tUart1.SendBefor = 0;						/* 发送数据前的回调函数 */
    g_tUart1.SendOver = 0;						/* 发送完毕后的回调函数 */
    g_tUart1.ReciveNew = 0;						/* 接收到新数据后的回调函数 */
    g_tUart1.Sending = 0;						/* 正在发送中标志 */
    g_tUart1.uartirq = USART1_IRQn;
    #endif

    #if UART2_FIFO_EN == 1
    g_tUart2.uart = USART2;						/* STM32 串口设备 */
    g_tUart2.pTxBuf = g_TxBuf2;					/* 发送缓冲区指针 */
    g_tUart2.pRxBuf = g_RxBuf2;					/* 接收缓冲区指针 */
    g_tUart2.usTxBufSize = UART2_TX_BUF_SIZE;	/* 发送缓冲区大小 */
    g_tUart2.usRxBufSize = UART2_RX_BUF_SIZE;	/* 接收缓冲区大小 */
    g_tUart2.usTxWrite = 0;						/* 发送FIFO写索引 */
    g_tUart2.usTxRead = 0;						/* 发送FIFO读索引 */
    g_tUart2.usRxWrite = 0;						/* 接收FIFO写索引 */
    g_tUart2.usRxRead = 0;						/* 接收FIFO读索引 */
    g_tUart2.usRxCount = 0;						/* 接收到的新数据个数 */
    g_tUart2.usTxCount = 0;						/* 待发送的数据个数 */
    g_tUart2.SendBefor = 0;						/* 发送数据前的回调函数 */
    g_tUart2.SendOver = 0;						/* 发送完毕后的回调函数 */
    g_tUart2.ReciveNew = 0;						/* 接收到新数据后的回调函数 */
    g_tUart2.Sending = 0;						/* 正在发送中标志 */
    g_tUart2.uartirq = USART2_IRQn;
    #endif

    #if UART3_FIFO_EN == 1
    g_tUart3.uart = USART3;						/* STM32 串口设备 */
    g_tUart3.pTxBuf = g_TxBuf3;					/* 发送缓冲区指针 */
    g_tUart3.pRxBuf = g_RxBuf3;					/* 接收缓冲区指针 */
    g_tUart3.usTxBufSize = UART3_TX_BUF_SIZE;	/* 发送缓冲区大小 */
    g_tUart3.usRxBufSize = UART3_RX_BUF_SIZE;	/* 接收缓冲区大小 */
    g_tUart3.usTxWrite = 0;						/* 发送FIFO写索引 */
    g_tUart3.usTxRead = 0;						/* 发送FIFO读索引 */
    g_tUart3.usRxWrite = 0;						/* 接收FIFO写索引 */
    g_tUart3.usRxRead = 0;						/* 接收FIFO读索引 */
    g_tUart3.usRxCount = 0;						/* 接收到的新数据个数 */
    g_tUart3.usTxCount = 0;						/* 待发送的数据个数 */
    g_tUart3.SendBefor = 0;		/* 发送数据前的回调函数 */
    g_tUart3.SendOver  = 0;			/* 发送完毕后的回调函数 */
    g_tUart3.ReciveNew = 0;		/* 接收到新数据后的回调函数 */
    g_tUart3.Sending = 0;						/* 正在发送中标志 */
    g_tUart3.uartirq = USART3_6_IRQn;
    #endif

    #if UART4_FIFO_EN == 1
    g_tUart4.uart = USART4;						/* STM32 串口设备 */
    g_tUart4.pTxBuf = g_TxBuf4;					/* 发送缓冲区指针 */
    g_tUart4.pRxBuf = g_RxBuf4;					/* 接收缓冲区指针 */
    g_tUart4.usTxBufSize = UART4_TX_BUF_SIZE;	/* 发送缓冲区大小 */
    g_tUart4.usRxBufSize = UART4_RX_BUF_SIZE;	/* 接收缓冲区大小 */
    g_tUart4.usTxWrite = 0;						/* 发送FIFO写索引 */
    g_tUart4.usTxRead = 0;						/* 发送FIFO读索引 */
    g_tUart4.usRxWrite = 0;						/* 接收FIFO写索引 */
    g_tUart4.usRxRead = 0;						/* 接收FIFO读索引 */
    g_tUart4.usRxCount = 0;						/* 接收到的新数据个数 */
    g_tUart4.usTxCount = 0;						/* 待发送的数据个数 */
    g_tUart4.SendBefor = 0;						/* 发送数据前的回调函数 */
    g_tUart4.SendOver  = 0;						/* 发送完毕后的回调函数 */
    g_tUart4.ReciveNew = 0;						/* 接收到新数据后的回调函数 */
    g_tUart4.Sending = 0;						/* 正在发送中标志 */
    g_tUart4.uartirq = USART3_6_IRQn;
    #endif

    #if UART5_FIFO_EN == 1
    g_tUart5.uart = USART5;						/* STM32 串口设备 */
    g_tUart5.pTxBuf = g_TxBuf5;					/* 发送缓冲区指针 */
    g_tUart5.pRxBuf = g_RxBuf5;					/* 接收缓冲区指针 */
    g_tUart5.usTxBufSize = UART5_TX_BUF_SIZE;	/* 发送缓冲区大小 */
    g_tUart5.usRxBufSize = UART5_RX_BUF_SIZE;	/* 接收缓冲区大小 */
    g_tUart5.usTxWrite = 0;						/* 发送FIFO写索引 */
    g_tUart5.usTxRead = 0;						/* 发送FIFO读索引 */
    g_tUart5.usRxWrite = 0;						/* 接收FIFO写索引 */
    g_tUart5.usRxRead = 0;						/* 接收FIFO读索引 */
    g_tUart5.usRxCount = 0;						/* 接收到的新数据个数 */
    g_tUart5.usTxCount = 0;						/* 待发送的数据个数 */
    g_tUart5.SendBefor = 0;						/* 发送数据前的回调函数 */
    g_tUart5.SendOver = 0;						/* 发送完毕后的回调函数 */
    g_tUart5.ReciveNew = 0;						/* 接收到新数据后的回调函数 */
    g_tUart5.Sending = 0;						  /* 正在发送中标志 */
    g_tUart5.uartirq = USART3_6_IRQn;
    #endif


    #if UART6_FIFO_EN == 1
    g_tUart6.uart = USART6;						/* STM32 串口设备 */
    g_tUart6.pTxBuf = g_TxBuf6;					/* 发送缓冲区指针 */
    g_tUart6.pRxBuf = g_RxBuf6;					/* 接收缓冲区指针 */
    g_tUart6.usTxBufSize = UART6_TX_BUF_SIZE;	/* 发送缓冲区大小 */
    g_tUart6.usRxBufSize = UART6_RX_BUF_SIZE;	/* 接收缓冲区大小 */
    g_tUart6.usTxWrite = 0;						/* 发送FIFO写索引 */
    g_tUart6.usTxRead = 0;						/* 发送FIFO读索引 */
    g_tUart6.usRxWrite = 0;						/* 接收FIFO写索引 */
    g_tUart6.usRxRead = 0;						/* 接收FIFO读索引 */
    g_tUart6.usRxCount = 0;						/* 接收到的新数据个数 */
    g_tUart6.usTxCount = 0;						/* 待发送的数据个数 */
    g_tUart6.SendBefor = 0;						/* 发送数据前的回调函数 */
    g_tUart6.SendOver = 0;						/* 发送完毕后的回调函数 */
    g_tUart6.ReciveNew = 0;						/* 接收到新数据后的回调函数 */
    g_tUart6.Sending = 0;						/* 正在发送中标志 */
    g_tUart6.uartirq = USART3_6_IRQn;
    #endif


}

/*
*********************************************************************************************************
*	函 数 名: bsp_SetUartParam
*	功能说明: 配置串口的硬件参数（波特率，数据位，停止位，起始位，校验位，中断使能）适合于STM32- H7开发板
*	形    参: Instance   USART_TypeDef类型结构体
*             BaudRate   波特率
*             Parity     校验类型，奇校验或者偶校验
*             Mode       发送和接收模式使能
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_SetUartParam(USART_TypeDef *Instance,  uint32_t BaudRate, uint32_t Parity, uint32_t Mode)
{
    UART_HandleTypeDef UartHandle;

    /*##-1- 配置串口硬件参数 ######################################*/
    /* 异步串口模式 (UART Mode) */
    /* 配置如下:
      - 字长    = 8 位
      - 停止位  = 1 个停止位
      - 校验    = 参数Parity
      - 波特率  = 参数BaudRate
      - 硬件流控制关闭 (RTS and CTS signals) */

    UartHandle.Instance        = Instance;

    UartHandle.Init.BaudRate   = BaudRate;
    UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
    UartHandle.Init.StopBits   = UART_STOPBITS_1;
    UartHandle.Init.Parity     = Parity;
    UartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    UartHandle.Init.Mode       = Mode;
    UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
    UartHandle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    UartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&UartHandle) != HAL_OK)
    {
        Error_Handler();
    }
}

/*
*********************************************************************************************************
*	函 数 名: InitHardUart
*	功能说明: 配置串口的硬件参数（波特率，数据位，停止位，起始位，校验位，中断使能）适合于STM32-H7开发板
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void InitHardUart(void)
{
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_USART4_UART_Init();
    MX_USART5_UART_Init();
    MX_USART6_UART_Init();
#if UART6_FIFO_EN == 1
    __HAL_RCC_DMA1_CLK_ENABLE();
    /* DMA CH2(USART6 TX) IRQ at low priority so USART3_6 stays responsive */
    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
#endif
}

/*
*********************************************************************************************************
*	函 数 名: UartSend
*	功能说明: 填写数据到UART发送缓冲区,并启动发送中断。中断处理函数发送完毕后，自动关闭发送中断
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void UartSendBlocking(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
    uint16_t i;
    /* blocking: poll TXE then write TDR; no TXE interrupt used */
    for (i = 0u; i < _usLen; i++)
    {
        while ((_pUart->uart->ISR & USART_ISR_TXE) == 0u) { }
        _pUart->uart->TDR = _ucaBuf[i];
    }
    /* do NOT wait TC: shared-ISR flag-clear (on RX IDLE) resets it -> deadlock */
    while ((_pUart->uart->ISR & USART_ISR_TXE) == 0u) { }
}

static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
    uint16_t i;

    for (i = 0; i < _usLen; i++)
    {
        /* 如果发送缓冲区已经满了，则等待缓冲区空 */
        while (1)
        {
            __IO uint16_t usCount;

            HAL_NVIC_DisableIRQ(_pUart->uartirq);
            usCount = _pUart->usTxCount;
            HAL_NVIC_EnableIRQ(_pUart->uartirq);

            if (usCount < _pUart->usTxBufSize)
            {
                break;
            }
            else if(usCount == _pUart->usTxBufSize)/* 数据已填满缓冲区 */
            {
                if((_pUart->uart->CR1 & USART_CR1_TXEIE) == 0)
                {
                    SET_BIT(_pUart->uart->CR1, USART_CR1_TXEIE);
                }
            }
        }

        /* 将新数据填入发送缓冲区 */
        _pUart->pTxBuf[_pUart->usTxWrite] = _ucaBuf[i];

        //DISABLE_INT();
        HAL_NVIC_DisableIRQ(_pUart->uartirq);

        if (++_pUart->usTxWrite >= _pUart->usTxBufSize)
        {
            _pUart->usTxWrite = 0;
        }

        _pUart->usTxCount++;
        //ENABLE_INT();
        HAL_NVIC_EnableIRQ(_pUart->uartirq);
    }

    SET_BIT(_pUart->uart->CR1, USART_CR1_TXEIE);	/* 使能发送中断（缓冲区空） */
}

/* UART6 DMA TX (CH2): async, no TXE interrupt */
static void uart6_dma_tx_start(const uint8_t *src, uint16_t len)
{
    uint16_t i;
    if (g_u6txbusy != 0u) { return; }
    if (len > UART6_TX_DMA_BUF) { return; }
    for (i = 0u; i < len; i++) { g_u6txbuf[i] = src[i]; }
    DMA1_Channel2->CCR = 0u;
    DMA1_Channel2->CMAR = (uint32_t)g_u6txbuf;
    DMA1_Channel2->CPAR = (uint32_t)&USART6->TDR;
    DMA1_Channel2->CNDTR = len;
    DMA1->CSELR = (DMA1->CSELR & ~DMA1_CSELR_CH2_USART6_TX_Msk) | DMA1_CSELR_CH2_USART6_TX;
    DMA1_Channel2->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_EN;
    SET_BIT(USART6->CR3, USART_CR3_DMAT);
    g_u6txbusy = 1u;
}

/* DMA1 CH2 TX complete (UART6) */
void DMA1_Channel2_3_IRQHandler(void)
{
    if ((DMA1->ISR & DMA_ISR_TCIF2) != 0u)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF2;
        CLEAR_BIT(USART6->CR3, USART_CR3_DMAT);
        g_u6txbusy = 0u;
    }
}

/*
*********************************************************************************************************
*	函 数 名: UartGetChar
*	功能说明: 从串口接收缓冲区读取1字节数据 （用于主程序调用）
*	形    参: _pUart : 串口设备
*			  _pByte : 存放读取数据的指针
*	返 回 值: 0 表示无数据  1表示读取到数据
*********************************************************************************************************
*/
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte)
{
    uint16_t usCount;

    /* usRxWrite 变量在中断函数中被改写，主程序读取该变量时，必须进行临界区保护 */
    HAL_NVIC_DisableIRQ(_pUart->uartirq);
    usCount = _pUart->usRxCount;
    ENABLE_INT();

    /* 如果读和写索引相同，则返回0 */
    //if (_pUart->usRxRead == usRxWrite)
    if (usCount == 0)	/* 已经没有数据 */
    {
        return 0;
    }
    else
    {
        *_pByte = _pUart->pRxBuf[_pUart->usRxRead];		/* 从串口接收FIFO取1个数据 */

        /* 改写FIFO读索引 */
        HAL_NVIC_DisableIRQ(_pUart->uartirq);

        if (++_pUart->usRxRead >= _pUart->usRxBufSize)
        {
            _pUart->usRxRead = 0;
        }

        _pUart->usRxCount--;
        HAL_NVIC_EnableIRQ(_pUart->uartirq);
        return 1;
    }
}

uint16_t UartGetRxcnt(COM_PORT_E _ucPort)
{
    uint16_t usCount;
    UART_T  *_pUart;
    _pUart = ComToUart(_ucPort);
    /* usRxWrite 变量在中断函数中被改写，主程序读取该变量时，必须进行临界区保护 */
    HAL_NVIC_DisableIRQ(_pUart->uartirq);
    usCount = _pUart->usRxCount;
    HAL_NVIC_EnableIRQ(_pUart->uartirq);
    return usCount;
}

/*
*********************************************************************************************************
*   函 数 名: UartTxEmpty
*   功能说明: 判断发送缓冲区是否为空。
*   形    参:  _pUart : 串口设备
*   返 回 值: 1为空。0为不空。
*********************************************************************************************************
*/
uint8_t UartTxEmpty(COM_PORT_E _ucPort)
{
    UART_T *pUart;
    uint8_t Sending;

    pUart = ComToUart(_ucPort);

    if (pUart == 0)
    {
        return 0;
    }

    Sending = pUart->Sending;

    if (Sending != 0)
    {
        return 0;
    }

    return 1;
}

/*
*********************************************************************************************************
*	函 数 名: UartIRQ
*	功能说明: 供中断服务程序调用，通用串口中断处理函数
*	形    参: _pUart : 串口设备
*	返 回 值: 无
*********************************************************************************************************
*/
#if (UART3_FIFO_EN == 1 && UART3_DMA_RX == 1) || (UART4_FIFO_EN == 1 && UART4_DMA_RX == 1) || (UART5_FIFO_EN == 1 && UART5_DMA_RX == 1)
/* --- channel UARTs RX via DMA circular + IDLE (experiment) --- */
#define UART_DMA_LEN   (512u)

#if UART3_FIFO_EN == 1 && UART3_DMA_RX == 1
static uint8_t  g_u3dma[UART_DMA_LEN];
static uint16_t g_u3dma_last = 0u;
#endif
#if UART4_FIFO_EN == 1 && UART4_DMA_RX == 1
static uint8_t  g_u4dma[UART_DMA_LEN];
static uint16_t g_u4dma_last = 0u;
#endif
#if UART5_FIFO_EN == 1 && UART5_DMA_RX == 1
static uint8_t  g_u5dma[UART_DMA_LEN];
static uint16_t g_u5dma_last = 0u;
#endif

/* move new bytes from circular DMA buf into the uart RX ring */
static void uart_dma_rx_move(DMA_Channel_TypeDef *ch, uint8_t *dma, uint16_t *last, UART_T *pu)
{
    uint16_t cur;
    uint16_t got;
    uint16_t i;
    uint16_t w;

    cur = (uint16_t)(UART_DMA_LEN - (uint16_t)READ_REG(ch->CNDTR));
    if (cur == *last) { return; }
    if (cur > *last)
    {
        got = (uint16_t)(cur - *last);
        for (i = 0u; i < got; i++)
        {
            w = pu->usRxWrite;
            pu->pRxBuf[w] = dma[(*last + i) & (UART_DMA_LEN - 1u)];
            if (++w >= pu->usRxBufSize) { w = 0u; }
            pu->usRxWrite = w;
            if (pu->usRxCount < pu->usRxBufSize) { pu->usRxCount++; }
        }
    }
    else
    {
        got = (uint16_t)(UART_DMA_LEN - *last);
        for (i = 0u; i < got; i++)
        {
            w = pu->usRxWrite;
            pu->pRxBuf[w] = dma[(*last + i) & (UART_DMA_LEN - 1u)];
            if (++w >= pu->usRxBufSize) { w = 0u; }
            pu->usRxWrite = w;
            if (pu->usRxCount < pu->usRxBufSize) { pu->usRxCount++; }
        }
        got = cur;
        for (i = 0u; i < got; i++)
        {
            w = pu->usRxWrite;
            pu->pRxBuf[w] = dma[i];
            if (++w >= pu->usRxBufSize) { w = 0u; }
            pu->usRxWrite = w;
            if (pu->usRxCount < pu->usRxBufSize) { pu->usRxCount++; }
        }
    }
    *last = cur;
}

/* enable DMA circular RX on one channel uart; called from MX_xxx */
static void uart_dma_rx_cfg(DMA_Channel_TypeDef *ch, uint32_t cselr_msk, uint32_t cselr_val,
                            uint8_t *dma, uint16_t *last, USART_TypeDef *uart)
{
    ch->CCR = 0u;
    ch->CMAR = (uint32_t)dma;
    ch->CPAR = (uint32_t)&uart->RDR;
    ch->CNDTR = UART_DMA_LEN;
    DMA1->CSELR = (DMA1->CSELR & ~cselr_msk) | cselr_val;
    ch->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_EN;
    *last = 0u;
    CLEAR_BIT(uart->CR1, USART_CR1_RXNEIE);
    SET_BIT(uart->CR3, USART_CR3_DMAR);
    SET_BIT(uart->CR1, USART_CR1_IDLEIE);
}
#endif
static void UartIRQ(UART_T *_pUart)
{
    uint32_t isrflags   = READ_REG(_pUart->uart->ISR);
    uint32_t cr1its     = READ_REG(_pUart->uart->CR1);
    uint32_t cr3its     = READ_REG(_pUart->uart->CR3);

#if (UART3_FIFO_EN == 1 && UART3_DMA_RX == 1) || (UART4_FIFO_EN == 1 && UART4_DMA_RX == 1) || (UART5_FIFO_EN == 1 && UART5_DMA_RX == 1)
    /* channel UARTs RX owned by DMA. IDLE marks frame end -> move bytes to ring.
       TXE/TC still handled by common code below (do NOT return). */
#if UART3_FIFO_EN == 1 && UART3_DMA_RX == 1
    if (_pUart->uart == USART3)
    {
        if ((isrflags & USART_ISR_IDLE) != RESET)
        {
            USART3->ICR = USART_ICR_IDLECF;
            uart_dma_rx_move(DMA1_Channel3, g_u3dma, &g_u3dma_last, &g_tUart3);
        }
        isrflags &= ~USART_ISR_RXNE;
    }
#endif
#if UART4_FIFO_EN == 1 && UART4_DMA_RX == 1
    if (_pUart->uart == USART4)
    {
        if ((isrflags & USART_ISR_IDLE) != RESET)
        {
            USART4->ICR = USART_ICR_IDLECF;
            uart_dma_rx_move(DMA1_Channel1, g_u4dma, &g_u4dma_last, &g_tUart4);
        }
        isrflags &= ~USART_ISR_RXNE;
    }
#endif
#if UART5_FIFO_EN == 1 && UART5_DMA_RX == 1
    if (_pUart->uart == USART5)
    {
        if ((isrflags & USART_ISR_IDLE) != RESET)
        {
            USART5->ICR = USART_ICR_IDLECF;
            uart_dma_rx_move(DMA1_Channel5, g_u5dma, &g_u5dma_last, &g_tUart5);
        }
        isrflags &= ~USART_ISR_RXNE;
    }
#endif
#endif

    /* 处理接收中断  */
    if ((isrflags & USART_ISR_RXNE) != RESET)
    {
        /* 从串口接收数据寄存器读取数据存放到接收FIFO */
        uint8_t ch;

        ch = READ_REG(_pUart->uart->RDR);
        _pUart->pRxBuf[_pUart->usRxWrite] = ch;

        if (++_pUart->usRxWrite >= _pUart->usRxBufSize)
        {
            _pUart->usRxWrite = 0;
        }

        if (_pUart->usRxCount < _pUart->usRxBufSize)
        {
            _pUart->usRxCount++;
        }

        /* 回调函数,通知应用程序收到新数据,一般是发送1个消息或者设置一个标记 */
        //if (_pUart->usRxWrite == _pUart->usRxRead)
        //if (_pUart->usRxCount == 1)
        {
            if (_pUart->ReciveNew)
            {
                _pUart->ReciveNew(ch); /* 比如，交给MODBUS解码程序处理字节流 */
            }
        }
    }

    /* 处理发送缓冲区空中断 */
    if ( ((isrflags & USART_ISR_TXE) != RESET) && (cr1its & USART_CR1_TXEIE) != RESET)
    {
        //if (_pUart->usTxRead == _pUart->usTxWrite)
        if (_pUart->usTxCount == 0)
        {
            /* 发送缓冲区的数据已取完时， 禁止发送缓冲区空中断 （注意：此时最后1个数据还未真正发送完毕）*/
            //USART_ITConfig(_pUart->uart, USART_IT_TXE, DISABLE);
            CLEAR_BIT(_pUart->uart->CR1, USART_CR1_TXEIE);

            /* 使能数据发送完毕中断 */
            //USART_ITConfig(_pUart->uart, USART_IT_TC, ENABLE);
            SET_BIT(_pUart->uart->CR1, USART_CR1_TCIE);
        }
        else
        {
            _pUart->Sending = 1;

            /* 从发送FIFO取1个字节写入串口发送数据寄存器 */
            //USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
            _pUart->uart->TDR = _pUart->pTxBuf[_pUart->usTxRead];

            if (++_pUart->usTxRead >= _pUart->usTxBufSize)
            {
                _pUart->usTxRead = 0;
            }

            _pUart->usTxCount--;
        }

    }

    /* 数据bit位全部发送完毕的中断 */
    if (((isrflags & USART_ISR_TC) != RESET) && ((cr1its & USART_CR1_TCIE) != RESET))
    {
        //if (_pUart->usTxRead == _pUart->usTxWrite)
        if (_pUart->usTxCount == 0)
        {
            /* 如果发送FIFO的数据全部发送完毕，禁止数据发送完毕中断 */
            //USART_ITConfig(_pUart->uart, USART_IT_TC, DISABLE);
            CLEAR_BIT(_pUart->uart->CR1, USART_CR1_TCIE);

            /* 回调函数, 一般用来处理RS485通信，将RS485芯片设置为接收模式，避免抢占总线 */
            if (_pUart->SendOver)
            {
                _pUart->SendOver();
            }

            _pUart->Sending = 0;
        }
        else
        {
            /* 正常情况下，不会进入此分支 */

            /* 如果发送FIFO的数据还未完毕，则从发送FIFO取1个数据写入发送数据寄存器 */
            //USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
            _pUart->uart->TDR = _pUart->pTxBuf[_pUart->usTxRead];

            if (++_pUart->usTxRead >= _pUart->usTxBufSize)
            {
                _pUart->usTxRead = 0;
            }

            _pUart->usTxCount--;
        }
    }

    /* 清除中断标志 */
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_PEF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_FEF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_NEF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_OREF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_IDLEF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_TCF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_CTSF);
    SET_BIT(_pUart->uart->ICR, UART_CLEAR_CMF);

//	*            @arg UART_CLEAR_PEF: Parity Error Clear Flag
//  *            @arg UART_CLEAR_FEF: Framing Error Clear Flag
//  *            @arg UART_CLEAR_NEF: Noise detected Clear Flag
//  *            @arg UART_CLEAR_OREF: OverRun Error Clear Flag
//  *            @arg UART_CLEAR_IDLEF: IDLE line detected Clear Flag
//  *            @arg UART_CLEAR_TCF: Transmission Complete Clear Flag
//  *            @arg UART_CLEAR_LBDF: LIN Break Detection Clear Flag
//  *            @arg UART_CLEAR_CTSF: CTS Interrupt Clear Flag
//  *            @arg UART_CLEAR_RTOF: Receiver Time Out Clear Flag
//  *            @arg UART_CLEAR_CMF: Character Match Clear Flag
//  *            @arg.UART_CLEAR_WUF:  Wake Up from stop mode Clear Flag
//  *            @arg UART_CLEAR_TXFECF: TXFIFO empty Clear Flag
}

/*
*********************************************************************************************************
*	函 数 名: USART1_IRQHandler  USART2_IRQHandler USART3_IRQHandler UART4_IRQHandler UART5_IRQHandler等
*	功能说明: USART中断服务程序
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
#if UART1_FIFO_EN == 1

void USART1_IRQHandler(void)
{
    UartIRQ(&g_tUart1);
}

#endif

#if UART2_FIFO_EN == 1

void USART2_IRQHandler(void)
{
    UartIRQ(&g_tUart2);
}

#endif

#if UART3_FIFO_EN == 1

void USART3_6_IRQHandler(void)
{
    UartIRQ(&g_tUart3);
    UartIRQ(&g_tUart4);
    UartIRQ(&g_tUart5);
    UartIRQ(&g_tUart6);
}

#endif




/*
*********************************************************************************************************
*	函 数 名: fputc
*	功能说明: 重定义putc函数，这样可以使用printf函数从串口1打印输出
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
    #if 1	/* 将需要printf的字符通过串口中断FIFO发送出去，printf函数会立即返回 */
    comSendChar(COM1, ch);

    return ch;
    #else	/* 采用阻塞方式发送每个字符,等待数据发送完毕 */
    /* 写一个字节到USART1 */
    USART1->TDR = ch;

    /* 等待发送结束 */
    while((USART1->ISR & USART_ISR_TC) == 0)
    {}

    return ch;
    #endif
}

/*
*********************************************************************************************************
*	函 数 名: fgetc
*	功能说明: 重定义getc函数，这样可以使用getchar函数从串口1输入数据
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int fgetc(FILE *f)
{

    #if 1	/* 从串口接收FIFO中取1个数据, 只有取到数据才返回 */
    uint8_t ucData;

    while(comGetChar(COM1, &ucData) == 0);

    return ucData;
    #else

    /* 等待接收到数据 */
    while((USART1->ISR & USART_ISR_RXNE) == 0)
    {}

    return (int)USART1->RDR;
    #endif
}



static void MX_USART1_UART_Init(void)
{

    /* USER CODE END USART1_Init 1 */
    IPC_huart1.Instance = USART1;
    IPC_huart1.Init.BaudRate = UART1_BAUD;
    IPC_huart1.Init.WordLength = UART_WORDLENGTH_8B;
    IPC_huart1.Init.StopBits = UART_STOPBITS_1;
    IPC_huart1.Init.Parity = UART_PARITY_NONE;
    IPC_huart1.Init.Mode = UART_MODE_TX_RX;
    IPC_huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    IPC_huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    IPC_huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    IPC_huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&IPC_huart1) != HAL_OK)
    {
        Error_Handler();
    }

    SET_BIT(USART1->ICR, USART_ICR_TCCF);	/* 清除TC发送完成标志 */
    SET_BIT(USART1->RQR, USART_RQR_RXFRQ);/* 清除RXNE接收标志 */
    SET_BIT(USART1->CR1, USART_CR1_RXNEIE);	/* 使能PE. RX接受中断 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

    /* USER CODE END USART2_Init 1 */
    CH2_huart2.Instance = USART2;
    CH2_huart2.Init.BaudRate = UART2_BAUD;
    CH2_huart2.Init.WordLength = UART_WORDLENGTH_8B;
    CH2_huart2.Init.StopBits = UART_STOPBITS_1;
    CH2_huart2.Init.Parity = UART_PARITY_NONE;
    CH2_huart2.Init.Mode = UART_MODE_TX_RX;
    CH2_huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    CH2_huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    CH2_huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    CH2_huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&CH2_huart2) != HAL_OK)
    {
        Error_Handler();
    }

    SET_BIT(USART2->ICR, USART_ICR_TCCF);	/* 清除TC发送完成标志 */
    SET_BIT(USART2->RQR, USART_RQR_RXFRQ);/* 清除RXNE接收标志 */
    SET_BIT(USART2->CR1, USART_CR1_RXNEIE);	/* 使能PE. RX接受中断 */
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{


    CH3_huart3.Instance = USART3;
    CH3_huart3.Init.BaudRate = UART3_BAUD;
    CH3_huart3.Init.WordLength = UART_WORDLENGTH_8B;
    CH3_huart3.Init.StopBits = UART_STOPBITS_1;
    CH3_huart3.Init.Parity = UART_PARITY_NONE;
    CH3_huart3.Init.Mode = UART_MODE_TX_RX;
    CH3_huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    CH3_huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    CH3_huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    CH3_huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;


    if (HAL_UART_Init(&CH3_huart3) != HAL_OK)
    {
        Error_Handler();
    }

    SET_BIT(USART3->ICR, USART_ICR_TCCF);	/* 清除TC发送完成标志 */
    SET_BIT(USART3->RQR, USART_RQR_RXFRQ);/* 清除RXNE接收标志 */

#if UART3_FIFO_EN == 1 && UART3_DMA_RX == 1
    __HAL_RCC_DMA1_CLK_ENABLE();
    uart_dma_rx_cfg(DMA1_Channel3, DMA1_CSELR_CH3_USART3_RX_Msk, DMA1_CSELR_CH3_USART3_RX,
                    g_u3dma, &g_u3dma_last, USART3);
#else
    SET_BIT(USART3->CR1, USART_CR1_RXNEIE);	/* 使能PE. RX接受中断 */
#endif

}

/**
  * @brief USART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART4_UART_Init(void)
{



    /* USER CODE END USART4_Init 1 */
    CH4_huart4.Instance = USART4;
    CH4_huart4.Init.BaudRate = UART4_BAUD;
    CH4_huart4.Init.WordLength = UART_WORDLENGTH_8B;
    CH4_huart4.Init.StopBits = UART_STOPBITS_1;
    CH4_huart4.Init.Parity = UART_PARITY_NONE;
    CH4_huart4.Init.Mode = UART_MODE_TX_RX;
    CH4_huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    CH4_huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    CH4_huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    CH4_huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&CH4_huart4) != HAL_OK)
    {
        Error_Handler();
    }

    SET_BIT(USART4->ICR, USART_ICR_TCCF);	/* 清除TC发送完成标志 */
    SET_BIT(USART4->RQR, USART_RQR_RXFRQ);/* 清除RXNE接收标志 */
#if UART4_FIFO_EN == 1 && UART4_DMA_RX == 1
    __HAL_RCC_DMA1_CLK_ENABLE();
    uart_dma_rx_cfg(DMA1_Channel1, DMA1_CSELR_CH1_USART4_RX_Msk, DMA1_CSELR_CH1_USART4_RX,
                    g_u4dma, &g_u4dma_last, USART4);
#else
    SET_BIT(USART4->CR1, USART_CR1_RXNEIE);	/* 使能PE. RX接受中断 */
#endif

}

/**
  * @brief USART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART5_UART_Init(void)
{



    /* USER CODE END USART5_Init 1 */
    CH5_huart5.Instance = USART5;
    CH5_huart5.Init.BaudRate = UART5_BAUD;
    CH5_huart5.Init.WordLength = UART_WORDLENGTH_8B;
    CH5_huart5.Init.StopBits = UART_STOPBITS_1;
    CH5_huart5.Init.Parity = UART_PARITY_NONE;
    CH5_huart5.Init.Mode = UART_MODE_TX_RX;
    CH5_huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    CH5_huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    CH5_huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    CH5_huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;



    if (HAL_UART_Init(&CH5_huart5) != HAL_OK)
    {
        Error_Handler();
    }

    SET_BIT(USART5->ICR, USART_ICR_TCCF);	/* 清除TC发送完成标志 */
    SET_BIT(USART5->RQR, USART_RQR_RXFRQ);/* 清除RXNE接收标志 */
#if UART5_FIFO_EN == 1 && UART5_DMA_RX == 1
    __HAL_RCC_DMA1_CLK_ENABLE();
    uart_dma_rx_cfg(DMA1_Channel5, DMA1_CSELR_CH5_USART5_RX_Msk, DMA1_CSELR_CH5_USART5_RX,
                    g_u5dma, &g_u5dma_last, USART5);
#else
    SET_BIT(USART5->CR1, USART_CR1_RXNEIE);	/* 使能PE. RX接受中断 */
#endif

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

    /* USER CODE END USART6_Init 1 */
    CH1_huart6.Instance = USART6;
    CH1_huart6.Init.BaudRate = UART6_BAUD;
    CH1_huart6.Init.WordLength = UART_WORDLENGTH_8B;
    CH1_huart6.Init.StopBits = UART_STOPBITS_1;
    CH1_huart6.Init.Parity = UART_PARITY_NONE;
    CH1_huart6.Init.Mode = UART_MODE_TX_RX;
    CH1_huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    CH1_huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    CH1_huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    CH1_huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&CH1_huart6) != HAL_OK)
    {
        Error_Handler();
    }

    SET_BIT(USART6->ICR, USART_ICR_TCCF);	/* 清除TC发送完成标志 */
    SET_BIT(USART6->RQR, USART_RQR_RXFRQ);/* 清除RXNE接收标志 */
    SET_BIT(USART6->CR1, USART_CR1_RXNEIE);	/* 使能PE. RX接受中断 */

}


uint16_t uart_recv(COM_PORT_E _ucPorts, void *buf, uint32_t len)
{
    uint16_t i = 0;
    uint8_t rdata = 0;
    uint8_t *rptr = (uint8_t*)buf;

    for( i = 0; i < len; i++)
    {
        if(comGetChar(_ucPorts, &rdata))
        {
            rptr[i] = rdata;
        }
        else
        {
            break;
        }

    }

    return i;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
