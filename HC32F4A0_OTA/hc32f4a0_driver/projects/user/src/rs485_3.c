#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"


/* USART RX/TX pin definition */ //COMMON_INTERFACE_UART3  RS485_1 20250317
#define USART5_RX_PORT                   (GPIO_PORT_D)   /* PH13: USART5_RX */
#define USART5_RX_PIN                    (GPIO_PIN_05)
#define USART5_RX_GPIO_FUNC              (GPIO_FUNC_35)

#define USART5_TX_PORT                   (GPIO_PORT_D)   /* PH15: USART5_TX */
#define USART5_TX_PIN                    (GPIO_PIN_04)
#define USART5_TX_GPIO_FUNC              (GPIO_FUNC_34)

/* USART unit definition */

#define USART5_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART5, ENABLE))

/* USART interrupt definition */
#define USART5_RX_ERR_IRQn               (INT010_IRQn)
#define USART5_RX_ERR_INT_SRC            (INT_SRC_USART5_EI)

#define USART5_RX_FULL_IRQn              (INT011_IRQn)
#define USART5_RX_FULL_INT_SRC           (INT_SRC_USART5_RI)





//stc_ring_buf_t Uart4RingBuf;

static void USART5_RxFull_IrqCallback(void)
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART_RS485_3);

   // (void)BUF_Write(&Uart4RingBuf, &u8Data, 1UL);
}

static void USART5_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART_RS485_3);
	   uart_err_clear(3);

    USART_ClearStatus(USART_RS485_3, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}



int uart5_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;

	  USART_DeInit(USART_RS485_3);
	
//    (void)BUF_Init(&Uart4RingBuf, gUart3RecvBuf, MAX_UART3_BUF_SIZE);

    USART5_FCG_ENABLE();

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
    GPIO_SetFunc(USART5_RX_PORT, USART5_RX_PIN, USART5_RX_GPIO_FUNC);
    GPIO_SetFunc(USART5_TX_PORT, USART5_TX_PIN, USART5_TX_GPIO_FUNC);

    /* Initialize UART */
    USART_UART_Init(USART_RS485_3, &stcInitCfg, NULL);

    stcIrqSigninConfig.enIRQn = USART5_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART5_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART5_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART5_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART5_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART5_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    USART_FuncCmd(USART_RS485_3, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    return  0;
}




