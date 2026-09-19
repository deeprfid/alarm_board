#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"



/* USART RX/TX pin definition */ //COMMON_INTERFACE_UART2  WIFI 20250317
#define USART4_RX_PORT                   (GPIO_PORT_D)   /* PH13: USART10_RX */
#define USART4_RX_PIN                    (GPIO_PIN_15)
#define USART4_RX_GPIO_FUNC              (GPIO_FUNC_39)

#define USART4_TX_PORT                   (GPIO_PORT_D)   /* PH15: USART10_TX */
#define USART4_TX_PIN                    (GPIO_PIN_14)
#define USART4_TX_GPIO_FUNC              (GPIO_FUNC_38)

/* USART unit definition */

#define USART4_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART10, ENABLE))

/* USART interrupt definition */
#define USART4_RX_ERR_IRQn               (INT020_IRQn)
#define USART4_RX_ERR_INT_SRC            (INT_SRC_USART10_EI)

#define USART4_RX_FULL_IRQn              (INT021_IRQn)
#define USART4_RX_FULL_INT_SRC           (INT_SRC_USART10_RI)







stc_ring_buf_t Uart4RingBuf;


static void USART4_RxFull_IrqCallback(void)
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART4_UNIT);

   // (void)BUF_Write(&Uart3RingBuf, &u8Data, 1UL);
}

static void USART4_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART4_UNIT);
    /* 本文件是 id 3（COMMON_INTERFACE_UART3 / USART4_UNIT=CM_USART10，模块口），
     * 原来传的 2 会去清 COMMON_INTERFACE_UART2 的读指针（uart3.c 拷过来的手误）。 */
    uart_err_clear(3);

    USART_ClearStatus(USART4_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}


int uart4_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;

	   USART_DeInit(USART4_UNIT);
   // (void)BUF_Init(&Uart3RingBuf, gUart2RecvBuf, MAX_UART2_BUF_SIZE);

    USART4_FCG_ENABLE();

    (void)USART_UART_StructInit(&stcInitCfg);

    stcInitCfg.u32Baudrate = ucpst->baud;
    stcInitCfg.u32ClockDiv = (ucpst->baud < 115200) ? (USART_CLK_DIV64) : (USART_CLK_DIV1);
    //stcInitCfg.u32DataWidth = ucpst->databits;
    //stcInitCfg.u32FirstBit = USART_FIRST_BIT_LSB;
    //stcInitCfg.u32StopBit  = ucpst->stopbits;
    //stcInitCfg.u32StartBitPolarity = ucpst->parity;
    stcInitCfg.u32OverSampleBit  = USART_OVER_SAMPLE_8BIT;
   // stcInitCfg.u32Parity         = USART_START_BIT_FALLING;
    //stcInitCfg.u32HWFlowControl  = ucpst->flowctol;

    /* Initialize USART IO  DISABLE 禁止副功能 */
    GPIO_SetFunc(USART4_RX_PORT, USART4_RX_PIN, USART4_RX_GPIO_FUNC);
    GPIO_SetFunc(USART4_TX_PORT, USART4_TX_PIN, USART4_TX_GPIO_FUNC);

    /* Initialize UART */
    USART_UART_Init(USART4_UNIT, &stcInitCfg, NULL);

    stcIrqSigninConfig.enIRQn = USART4_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART4_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART4_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART4_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART4_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART4_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    USART_FuncCmd(USART4_UNIT, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    return  0;
}




