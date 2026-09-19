#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"
#include "string.h"
#include "bsp.h"
commonUartParaLocal gUartParams[TOTAL_UART_NUM];

uint8_t gUart0RecvBuf[MAX_UART0_BUF_SIZE];
uint8_t *gUart1RecvBuf = NULL;
uint8_t gUart2RecvBuf[MAX_UART2_BUF_SIZE];
uint8_t *gUart3RecvBuf = NULL;

extern stc_ring_buf_t Uart1RingBuf;
extern stc_ring_buf_t Uart2RingBuf;
extern stc_ring_buf_t Uart3RingBuf;
extern stc_ring_buf_t Uart4RingBuf;
volatile uint16_t  uart1reccount = 0;
volatile uint16_t  uart2reccount = 0;
volatile uint16_t  uart3reccount = 0;
volatile uint16_t  uart4reccount = 0;

/* RS485_1/2/3 = COMMON_INTERFACE_RS485_1/2/3(104/105/106) = gUartParams 下标 4/5/6。
 * RX 中断（rs485_1/2/3.c）按 rs485_Nreccount 当写指针填这三个缓冲；
 * 用静态数组而非 malloc_hexp：中断里挂 NULL 缓冲会直接 HardFault，静态数组上电即有效。 */
uint8_t gRs485_1RecvBuf[MAX_RS485_1_BUF_SIZE];
uint8_t gRs485_2RecvBuf[MAX_RS485_2_BUF_SIZE];
uint8_t gRs485_3RecvBuf[MAX_RS485_3_BUF_SIZE];
volatile uint16_t  rs485_1reccount = 0;
volatile uint16_t  rs485_2reccount = 0;
volatile uint16_t  rs485_3reccount = 0;


void INTC_IrqInstalHandler(const stc_irq_signin_config_t *pstcConfig, uint32_t u32Priority)
{
    if (NULL != pstcConfig)
    {
        (void)INTC_IrqSignIn(pstcConfig);
        NVIC_ClearPendingIRQ(pstcConfig->enIRQn);
        NVIC_SetPriority(pstcConfig->enIRQn, u32Priority);
        NVIC_EnableIRQ(pstcConfig->enIRQn);
    }
}

int hc32f460_uart_init(uart_cfg_para_st *ucpst)
{
    if(ucpst->uartid == 0)
    {
        gUartParams[ucpst->uartid].recvbuf = gUart0RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_UART0_BUF_SIZE;
        ucpst->recvbuf = gUart0RecvBuf;
        ucpst->buflen = MAX_UART0_BUF_SIZE;
        uart1reccount = 0;
        uart1_Init(ucpst);
    }
    else if(ucpst->uartid == 1)
    {
        if (gUart1RecvBuf == NULL)
        {
            int tmpaddr;
            gUart1RecvBuf = malloc_hexp(MAX_UART1_BUF_SIZE + 8);
            tmpaddr = (int)gUart1RecvBuf;

            if (tmpaddr % 8 != 0)
            {
                tmpaddr += 8 - (tmpaddr % 8);
                gUart1RecvBuf = (uint8 *)tmpaddr;
            }
        }

        gUartParams[ucpst->uartid].recvbuf = gUart1RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_UART1_BUF_SIZE;
        uart2reccount = 0;
        uart2_Init(ucpst);
    }
    else if(ucpst->uartid == 2)
    {
        gUartParams[ucpst->uartid].recvbuf = gUart2RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_UART2_BUF_SIZE;
        ucpst->recvbuf = gUart2RecvBuf;
        ucpst->buflen = MAX_UART2_BUF_SIZE;
        uart3reccount = 0;
        uart3_Init(ucpst);
    }
    else if(ucpst->uartid == 3)
    {
        if (gUart3RecvBuf == NULL)
            gUart3RecvBuf = malloc_hexp(MAX_UART3_BUF_SIZE);

        gUartParams[ucpst->uartid].recvbuf = gUart3RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_UART3_BUF_SIZE;
        uart4reccount = 0;
        uart4_Init(ucpst);
    }
    else if(ucpst->uartid == 4)
    {
        /* 注意顺序：先挂缓冲、再 uart_rs485_Init()（它里面才使能 RX 中断），
         * 否则第一个字节进来时 recvbuf 还是 NULL。 */
        gUartParams[ucpst->uartid].recvbuf = gRs485_1RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_RS485_1_BUF_SIZE;
        gUartParams[ucpst->uartid].uart_head = 0;
        gUartParams[ucpst->uartid].uart_tail = 0;
        rs485_1reccount = 0;
        uart_rs485_Init(ucpst);
    }
    else if(ucpst->uartid == 5)
    {
        gUartParams[ucpst->uartid].recvbuf = gRs485_2RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_RS485_2_BUF_SIZE;
        gUartParams[ucpst->uartid].uart_head = 0;
        gUartParams[ucpst->uartid].uart_tail = 0;
        rs485_2reccount = 0;
        uart8_Init(ucpst);
    }
    else if(ucpst->uartid == 6)
    {
        gUartParams[ucpst->uartid].recvbuf = gRs485_3RecvBuf;
        gUartParams[ucpst->uartid].recvbufsize = MAX_RS485_3_BUF_SIZE;
        gUartParams[ucpst->uartid].uart_head = 0;
        gUartParams[ucpst->uartid].uart_tail = 0;
        rs485_3reccount = 0;
        uart5_Init(ucpst);
    }
    else
    {
        TRACE("hc32f460_uart_init invlaid uartid\n");
        return -1;
    }

    return 0;
}

