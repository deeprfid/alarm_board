/*******************************************************************************
 * radar_port.c -- 雷达串口硬件层实现(三口参数化版)
 *
 * 每个口: USART 初始化/换档(整套 DeInit+Init+BRR回读校验) + 逐字节 RI 中断接收
 *         + 发送(轮询 TXE 逐字节写, 不用 DMA) + 软件环形缓冲
 * 口与硬件对应见 radar_cfg.h 的 RADARn_UART_* 宏; 帧间隔用全局 m_u32Tickms, 不需要定时器。
 ******************************************************************************/
#include "radar_port.h"
#include "ring_buf.h"
#include <string.h>

extern volatile uint32_t m_u32Tickms;

/* ------------------------------ 端口硬件描述 ------------------------------ */
typedef struct {
    CM_USART_TypeDef *unit;
    void (*fcg)(void);
    uint16_t tx_port, tx_pin, tx_func;
    uint16_t rx_port, rx_pin, rx_func;
    IRQn_Type ri_irqn, ei_irqn;
    en_int_src_t ri_src, ei_src;
} radar_hw_t;

static void radar_fcg0(void) { RADAR_UART_FCG_ENABLE(); }
static void radar_fcg1(void) { RADAR2_UART_FCG_ENABLE(); }
static void radar_fcg2(void) { RADAR3_UART_FCG_ENABLE(); }
static const radar_hw_t s_hw[RADAR_PORT_CNT] = {
    { RADAR_UART_UNIT,  radar_fcg0,  RADAR_UART_TX_PORT,  RADAR_UART_TX_PIN,  RADAR_UART_TX_FUNC,
      RADAR_UART_RX_PORT,  RADAR_UART_RX_PIN,  RADAR_UART_RX_FUNC,
      RADAR_UART_RX_IRQn,  RADAR_UART_RX_ERR_IRQn,  RADAR_UART_RX_INT_SRC,  RADAR_UART_RX_ERR_INT_SRC },
    { RADAR2_UART_UNIT, radar_fcg1, RADAR2_UART_TX_PORT, RADAR2_UART_TX_PIN, RADAR2_UART_TX_FUNC,
      RADAR2_UART_RX_PORT, RADAR2_UART_RX_PIN, RADAR2_UART_RX_FUNC,
      RADAR2_UART_RX_IRQn, RADAR2_UART_RX_ERR_IRQn, RADAR2_UART_RX_INT_SRC, RADAR2_UART_RX_ERR_INT_SRC },
    { RADAR3_UART_UNIT, radar_fcg2, RADAR3_UART_TX_PORT, RADAR3_UART_TX_PIN, RADAR3_UART_TX_FUNC,
      RADAR3_UART_RX_PORT, RADAR3_UART_RX_PIN, RADAR3_UART_RX_FUNC,
      RADAR3_UART_RX_IRQn, RADAR3_UART_RX_ERR_IRQn, RADAR3_UART_RX_INT_SRC, RADAR3_UART_RX_ERR_INT_SRC },
};

/* ------------------------------ 每口上下文 ------------------------------ */
static uint8_t             s_rx_buf[RADAR_PORT_CNT][RADAR_RX_RING_SIZE];
static stc_ring_buf_t      s_rx_ring[RADAR_PORT_CNT];
static uint8_t             s_tx_buf[RADAR_PORT_CNT][RADAR_TX_MAX];
static volatile uint16_t   s_tx_len[RADAR_PORT_CNT];
static volatile uint16_t   s_tx_idx[RADAR_PORT_CNT];
static volatile uint8_t    s_tx_busy[RADAR_PORT_CNT];
static volatile uint32_t   s_tx_ms[RADAR_PORT_CNT];
static volatile uint32_t   s_rx_bytes[RADAR_PORT_CNT];
static volatile uint32_t   s_rx_drop[RADAR_PORT_CNT];
static uint32_t            s_baud[RADAR_PORT_CNT];
static volatile uint8_t    s_baud_ok[RADAR_PORT_CNT];
static void (*s_rx_cb[RADAR_PORT_CNT])(const uint8_t *data, uint16_t len);

