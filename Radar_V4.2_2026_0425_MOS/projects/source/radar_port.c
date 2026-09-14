/*******************************************************************************
 * radar_port.c -- 雷达串口硬件层实现
 *
 * 硬件通路: USART1_RI -> DMA2 CH1(循环窗口) -> TMR0_1 空闲超时 -> 软件环形缓冲
 *           DMA2 CH0 -> USART1_TI 发送
 * 参考同工程 bsp_rs485.c(USART4)的可用实现, 只是换成 USART1 与独立缓冲。
 ******************************************************************************/
#include "radar_port.h"
#include "ring_buf.h"          /* BUF_* 环形缓冲 */
#include <string.h>           /* memset */

/* 1ms 计数(定义在 bsp_exint.c) */
extern uint32_t m_u32Tickms;

/* ------------------------------ 静态数据 ------------------------------ */
static __align(64) uint8_t s_rx_win[RADAR_RX_WIN];
static __align(64) uint8_t s_rx_ring_buf[RADAR_RX_RING_SIZE];
static stc_ring_buf_t      s_rx_ring;

static volatile uint8_t    s_tx_busy;
static volatile uint32_t   s_tx_ms;          /* 本次发送开始时刻 */

volatile uint32_t          g_radar_tx_dma_tc_cnt;
volatile uint32_t          g_radar_tx_tci_cnt;
volatile uint32_t          g_radar_tx_timeout_cnt;
static volatile uint32_t   s_rx_bytes;
static volatile uint32_t   s_rx_drop;
static uint32_t            s_baud;
volatile uint32_t          g_radar_brr;   /* 波特率寄存器实际值(16倍过采样: 9600->0xA17F=41471, 460800->0x0262=610) */
volatile uint32_t          g_radar_rx_bytes;
volatile uint32_t          g_radar_pin_low;
volatile uint32_t          g_radar_dma_left;
volatile uint32_t          g_radar_rx_err;   /* RX 错误中断(帧错/校验错/溢出)次数 */
volatile uint32_t          g_radar_low_pct;   /* 最近 1 秒 PA3 为低电平的采样占比(0~100) */
volatile uint32_t          g_radar_bps;       /* 最近 1 秒搬进环形缓冲的字节数 */
volatile uint32_t          g_radar_dma_fill;  /* 最近 1 秒 RX DMA 实际搬走的字节数(不依赖超时路径) */
volatile uint32_t          g_radar_poll_hz;   /* 最近 1 秒 radar_port_poll 被调用次数 */
volatile uint32_t          g_radar_to_hz;     /* 最近 1 秒 RX 空闲超时中断次数 */
volatile uint32_t          g_radar_win_hz;    /* 最近 1 秒 RX 窗口满(DMA TC)中断次数 */
static uint32_t            s_to_cnt;
static uint32_t            s_win_cnt;
static uint32_t            s_prev_to;
static uint32_t            s_prev_win;
static uint32_t            s_win_ms;
static uint32_t            s_win_bytes;
static uint32_t            s_win_polls;
static uint32_t            s_win_lows;
static uint32_t            s_win_fill;
static void (*s_rx_cb)(const uint8_t *data, uint16_t len) = 0;

/* ------------------------------ 中断回调 ------------------------------ */
static void radar_rx_dma_tc_cb(void)
{
    /* 窗口满: 整窗上抛(雷达帧远小于窗口, 只有异常突发才会走到这里) */
    if (BUF_Write(&s_rx_ring, s_rx_win, RADAR_RX_WIN) != RADAR_RX_WIN) { s_rx_drop++; }
    s_rx_bytes += RADAR_RX_WIN;
    s_win_cnt++;

    AOS_SW_Trigger();                                   /* 重新装载 RX DMA */
    DMA_ClearTransCompleteStatus(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_TC_FLAG);
}

static void radar_rx_timeout_cb(void)
{
    uint16_t left = (uint16_t)DMA_GetTransCount(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH);
    uint16_t got = (uint16_t)(RADAR_RX_WIN - left);
    s_to_cnt++;

    if (got > 0U)
    {
        if (BUF_Write(&s_rx_ring, s_rx_win, got) != got) { s_rx_drop += (uint32_t)got; }
        s_rx_bytes += got;
    }

    AOS_SW_Trigger();                                   /* 重新装载 RX DMA */
    TMR0_Stop(RADAR_TMR0_UNIT, RADAR_TMR0_CH);
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_RX_TIMEOUT);
}

