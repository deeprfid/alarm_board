/*******************************************************************************
 * radar_port.c -- 雷达串口硬件层实现
 *
 * 硬件通路: USART1 接收中断(RI) -> 软件环形缓冲 -> radar_port_poll() 交给上层
 *           DMA2 CH0 -> USART1_TI 发送(命令帧)
 *
 * 2026-09-14 两处大改(针对『460800 能通、9600/115200 不通』):
 *  1) 接收不再用 DMA: 原实现是『256B DMA 窗口 + AOS/LLP 重装 + 靠空闲超时中断上抛』,
 *     实测空闲超时上抛从来没工作过; 高波特率下窗口 20ms 就填满所以看不出问题,
 *     低波特率下窗口要一秒以上才填满, 于是帧永远到不了解析层。
 *     改为逐字节中断接收(SDK usart_uart_int 例程同款做法): 对波特率零依赖,
 *     换档也不需要动 DMA —— 自适应探测因此才可靠。
 *  2) 时钟分频改为**按波特率自动选**(见 radar_pick_clk_div): 原来写死 DIV4,
 *     8 倍过采样下 9600 需要分频比 324 > 255, USART_SetBaudrate() 会返回错误**且不写 BRR**,
 *     端口静默停在旧波特率 —— 写死一个分频值本身就是错的。
 ******************************************************************************/
#include "radar_port.h"
#include "ring_buf.h"          /* BUF_* 环形缓冲 */
#include <string.h>           /* memset */

/* 1ms 计数(定义在 bsp_exint.c) */
extern uint32_t m_u32Tickms;

/* ------------------------------ 静态数据 ------------------------------ */
static uint8_t             s_rx_ring_buf[RADAR_RX_RING_SIZE];
static stc_ring_buf_t      s_rx_ring;
static uint8_t             s_tx_dummy[RADAR_TX_MAX];   /* TX DMA 初值占位, 每次发送由 DMA_SetSrcAddr 覆盖 */

static volatile uint8_t    s_tx_busy;
static volatile uint32_t   s_tx_ms;          /* 本次发送开始时刻 */

static volatile uint32_t   s_rx_bytes;
static volatile uint32_t   s_rx_drop;
static uint32_t            s_baud;
static volatile uint8_t    s_baud_ok;        /* 0 = 该波特率本档分频表示不出来(换档失败) */
static void (*s_rx_cb)(const uint8_t *data, uint16_t len) = 0;

/* --------------------- 时钟分频: 随波特率自动选(不是定死的) ---------------------
 * C = PCLK1(本工程 100MHz) / 分频;  DDL 的 BRR 整数分频只有 8 位:
 *     DIV_Integer = C/(B*8*(2-OVER8)) - 1 必须 <= 255   =>   C <= B*8*(2-OVER8)*256
 * 取『满足约束的最小小分频』(C 越大, 小数分辨率越高、误差越小):
 *   B >= 12207  -> DIV4  (C = 25MHz)    19200 及以上都走这一档
 *   B >= 3052   -> DIV16 (C = 6.25MHz)  **9600 必须用这一档**
 *   B >= 763    -> DIV64 (C = 1.5625MHz)
 * 即: 波特率高用小分频, 波特率低用大分频。
 * 备注: 57600 以上理论上还能用 DIV1(C = 100MHz)拿更高分辨率, 但本工程所有已验证配置
 *       都是 DIV4, 未经验证不引入; 将来需要时可把 DIV1 加到这个候选链的最前面。 */
static uint32_t radar_pick_clk_div(uint32_t baud)
{
    uint32_t c_max = (baud * 8UL) * 256UL;              /* 8 倍过采样下允许的最大 C */

    if (RADAR_UART_PCLK_HZ <= c_max)         { return USART_CLK_DIV4; }
    if ((RADAR_UART_PCLK_HZ / 4UL) <= c_max) { return USART_CLK_DIV16; }
    return USART_CLK_DIV64;
}

/* ------------------------------ 中断回调 ------------------------------ */
/* 接收中断: 一字节进一字节出。 */
static void radar_rx_ri_cb(void)
{
    uint8_t b = (uint8_t)USART_ReadData(RADAR_UART_UNIT);   /* 读 RDR 同时清 RI 标志 */

    s_rx_bytes++;
    if (BUF_Write(&s_rx_ring, &b, 1U) != 1U) { s_rx_drop++; }
}

