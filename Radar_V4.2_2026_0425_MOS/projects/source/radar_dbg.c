/*******************************************************************************
 * radar_dbg.c -- 雷达驱动"上板验证"用的调试输出(临时模块)
 *   只调用 radar.h 的公开接口读取状态, 不碰驱动内部, 删除后业务不受影响。
 *   不引入 printf(自带极简整数转 ASCII), 避免 microlib/重定向依赖。
 ******************************************************************************/
#include "main.h"
#include "radar_dbg.h"

#if (RADAR_DBG_EN != 0U)

#include "bsp_rs485.h"                  /* SINK_RS485: RS485 主机口发送 */

extern uint32_t m_u32Tickms;            /* 1ms 计数(定义在 bsp_exint.c) */

volatile radar_dbg_snap_t g_radar_dbg;
char                      g_radar_dbg_line[RADAR_DBG_LINE_MAX];
char                      g_radar_dbg_evt[RADAR_DBG_EVT_MAX];
uint32_t                  g_radar_dbg_cnt;
uint32_t                  g_radar_dbg_evt_cnt;

static uint32_t s_line_ms;              /* 上次刷状态行的时刻 */
static uint32_t s_rx_last;              /* 上次的收字节数 */
static uint32_t s_rx_ms;                /* 收字节数最近一次变化的时刻 */
static uint32_t s_fer_last;             /* 上次的分帧错误数 */
static uint8_t  s_rdy_last;
static uint8_t  s_st_last;
static uint8_t  s_booted;
static uint8_t  s_stalled;

/* ------------------------------ 极简文本拼装 ------------------------------ */
static char *dbg_u32(char *p, uint32_t v)
{
    char    tmp[12];
    uint8_t n = 0U;

    if (v == 0U)
    {
        *p++ = '0';
        return p;
    }
    while ((v != 0U) && (n < (uint8_t)sizeof(tmp)))
    {
        tmp[n] = (char)('0' + (char)(v % 10U));
        n++;
        v /= 10U;
    }
    while (n != 0U)
    {
        n--;
        *p++ = tmp[n];
    }

    return p;
}

static char *dbg_str(char *p, const char *s)
{
    while (*s != '\0')
    {
        *p++ = *s;
        s++;
    }

    return p;
}

/* 限长拷贝, 防止误传长字符串溢出缓冲 */
static char *dbg_str_lim(char *p, const char *s, uint8_t max)
{
    uint8_t n = 0U;

    while ((*s != '\0') && (n < max))
    {
        *p++ = *s;
        s++;
        n++;
    }

    return p;
}

static char *dbg_kv(char *p, const char *k, uint32_t v)
{
    p = dbg_str(p, k);
    return dbg_u32(p, v);
}

static char *dbg_kv2(char *p, const char *k, uint32_t a, uint32_t b)
{
    p = dbg_str(p, k);
    p = dbg_u32(p, a);
    *p++ = ':';
    return dbg_u32(p, b);
}

/* ------------------------------ 输出通道 ------------------------------ */
#if (RADAR_DBG_SINK_ITM != 0U)
static void dbg_send_itm(const char *s)
{
    while (*s != '\0')
    {
        (void)ITM_SendChar((uint32_t)(uint8_t)(*s));
        s++;
    }
}
#endif

#if (RADAR_DBG_SINK_RS485 != 0U)
static void dbg_send_rs485(const char *s)
{
    /* 注意: 与 STM32 挂同一总线时属"抢总线"(STM32 每 20ms 查询一次),
     *       仅建议单独给本板上电、用 USB-RS485/串口助手 @460800 直连时使用 */
    Uart4_DMA_Trans((const void *)s, (uint32_t)strlen(s));
}
#endif

static void dbg_sink(const char *s)
{
#if (RADAR_DBG_SINK_ITM != 0U)
    dbg_send_itm(s);
#endif
#if (RADAR_DBG_SINK_RS485 != 0U)
    dbg_send_rs485(s);
#endif
#if ((RADAR_DBG_SINK_ITM == 0U) && (RADAR_DBG_SINK_RS485 == 0U))
    (void)s;                            /* 只用 Keil Watch 看 g_radar_dbg / g_radar_dbg_line */
#endif
}

/* ------------------------------ 状态快照与状态行 ------------------------------ */
static void dbg_update_snap(void)
{
    const radar_dev_t    *d = radar_dev(0);
    const radar_report_t *r = radar_report(0);

    g_radar_dbg.ms     = m_u32Tickms;
    g_radar_dbg.rdy    = (uint32_t)radar_ready();
    g_radar_dbg.lock   = (uint32_t)radar_baud_locked();
    g_radar_dbg.baud   = radar_get_baud();
    g_radar_dbg.rep    = radar_reports();
    g_radar_dbg.fok    = radar_frames_ok();
    g_radar_dbg.fer    = radar_frames_err();
    g_radar_dbg.rx     = radar_rx_bytes();
    g_radar_dbg.drp    = radar_rx_drop();
    g_radar_dbg.out    = (d != 0) ? (uint32_t)d->out_present : 0U;
    g_radar_dbg.online = (d != 0) ? (uint32_t)d->uart_online : 0U;
    g_radar_dbg.pre    = (uint32_t)radar_presence(0);
    g_radar_dbg.st     = (r != 0) ? (uint32_t)r->target_state : 0U;
    g_radar_dbg.mv_dist = (r != 0) ? (uint32_t)r->moving_distance_cm : 0U;
    g_radar_dbg.mv_eng  = (r != 0) ? (uint32_t)r->moving_energy : 0U;
    g_radar_dbg.st_dist = (r != 0) ? (uint32_t)r->still_distance_cm : 0U;
    g_radar_dbg.st_eng  = (r != 0) ? (uint32_t)r->still_energy : 0U;
    g_radar_dbg.dd      = (r != 0) ? (uint32_t)r->detect_distance_cm : 0U;
}