int uart_DmaRX_Init(int dma_id, CM_USART_TypeDef *M4_USART_ID,
                    en_event_src_t EVT_USART_ID_RI, IRQn_Type Int_ID_IRQn,
                    int DDL_IRQ_PRIORITY_ID, uint16_t cont, uint8_t *recadd) //DMA2 ch0
{

    return 0;
}

int Uart_RS485_send(int uartid, const void *buf, uint32 len)
{
    uint8_t *dat = (uint8*)buf;
    uint16_t i;

    for(i = 0; i < len; i++)
    {
        if (uartid == COMMON_INTERFACE_RS485_1)
        {
            USART_WriteData      (USART_RS485_1, dat[i]);

            while(USART_GetStatus(USART_RS485_1, USART_FLAG_TX_EMPTY) != SET)
            {
            }

        }

        if (uartid == COMMON_INTERFACE_RS485_2)
        {
            USART_WriteData      (USART_RS485_2, dat[i]);

            while(USART_GetStatus(USART_RS485_2, USART_FLAG_TX_EMPTY) != SET)
            {
            }

        }

        if (uartid == COMMON_INTERFACE_RS485_3)
        {
            USART_WriteData      (USART_RS485_3, dat[i]);

            while(USART_GetStatus(USART_RS485_3, USART_FLAG_TX_EMPTY) != SET)
            {
            }

        }
    }

    return len;
}

void uart1_Tx(uint8 c)   //通过串口1把数据C发送出去
{
    USART_WriteData(USART1_UNIT, c);
}

void uart2_Tx(uint8 c)   //通过串口2把数据C发送出去
{
    USART_WriteData(USART2_UNIT, c);
}

void uart3_Tx(uint8 c)   //通过串口3把数据C发送出去
{
    USART_WriteData(USART3_UNIT, c);
}

void uart4_Tx(uint8 c)   //通过串口4把数据C发送出去
{
    USART_WriteData(USART4_UNIT, c);
}

void uart_reloaddmarxptr(int uid)
{

}


int hc32f460_uart_get_bytes_cnt(int uartid, int isrdma)
{

    if(uartid == 0)
    {
        return uart1reccount;
    }
    else if(uartid == 1)
    {
        return uart2reccount;
    }
    else if(uartid == 2)
    {
        return uart3reccount;

    }
    else if(uartid == 3)
    {
        return uart4reccount;
    }
    /* RS485_1/2/3（104/105/106）→ 下标 4/5/6：返回各自的写指针当尾指针。
     * 原来落到 else return 0，read() 永远认为"没数据"。 */
    else if(uartid == 4)
    {
        return (int)rs485_1reccount;
    }
    else if(uartid == 5)
    {
        return (int)rs485_2reccount;
    }
    else if(uartid == 6)
    {
        return (int)rs485_3reccount;
    }
    else
        return 0;
}



/* v9.81cj-e: 串口 TX 互斥锁——替代 v9.80 的 __disable_irq() 全局关中断。
 * 原实现发送期间屏蔽全部中断（480B@115200 ≈ 42ms），模块口(CM_USART4,921600)
 * RX 中断被阻塞 → 读标签丢帧变慢（用户实测：标签移开 4-5s 才输出）。
 * 互斥锁同样防并发写者（printf/uart_send）踩 USART 数据寄存器，但中断全程开放，
 * 模块口 RX 不阻塞。仅线程上下文调用（无 ISR 使用），锁安全。 */
static osMutexId_t s_uart_tx_mux = NULL;
static osRtxMutex_t s_uart_tx_mux_cb;
static void uart_tx_lock(void)
{
    if (s_uart_tx_mux == NULL) {
        osMutexAttr_t attr = { NULL, osMutexPrioInherit, &s_uart_tx_mux_cb, sizeof(s_uart_tx_mux_cb) };
        s_uart_tx_mux = osMutexNew(&attr);
        if (s_uart_tx_mux == NULL) s_uart_tx_mux = (osMutexId_t)1;  /* 失败退化为无锁（不关中断） */
    }
    if (s_uart_tx_mux != (osMutexId_t)1)
        osMutexAcquire(s_uart_tx_mux, osWaitForever);
}
static void uart_tx_unlock(void)
{
    if (s_uart_tx_mux != NULL && s_uart_tx_mux != (osMutexId_t)1)
        osMutexRelease(s_uart_tx_mux);
}