/* 接收错误中断(SDK 例程同款): 必须读 RDR + 清 PE/FE/ORE, 否则标志一直挂着会堵住后续接收 */
static void radar_rx_err_cb(void)
{
    (void)USART_ReadData(RADAR_UART_UNIT);
    USART_ClearStatus(RADAR_UART_UNIT,
                      (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}

/* 发送完成: DMA TC -> 使能 USART TCI -> 清 s_tx_busy */
static void radar_tx_complete_cb(void)
{
    USART_FuncCmd(RADAR_UART_UNIT, (USART_TX | USART_INT_TX_CPLT), DISABLE);
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_TX_CPLT);

    s_tx_busy = 0U;                                     /* 发送真正完成, 允许下一帧 */
}

static void radar_tx_dma_tc_cb(void)
{
    USART_FuncCmd(RADAR_UART_UNIT, USART_INT_TX_CPLT, ENABLE);
    DMA_ClearTransCompleteStatus(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_FLAG);
}

/* ------------------------------ TX DMA ------------------------------ */
static int32_t radar_tx_dma_config(void)
{
    stc_dma_init_t stcDmaInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    int32_t i32Ret;

    RADAR_TX_DMA_FCG_ENABLE();
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn       = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize   = 1UL;
    stcDmaInit.u32TransCount  = 1UL;
    stcDmaInit.u32DataWidth   = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr    = (uint32_t)(&RADAR_UART_UNIT->TDR);
    stcDmaInit.u32SrcAddr     = (uint32_t)s_tx_dummy;
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

/* ------------------------------ 对外接口 ------------------------------ */
void radar_port_init(void)
{
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    s_tx_busy = 0U;
    s_rx_bytes = 0U;
    s_rx_drop = 0U;
#if (RADAR_BAUD_INIT_FIXED != 0UL)
    s_baud = RADAR_BAUD_INIT_FIXED;      /* 指定上电波特率(单档定位用) */
#else
    s_baud = RADAR_BAUD_FALLBACK;        /* 先按这一档收, 随后由探测逐档试出模块真实波特率 */
#endif

    (void)BUF_Init(&s_rx_ring, s_rx_ring_buf, sizeof(s_rx_ring_buf));
    memset(s_rx_ring_buf, 0, sizeof(s_rx_ring_buf));

    GPIO_SetFunc(RADAR_UART_RX_PORT, RADAR_UART_RX_PIN, RADAR_UART_RX_FUNC);
    GPIO_SetFunc(RADAR_UART_TX_PORT, RADAR_UART_TX_PIN, RADAR_UART_TX_FUNC);

    RADAR_UART_FCG_ENABLE();

    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv      = radar_pick_clk_div(s_baud);   /* 分频随波特率走 */
    stcUartInit.u32CKOutput      = USART_CK_OUTPUT_ENABLE;
    stcUartInit.u32Baudrate      = s_baud;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;       /* 与 SDK 例程/已验证配置一致 */
    s_baud_ok = (LL_OK == USART_UART_Init(RADAR_UART_UNIT, &stcUartInit, NULL)) ? 1U : 0U;

    (void)radar_tx_dma_config();

    /* 接收中断(逐字节) —— SDK 例程 usart_uart_int 同款: 先 signin, 再统一 FuncCmd 使能 */
    stcIrqSigninConfig.enIRQn      = RADAR_UART_RX_IRQn;
    stcIrqSigninConfig.enIntSrc    = RADAR_UART_RX_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &radar_rx_ri_cb;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* 接收错误中断 */
    stcIrqSigninConfig.enIRQn      = RADAR_UART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc    = RADAR_UART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &radar_rx_err_cb;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* 发送完成 */
    stcIrqSigninConfig.enIRQn      = RADAR_UART_TX_CPLT_IRQn;
    stcIrqSigninConfig.enIntSrc    = RADAR_UART_TX_CPLT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &radar_tx_complete_cb;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    USART_FuncCmd(RADAR_UART_UNIT, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
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
 * 这里按时间兜底: 超过 RADAR_TX_TIMEOUT_MS 仍未完成 -> 复位 TX 通路并放行。 */
void radar_port_tx_watchdog(uint32_t now_ms)
{
    if (s_tx_busy == 0U) { return; }
    if ((now_ms - s_tx_ms) < RADAR_TX_TIMEOUT_MS) { return; }

    (void)DMA_ChCmd(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, DISABLE);
    USART_FuncCmd(RADAR_UART_UNIT, (USART_TX | USART_INT_TX_CPLT), DISABLE);
    DMA_ClearTransCompleteStatus(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_FLAG);
    USART_ClearStatus(RADAR_UART_UNIT, USART_FLAG_TX_CPLT);

    s_tx_busy = 0U;
}

/* 丢弃接收缓冲里『上一个波特率』的残留字节。换档时调用, 避免用旧档的字节误判。
 * 逐字节中断方案下只剩清缓冲, 不再需要动 DMA/AOS。 */
void radar_port_rx_flush(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    (void)BUF_Init(&s_rx_ring, s_rx_ring_buf, sizeof(s_rx_ring_buf));
    __set_PRIMASK(primask);
}

/* 换波特率: 分频按新波特率重选, 再算 BRR; 失败置 s_baud_ok=0 供上层跳过该档。
 * 不再需要停/重挂 DMA —— 这是逐字节中断方案带来的最大好处。 */
void radar_port_set_baud(uint32_t baud)
{
    float32_t f32Err = 0.0F;

    USART_SetClockDiv(RADAR_UART_UNIT, radar_pick_clk_div(baud));
    s_baud = baud;
    s_baud_ok = (LL_OK == USART_SetBaudrate(RADAR_UART_UNIT, baud, &f32Err)) ? 1U : 0U;
    radar_port_rx_flush();
}

uint32_t radar_port_get_baud(void)
{
    return s_baud;
}

uint8_t radar_port_baud_ok(void)
{
    return s_baud_ok;
}

void radar_port_set_rx_handler(void (*handler)(const uint8_t *data, uint16_t len))
{
    s_rx_cb = handler;
}

void radar_port_poll(void)
{
    uint8_t b;

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