static void radar_tx_complete_cb(void)
{
    USART_FuncCmd(RADAR_UART_UNIT, (USART_TX | USART_INT_TX_CPLT), DISABLE);
    g_radar_tx_tci_cnt++;

    TMR0_Stop(RADAR_TMR0_UNIT, RADAR_TMR0_CH);
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_RX_TIMEOUT);
    USART_FuncCmd(RADAR_UART_UNIT, USART_RX_TIMEOUT, ENABLE);
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_TX_CPLT);

    s_tx_busy = 0U;                                     /* 发送真正完成, 允许下一帧 */
}

static void radar_rx_error_cb(void)
{
    g_radar_rx_err++;
    (void)USART_ReadData(RADAR_UART_UNIT);
    USART_ClearStatus(RADAR_UART_UNIT,
                      (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}

static void radar_tx_dma_tc_cb(void)
{
    USART_FuncCmd(RADAR_UART_UNIT, USART_INT_TX_CPLT, ENABLE);
    g_radar_tx_dma_tc_cnt++;
    DMA_ClearTransCompleteStatus(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_FLAG);
}

/* ------------------------------ DMA / TMR0 ------------------------------ */
static int32_t radar_dma_config(void)
{
    int32_t i32Ret;
    stc_dma_init_t stcDmaInit;
    stc_dma_llp_init_t stcDmaLlpInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    static stc_dma_llp_descriptor_t stcLlpDesc;

    RADAR_RX_DMA_FCG_ENABLE();
    RADAR_TX_DMA_FCG_ENABLE();
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

    /* ---- RX: USART1_RDR -> s_rx_win, 循环窗口 + 软件重装 ---- */
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn       = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = ARRAY_SZ(s_rx_win);
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr    = (uint32_t)s_rx_win;
    stcDmaInit.u32SrcAddr     = (uint32_t)(&RADAR_UART_UNIT->RDR);
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    i32Ret = DMA_Init(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, &stcDmaInit);
    if (LL_OK != i32Ret) { return i32Ret; }

    (void)DMA_LlpStructInit(&stcDmaLlpInit);
    stcDmaLlpInit.u32State = DMA_LLP_ENABLE;
    stcDmaLlpInit.u32Mode  = DMA_LLP_WAIT;
    stcDmaLlpInit.u32Addr  = (uint32_t)&stcLlpDesc;
    (void)DMA_LlpInit(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, &stcDmaLlpInit);

    stcLlpDesc.SARx   = stcDmaInit.u32SrcAddr;
    stcLlpDesc.DARx   = stcDmaInit.u32DestAddr;
    stcLlpDesc.DTCTLx = (stcDmaInit.u32TransCount << DMA_DTCTL_CNT_POS) |
                        (stcDmaInit.u32BlockSize << DMA_DTCTL_BLKSIZE_POS);
    stcLlpDesc.LLPx   = (uint32_t)&stcLlpDesc;
    stcLlpDesc.CHCTLx = stcDmaInit.u32SrcAddrInc | stcDmaInit.u32DestAddrInc |
                        stcDmaInit.u32DataWidth | stcDmaInit.u32IntEn |
                        stcDmaLlpInit.u32State | stcDmaLlpInit.u32Mode;

    DMA_ReconfigLlpCmd(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, ENABLE);
    DMA_ReconfigCmd(RADAR_RX_DMA_UNIT, ENABLE);
    AOS_SetTriggerEventSrc(RADAR_RX_DMA_RECONF_TRIG_SEL, RADAR_RX_DMA_RECONF_TRIG_EVT_SRC);

    stcIrqSignConfig.enIntSrc    = RADAR_RX_DMA_TC_INT_SRC;
    stcIrqSignConfig.enIRQn      = RADAR_RX_DMA_TC_IRQn;
    stcIrqSignConfig.pfnCallback = &radar_rx_dma_tc_cb;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

    AOS_SetTriggerEventSrc(RADAR_RX_DMA_TRIG_SEL, RADAR_RX_DMA_TRIG_EVT_SRC);

    DMA_Cmd(RADAR_RX_DMA_UNIT, ENABLE);
    DMA_TransCompleteIntCmd(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_TC_INT, ENABLE);
    (void)DMA_ChCmd(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, ENABLE);

    /* ---- TX: 内存 -> USART1_TDR ---- */
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn       = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = 1UL;
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr    = (uint32_t)(&RADAR_UART_UNIT->TDR);
    stcDmaInit.u32SrcAddr     = (uint32_t)s_rx_win;
    stcDmaInit.u32SrcAddrInc  = DMA_SRC_ADDR_INC;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_FIX;
    i32Ret = DMA_Init(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, &stcDmaInit);
    if (LL_OK != i32Ret) { return i32Ret; }

    stcIrqSignConfig.enIntSrc    = RADAR_TX_DMA_TC_INT_SRC;
    stcIrqSignConfig.enIRQn      = RADAR_TX_DMA_TC_IRQn;
    stcIrqSignConfig.pfnCallback = &radar_tx_dma_tc_cb;
    (void)INTC_IrqSignIn(&stcIrqSignConfig);
    NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
    NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

    AOS_SetTriggerEventSrc(RADAR_TX_DMA_TRIG_SEL, RADAR_TX_DMA_TRIG_EVT_SRC);

    DMA_Cmd(RADAR_TX_DMA_UNIT, ENABLE);
    DMA_TransCompleteIntCmd(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_INT, ENABLE);

    return LL_OK;
}

static void radar_tmr0_config(uint16_t u16TimeoutBits)
{
    uint16_t u16Div;
    uint16_t u16Delay;
    uint16_t u16CompareValue;
    stc_tmr0_init_t stcTmr0Init;

    RADAR_TMR0_FCG_ENABLE();

    stcTmr0Init.u32ClockSrc = TMR0_CLK_SRC_XTAL32;
    stcTmr0Init.u32ClockDiv = TMR0_CLK_DIV8;
    stcTmr0Init.u32Func     = TMR0_FUNC_CMP;

    if (TMR0_CLK_DIV1 == stcTmr0Init.u32ClockDiv) { u16Delay = 7U; }
    else if (TMR0_CLK_DIV2 == stcTmr0Init.u32ClockDiv) { u16Delay = 5U; }
    else if ((TMR0_CLK_DIV4 == stcTmr0Init.u32ClockDiv) ||
             (TMR0_CLK_DIV8 == stcTmr0Init.u32ClockDiv) ||
             (TMR0_CLK_DIV16 == stcTmr0Init.u32ClockDiv)) { u16Delay = 3U; }
    else { u16Delay = 2U; }

    u16Div = (uint16_t)1U << (stcTmr0Init.u32ClockDiv >> TMR0_BCONR_CKDIVA_POS);
    u16CompareValue = ((u16TimeoutBits + u16Div - 1U) / u16Div) - u16Delay;
    stcTmr0Init.u16CompareValue = u16CompareValue;
    (void)TMR0_Init(RADAR_TMR0_UNIT, RADAR_TMR0_CH, &stcTmr0Init);

    TMR0_HWStartCondCmd(RADAR_TMR0_UNIT, RADAR_TMR0_CH, ENABLE);
    TMR0_HWClearCondCmd(RADAR_TMR0_UNIT, RADAR_TMR0_CH, ENABLE);
}

/* ------------------------------ 对外接口 ------------------------------ */
void radar_port_init(void)
{
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    s_tx_busy = 0U;
    s_rx_bytes = 0U;
    s_rx_drop = 0U;
#if (RADAR_BAUD_INIT_FIXED != 0UL)
    s_baud = RADAR_BAUD_INIT_FIXED;          /* 调试: 上电即该波特率(走初始化路径, 不运行时切换) */
#else
    s_baud = RADAR_BAUD_FALLBACK;
#endif

    (void)BUF_Init(&s_rx_ring, s_rx_ring_buf, sizeof(s_rx_ring_buf));
    memset(s_rx_win, 0, sizeof(s_rx_win));
    memset(s_rx_ring_buf, 0, sizeof(s_rx_ring_buf));

    GPIO_SetFunc(RADAR_UART_RX_PORT, RADAR_UART_RX_PIN, RADAR_UART_RX_FUNC);
    GPIO_SetFunc(RADAR_UART_TX_PORT, RADAR_UART_TX_PIN, RADAR_UART_TX_FUNC);

    RADAR_UART_FCG_ENABLE();

    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv      = RADAR_BAUD_CLK_DIV;
    stcUartInit.u32CKOutput      = USART_CK_OUTPUT_ENABLE;
    stcUartInit.u32Baudrate      = s_baud;
    /* 时钟分频/过采样: 取值说明见 radar_cfg.h 的 RADAR_BAUD_CLK_DIV。
     * DDL 的 BRR 整数分频只有 8 位(<=255), 8 倍过采样下 C 必须 <= 2048*最低波特率;
     * 本档 C = 100MHz/16 = 6.25MHz + 8 倍过采样 -> 9600~460800 全部可表示。 */
    stcUartInit.u32OverSampleBit = RADAR_BAUD_OVER_SAMPLE;
    (void)USART_UART_Init(RADAR_UART_UNIT, &stcUartInit, NULL);

    (void)radar_dma_config();
    radar_tmr0_config(RADAR_RX_TIMEOUT_BITS);

    g_radar_brr = RADAR_UART_UNIT->BRR;      /* 回读: 确认初始化时波特率真的写进去了 */

    /* TX 完成 */
    stcIrqSigninConfig.enIRQn      = RADAR_UART_TX_CPLT_IRQn;
    stcIrqSigninConfig.enIntSrc    = RADAR_UART_TX_CPLT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &radar_tx_complete_cb;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* RX 错误 */
    stcIrqSigninConfig.enIRQn      = RADAR_UART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc    = RADAR_UART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &radar_rx_error_cb;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* RX 空闲超时 */
    stcIrqSigninConfig.enIRQn      = RADAR_UART_RX_TIMEOUT_IRQn;
    stcIrqSigninConfig.enIntSrc    = RADAR_UART_RX_TIMEOUT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &radar_rx_timeout_cb;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    USART_FuncCmd(RADAR_UART_UNIT, (USART_RX | USART_TX | USART_INT_RX | USART_RX_TIMEOUT |
                                    USART_INT_RX_TIMEOUT), ENABLE);
}

int32_t radar_port_write(const uint8_t *buf, uint16_t len)
{
    if ((buf == 0) || (len == 0U) || (len > RADAR_TX_MAX)) { return LL_ERR_INVD_PARAM; }
    if (s_tx_busy != 0U) { return LL_ERR_BUSY; }

    s_tx_busy = 1U;
    s_tx_ms   = m_u32Tickms;

    (void)DMA_SetSrcAddr(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, (uint32_t)buf);
    (void)DMA_SetTransCount(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, len);
    (void)DMA_ChCmd(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, ENABLE);
    USART_FuncCmd(RADAR_UART_UNIT, USART_TX, ENABLE);

    return LL_OK;
}

uint8_t radar_port_tx_busy(void)
{
    return s_tx_busy;
}

/* 兜底: 发送完成后 DMA TC -> 使能 USART TCI -> 清 s_tx_busy。
 * 若这条链任何一环没来, s_tx_busy 会一直为 1, 之后所有命令都发不出去(返回 LL_ERR_BUSY)。
 * 这里按时间兜底: 超过 RADAR_TX_TIMEOUT_MS 仍未完成 -> 复位 TX 通路并放行(同时计数, 便于定位)。 */
void radar_port_tx_watchdog(uint32_t now_ms)
{
    if (s_tx_busy == 0U) { return; }
    if ((now_ms - s_tx_ms) < RADAR_TX_TIMEOUT_MS) { return; }

    (void)DMA_ChCmd(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, DISABLE);
    USART_FuncCmd(RADAR_UART_UNIT, (USART_TX | USART_INT_TX_CPLT), DISABLE);
    DMA_ClearTransCompleteStatus(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_FLAG);
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_TX_CPLT);
    USART_FuncCmd(RADAR_UART_UNIT, USART_RX_TIMEOUT, ENABLE);

    s_tx_busy = 0U;
    g_radar_tx_timeout_cnt++;
}

/* 丢弃接收缓冲里"上一个波特率"的残留字节。
 * 必要性: 换波特率时 DMA 窗口/环形缓冲里可能还压着旧波特率下收到的字节, 若不丢弃,
 *         它们会在换档后被当成本档的数据解析, 可能用"上一档的合法帧"误判波特率。 */
void radar_port_rx_flush(void)
{
    uint32_t primask;

    /* 环形缓冲由 DMA/超时中断写入, 这里用极短临界区清空(仅在换波特率时调用) */
    primask = __get_PRIMASK();
    __disable_irq();
    (void)BUF_Init(&s_rx_ring, s_rx_ring_buf, sizeof(s_rx_ring_buf));
    memset(s_rx_win, 0, sizeof(s_rx_win));
    __set_PRIMASK(primask);

    /* 重新装载 RX DMA, 让窗口基准回到 s_rx_win[0] */
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_RX_TIMEOUT);
    TMR0_Stop(RADAR_TMR0_UNIT, RADAR_TMR0_CH);
    AOS_SW_Trigger();
    DMA_ClearTransCompleteStatus(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_TC_FLAG);
}

