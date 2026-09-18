#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"



/* USART RX/TX pin definition */  //COMMON_INTERFACE_UART1 USB-USART CH340 20250317
#define USART2_RX_PORT                   (GPIO_PORT_B)   /* PH13: USART1_RX */
#define USART2_RX_PIN                    (GPIO_PIN_01)
#define USART2_RX_GPIO_FUNC              (GPIO_FUNC_33)

#define USART2_TX_PORT                   (GPIO_PORT_B)   /* PH15: USART1_TX */
#define USART2_TX_PIN                    (GPIO_PIN_00)
#define USART2_TX_GPIO_FUNC              (GPIO_FUNC_32)

/* USART unit definition */

#define USART2_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART1, ENABLE))

/* USART interrupt definition */
#define USART2_RX_ERR_IRQn               (INT086_IRQn)   /* v9.81v: USART1 grp */
#define USART2_RX_ERR_INT_SRC            (INT_SRC_USART1_EI)

#define USART2_RX_FULL_IRQn              (INT087_IRQn)
#define USART2_RX_FULL_INT_SRC           (INT_SRC_USART1_RI)





stc_ring_buf_t Uart2RingBuf;


static void USART2_RxFull_IrqCallback(void)
{
    uint8_t u8Data = (uint8_t)USART_ReadData(USART2_UNIT);

    /* OTA/命令口 UART1(USART1/CH340) RX 缓冲：写入 gUart1RecvBuf，与 uart_recv 通路一致 */
    gUart1RecvBuf[uart2reccount] = u8Data;
    uart2reccount++;

    if (uart2reccount >= MAX_UART1_BUF_SIZE)
        uart2reccount = 0;
}

static void USART2_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART2_UNIT);
	   uart_err_clear(1);

    USART_ClearStatus(USART2_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}





int uart2_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;
	
    /* Initialize USART IO  DISABLE 禁止副功能 */
    GPIO_SetFunc(USART2_RX_PORT, USART2_RX_PIN, USART2_RX_GPIO_FUNC);
    GPIO_SetFunc(USART2_TX_PORT, USART2_TX_PIN, USART2_TX_GPIO_FUNC);
     USART_DeInit(USART2_UNIT);
	
   // (void)BUF_Init(&Uart2RingBuf, gUart1RecvBuf, MAX_UART1_BUF_SIZE);

    USART2_FCG_ENABLE();

    (void)USART_UART_StructInit(&stcInitCfg);

    stcInitCfg.u32Baudrate = ucpst->baud;
    stcInitCfg.u32ClockDiv = (ucpst->baud < 115200) ? (USART_CLK_DIV64) : (USART_CLK_DIV1);
   // stcInitCfg.u32DataWidth = ucpst->databits;
   // stcInitCfg.u32FirstBit = USART_FIRST_BIT_LSB;
   // stcInitCfg.u32StopBit  = ucpst->stopbits;
   // stcInitCfg.u32StartBitPolarity = USART_START_BIT_FALLING;   /* v9.81t-c: was ucpst->parity (wrong) */
    stcInitCfg.u32OverSampleBit  = USART_OVER_SAMPLE_8BIT;
   // stcInitCfg.u32Parity         = USART_PARITY_NONE;              /* v9.81t-c: was SBS -> parity enabled, RX frame err */
   // stcInitCfg.u32HWFlowControl  = ucpst->flowctol;


    /* Initialize UART */
    USART_UART_Init(USART2_UNIT, &stcInitCfg, NULL);

    stcIrqSigninConfig.enIRQn = USART2_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART2_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART2_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART2_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART2_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART2_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    /* v9.81t-c diag: USART1_RI(301)->INT003 mapping + NVIC enable */
    USART_FuncCmd(USART2_UNIT, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    /* v9.81t-c: UART_Init WRITE_REG32 overwrote CR1; ensure RX + RX-INT */
    /* v9.81t-c: direct CR1 write - force 8N1, RX+TX+RX-INT, no parity */
    /* v9.81t-c: match official example - NO SBS (start-bit level detect), 8N1, RX+TX+RX-INT */
    CM_USART1->CR1 = (USART_CR1_OVER8 | USART_CR1_TE | USART_CR1_RE | USART_CR1_RIE);
    return  0;
}