int uart_send(int s, const void *buf, uint32 len, int t485)   //通过串口把数据发送出去
{
    uint8_t *dat = (uint8*)buf;
    uint16_t i;

    if(s == 3 && t485 == 1)
        RS485_set_send();

    uart_tx_lock();

    for(i = 0; i < len; i++)
    {
        if (s == 0)
        {
            USART_WriteData(USART1_UNIT, dat[i]);

            while(USART_GetStatus(USART1_UNIT, USART_FLAG_TX_EMPTY) != SET)
            {
            }


        }
        else if(s == 1)
        {
            USART_WriteData(USART2_UNIT, dat[i]);

            while(USART_GetStatus(USART2_UNIT, USART_FLAG_TX_EMPTY) != SET)
            {
            }



        }
        else if(s == 2)
        {
            USART_WriteData(USART3_UNIT, dat[i]);

            while(USART_GetStatus(USART3_UNIT, USART_FLAG_TX_EMPTY) != SET)
            {

            }


        }
        else if(s == 3)
        {
            USART_WriteData(USART4_UNIT, dat[i]);

            while(USART_GetStatus(USART4_UNIT, USART_FLAG_TX_EMPTY) != SET)
            {

            }


        }
    }

    uart_tx_unlock();

    if(s == 3 && t485 == 1)
        RS485_set_rec();

    return len;
}

int hc32f460_uart_clear_buf(int uartid, int isrdma)
{
    if(uartid == 0)
    {
        uart1reccount = 0;;
        return 0;
    }
    else if(uartid == 1)
    {
        uart2reccount = 0;
        return 0;
    }
    else if(uartid == 2)
    {
        uart3reccount = 0;
        return 0;
    }
    else if(uartid == 3)
    {
        uart4reccount = 0;
        return 0;
    }
    else if(uartid == 4)
    {
        rs485_1reccount = 0;
        return 0;
    }
    else if(uartid == 5)
    {
        rs485_2reccount = 0;
        return 0;
    }
    else if(uartid == 6)
    {
        rs485_3reccount = 0;
        return 0;
    }
    else
        return -1;
}

int  hc32f460_init_uart_close(int uartid)
{
    if(uartid == 0)
    {
        USART_FuncCmd(USART1_UNIT, USART_RX, DISABLE);
        USART_FuncCmd(USART1_UNIT, USART_TX, DISABLE);

        return 0;
    }
    else if(uartid == 1)
    {
        USART_FuncCmd(USART2_UNIT, USART_RX, DISABLE);
        USART_FuncCmd(USART2_UNIT, USART_TX, DISABLE);

        return 0;
    }
    else if(uartid == 2)
    {
        USART_FuncCmd(USART3_UNIT, USART_RX, DISABLE);
        USART_FuncCmd(USART3_UNIT, USART_TX, DISABLE);

        return 0;
    }
    else if(uartid == 3)
    {
        USART_FuncCmd(USART4_UNIT, USART_RX, DISABLE);
        USART_FuncCmd(USART4_UNIT, USART_TX, DISABLE);
        return 0;
    }
    else if(uartid == 4)
    {
        USART_FuncCmd(USART_RS485_1, (USART_RX | USART_TX | USART_INT_RX), DISABLE);
        return 0;
    }
    else if(uartid == 5)
    {
        USART_FuncCmd(USART_RS485_2, (USART_RX | USART_TX | USART_INT_RX), DISABLE);
        return 0;
    }
    else if(uartid == 6)
    {
        USART_FuncCmd(USART_RS485_3, (USART_RX | USART_TX | USART_INT_RX), DISABLE);
        return 0;
    }
    else
    {
        TRACE("hc32f460_init_uart_close err: invalid uartid\n");
        return -1;
    }
}

void uart_err_clear(int s)
{
    commonUartParaLocal *uartpara = &gUartParams[s];
    uartpara->uart_head = 0;
}

//int uart_recv(int s, void *buf, uint32 len)
//{
//    int recvLen = len;
//    commonUartParaLocal *uartpara = &gUartParams[s];

//    if(uartpara->isOpen == 0)
//        return -1;

//    else
//    {
//        switch(s)
//        {
//            case 0:
//            {
//                BUF_Read(&Uart1RingBuf, buf, len);
//                break;
//            }

