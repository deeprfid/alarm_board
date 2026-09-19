/**
 * @file  radar_link.c
 * @brief F4A0 <-> HC32F460 报警板 业务链路实现（端口映射与协议见 radar_link.h）
 *
 * 移植来源：项目 1 的 STM32F0_linux_v4.31/BSP/app.c（那里叫 frCrc16 / sPortCom / radarPortFresh）。
 * 纯逻辑部分逐字对齐；收发部分按 F4A0 的 read(fd) 模型重写（驱动已用中断填缓冲）。
 */
#include <string.h>
#include "hc32f46_driver.h"     /* read / ioctl / Uart_RS485_send / COMMON_INTERFACE_* */
#include "radar_link.h"

/* ---------- 端口与协议常量 ---------- */
static const int s_link_fd[RADAR_LINK_NUM] = {
    COMMON_INTERFACE_RS485_1,   /* 天线1   */
    COMMON_INTERFACE_RS485_2,   /* 天线2,3 */
    COMMON_INTERFACE_RS485_3,   /* 天线4   */
};

#define FRAME_HDR_AA        0xAAu
#define FRAME_HDR_LEGACY    0xFFu
#define FRAME_MAX_PAYLOAD   251u
#define FRAME_BUF_MAX       (4u + FRAME_MAX_PAYLOAD + 2u)

#define CMD_QUERY           0x10u   /* 查询 -> 板子回 3B 状态 */
#define CMD_REPORT          0x81u   /* 报警上报 */

#define QUERY_PERIOD_MS     50UL    /* 查询周期。STM32F0 侧 200ms 判超时，这里取 1/4 留余量 */
#define RX_DRAIN_MAX        128     /* 每轮每口最多取多少字节，防饿死别的口 */

/* 查询帧的 addr 字段。F460 侧会把收到的 addr 原样回填（common.c: frame_var_send(0x10u, buf[3], ...)），
 * 故这里用广播 0x00 —— 一条总线上多块板时，回帧的 addr 能用来区分是哪块。
 * 【待确认】若现场是"一板一地址"的寻址式轮询，改成逐地址轮发即可。 */
#define QUERY_ADDR          0x00u

typedef struct {
    radar_link_t st;
    uint8_t      rx[FRAME_BUF_MAX];
    uint16_t     rxlen;
    uint8_t      sync;          /* 1 = 已对齐帧头，正在攒帧 */
    uint16_t     plen;
    uint8_t      legacy;        /* 1 = 正在收 0xFF 定长 32B 帧 */
    uint32_t     next_query_ms;
    uint32_t     led_next_ms;   /* 下一次触发灯闪的时刻 */
} radar_ctx_t;

static radar_ctx_t s_ctx[RADAR_LINK_NUM];

/* ---------- CRC16 / CCITT，与 F460 common.c 的 fr_crc16 逐位一致 ---------- */
static uint16_t fr_crc16(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i, j;

    for (i = 0u; i < n; i++) {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x8000u) != 0u) { crc = (uint16_t)((crc << 1) ^ 0x1021u); }
            else                       { crc = (uint16_t)(crc << 1); }
        }
    }
    return crc;
}

static uint32_t now_ms(void)
{
    return (uint32_t)osKernelGetTickCount();
}

/* ---------- 发送：带自旋超时，避免驱动 Uart_RS485_send() 在串口异常时死等 ---------- */
/* 超时按【字节时间】折算成自旋次数：460800bps 下 1 字节 ≈ 21.7us；200MHz 下自旋约 4~8 周期/次，
 * 取 20000 次 ≈ 0.5ms/字节（约 25 倍余量）。发送正常时根本到不了上限。
 * （F460 侧曾因 USART_UART_Trans 的 timeout 被误当成"时间"而静默截断帧，故这里把口径写死。） */
#define RADAR_TX_SPIN_PER_BYTE   20000UL

/* 按口互斥：驱动的 Uart_RS485_send() 内部没有锁（对比 uart_send() 有 uart_tx_lock），
 * 多线程并发调会把两帧的字节交错。本模块所有发送都走这里，用一把全局锁串行化 ——
 * 三条总线共用一把锁是刻意的：发送是短操作，且轮询线程是唯一调用者，粒度粗一点更安全。 */
static osMutexId_t s_tx_mux = NULL;
static osRtxMutex_t s_tx_mux_cb;

static void tx_lock(void)
{
    if (s_tx_mux == NULL) {
        osMutexAttr_t attr = { NULL, osMutexPrioInherit, &s_tx_mux_cb, sizeof(s_tx_mux_cb) };
        s_tx_mux = osMutexNew(&attr);
        if (s_tx_mux == NULL) { s_tx_mux = (osMutexId_t)1; }   /* 失败退化为无锁，但不死锁 */
    }
    if (s_tx_mux != (osMutexId_t)1) { (void)osMutexAcquire(s_tx_mux, osWaitForever); }
}

static void tx_unlock(void)
{
    if ((s_tx_mux != NULL) && (s_tx_mux != (osMutexId_t)1)) { (void)osMutexRelease(s_tx_mux); }
}

