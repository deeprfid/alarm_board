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
 * 取值规则直接照抄**扫描台主板(同款 HC32F460)量产在用的 UART 初始化**:
 *     (baud < 115200) ? UsartClkDiv_64 : UsartClkDiv_1
 * 换算到本工程(C = PCLK1 = RADAR_UART_PCLK_HZ = 100MHz / 分频, 8 倍过采样):
 *     B <  115200 -> DIV64, C = 1.5625MHz   9600 时整数分频 20, 误差 +0.13%
 *     B >= 115200 -> DIV1 , C = 100MHz      460800 时整数分频 27, 误差 +0.08%
 * 约束依据(DDL 的 BRR 整数分频只有 8 位):
 *     DIV_Integer = C/(B*8*(2-OVER8)) - 1 必须 <= 255   =>  C <= B*8*(2-OVER8)*256
 *     且 C/(B*8*(2-OVER8)) >= 1                         =>  C >= B*8*(2-OVER8)
 * 写死一个分频值一定会踩线: 例如写死 DIV4 + 8 倍过采样时, 9600 需要分频比 324 > 255,
 * USART_SetBaudrate() 会返回错误, **且一个字节都不写 BRR**, 端口静默停在旧波特率 ——
 * 这就是现场『460800 能通、改到 9600 不行』的直接原因之一。
 * 所以: 波特率高用小分频、波特率低用大分频, 按波特率现算。 */
static uint32_t radar_pick_clk_div(uint32_t baud)
{
    return (baud < 115200UL) ? USART_CLK_DIV64 : USART_CLK_DIV1;
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

    /* 先复位到确定状态: 使初始化幂等, 不受上电前残留配置/引导程序影响 */
    USART_DeInit(RADAR_UART_UNIT);

    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv      = radar_pick_clk_div(s_baud);   /* 分频随波特率走 */
    stcUartInit.u32CKOutput      = USART_CK_OUTPUT_DISABLE;   /* 同扫描台参考实现: 时钟不输出 */
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
/* 换波特率: **走初始化路径**(USART_UART_Init), 不再用运行时 USART_SetBaudrate()。
 * 原因(2026-09-14 现场用 BRR 指纹查出): 运行时 SetClockDiv+SetBaudrate 即使返回 LL_OK,
 * BRR 也可能没真正改变 —— 现象是 g_radar_baud 显示 460800, 而 BRR 整数分频仍是 256000 的值(47),
 * 于是自适应在 460800 档收到的是模块 256000 的帧, 被误判成已锁定。
 * 初始化路径已被现场证明能正确写入(INIT_FIXED=460800 那版就是靠它通的), 所以换档也用它。
 * 逐字节中断方案下换档不需要碰 DMA, 重新 Init 之后把 RX/TX 再使能一次即可。 */
/* 换波特率: **只动 PR + BRR**(最小改动), 并且**写完回读确认**, 不信任返回值。
 * 1) 先按新波特率选分频并写 PR, 再算 BRR(DIV_Integer = C/(B*8*(2-OVER8)) - 1) 写进去;
 * 2) 回读 BRR 的整数分频, 与我们的期望值比对:
 *      - 一致 -> 生效(正常路径, 不动其它寄存器, RX/TX 保持使能);
 *      - 不一致 -> 说明这条最小写路径在当前状态下没写进去, 退回**完整初始化**兜底
 *        (USART_UART_Init 重写 CR1/CR2/CR3/PR/BRR, 之后必须重新使能 RX/TX)。
 * 之所以要回读: 现场曾出现 USART_SetBaudrate() 返回 LL_OK 但 BRR 没变的静默失败,
 *           软件记录值(460800)与硬件真值(256000)不一致, 直接把自适应带偏。 */
/* 换波特率: **整套重来一遍, 不留任何痕迹** ——
 *   关收发 -> USART_DeInit(把 CR1/CR2/CR3/PR/BRR 全部清回默认) -> 重新 StructInit + Init
 *   -> 清状态标志与 NVIC 挂起 -> 重新使能收发 -> 清环形缓冲。
 * 不再用只改 PR+BRR 的最小写法: 换档本来就极少发生(自适应探测每档一次),
 * 一次干净的重初始化比省几个寄存器写更可靠、更好推理。
 * 仍保留一道**回读校验**: Init 之后按 PR 反推 C, 核对 BRR 整数分频是否等于
 *   DIV_Integer = C/(B*8*(2-OVER8)) - 1
 * 因为现场出现过返回 LL_OK 但没写进 BRR 的静默失败, 结果记入 s_baud_ok / g_radar_comm 的 0x400 位。 */
void radar_port_set_baud(uint32_t baud)
{
    stc_usart_uart_init_t stcUartInit;
    float32_t f32Err = 0.0F;
    uint32_t  psc;
    uint32_t  c;
    uint32_t  exp_int;
    uint32_t  got_int;

    /* 1) 先停收发, 再整片复位 */
    /* TX DMA 的残留传输也要一起停: USART 侧已被 DeInit 清掉, DMA 若还挂着会留下『半截发送』 */
    (void)DMA_ChCmd(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, DISABLE);
    DMA_ClearTransCompleteStatus(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_FLAG);
    USART_FuncCmd(RADAR_UART_UNIT, (USART_RX | USART_TX | USART_INT_RX), DISABLE);
    USART_DeInit(RADAR_UART_UNIT);

    /* 2) 完整(重新)初始化 */
    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv      = radar_pick_clk_div(baud);   /* 分频随波特率走 */
    stcUartInit.u32CKOutput      = USART_CK_OUTPUT_DISABLE;
    stcUartInit.u32Baudrate      = baud;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;

    s_baud    = baud;
    s_baud_ok = (LL_OK == USART_UART_Init(RADAR_UART_UNIT, &stcUartInit, &f32Err)) ? 1U : 0U;

    /* 3) 回读校验: 防止返回 OK 却没写进 BRR(现场踩过) */
    psc     = READ_REG32_BIT(RADAR_UART_UNIT->PR, USART_PR_PSC);
    c       = RADAR_UART_PCLK_HZ >> (psc * 2UL);
    exp_int = (c / (baud * 8UL)) - 1UL;
    got_int = (RADAR_UART_UNIT->BRR >> 8) & 0xFFUL;
    if (got_int != exp_int) { s_baud_ok = 0U; }

    /* 4) 不留痕迹: 清状态标志 / NVIC 挂起 / 发送忙标志, 再开收发并清缓冲 */
    USART_ClearStatus(RADAR_UART_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR |
                                       USART_FLAG_OVERRUN | USART_FLAG_TX_CPLT));
    NVIC_ClearPendingIRQ(RADAR_UART_RX_IRQn);
    NVIC_ClearPendingIRQ(RADAR_UART_TX_CPLT_IRQn);
    s_tx_busy = 0U;

    USART_FuncCmd(RADAR_UART_UNIT, (USART_RX | USART_TX | USART_INT_RX), ENABLE);
    radar_port_rx_flush();
}