static void dbg_build_line(void)
{
    char *p = g_radar_dbg_line;

    p = dbg_kv (p, "ms=",    g_radar_dbg.ms);
    p = dbg_kv (p, " rdy=",  g_radar_dbg.rdy);
    p = dbg_kv (p, " lock=", g_radar_dbg.lock);
    p = dbg_kv (p, " baud=", g_radar_dbg.baud);
    p = dbg_kv (p, " rep=",  g_radar_dbg.rep);
    p = dbg_kv (p, " fok=",  g_radar_dbg.fok);
    p = dbg_kv (p, " fer=",  g_radar_dbg.fer);
    p = dbg_kv (p, " rx=",   g_radar_dbg.rx);
    p = dbg_kv (p, " drp=",  g_radar_dbg.drp);
    p = dbg_kv (p, " st=",   g_radar_dbg.st);
    p = dbg_kv2(p, " mv=",   g_radar_dbg.mv_dist, g_radar_dbg.mv_eng);
    p = dbg_kv2(p, " stl=",  g_radar_dbg.st_dist, g_radar_dbg.st_eng);
    p = dbg_kv (p, " dd=",   g_radar_dbg.dd);
    p = dbg_kv (p, " out=",  g_radar_dbg.out);
    p = dbg_kv (p, " on=",   g_radar_dbg.online);
    p = dbg_kv (p, " pre=",  g_radar_dbg.pre);
    *p++ = '\r';
    *p++ = '\n';
    *p   = '\0';
}

/* ------------------------------ 事件 ------------------------------ */
static char *dbg_evt_begin(void)
{
    char *p = g_radar_dbg_evt;

    p = dbg_str(p, "t=");
    p = dbg_u32(p, m_u32Tickms);
    *p++ = ' ';

    return p;
}

static void dbg_evt_end(char *p)
{
    *p++ = '\r';
    *p++ = '\n';
    *p   = '\0';
    g_radar_dbg_evt_cnt++;
    dbg_sink(g_radar_dbg_evt);
}

static void dbg_event(const char *tag)
{
    char *p = dbg_evt_begin();

    p = dbg_str_lim(p, tag, (uint8_t)(RADAR_DBG_EVT_MAX - 20U));
    dbg_evt_end(p);
}

static void dbg_event_u32(const char *tag, uint32_t v)
{
    char *p = dbg_evt_begin();

    p = dbg_str_lim(p, tag, (uint8_t)(RADAR_DBG_EVT_MAX - 30U));
    p = dbg_u32(p, v);
    dbg_evt_end(p);
}

static void dbg_event_st(uint32_t from, uint32_t to)
{
    char *p = dbg_evt_begin();

    p = dbg_str(p, "st ");
    p = dbg_u32(p, from);
    p = dbg_str(p, " -> ");
    p = dbg_u32(p, to);
    dbg_evt_end(p);
}

/* ------------------------------ 事件检测 ------------------------------ */
static void dbg_check_events(void)
{
    const radar_dev_t *d = radar_dev(0);
    uint32_t           rx;
    uint32_t           fer;

    if (d == 0)
    {
        return;
    }

    rx  = radar_rx_bytes();
    fer = radar_frames_err();

    if (s_booted == 0U)
    {
        s_booted = 1U;
        s_rx_ms  = m_u32Tickms;
        dbg_event("boot");
    }

    if ((s_rdy_last == 0U) && (radar_ready() != 0U))
    {
        if (radar_baud_locked() != 0U)
        {
            dbg_event_u32("probe ok baud=", radar_get_baud());
        }
        else
        {
            dbg_event("probe FAIL -> fallback 256000");
        }
    }
    s_rdy_last = radar_ready();

    if (d->rep.target_state != s_st_last)
    {
        dbg_event_st((uint32_t)s_st_last, (uint32_t)d->rep.target_state);
        s_st_last = d->rep.target_state;
    }

    if (fer != s_fer_last)
    {
        s_fer_last = fer;
        dbg_event_u32("frame err cnt=", fer);
    }

    if (rx != s_rx_last)
    {
        s_rx_last = rx;
        s_rx_ms   = m_u32Tickms;
        s_stalled = 0U;
    }
    else if ((rx != 0U) && (s_stalled == 0U) &&
             ((m_u32Tickms - s_rx_ms) >= RADAR_DBG_RX_STALL_MS))
    {
        s_stalled = 1U;
        dbg_event("rx stalled(no byte)");
    }
}

/* ------------------------------ 对外接口 ------------------------------ */
void radar_dbg_note(const char *tag)
{
    if (tag != 0)
    {
        dbg_event(tag);
    }
}

void radar_dbg_poll(void)
{
    dbg_check_events();

    if ((m_u32Tickms - s_line_ms) >= RADAR_DBG_PERIOD_MS)
    {
        s_line_ms = m_u32Tickms;
        dbg_update_snap();
        dbg_build_line();
        g_radar_dbg_cnt++;
        dbg_sink(g_radar_dbg_line);
    }
}

#else   /* RADAR_DBG_EN == 0: 关闭调试, 保留空实现, 调用方不必加 #if */

void radar_dbg_note(const char *tag)
{
    (void)tag;
}

void radar_dbg_poll(void)
{
}

#endif