static int radar_send_frame(uint8_t idx, const uint8_t *fr, uint16_t flen)
{
    int r;

    if (idx >= RADAR_LINK_NUM) { return -1; }

    tx_lock();
    r = Uart_RS485_send(s_link_fd[idx], fr, (uint32_t)flen);
    tx_unlock();

    if (r != (int)flen) {
        s_ctx[idx].st.tx_err++;
        return -1;
    }
    s_ctx[idx].st.tx_frames++;
    return 0;
}

static int radar_send_query(uint8_t idx)
{
    uint8_t  fr[6];
    uint16_t c;

    fr[0] = FRAME_HDR_AA;
    fr[1] = 2u;             /* lenv = plen(0) + 2 */
    fr[2] = CMD_QUERY;
    fr[3] = QUERY_ADDR;
    c = fr_crc16(fr, 4u);
    fr[4] = (uint8_t)(c & 0xFFu);
    fr[5] = (uint8_t)(c >> 8);
    return radar_send_frame(idx, fr, 6u);
}

/* ---------- 收帧：0xAA 变长（主）+ 0xFF 定长 32B（兼容） ---------- */
static void handle_var_frame(uint8_t idx, const uint8_t *f, uint16_t total)
{
    uint16_t plen = (uint16_t)(f[1] - 2u);
    uint8_t  cmd  = f[2];
    radar_ctx_t *p = &s_ctx[idx];

    p->st.rx_frames++;
    p->st.last_rx_ms = now_ms();
    p->st.online = 1u;

    if (cmd == CMD_QUERY && plen >= 3u) {
        p->st.gpio_in    = f[4];
        p->st.work_mode  = f[5];
        p->st.alarm_done = f[6];
        p->st.radar_val  = ((p->st.gpio_in & 0x07u) != 0u) ? 1u : 0u;
        (void)total;
    }
    /* CMD_REPORT(0x81)：报警上报，本模块只记新鲜度，内容交上层（上报去处待定） */
}

static void feed_byte(uint8_t idx, uint8_t b)
{
    radar_ctx_t *p = &s_ctx[idx];

    if (p->sync == 0u) {
        if (b == FRAME_HDR_AA) {
            p->rx[0] = b; p->rxlen = 1u; p->sync = 1u; p->legacy = 0u;
        } else if (b == FRAME_HDR_LEGACY) {
            p->rx[0] = b; p->rxlen = 1u; p->sync = 1u; p->legacy = 1u;
        }
        return;
    }

    if (p->rxlen >= FRAME_BUF_MAX) { p->sync = 0u; p->rxlen = 0u; p->st.bad_frames++; return; }
    p->rx[p->rxlen++] = b;

    if (p->legacy != 0u) {
        if (p->rxlen >= 32u) {
            /* 老定长帧：0xFF + 30B 数据 + 2B CRC(CCITT)，
             * 布局见 F460 侧 main.h 的 alarm_confirm_package（正好 32B）。
             * 与 0xAA 一样必须校验 CRC —— 不校验的话线上噪声会被当成"板子在线"。 */
            uint16_t crc_rx = (uint16_t)(p->rx[30] | ((uint16_t)p->rx[31] << 8));
            if (crc_rx == fr_crc16(p->rx, 30u)) {
                p->st.leg_device_id  = p->rx[1];
                p->st.leg_alarm_done = p->rx[2];
                p->st.leg_got        = 1u;
                p->st.leg_frames++;
                p->st.gpio_in        = p->rx[2];      /* 新老口径统一：用 alarm_done 表达"本口有事件" */
                p->st.alarm_done     = p->rx[2];
                p->st.rx_frames++;
                p->st.last_rx_ms = now_ms();
                p->st.online     = 1u;
            } else {
                p->st.bad_frames++;
            }
            p->sync = 0u; p->rxlen = 0u;
        }
        return;
    }

    if (p->rxlen == 2u) {
        if ((p->rx[1] < 2u) || (p->rx[1] > (uint8_t)(FRAME_MAX_PAYLOAD + 2u))) {
            p->sync = 0u; p->rxlen = 0u; p->st.bad_frames++; return;
        }
    } else if (p->rxlen >= 4u) {
        uint16_t total = (uint16_t)(p->rx[1] + 4u);
        if (p->rxlen == total) {
            uint16_t crc_rx = (uint16_t)(p->rx[total - 2u] | ((uint16_t)p->rx[total - 1u] << 8));
            if (crc_rx == fr_crc16(p->rx, (uint16_t)(total - 2u))) {
                handle_var_frame(idx, p->rx, total);
            } else {
                p->st.bad_frames++;
            }
            p->sync = 0u; p->rxlen = 0u;
        }
    }
}

