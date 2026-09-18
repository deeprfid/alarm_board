#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"


/* USART RX/TX pin definition */ //COMMON_INTERFACE_UART3  RS485_1 20250317
#define USART8_RX_PORT                   (GPIO_PORT_D)   /* PH13: USART8_RX */
#define USART8_RX_PIN                    (GPIO_PIN_03)
#define USART8_RX_GPIO_FUNC              (GPIO_FUNC_35)

#define USART8_TX_PORT                   (GPIO_PORT_D)   /* PH15: USART8_TX */
#define USART8_TX_PIN                    (GPIO_PIN_02)
#define USART8_TX_GPIO_FUNC              (GPIO_FUNC_34)

/* USART unit definition */

#define USART8_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART8, ENABLE))

/* USART interrupt definition */
#define USART8_RX_ERR_IRQn               (INT008_IRQn)
#define USART8_RX_ERR_INT_SRC            (INT_SRC_USART8_EI)

#define USART8_RX_FULL_IRQn              (INT009_IRQn)
#define USART8_RX_FULL_INT_SRC           (INT_SRC_USART8_RI)





//stc_ring_buf_t Uart4RingBuf;

static void USART8_RxFull_IrqCallback(void)
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART_RS485_2);

   // (void)BUF_Write(&Uart4RingBuf, &u8Data, 1UL);
}

static void USART8_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART_RS485_2);
	   uart_err_clear(3);

    USART_ClearStatus(USART_RS485_2, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}



int uart8_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;

	  USART_DeInit(USART_RS485_2);
	
//    (void)BUF_Init(&Uart4RingBuf, gUart3RecvBuf, MAX_UART3_BUF_SIZE);

    USART8_FCG_ENABLE();

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
    GPIO_SetFunc(USART8_RX_PORT, USART8_RX_PIN, USART8_RX_GPIO_FUNC);
    GPIO_SetFunc(USART8_TX_PORT, USART8_TX_PIN, USART8_TX_GPIO_FUNC);

    /* Initialize UART */
    USART_UART_Init(USART_RS485_2, &stcInitCfg, NULL);

    stcIrqSigninConfig.enIRQn = USART8_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART8_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART8_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART8_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART8_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART8_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    USART_FuncCmd(USART_RS485_2, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    return  0;
}