uint32_t radar_port_baud_actual(void)
{
    static const uint32_t tab[] = RADAR_BAUD_TABLE;
    uint32_t psc = READ_REG32_BIT(RADAR_UART_UNIT->PR, USART_PR_PSC);
    uint32_t c   = RADAR_UART_PCLK_HZ >> (psc * 2UL);
    uint32_t k   = ((RADAR_UART_UNIT->BRR >> 8) & 0xFFUL) + 1UL;
    uint32_t approx;
    uint32_t best = 0UL;
    uint32_t diff;
    uint32_t bestdiff = 0xFFFFFFFFUL;
    uint8_t  i;

    if (k == 0UL) { return 0UL; }
    approx = c / (8UL * k);

    for (i = 0U; i < (uint8_t)RADAR_BAUD_TABLE_CNT; i++)
    {
        diff = (approx > tab[i]) ? (approx - tab[i]) : (tab[i] - approx);
        if (diff < bestdiff) { bestdiff = diff; best = tab[i]; }
    }

    return best;
}
uint32_t radar_port_get_baud(void)
{
    return s_baud;
}

uint8_t radar_port_baud_ok(void)
{
    return s_baud_ok;
}

/* 当前 BRR 寄存器值(回读): 高字节 = 整数分频, 用来反查硬件真正生效的波特率 */
uint32_t radar_port_brr(void)
{
    return RADAR_UART_UNIT->BRR;
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