void radar_port_set_baud(uint32_t baud)
{
    float32_t f32Err = 0.0F;

    s_baud = baud;
    (void)USART_SetBaudrate(RADAR_UART_UNIT, baud, &f32Err);
    g_radar_brr = RADAR_UART_UNIT->BRR;
    radar_port_rx_flush();          /* 换波特率后, 旧波特率的残留字节全部作废 */
}

uint32_t radar_port_get_baud(void)
{
    return s_baud;
}

void radar_port_set_rx_handler(void (*handler)(const uint8_t *data, uint16_t len))
{
    s_rx_cb = handler;
}

void radar_port_poll(void)
{
    uint8_t  b;
    uint32_t now;
    uint32_t left;

    /* 1 秒窗口统计: 区分"线上没数据 / 常低(断裂) / 悬空噪声 / 正常数据", 以及 3 个关键速率:
     * DMA 实际搬了多少字节 / 空闲超时中断多少次 / 窗口满中断多少次。
     * 背景: 现场看到"运行时 rx_bytes 一直是 0, 暂停再跑就变 0x100",
     *       需要用 g_radar_dma_fill 与 g_radar_to_hz 判断是不是超时中断把 DMA 计数反复重置了。 */
    now = m_u32Tickms;
    g_radar_rx_bytes = s_rx_bytes;
    s_win_polls++;
    if (PIN_SET != GPIO_ReadInputPins(RADAR_UART_RX_PORT, RADAR_UART_RX_PIN)) { g_radar_pin_low++; s_win_lows++; }
    left = (uint32_t)DMA_GetTransCount(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH);
    g_radar_dma_left = left;
    if ((RADAR_RX_WIN - left) > s_win_fill) { s_win_fill = RADAR_RX_WIN - left; }

    if ((uint32_t)(now - s_win_ms) >= 1000U)
    {
        g_radar_poll_hz  = s_win_polls;
        g_radar_low_pct  = (s_win_polls != 0U) ? ((s_win_lows * 100U) / s_win_polls) : 0U;
        g_radar_bps      = s_rx_bytes - s_win_bytes;
        g_radar_dma_fill = s_win_fill;
        g_radar_to_hz    = s_to_cnt  - s_prev_to;
        g_radar_win_hz   = s_win_cnt - s_prev_win;
        s_prev_to   = s_to_cnt;
        s_prev_win  = s_win_cnt;
        s_win_bytes = s_rx_bytes;
        s_win_ms    = now;
        s_win_polls = 0U;
        s_win_lows  = 0U;
        s_win_fill  = 0U;
    }

    while (BUF_UsedSize(&s_rx_ring) > 0U)
    {
        if (BUF_Read(&s_rx_ring, &b, 1U) != 1U) { break; }
        if (s_rx_cb != 0) { s_rx_cb(&b, 1U); }
    }
}

uint32_t radar_port_rx_drop(void)
{
    return s_rx_drop;
}

uint32_t radar_port_rx_bytes(void)
{
    return s_rx_bytes;
}
