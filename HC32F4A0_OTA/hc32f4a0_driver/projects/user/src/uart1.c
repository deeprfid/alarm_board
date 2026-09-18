#include "uart.h"
#include "irq.h"
#include "pio.h"
#include <stdlib.h>
#include "ring_buf.h"



/* USART RX/TX pin definition */ // COMMON_INTERFACE_UART0   RFID MODULE 20250317
#define USART1_RX_PORT                   (GPIO_PORT_B)   /* PH13: USART1_RX */
#define USART1_RX_PIN                    (GPIO_PIN_04)
#define USART1_RX_GPIO_FUNC              (GPIO_FUNC_39)

#define USART1_TX_PORT                   (GPIO_PORT_B)   /* PH15: USART1_TX */
#define USART1_TX_PIN                    (GPIO_PIN_03)
#define USART1_TX_GPIO_FUNC              (GPIO_FUNC_38)

/* USART unit definition */

#define USART1_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART4, ENABLE))

/* USART interrupt definition */
#define USART1_RX_ERR_IRQn               (INT000_IRQn)
#define USART1_RX_ERR_INT_SRC            (INT_SRC_USART4_EI)

#define USART1_RX_FULL_IRQn              (INT001_IRQn)
#define USART1_RX_FULL_INT_SRC           (INT_SRC_USART4_RI)






stc_ring_buf_t Uart1RingBuf;


static void USART1_RxFull_IrqCallback(void)
{
//    uint8_t u8Data = (uint8_t)USART_ReadData(USART1_UNIT);

//    (void)BUF_Write(&Uart1RingBuf, &u8Data, 1UL);
//	if (SET == USART_GetStatus(USART1_UNIT, USART_FLAG_RX_FULL ))
    {
        gUart0RecvBuf[uart1reccount] = (uint8_t)USART_ReadData(USART1_UNIT);
        uart1reccount++;

        if(uart1reccount >= MAX_UART0_BUF_SIZE)
           uart1reccount = 0;
    }
}

static void USART1_RxError_IrqCallback(void)
{
    (void)USART_ReadData(USART1_UNIT);
    uart_err_clear(0);
    USART_ClearStatus(USART1_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}





int uart1_Init(uart_cfg_para_st *ucpst)//串口初始化配置函数，即是上电后的配置函数,配置成功返回0，不成功返回1
{
    stc_irq_signin_config_t stcIrqSigninConfig;
    stc_usart_uart_init_t stcInitCfg;
    
	  USART_DeInit(USART1_UNIT);
	
	  USART_FuncCmd(USART1_UNIT, (USART_RX | USART_TX | USART_INT_RX), DISABLE);
	
	  
	
  //  (void)BUF_Init(&Uart1RingBuf, gUart0RecvBuf, MAX_UART0_BUF_SIZE);

    USART1_FCG_ENABLE();

    (void)USART_UART_StructInit(&stcInitCfg);

//    stcInitCfg.u32Baudrate = ucpst->baud;
//    stcInitCfg.u32ClockDiv = (ucpst->baud < 115200) ? (USART_CLK_DIV64) : (USART_CLK_DIV1);
//    stcInitCfg.u32DataWidth = ucpst->databits;
//    stcInitCfg.u32FirstBit = USART_FIRST_BIT_LSB;
//    stcInitCfg.u32StopBit  = ucpst->stopbits;
//    stcInitCfg.u32StartBitPolarity = ucpst->parity;
//    stcInitCfg.u32OverSampleBit  = USART_OVER_SAMPLE_8BIT;
//    stcInitCfg.u32Parity         = USART_START_BIT_FALLING;
//    stcInitCfg.u32HWFlowControl  = ucpst->flowctol;

    /* Initialize USART IO  DISABLE 禁止副功能 */
    GPIO_SetFunc(USART1_RX_PORT, USART1_RX_PIN, USART1_RX_GPIO_FUNC);
    GPIO_SetFunc(USART1_TX_PORT, USART1_TX_PIN, USART1_TX_GPIO_FUNC);

    /* Initialize UART */
    USART_UART_Init(USART1_UNIT, &stcInitCfg, NULL);
		float  temp;
		USART_SetBaudrate(USART1_UNIT,ucpst->baud, &temp);
		
		USART_ClearStatus(USART1_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));

    stcIrqSigninConfig.enIRQn      = USART1_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc    = USART1_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART1_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn      = USART1_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc    = USART1_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART1_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);
    USART_FuncCmd(USART1_UNIT, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    return  0;
}


