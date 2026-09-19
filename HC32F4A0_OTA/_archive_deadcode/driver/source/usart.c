
#include "main.h"

/* Peripheral register WE/WP selection */
#define LL_PERIPH_SEL                   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                                         LL_PERIPH_EFM | LL_PERIPH_SRAM)

/* USART RX/TX pin definition */
#define USART_RX_PORT                   (GPIO_PORT_B)   /* PH13: USART1_RX */
#define USART_RX_PIN                    (GPIO_PIN_01)
#define USART_RX_GPIO_FUNC              (GPIO_FUNC_33)

#define USART_TX_PORT                   (GPIO_PORT_B)   /* PH15: USART1_TX */
#define USART_TX_PIN                    (GPIO_PIN_00)
#define USART_TX_GPIO_FUNC              (GPIO_FUNC_32)

/* USART unit definition */
#define USART_UNIT                      (CM_USART1)
#define USART_FCG_ENABLE()              (FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_USART1, ENABLE))

/* USART interrupt definition */
#define USART_RX_ERR_IRQn               (INT000_IRQn)
#define USART_RX_ERR_INT_SRC            (INT_SRC_USART1_EI)

#define USART_RX_FULL_IRQn              (INT001_IRQn)
#define USART_RX_FULL_INT_SRC           (INT_SRC_USART1_RI)

#define USART_TX_EMPTY_IRQn             (INT002_IRQn)
#define USART_TX_EMPTY_INT_SRC          (INT_SRC_USART1_TI)

#define USART_TX_CPLT_IRQn              (INT003_IRQn)
#define USART_TX_CPLT_INT_SRC           (INT_SRC_USART1_TCI)

/* Ring buffer size */
#define RING_BUF_SIZE                   (500UL)

/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/
static uint8_t m_au8DataBuf[RING_BUF_SIZE];
static stc_ring_buf_t m_stcRingBuf;
static __IO en_flag_status_t m_enTxCompleteFlag = SET;

/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

/**
 * @brief  USART transmit data register empty IRQ callback.
 * @param  None
 * @retval None
 */
static void USART_TxEmpty_IrqCallback(void)
{
    uint8_t u8Data;

    if (!BUF_Empty(&m_stcRingBuf)) {
        (void)BUF_Read(&m_stcRingBuf, &u8Data, 1UL);
        USART_WriteData(USART_UNIT, (uint16_t)u8Data);
    } else {
        USART_FuncCmd(USART_UNIT, USART_INT_TX_CPLT, ENABLE);
    }
}

/**
 * @brief  USART transmit complete IRQ callback.
 * @param  None
 * @retval None
 */
static void USART_TxComplete_IrqCallback(void)
{
    m_enTxCompleteFlag = SET;
    USART_FuncCmd(USART_UNIT, (USART_TX | USART_INT_TX_CPLT | USART_INT_TX_EMPTY), DISABLE);
}

/**
 * @brief  USART RX IRQ callback
 * @param  None
 * @retval None
 */


/**
 * @brief  Instal IRQ handler.
 * @param  [in] pstcConfig      Pointer to struct @ref stc_irq_signin_config_t
 * @param  [in] u32Priority     Interrupt priority
 * @retval None
 */
static void INTC_IrqInstalHandler(const stc_irq_signin_config_t *pstcConfig, uint32_t u32Priority)
{
    if (NULL != pstcConfig) {
        (void)INTC_IrqSignIn(pstcConfig);
        NVIC_ClearPendingIRQ(pstcConfig->enIRQn);
        NVIC_SetPriority(pstcConfig->enIRQn, u32Priority);
        NVIC_EnableIRQ(pstcConfig->enIRQn);
    }
}

/**
 * @brief  Main function of UART interrupt project
 * @param  None
 * @retval int32_t return value, if needed
 */
void Usart_init(void)
{
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    /* Configure USART RX/TX pin. */
    GPIO_SetFunc(USART_RX_PORT, USART_RX_PIN, USART_RX_GPIO_FUNC);
    GPIO_SetFunc(USART_TX_PORT, USART_TX_PIN, USART_TX_GPIO_FUNC);

    /* Enable peripheral clock */
    USART_FCG_ENABLE();

    /* Initialize ring buffer function. */
    (void)BUF_Init(&m_stcRingBuf, m_au8DataBuf, sizeof(m_au8DataBuf));

    /* Initialize UART. */
    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv = USART_CLK_DIV64;
    stcUartInit.u32Baudrate = 115200UL;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
    if (LL_OK != USART_UART_Init(USART_UNIT, &stcUartInit, NULL)) {
      
    }

    /* Register RX error IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxError_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register RX full IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_RX_FULL_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_RX_FULL_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_RxFull_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register TX empty IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_TX_EMPTY_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_TX_EMPTY_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_TxEmpty_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* Register TX complete IRQ handler && configure NVIC. */
    stcIrqSigninConfig.enIRQn = USART_TX_CPLT_IRQn;
    stcIrqSigninConfig.enIntSrc = USART_TX_CPLT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &USART_TxComplete_IrqCallback;
    INTC_IrqInstalHandler(&stcIrqSigninConfig, DDL_IRQ_PRIO_DEFAULT);

    /* MCU Peripheral registers write protected */
   // LL_PERIPH_WP(LL_PERIPH_SEL);

    /* Enable RX function */
    USART_FuncCmd(USART_UNIT, (USART_RX | USART_INT_RX), ENABLE);

}

/**
 * @}
 */

/**
 * @}
 */

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