//            case 1:
//            {
//                BUF_Read(&Uart2RingBuf, buf, len);
//                break;
//            }

//            case 2:
//            {
//                BUF_Read(&Uart3RingBuf, buf, len);
//                break;
//            }

//            case 3:
//            {
//                BUF_Read(&Uart4RingBuf, buf, len);
//                break;
//            }

//            default:
//            {
//                break;
//            }

//        }


//    }

//    return recvLen;
//}

int uart_recv(int s, void *buf, uint32 len)
{
    int recvLen = 0;
    commonUartParaLocal *uartpara = &gUartParams[s];

    if(uartpara->isOpen == 0)
        return -1;

    if (uartpara->uart_head == uartpara->uart_tail)
        return 0;
    else
    {
//		TRACE("start uart_tail:%d, uart_head:%d, len:%d\n", uartpara->uart_tail, uartpara->uart_head, len);
        if (uartpara->uart_tail > uartpara->uart_head)
        {
            recvLen = uartpara->uart_tail - uartpara->uart_head;

            if(recvLen > len)
                recvLen = len;

            memcpy_byb(buf, uartpara->recvbuf + uartpara->uart_head, recvLen);
            uartpara->uart_head += recvLen;
//			printf("00000  recvLen:%d\n", recvLen);
        }
        else
        {
            recvLen = uartpara->recvbufsize - uartpara->uart_head;

            if(recvLen > len)
                recvLen = len;

            memcpy_byb(buf, (char *)uartpara->recvbuf + uartpara->uart_head, recvLen);
            uartpara->uart_head += recvLen;

//			printf("11111  recvLen:%d\n", recvLen);
            if (recvLen < len)
            {
                int band2len = len - recvLen;

                if (uartpara->uart_tail <= band2len)
                    band2len = uartpara->uart_tail;

                memcpy_byb((char *)buf + recvLen, (char *)uartpara->recvbuf, band2len);
                recvLen += band2len;
                uartpara->uart_head = band2len;
//				printf("11111  band2len:%d\n", band2len);
            }
        }
    }

//	TRACE("end uart1_tail:%d, uart1_head:%d, len:%d\n", uart1_tail, uart1_head, len);
    /*
    for (i = 0; i < recvLen; ++i)
    	printf("%02X ", ((char*)buf)[i]);
    printf("\n");
    */
    if (uartpara->uart_head >= uartpara->recvbufsize)
        uartpara->uart_head = 0;

    return recvLen;
}

void Uart_port_test(void)
{
    commonUartPara uartPara;
    uint8_t testbuf[32];
    memset(&uartPara, 0, sizeof(commonUartPara));
    uartPara.isBlock = O_BLOCK;
    uartPara.isPrintf = 0;
    uartPara.baudrate = 460800;
    uartPara.timeout = -1;
    uartPara.isRdam = 1;
    TRACE("[test] uart1 open\n");
    uart_open(COMMON_INTERFACE_UART1, &uartPara);
    TRACE("[test] CR1=%08X SR=%08X\n",
          (unsigned)CM_USART1->CR1, (unsigned)CM_USART1->SR);
    for (int i = 0; i < 32; i++) testbuf[i] = i;
    uart_send(1, testbuf, sizeof(testbuf), 0);   /* s=1 -> USART2_UNIT(CM_USART1/CH340); COMMON_INTERFACE_UART1=101 is WRONG for uart_send */
    TRACE("[test] sent 32, polling SR.RXNE...\n");
    while (1) {
        if (CM_USART1->SR & USART_SR_RXNE) {
            uint8_t d = (uint8_t)(CM_USART1->RDR & 0xFF);
            TRACE("[test] RX 0x%02X\n", d);
            uart_send(1, &d, 1, 0);   /* echo */
        }
        DDL_DelayMS(20);
    }
}


void UART_DeInit(void)
{
    USART_DeInit(USART3_UNIT);
}


void USART3_IT_ENABLE(void)
{
    USART_FuncCmd(USART3_UNIT, USART_INT_RX, ENABLE);//使能接收中断
}

void USART3_IT_DISABLE(void)
{
    USART_FuncCmd(USART3_UNIT, USART_INT_RX, DISABLE);//不使能接收中断
}


void Usart_RS485_init(void)
{
    commonUartPara uartPara;
    memset(&uartPara, 0, sizeof(commonUartPara));
    uartPara.isBlock	= O_BLOCK;
    uartPara.isPrintf	= 1;
    uartPara.baudrate	= 460800;
    uartPara.timeout	= 20;
    uartPara.isRdam = 0;
    uart_open(COMMON_INTERFACE_RS485_1, &uartPara);
    uart_open(COMMON_INTERFACE_RS485_2, &uartPara);
    uart_open(COMMON_INTERFACE_RS485_3, &uartPara);

}