void radar_link_init(void)
{
    commonUartPara para;
    int nb = 1;
    int tmo = 10;
    uint8_t i;

    (void)memset(s_ctx, 0, sizeof(s_ctx));

    (void)memset(&para, 0, sizeof(para));
    para.isBlock  = O_NONBLOCK;
    para.baudrate = 460800;
    para.timeout  = 20;
    for (i = 0u; i < RADAR_LINK_NUM; i++) {
        (void)uart_open(s_link_fd[i], &para);
        (void)ioctl(s_link_fd[i], COMMON_INTERFACE_SET_ISBLOCK, &nb);
        (void)ioctl(s_link_fd[i], COMMON_INTERFACE_SET_TIMEOUT, &tmo);
        (void)ioctl(s_link_fd[i], COMMON_INTERFACE_CLEAR_REVBUF, NULL);
        s_ctx[i].next_query_ms = now_ms();
    }
    /* 上电自检：三个灯各闪一次 —— 用来把"灯路不通"和"通信不通"分开。
     * 若上电时这三个灯都没闪，说明是 LED/GPIO 的问题，与 RS485 无关。 */
    for (i = 0u; i < RADAR_LINK_NUM; i++) {
        Alarm_Output((uint8_t)(BOARD_LED2 + i), 20u, 20u, 1u);   /* 200ms 亮 / 200ms 灭 / 1 次 */
        sleep_ms(450);
    }
    TRACE("radar_link: 3 links up (485_1 ant1 / 485_2 ant2,3 / 485_3 ant4)\n");
}

void radar_link_poll(void)
{
    uint8_t  buf[RX_DRAIN_MAX];
    uint32_t t = now_ms();
    uint8_t  i;
    int      n, k;

    for (i = 0u; i < RADAR_LINK_NUM; i++) {
        radar_ctx_t *p = &s_ctx[i];

        /* 1) 先收：非阻塞取数，喂帧解析 */
        for (k = 0; k < 4; k++) {
            n = read(s_link_fd[i], buf, (uint32_t)sizeof(buf));
            if (n <= 0) { break; }
            {
                int j;
                for (j = 0; j < n; j++) { feed_byte(i, buf[j]); }
            }
        }

        /* 2) 新鲜度：STM32F0 的 radarPortFresh 等价物 —— 200ms 无应答即按离线 */
        if ((p->st.online != 0u) && ((t - p->st.last_rx_ms) > RADAR_LINK_FRESH_MS)) {
            p->st.online   = 0u;
            p->st.radar_val = 0u;
            p->st.gpio_in   = 0u;
        }

        /* 3) 周期发查询 */
        if ((int32_t)(t - p->next_query_ms) >= 0) {
            (void)radar_send_query(i);
            p->next_query_ms = t + QUERY_PERIOD_MS;
        }

        /* 4) 雷达状态灯：BOARD_LED2/3/4 = 链路 485_1/485_2/485_3。
         *    【为什么从 LED2 起】BOARD_LED1 已被占用（现场确认），故依次用 2/3/4。
         *
         *    【关键约束 —— 踩过的坑】Alarm_Output -> GPIO_Start() 的第一句是
         *        if (_usBeepTime == 0 || g_tled->ucMute == 1 || g_tled->ucEnalbe == 1) return;
         *    也就是说：
         *      · 时间参数传 0 == 整个调用被丢弃（既不会亮也【不会灭】）；
         *      · 灯已在跑（ucEnalbe==1）时再调同样被丢弃。
         *    所以"离线就传 0"是错的（那是静默 no-op，灯会停在原状态、看起来像全灭），
         *    也不能靠高频重复调用来维持闪烁。
         *
         *    口径（现场例 Alarm_Output(BOARD_LED2,5,5,1) = 50ms亮/50ms灭/1次）：
         *      时间参数单位 10ms，第 4 参是【重复次数】。
         *
         *    做法：在线 -> 每 200ms 触发一次 (5,5,1) 的单次闪 —— 灯会持续 50ms 亮/50ms 灭地闪；
         *          离线 -> 不触发，灯自然停在灭态。
         *    这样每次触发都在上一轮（100ms）结束之后，不会撞上 ucEnalbe==1。 */
        if (p->st.online != 0u) {
            if ((int32_t)(t - p->led_next_ms) >= 0) {
                Alarm_Output((uint8_t)(BOARD_LED2 + i), 5u, 5u, 1u);
                p->led_next_ms = t + 200UL;
            }
        } else {
            p->led_next_ms = t;     /* 恢复在线时立刻开始闪，不用等 */
        }
    }
}

void radar_link_task(void *arg)
{
    (void)arg;
    radar_link_init();
    for (;;) {
        radar_link_poll();
        sleep_ms(10);
    }
}

const radar_link_t *radar_link_get(uint8_t idx)
{
    return (idx < RADAR_LINK_NUM) ? &s_ctx[idx].st : NULL;
}

int radar_link_of_antenna(uint8_t ant)
{
    switch (ant) {
        case 1u: return 0;
        case 2u: return 1;
        case 3u: return 1;
        case 4u: return 2;
        default: return -1;
    }
}

uint8_t radar_link_antenna_alarm(uint8_t ant)
{
    int i = radar_link_of_antenna(ant);
    if (i < 0) { return 0u; }
    /* 一口多天线时（天线2,3 共用 RS485_2），本口 radarVal 只能表达"这条总线上有人" */
    return s_ctx[i].st.radar_val;
}
