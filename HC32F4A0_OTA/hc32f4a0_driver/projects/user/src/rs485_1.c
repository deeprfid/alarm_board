#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"


/* USART RX/TX pin definition */ //COMMON_INTERFACE_UART3  RS485_1 20250317
#define USART_RS485_RX_PORT                   (GPIO_PORT_C)   /* PH13: USART_RS485_RX */
#define USART_RS485_RX_PIN                    (GPIO_PIN_01)
#define USART_RS485_RX_GPIO_FUNC              (GPIO_FUNC_33)

#define USART_RS485_TX_PORT                   (GPIO_PORT_C)   /* PH15: USART_RS485_TX */
#define USART_RS485_TX_PIN                    (GPIO_PIN_00)
#define USART_RS485_TX_GPIO_FUNC              (GPIO_FUNC_32)

/* USART unit definition */

#define USART_RS485_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART3, ENABLE))

/* USART interrupt definition */
#define USART_RS485_RX_ERR_IRQn               (INT012_IRQn)
#define USART_RS485_RX_ERR_INT_SRC            (INT_SRC_USART3_EI)

#define USART_RS485_RX_FULL_IRQn              (INT013_IRQn)
#define USART_RS485_RX_FULL_INT_SRC           (INT_SRC_USART3_RI)






static void USART_RS485_RxFull_IrqCallback(void)
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART_RS485_1);

    /* RX 通路（三处断点之一）：中断里必须把字节落到 gUartParams[4].recvbuf
     * = gRs485_1RecvBuf，并把写指针 rs485_1reccount 往前推——
     * 它就是 hc32f460_uart_get_bytes_cnt() 返回给 read()/uart_recv() 的尾指针。
     * 之前这行被注释掉，读出来的字节直接丢弃，read() 永远取不到数据。 */
    gRs485_1RecvBuf[rs485_1reccount] = u8Data;
    rs485_1reccount++;

    if (rs485_1reccount >= MAX_RS485_1_BUF_SIZE)
        rs485_1reccount = 0;
}

static void USART_RS485_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART_RS485_1);
    /* uart_err_clear(s) 清的是 gUartParams[s].uart_head —— 必须传【本口】下标。
     * RS485_1 = COMMON_INTERFACE_RS485_1(104)，即下标 4（原来写的是 3，清到 RFID 模块口去了）。 */
    uart_err_clear(4);

    USART_ClearStatus(USART_RS485_1, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}



int uart_rs485_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;

	  USART_DeInit(USART_RS485_1);
	
    GPIO_SetDebugPort(GPIO_PIN_TRST,DISABLE);
	  GPIO_SetDebugPort(GPIO_PIN_TDO ,DISABLE);
    USART_RS485_FCG_ENABLE();

    (void)USART_UART_StructInit(&stcInitCfg);

    stcInitCfg.u32Baudrate = ucpst->baud;
    stcInitCfg.u32ClockDiv = (ucpst->baud < 115200) ? (USART_CLK_DIV64) : (USART_CLK_DIV1);
    stcInitCfg.u32DataWidth = ucpst->databits;
    stcInitCfg.u32FirstBit = USART_FIRST_BIT_LSB;
    stcInitCfg.u32StopBit  = ucpst->stopbits;
    stcInitCfg.u32StartBitPolarity = ucpst->parity;
    stcInitCfg.u32OverSampleBit  = USART_OVER_SAMPLE_8BIT;
    stcInitCfg.u32Parity         = USART_START_BIT_FALLING;
    stcInitCfg.u32HWFlowControl  = ucpst->flowctol;

    /* Initialize USART IO  DISABLE 禁止副功能 */
    GPIO_SetFunc(USART_RS485_RX_PORT, USART_RS485_RX_PIN, USART_RS485_RX_GPIO_FUNC);
    GPIO_SetFunc(USART_RS485_TX_PORT, USART_RS485_TX_PIN, USART_RS485_TX_GPIO_FUNC);

    /* Initialize UART */
    USART_UART_Init(USART_RS485_1, &stcInitCfg, NULL);

    stcIrqSigninConfig.enIRQn = USART_RS485_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RS485_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RS485_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_RS485_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RS485_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RS485_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    USART_FuncCmd(USART_RS485_1, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    return  0;
}