/* --------------------- 时钟分频: 随波特率自动选 ---------------------
 * 规则照抄扫描台主板(同款 HC32F460)量产写法: baud < 115200 -> DIV64, 否则 DIV1。
 * 依据: BRR 整数分频只有 8 位, DIV_Integer = C/(B*8*(2-OVER8)) - 1 必须 <= 255。 */
static uint32_t radar_pick_clk_div(uint32_t baud)
{
    return (baud < 115200UL) ? USART_CLK_DIV64 : USART_CLK_DIV1;
}

/* ------------------------------ 中断处理 ------------------------------ */
static void radar_rx_isr(uint8_t port)
{
    uint8_t b = (uint8_t)USART_ReadData(s_hw[port].unit);   /* 读 RDR 同时清 RI */

    s_rx_bytes[port]++;
    if (BUF_Write(&s_rx_ring[port], &b, 1U) != 1U) { s_rx_drop[port]++; }
}

static void radar_err_isr(uint8_t port)
{
    (void)USART_ReadData(s_hw[port].unit);
    USART_ClearStatus(s_hw[port].unit, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}

static void radar0_rx_isr(void) { radar_rx_isr(0U); }
static void radar0_err_isr(void) { radar_err_isr(0U); }
static void radar1_rx_isr(void) { radar_rx_isr(1U); }
static void radar1_err_isr(void) { radar_err_isr(1U); }
static void radar2_rx_isr(void) { radar_rx_isr(2U); }
static void radar2_err_isr(void) { radar_err_isr(2U); }

/* ------------------------------ 内部: USART 完整初始化 ------------------------------ */
static void radar_usart_init(uint8_t port)
{
    stc_usart_uart_init_t stcInit;
    float32_t f32Err = 0.0F;
    uint32_t  psc, c, exp_int, got_int;
    const radar_hw_t *hw = &s_hw[port];

    (void)USART_UART_StructInit(&stcInit);
    stcInit.u32ClockDiv      = radar_pick_clk_div(s_baud[port]);
    stcInit.u32CKOutput      = USART_CK_OUTPUT_DISABLE;
    stcInit.u32Baudrate      = s_baud[port];
    stcInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;

    s_baud_ok[port] = (LL_OK == USART_UART_Init(hw->unit, &stcInit, &f32Err)) ? 1U : 0U;

    /* 回读校验: 防止返回 OK 却没写进 BRR(现场踩过) */
    psc     = READ_REG32_BIT(hw->unit->PR, USART_PR_PSC);
    c       = RADAR_UART_PCLK_HZ >> (psc * 2UL);
    exp_int = (c / (s_baud[port] * 8UL)) - 1UL;
    got_int = (hw->unit->BRR >> 8) & 0xFFUL;
    if (got_int != exp_int) { s_baud_ok[port] = 0U; }
}

/* ------------------------------ 对外接口 ------------------------------ */
int32_t radar_port_init(uint8_t port)
{
    stc_irq_signin_config_t cfg;
    const radar_hw_t *hw;
    void (*rx_isr)(void);
    void (*err_isr)(void);

    if (port >= (uint8_t)RADAR_PORT_CNT) { return LL_ERR_INVD_PARAM; }
    hw = &s_hw[port];

    s_tx_busy[port] = 0U; s_tx_len[port] = 0U; s_tx_idx[port] = 0U;
    s_rx_bytes[port] = 0U; s_rx_drop[port] = 0U; s_rx_cb[port] = 0;
#if (RADAR_BAUD_INIT_FIXED != 0UL)
    s_baud[port] = RADAR_BAUD_INIT_FIXED;
#else
    s_baud[port] = RADAR_BAUD_FALLBACK;
#endif

    (void)BUF_Init(&s_rx_ring[port], s_rx_buf[port], sizeof(s_rx_buf[port]));
    memset(s_rx_buf[port], 0, sizeof(s_rx_buf[port]));

    GPIO_SetFunc((uint8_t)hw->rx_port, (uint16_t)hw->rx_pin, (uint16_t)hw->rx_func);
    GPIO_SetFunc((uint8_t)hw->tx_port, (uint16_t)hw->tx_pin, (uint16_t)hw->tx_func);

    hw->fcg();
    USART_DeInit(hw->unit);                    /* 幂等: 先清成确定状态 */
    radar_usart_init(port);

    rx_isr  = (port == 0U) ? &radar0_rx_isr  : ((port == 1U) ? &radar1_rx_isr  : &radar2_rx_isr);
    err_isr = (port == 0U) ? &radar0_err_isr : ((port == 1U) ? &radar1_err_isr : &radar2_err_isr);

    cfg.enIRQn      = hw->ri_irqn;  cfg.enIntSrc = hw->ri_src;  cfg.pfnCallback = rx_isr;
    (void)INTC_IrqSignIn(&cfg);
    NVIC_ClearPendingIRQ(cfg.enIRQn); NVIC_SetPriority(cfg.enIRQn, DDL_IRQ_PRIO_DEFAULT); NVIC_EnableIRQ(cfg.enIRQn);

    cfg.enIRQn      = hw->ei_irqn;  cfg.enIntSrc = hw->ei_src;  cfg.pfnCallback = err_isr;
    (void)INTC_IrqSignIn(&cfg);
    NVIC_ClearPendingIRQ(cfg.enIRQn); NVIC_SetPriority(cfg.enIRQn, DDL_IRQ_PRIO_DEFAULT); NVIC_EnableIRQ(cfg.enIRQn);

    USART_FuncCmd(hw->unit, (USART_RX | USART_TX | USART_INT_RX), ENABLE);

    return LL_OK;
}

int32_t radar_port_write(uint8_t port, const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if ((port >= (uint8_t)RADAR_PORT_CNT) || (buf == 0) || (len == 0U) || (len > RADAR_TX_MAX)) { return LL_ERR_INVD_PARAM; }
    if (s_tx_busy[port] != 0U) { return LL_ERR_BUSY; }

    for (i = 0U; i < len; i++) { s_tx_buf[port][i] = buf[i]; }
    s_tx_len[port]  = len;
    s_tx_idx[port]  = 0U;
    s_tx_busy[port] = 1U;
    s_tx_ms[port]   = m_u32Tickms;

    return LL_OK;
}

uint8_t radar_port_tx_busy(uint8_t port)
{
    return (port < (uint8_t)RADAR_PORT_CNT) ? s_tx_busy[port] : 0U;
}

/* 发送泵: 轮询 TXE 逐字节写(不用 DMA/中断)。调用频率足够高即可, 字节间偶有空隙无害。 */
static void radar_tx_pump(uint8_t port)
{
    CM_USART_TypeDef *u = s_hw[port].unit;

    if (s_tx_busy[port] == 0U) { return; }

    if (s_tx_idx[port] < s_tx_len[port])
    {
        if (SET == USART_GetStatus(u, USART_FLAG_TX_EMPTY))
        {
            USART_WriteData(u, (uint16_t)s_tx_buf[port][s_tx_idx[port]]);
            s_tx_idx[port]++;
        }
    }
    else if (SET == USART_GetStatus(u, USART_FLAG_TX_CPLT))
    {
        s_tx_busy[port] = 0U;                  /* 全部发完 */
    }
}

void radar_port_tx_watchdog(uint8_t port, uint32_t now_ms)
{
    if (port >= (uint8_t)RADAR_PORT_CNT) { return; }
    if (s_tx_busy[port] == 0U) { return; }
    if ((now_ms - s_tx_ms[port]) < RADAR_TX_TIMEOUT_MS) { return; }

    s_tx_busy[port] = 0U;                      /* 兜底放行 */
    USART_ClearStatus(s_hw[port].unit, USART_FLAG_TX_CPLT);
}

void radar_port_rx_flush(uint8_t port)
{
    uint32_t primask;

    if (port >= (uint8_t)RADAR_PORT_CNT) { return; }

    primask = __get_PRIMASK();
    __disable_irq();
    (void)BUF_Init(&s_rx_ring[port], s_rx_buf[port], sizeof(s_rx_buf[port]));
    __set_PRIMASK(primask);
}

/* 换波特率: 整套重来一遍, 不留任何痕迹(关收发 -> DeInit -> 重新初始化 -> 清状态 -> 重开收发) */
void radar_port_set_baud(uint8_t port, uint32_t baud)
{
    if (port >= (uint8_t)RADAR_PORT_CNT) { return; }

    USART_FuncCmd(s_hw[port].unit, (USART_RX | USART_TX | USART_INT_RX), DISABLE);
    USART_DeInit(s_hw[port].unit);

    s_baud[port]    = baud;
    s_tx_busy[port] = 0U;
    radar_usart_init(port);

    USART_ClearStatus(s_hw[port].unit, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR |
                                       USART_FLAG_OVERRUN | USART_FLAG_TX_CPLT));
    NVIC_ClearPendingIRQ(s_hw[port].ri_irqn);
    NVIC_ClearPendingIRQ(s_hw[port].ei_irqn);

    USART_FuncCmd(s_hw[port].unit, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    radar_port_rx_flush(port);
}

uint32_t radar_port_get_baud(uint8_t port)   { return s_baud[port]; }
uint8_t  radar_port_baud_ok(uint8_t port)    { return s_baud_ok[port]; }
uint32_t radar_port_brr(uint8_t port)        { return s_hw[port].unit->BRR; }
uint32_t radar_port_rx_bytes(uint8_t port)   { return s_rx_bytes[port]; }
uint32_t radar_port_rx_drop(uint8_t port)    { return s_rx_drop[port]; }

/* 硬件实际在跑的波特率(PR 分频 + BRR 整数分频反推, 就近取协议表档位) */
uint32_t radar_port_baud_actual(uint8_t port)
{
    static const uint32_t tab[] = RADAR_BAUD_TABLE;
    uint32_t psc, c, k, approx, best = 0UL, diff, bestdiff = 0xFFFFFFFFUL;
    uint8_t  i;

    psc = READ_REG32_BIT(s_hw[port].unit->PR, USART_PR_PSC);
    c   = RADAR_UART_PCLK_HZ >> (psc * 2UL);
    k   = ((s_hw[port].unit->BRR >> 8) & 0xFFUL) + 1UL;
    if (k == 0UL) { return 0UL; }
    approx = c / (8UL * k);

    for (i = 0U; i < (uint8_t)RADAR_BAUD_TABLE_CNT; i++)
    {
        diff = (approx > tab[i]) ? (approx - tab[i]) : (tab[i] - approx);
        if (diff < bestdiff) { bestdiff = diff; best = tab[i]; }
    }
    return best;
}

void radar_port_set_rx_handler(uint8_t port, void (*handler)(const uint8_t *data, uint16_t len))
{
    if (port < (uint8_t)RADAR_PORT_CNT) { s_rx_cb[port] = handler; }
}

void radar_port_poll(uint8_t port)
{
    uint8_t b;

    if (port >= (uint8_t)RADAR_PORT_CNT) { return; }

    radar_tx_pump(port);

    while (BUF_UsedSize(&s_rx_ring[port]) > 0U)
    {
        if (BUF_Read(&s_rx_ring[port], &b, 1U) != 1U) { break; }
        if (s_rx_cb[port] != 0) { s_rx_cb[port](&b, 1U); }
    }
}
