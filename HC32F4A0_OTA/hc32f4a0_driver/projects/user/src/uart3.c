#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"



/* USART RX/TX pin definition */ //COMMON_INTERFACE_UART2  WIFI 20250317
#define USART3_RX_PORT                   (GPIO_PORT_B)   /* PH13: USART2_RX */
#define USART3_RX_PIN                    (GPIO_PIN_06)
#define USART3_RX_GPIO_FUNC              (GPIO_FUNC_35)

#define USART3_TX_PORT                   (GPIO_PORT_B)   /* PH15: USART2_TX */
#define USART3_TX_PIN                    (GPIO_PIN_05)
#define USART3_TX_GPIO_FUNC              (GPIO_FUNC_34)

/* USART unit definition */

#define USART3_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART2, ENABLE))

/* USART interrupt definition */
#define USART3_RX_ERR_IRQn               (INT004_IRQn)
#define USART3_RX_ERR_INT_SRC            (INT_SRC_USART2_EI)

#define USART3_RX_FULL_IRQn              (INT005_IRQn)
#define USART3_RX_FULL_INT_SRC           (INT_SRC_USART2_RI)







stc_ring_buf_t Uart3RingBuf;


static void USART3_RxFull_IrqCallback(void)
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART3_UNIT);

   // (void)BUF_Write(&Uart3RingBuf, &u8Data, 1UL);
}

static void USART3_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART3_UNIT);
	   uart_err_clear(2);

    USART_ClearStatus(USART3_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}


int uart3_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;

	   USART_DeInit(USART3_UNIT);
   // (void)BUF_Init(&Uart3RingBuf, gUart2RecvBuf, MAX_UART2_BUF_SIZE);

    USART3_FCG_ENABLE();

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
    GPIO_SetFunc(USART3_RX_PORT, USART3_RX_PIN, USART3_RX_GPIO_FUNC);
    GPIO_SetFunc(USART3_TX_PORT, USART3_TX_PIN, USART3_TX_GPIO_FUNC);

    /* Initialize UART */
    USART_UART_Init(USART3_UNIT, &stcInitCfg, NULL);

    stcIrqSigninConfig.enIRQn = USART3_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART3_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART3_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART3_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART3_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART3_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    USART_FuncCmd(USART3_UNIT, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    return  0;
}




