/*******************************************************************************
 * radar.c -- LD2410C 雷达服务层实现
 ******************************************************************************/
#include "radar.h"
#include "radar_dbg.h"       /* 调试接口声明(上板验证用, 实现见文件末尾) */
#include "radar_port.h"       /* 硬件层: init/write/tx_busy/set_baud/poll */
#include <string.h>           /* memset */

/* 1ms 计数(定义在 bsp_exint.c) */
extern uint32_t m_u32Tickms;

static radar_frame_rx_t s_rx;
static radar_dev_t      s_dev[RADAR_DEV_CNT];
static radar_ack_t      s_ack;
static volatile uint8_t s_ack_ready;
static uint32_t         s_reports;
static uint8_t          s_baud_locked;
static uint8_t          s_presence_src = RADAR_SRC_OUT;
static uint8_t          s_probe_st = 0U;      /* 0=待启动 1=已切波特率待发 2=等 ACK 3=收尾 9=结束 */
static uint8_t          s_probe_idx;
static uint32_t         s_probe_t0;
static uint32_t         s_probe_base;      /* 验证窗口的"上报帧数"基线 */
static uint32_t         s_rep_frames;      /* 本档收到的上报帧数(换档清零) */
static uint32_t         s_ack_frames;      /* 本档收到的 ACK 帧数(换档清零) */

/* ------------------------------ 收字节 -> 分帧 -> 解析 ------------------------------ */
#if (RADAR_DBG_EN != 0U)
static void radar_dbg_capture(const uint8_t *data, uint16_t len);   /* 定义见文件末尾调试段 */
#endif
#if (RADAR_DBG_EN != 0U)
static void radar_dbg_ack_dump(const uint8_t *data, uint16_t len);  /* 定义见文件末尾调试段 */
#endif

static void radar_on_bytes(const uint8_t *data, uint16_t len)
{
    radar_frame_t f;
    uint16_t i;

#if (RADAR_DBG_EN != 0U)
    radar_dbg_capture(data, len);       /* 抓原始字节(判断波特率/接线) */
#endif

    for (i = 0U; i < len; i++)
    {
        if (radar_frame_feed(&s_rx, data[i], m_u32Tickms, &f) != 0)
        {
            if (f.kind == RADAR_FRAME_KIND_ACK)
            {
                if (radar_proto_parse_ack(&f, &s_ack) != 0)
                {
                    s_ack_frames++;
                    s_ack_ready = 1U;
#if (RADAR_DBG_EN != 0U)
                    radar_dbg_ack_dump(f.data, f.data_len);
#endif
                }
            }
            else if (f.kind == RADAR_FRAME_KIND_REPORT)
            {
                if (radar_proto_parse_report(&f, &s_dev[0].rep) != 0)
                {
                    s_rep_frames++;
                    s_dev[0].last_rx_ms = m_u32Tickms;
                    s_reports++;
                }
            }
        }
    }
}

/* ------------------------------ 生命周期 ------------------------------ */
int32_t radar_init(void)
{
    uint8_t i;

    radar_frame_init(&s_rx);
    memset(s_dev, 0, sizeof(s_dev));
    memset(&s_ack, 0, sizeof(s_ack));
    s_ack_ready = 0U;
    s_reports = 0U;
    s_baud_locked = 0U;

    radar_port_init();
    radar_port_set_rx_handler(radar_on_bytes);

#if (RADAR_BAUD_FORCE != 0UL)
    radar_port_set_baud(RADAR_BAUD_FORCE);   /* 固定波特率: 跳过自适应探测 */
    s_baud_locked = 1U;
    s_probe_st    = 9U;
#else
    s_probe_st   = 0U;               /* 波特率自适应由 radar_poll() 推进(非阻塞) */
    s_probe_t0   = m_u32Tickms;      /* 启动延时基准: 等模块上电启动完成再探测 */
    s_rep_frames = 0U;
    s_ack_frames = 0U;
#endif

    for (i = 0U; i < RADAR_DEV_CNT; i++)
    {
        s_dev[i].out_present = 0U;
        s_dev[i].uart_online = 0U;
    }

    return LL_OK;
}

/* 非阻塞波特率自适应: 依次试候选波特率, 谁能回"使能配置"的 ACK 就锁定谁 */
static void radar_send_raw(uint16_t cmd, const uint8_t *val, uint8_t val_len)
{
    uint8_t  frame[RADAR_TX_MAX];
    uint16_t flen;

    flen = radar_proto_build_cmd(cmd, val, val_len, frame, sizeof(frame));
    if (flen == 0U) { return; }
    if (radar_port_tx_busy() != 0U) { return; }
    s_ack_ready = 0U;
    (void)radar_port_write(frame, flen);
}

/* ---------- 候选波特率表 ---------- */
static uint32_t radar_probe_baud(uint8_t idx)
{
    static const uint32_t baud_tab[] = RADAR_BAUD_TABLE;

    if (idx >= (uint8_t)RADAR_BAUD_TABLE_CNT) { return RADAR_BAUD_FALLBACK; }

    return baud_tab[idx];
}

/* 本档不收: 换下一档; 全试完 -> 回落默认波特率(locked=0) */
static void radar_probe_next(void)
{
    s_ack_ready  = 0U;
    s_probe_idx++;

    if (s_probe_idx >= (uint8_t)RADAR_BAUD_TABLE_CNT)
    {
        radar_port_set_baud(RADAR_BAUD_FALLBACK);
        radar_frame_init(&s_rx);
        s_rep_frames = 0U;
        s_ack_frames = 0U;
        s_probe_st   = 9U;
        radar_dbg_note_u32("probe FAIL, fallback ", RADAR_BAUD_FALLBACK);
        return;
    }

    radar_port_set_baud(radar_probe_baud(s_probe_idx));
    radar_frame_init(&s_rx);
    s_rep_frames = 0U;
    s_ack_frames = 0U;
    s_probe_t0   = m_u32Tickms;
    s_probe_st   = 1U;
    radar_dbg_note_u32("try baud ", radar_probe_baud(s_probe_idx));
}

/* 认定本档为模块真实波特率 */
static void radar_probe_accept(void)
{
    s_baud_locked = 1U;
    s_probe_st    = 9U;
    radar_dbg_note_u32("lock baud ", radar_port_get_baud());
}

/* 非阻塞波特率自适应:
 *   ① 先等模块上电启动完成(RADAR_PROBE_BOOT_MS) —— 模块没起来时发命令不会被应答,
 *      会导致第一档(通常是 256000)误判失败;
 *   ② 逐档: 发"使能配置(0x00FF)" -> 等响应;
 *   ③ 判定:
 *        - 收到 >= RADAR_BAUD_LOCK_FRAMES 个"上报帧"  -> 直接认定(无需 ACK, 兼容 TX 不通);
 *        - 收到 ACK -> 发"结束配置(0x00FE)"让模块恢复上报, 再用"上报帧"验证
 *          (RADAR_PROBE_VERIFY_MS 内), 验证通过才认定 —— 避免被乱码/残留字节误判;
 *        - 两者都没有 -> 换下一档。
 *   ④ 全部失败 -> 回落 RADAR_BAUD_FALLBACK 且 baud_locked=0。
 */
static void radar_probe_tick(void)
{
    uint8_t v[2];

    if (s_probe_st >= 9U) { return; }        /* 已结束 */

    v[0] = 0x01U;
    v[1] = 0x00U;

    switch (s_probe_st)
    {
        case 0U:                             /* 等模块启动完成, 再试第 1 档 */
            if ((m_u32Tickms - s_probe_t0) >= RADAR_PROBE_BOOT_MS)
            {
                s_probe_idx  = 0U;
                s_rep_frames = 0U;
                s_ack_frames = 0U;
                radar_port_set_baud(radar_probe_baud(0U));
                radar_frame_init(&s_rx);
                s_probe_t0 = m_u32Tickms;
                s_probe_st = 1U;
                radar_dbg_note_u32("try baud ", radar_probe_baud(0U));
            }
            break;

        case 1U:                             /* 发"使能配置" */
            if ((m_u32Tickms - s_probe_t0) >= 5U)
            {
                radar_send_raw(RADAR_CMD_ENABLE_CFG, v, 2U);
                s_probe_t0 = m_u32Tickms;
                s_probe_st = 2U;
            }
            break;

        case 2U:                             /* 本档判定 */
            if (s_rep_frames >= (uint32_t)RADAR_BAUD_LOCK_FRAMES)
            {
                /* 已收到足够多的上报帧 -> 就是这一档(模块没进配置态, 不必发结束配置) */
                radar_dbg_note("lock: frames (no ack)");
                radar_probe_accept();
            }
            else if (s_ack_frames != 0U)
            {
                /* 有应答只说明"这档有反应", 还要验证: 退出配置态后必须能收到上报帧 */
                radar_send_raw(RADAR_CMD_DISABLE_CFG, 0, 0U);
                s_probe_base = s_rep_frames;
                s_probe_t0   = m_u32Tickms;
                s_probe_st   = 4U;
                radar_dbg_note_u32("ack, verify baud ", radar_port_get_baud());
            }
            else if ((m_u32Tickms - s_probe_t0) >= RADAR_BAUD_PROBE_TIMEOUT_MS)
            {
                radar_probe_next();
            }
            else
            {
                /* 继续等 */
            }
            break;

        case 4U:                             /* 用上报帧验证本档(已发过结束配置) */
            if (s_rep_frames > s_probe_base)
            {
                radar_dbg_note("lock: ack + frames");
                radar_probe_accept();
            }
            else if ((m_u32Tickms - s_probe_t0) >= RADAR_PROBE_VERIFY_MS)
            {
                radar_dbg_note_u32("no report at ", radar_port_get_baud());
                radar_probe_next();
            }
            else
            {
                /* 继续等 */
            }
            break;

        default:
            s_probe_st = 9U;
            break;
    }
}

void radar_poll(void)
{
    radar_frame_tick(&s_rx, m_u32Tickms);
    radar_port_poll();
    radar_probe_tick();

    /* 与串口同一模块的 OUT 脚(可选判定源) */
    s_dev[0].out_present = (GPIO_ReadInputPins(RADAR_UART_DEV_OUT_PORT, RADAR_UART_DEV_OUT_PIN) == PIN_SET) ? 1U : 0U;
    s_dev[0].uart_online = ((m_u32Tickms - s_dev[0].last_rx_ms) <= RADAR_REPORT_STALE_MS) ? 1U : 0U;
}

/* ------------------------------ 状态查询 ------------------------------ */
const radar_dev_t *radar_dev(uint8_t dev)
{
    if (dev >= RADAR_DEV_CNT) { return 0; }
    return &s_dev[dev];
}

const radar_report_t *radar_report(uint8_t dev)
{
    if (dev >= RADAR_DEV_CNT) { return 0; }
    return &s_dev[dev].rep;
}

uint8_t radar_uart_online(uint8_t dev)
{
    if (dev >= RADAR_DEV_CNT) { return 0U; }
    return s_dev[dev].uart_online;
}

uint8_t radar_presence(uint8_t dev)
{
    uint8_t out;
    uint8_t uart_target;

    if (dev >= RADAR_DEV_CNT) { return 0U; }

    out = s_dev[dev].out_present;
    uart_target = ((s_dev[dev].uart_online != 0U) && (s_dev[dev].rep.target_state != RADAR_STATE_NONE)) ? 1U : 0U;

    switch (s_presence_src)
    {
        case RADAR_SRC_UART:
            return uart_target;

        case RADAR_SRC_OUT_OR_UART:
            return ((out != 0U) || (uart_target != 0U)) ? 1U : 0U;

        case RADAR_SRC_UART_FALLBACK_OUT:
            return (s_dev[dev].uart_online != 0U) ? uart_target : out;

        case RADAR_SRC_OUT:
        default:
            return out;
    }
}

void radar_set_presence_src(uint8_t src)
{
    s_presence_src = src;
}

uint8_t radar_presence_src(void)
{
    return s_presence_src;
}

uint8_t radar_baud_locked(void)
{
    return s_baud_locked;
}

uint32_t radar_get_baud(void)
{
    return radar_port_get_baud();
}

uint32_t radar_frames_ok(void)
{
    return s_rx.ok_cnt;
}

uint32_t radar_frames_err(void)
{
    return s_rx.err_cnt;
}

uint32_t radar_reports(void)
{
    return s_reports;
}

uint32_t radar_rx_bytes(void)
{
    return radar_port_rx_bytes();
}

uint32_t radar_rx_drop(void)
{
    return radar_port_rx_drop();
}

/* ------------------------------ 命令 ------------------------------ */
int32_t radar_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                  radar_ack_t *ack, uint32_t timeout_ms)
{
    uint8_t  frame[RADAR_TX_MAX];
    uint16_t flen;
    uint32_t t0;

    if (radar_ready() == 0U) { return LL_ERR_NOT_RDY; }   /* 波特率探测未结束 */
    flen = radar_proto_build_cmd(cmd, val, val_len, frame, sizeof(frame));
    if (flen == 0U) { return LL_ERR_INVD_PARAM; }

    /* 等上一条发完(最多 50ms) */
    t0 = m_u32Tickms;
    while (radar_port_tx_busy() != 0U)
    {
        radar_poll();
        if ((m_u32Tickms - t0) > 50U) { return LL_ERR_BUSY; }
    }

    s_ack_ready = 0U;
    if (radar_port_write(frame, flen) != LL_OK) { return LL_ERR; }

    t0 = m_u32Tickms;
    while ((m_u32Tickms - t0) < timeout_ms)
    {
        radar_poll();

        if (s_ack_ready != 0U)
        {
            if (RADAR_ACK_CMD_MATCH(s_ack.cmd, cmd))    /* 只认本条命令的 ACK(比低字节) */
            {
                if (ack != 0) { *ack = s_ack; }
                return (s_ack.status == 0U) ? LL_OK : LL_ERR;
            }
            s_ack_ready = 0U;                           /* 别的命令的 ACK, 丢弃继续等 */
        }
    }

    return LL_ERR_TIMEOUT;
}

/* 探测是否已结束(9 = 结束) */
uint8_t radar_ready(void)
{
    return (s_probe_st >= 9U) ? 1U : 0U;
}

/* 配置事务: 使能配置 -> 命令 -> 结束配置 */
static int32_t radar_cfg_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                             radar_ack_t *ack, uint32_t timeout_ms)
{
    uint8_t en[2];
    int32_t ret;

    en[0] = 0x01U;
    en[1] = 0x00U;

    ret = radar_cmd(RADAR_CMD_ENABLE_CFG, en, 2U, 0, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    ret = radar_cmd(cmd, val, val_len, ack, timeout_ms);

    (void)radar_cmd(RADAR_CMD_DISABLE_CFG, 0, 0U, 0, RADAR_CMD_TIMEOUT_MS);

    return ret;
}

int32_t radar_read_params(radar_params_t *out)
{
    radar_ack_t ack;
    int32_t ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(RADAR_CMD_READ_PARAM, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_params(&ack, out) != 0) ? LL_OK : LL_ERR;
}

/* 距离门灵敏度: 值 = [00 00][门号 LE32][01 00][运动 LE32][02 00][静止 LE32] */
static void radar_put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

int32_t radar_set_sensitivity(uint16_t gate, uint16_t move_sens, uint16_t still_sens)
{
    uint8_t v[18];

    v[0] = 0x00U; v[1] = 0x00U;                         /* 距离门字 */
    radar_put_u32le(&v[2], (uint32_t)gate);
    v[6] = 0x01U; v[7] = 0x00U;                         /* 运动灵敏度字 */
    radar_put_u32le(&v[8], (uint32_t)move_sens);
    v[12] = 0x02U; v[13] = 0x00U;                       /* 静止灵敏度字 */
    radar_put_u32le(&v[14], (uint32_t)still_sens);

    return radar_cfg_cmd(RADAR_CMD_SENSITIVITY, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_set_max_gate(uint16_t move_gate, uint16_t still_gate, uint16_t no_body_sec)
{
    uint8_t v[18];

    v[0] = 0x00U; v[1] = 0x00U;                         /* 最大运动距离门字 */
    radar_put_u32le(&v[2], (uint32_t)move_gate);
    v[6] = 0x01U; v[7] = 0x00U;                         /* 最大静止距离门字 */
    radar_put_u32le(&v[8], (uint32_t)still_gate);
    v[12] = 0x02U; v[13] = 0x00U;                       /* 无人持续时间字 */
    radar_put_u32le(&v[14], (uint32_t)no_body_sec);

    return radar_cfg_cmd(RADAR_CMD_MAX_GATE, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_set_resolution(uint8_t idx)
{
    uint8_t v[2];

    v[0] = idx; v[1] = 0x00U;                           /* 0 = 0.75m, 1 = 0.2m */

    return radar_cfg_cmd(RADAR_CMD_RESOLUTION, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

/* 0x00A3: 模块在"应答发送完成后"自动重启。
 * 需要重启才生效的配置: 串口波特率(0x00A1)、距离分辨率(0x00AA)、蓝牙(0x00A4)、
 * 蓝牙密码(0x00A9)、恢复出厂(0x00A2); 灵敏度(0x0064)与最大距离门(0x0060)立即生效。 */
int32_t radar_restart(void)
{
    return radar_cmd(RADAR_CMD_RESTART, 0, 0U, 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_set_uart_baud_index(uint8_t idx)
{
    uint8_t v[2];

    v[0] = idx; v[1] = 0x00U;                           /* 0x0007=256000, 0x0008=460800 */

    return radar_cfg_cmd(RADAR_CMD_UART_BAUD, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_eng_mode(uint8_t on)
{
    return radar_cfg_cmd((on != 0U) ? RADAR_CMD_ENG_MODE_ON : RADAR_CMD_ENG_MODE_OFF,
                         0, 0U, 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_noise_start(uint16_t sec)
{
    uint8_t v[2];

    v[0] = (uint8_t)(sec & 0xFFU);
    v[1] = (uint8_t)((sec >> 8) & 0xFFU);

    return radar_cfg_cmd(RADAR_CMD_NOISE_START, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_noise_status(uint16_t *status)
{
    radar_ack_t ack;
    int32_t ret;

    if (status == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(RADAR_CMD_NOISE_STATUS, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_u16(&ack, status) != 0) ? LL_OK : LL_ERR;
}

/* ==========================================================================================
 * 雷达调试输出(上板验证用, 临时) —— 总开关: radar_cfg.h 的 RADAR_DBG_EN
 *
 * 为什么不单独放 radar_dbg.c: 新建 .c 必须在 Keil 工程里手工添加(且 Keil GUI 打开时会
 * 覆盖 .uvprojx 的改动), 放在本文件里只需重新编译, 不动工程。
 *
 * 验证通过后清理(三选一, 建议 1):
 *   1) radar_cfg.h 里 RADAR_DBG_EN 置 0 —— 只剩空实现, 不占 Flash;
 *   2) 删除本文件中 "#if (RADAR_DBG_EN != 0U)" 到文件末尾的整段 + main.c 的 radar_dbg_poll()
 *      + main.h 的 #include "radar_dbg.h" + 删除 radar_dbg.h;
 *   3) 全部保留, 长期用作现场诊断。
 *
 * 对外接口与结构体定义在 radar_dbg.h; 使用说明见 docs/hc32_radar_bringup.md。
 * ========================================================================================== */
#if (RADAR_DBG_EN != 0U)

#if (RADAR_DBG_SINK_RS485 != 0U)
#include "bsp_rs485.h"                  /* Uart4_DMA_Trans (RS485 主机口发送) */
#endif

volatile radar_dbg_snap_t g_radar_dbg;
char                      g_radar_dbg_line[RADAR_DBG_LINE_MAX];
char                      g_radar_dbg_evt[RADAR_DBG_EVT_MAX];
uint32_t                  g_radar_dbg_cnt;
uint32_t                  g_radar_dbg_evt_cnt;
char                      g_radar_dbg_hex[RADAR_DBG_HEX_MAX];
char                      g_radar_dbg_ack[RADAR_DBG_ACK_MAX];

static uint16_t           s_dbg_hex_n;      /* 已抓字节数 */
static uint32_t           s_dbg_baud_last;  /* 上一拍的波特率(变化则重抓) */
#if (RADAR_DBG_SET_BAUD_IDX != 0U)
static uint8_t            s_dbg_setbaud_st;     /* 0=待做 1=已发命令 2=等模块重启 3=完成 */
static uint32_t           s_dbg_setbaud_ms;
static uint32_t           s_dbg_vfy_rep;     /* 校验基线: 切换前已解析的上报帧数 */
static uint32_t           s_dbg_vfy_baud;    /* 切换前的波特率(用于回退) */
#endif

static uint32_t s_dbg_line_ms;          /* 上次刷状态行的时刻 */
static uint32_t s_dbg_rx_last;          /* 上次的收字节数 */
static uint32_t s_dbg_rx_ms;            /* 收字节数最近一次变化的时刻 */
static uint32_t s_dbg_fer_last;         /* 上次的分帧错误数 */
static uint8_t  s_dbg_rdy_last;
static uint8_t  s_dbg_st_last;
static uint8_t  s_dbg_booted;
static uint8_t  s_dbg_stalled;

/* ------------------------------ 极简文本拼装(不引入 printf) ------------------------------ */
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

static char *dbg_hex_nib(char *p, uint8_t n)
{
    *p++ = (char)((n < 10U) ? ((char)(48U + n)) : ((char)(55U + n)));
    return p;
}

static char *dbg_hex_byte(char *p, uint8_t v)
{
    p = dbg_hex_nib(p, (uint8_t)((v >> 4) & 0x0FU));
    return dbg_hex_nib(p, (uint8_t)(v & 0x0FU));
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
    /* 与 STM32 挂同一总线时属"抢总线"(STM32 每 20ms 查询一次), 只在单独给本板上电、
     * 用 USB-RS485/串口助手 @460800 直连时使用 */
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

/* 抓本波特率下收到的前 RADAR_DBG_HEX_BYTES 个字节(十六进制), 换波特率时清空。
 * 判读: 开头是 "F4 F3 F2 F1" = 上报帧头 / "FD FC FB FA" = ACK, 说明波特率正确;
 *       全是乱码或个别字节 => 波特率不对或接线有干扰; 一个字节都没有 => RX 没接对/模块没发。 */
static void radar_dbg_capture(const uint8_t *data, uint16_t len)
{
    char    *p;
    uint16_t i;

    if ((data == 0) || (s_dbg_hex_n >= (uint16_t)RADAR_DBG_HEX_BYTES))
    {
        return;
    }

    p = &g_radar_dbg_hex[(uint32_t)s_dbg_hex_n * 3UL];
    for (i = 0U; i < len; i++)
    {
        if (s_dbg_hex_n >= (uint16_t)RADAR_DBG_HEX_BYTES)
        {
            break;
        }
        p = dbg_hex_byte(p, data[i]);
        *p++ = (char)32;
        s_dbg_hex_n++;
    }
    *p = (char)0;
}

/* 记录最近一帧 ACK: 解析出的命令字/状态字 + 原始数据字节(十六进制)。
 * 用途: 现场核对 ACK 的真实字段布局(文档示例里命令字高字节与状态字存在歧义), 也用于
 *       排查"发命令没反应"时到底是没收到 ACK 还是 ACK 没被认出来。 */
static void radar_dbg_ack_dump(const uint8_t *data, uint16_t len)
{
    char    *p = g_radar_dbg_ack;
    uint16_t i;

    p = dbg_str(p, "c=");
    p = dbg_hex_byte(p, (uint8_t)(s_ack.cmd & 0x00FFU));
    p = dbg_hex_byte(p, (uint8_t)((s_ack.cmd >> 8) & 0x00FFU));
    p = dbg_str(p, " st=");
    p = dbg_hex_byte(p, (uint8_t)(s_ack.status & 0x00FFU));
    p = dbg_hex_byte(p, (uint8_t)((s_ack.status >> 8) & 0x00FFU));
    p = dbg_str(p, " d=");
    p = dbg_u32(p, (uint32_t)len);
    p = dbg_str(p, " : ");

    for (i = 0U; i < len; i++)
    {
        if (((uint32_t)(p - g_radar_dbg_ack) + 4UL) >= (uint32_t)RADAR_DBG_ACK_MAX) { break; }
        p = dbg_hex_byte(p, data[i]);
        *p++ = (char)32;
    }
    *p = (char)0;
}

/* ------------------------------ 状态快照与状态行 ------------------------------ */
static void dbg_update_snap(void)
{
    const radar_dev_t    *d = radar_dev(0);
    const radar_report_t *r = radar_report(0);

    g_radar_dbg.ms      = m_u32Tickms;
    g_radar_dbg.rdy     = (uint32_t)radar_ready();
    g_radar_dbg.lock    = (uint32_t)radar_baud_locked();
    g_radar_dbg.baud    = radar_get_baud();
    g_radar_dbg.rep     = radar_reports();
    g_radar_dbg.repf    = s_rep_frames;
    g_radar_dbg.ackf    = s_ack_frames;
    g_radar_dbg.fok     = radar_frames_ok();
    g_radar_dbg.fer     = radar_frames_err();
    g_radar_dbg.rx      = radar_rx_bytes();
    g_radar_dbg.drp     = radar_rx_drop();
    g_radar_dbg.out     = (d != 0) ? (uint32_t)d->out_present : 0U;
    g_radar_dbg.online  = (d != 0) ? (uint32_t)d->uart_online : 0U;
    g_radar_dbg.pre     = (uint32_t)radar_presence(0);
    g_radar_dbg.st      = (r != 0) ? (uint32_t)r->target_state : 0U;
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
    p = dbg_kv (p, " repf=", g_radar_dbg.repf);
    p = dbg_kv (p, " ackf=", g_radar_dbg.ackf);
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

    if (s_dbg_booted == 0U)
    {
        s_dbg_booted = 1U;
        s_dbg_rx_ms  = m_u32Tickms;
        dbg_event("boot");
    }

    if (radar_get_baud() != s_dbg_baud_last)
    {
        s_dbg_baud_last   = radar_get_baud();
        s_dbg_hex_n       = 0U;
        g_radar_dbg_hex[0] = (char)0;
        dbg_event_u32("baud=", radar_get_baud());
    }

    if ((s_dbg_rdy_last == 0U) && (radar_ready() != 0U))
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
    s_dbg_rdy_last = radar_ready();

    if (d->rep.target_state != s_dbg_st_last)
    {
        dbg_event_st((uint32_t)s_dbg_st_last, (uint32_t)d->rep.target_state);
        s_dbg_st_last = d->rep.target_state;
    }

    if (fer != s_dbg_fer_last)
    {
        s_dbg_fer_last = fer;
        dbg_event_u32("frame err cnt=", fer);
    }

    if (rx != s_dbg_rx_last)
    {
        s_dbg_rx_last = rx;
        s_dbg_rx_ms   = m_u32Tickms;
        s_dbg_stalled = 0U;
    }
    else if ((rx != 0U) && (s_dbg_stalled == 0U) &&
             ((m_u32Tickms - s_dbg_rx_ms) >= RADAR_DBG_RX_STALL_MS))
    {
        s_dbg_stalled = 1U;
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

void radar_dbg_note_u32(const char *tag, uint32_t v)
{
    if (tag != 0)
    {
        dbg_event_u32(tag, v);
    }
}

void radar_dbg_poll(void)
{
#if (RADAR_DBG_SET_BAUD_IDX != 0U)
    /* 一次性: 改模块波特率(0x00A1) -> 重启模块(0x00A3) -> 800ms 后驱动跟着切。
     * 协议规定: 该配置"重启模块后生效", 所以模块切换前驱动必须留在旧波特率上, 否则丢链路。 */
    if ((s_dbg_setbaud_st == 0U) && (radar_baud_locked() != 0U))
    {
        int32_t ret;

        s_dbg_setbaud_st = 1U;
        ret = radar_set_uart_baud_index((uint8_t)RADAR_DBG_SET_BAUD_IDX);
        if (ret == LL_OK)
        {
            dbg_event_u32("module baud idx OK ", (uint32_t)RADAR_DBG_SET_BAUD_IDX);
            ret = radar_restart();
            if (ret == LL_OK)
            {
                dbg_event("module restart sent");
                s_dbg_setbaud_ms = m_u32Tickms;
                s_dbg_setbaud_st = 2U;
            }
            else
            {
                dbg_event_u32("module restart FAIL ret=", (uint32_t)ret);
                s_dbg_setbaud_st = 3U;
            }
        }
        else
        {
            dbg_event_u32("module baud FAIL ret=", (uint32_t)ret);
            s_dbg_setbaud_st = 3U;
        }
    }
    else if ((s_dbg_setbaud_st == 2U) && ((m_u32Tickms - s_dbg_setbaud_ms) >= 800U))
    {
        /* 模块已按新波特率重启: 驱动跟着切, 并开始自检(等新波特率下的上报帧) */
        s_dbg_vfy_baud = radar_get_baud();
        s_dbg_vfy_rep  = radar_frames_ok();
        radar_port_set_baud(RADAR_DBG_SET_BAUD_VALUE);
        radar_frame_init(&s_rx);
        s_probe_st    = 9U;
        s_baud_locked = 1U;
        s_dbg_setbaud_ms = m_u32Tickms;
        s_dbg_setbaud_st = 3U;
        dbg_event_u32("driver baud set ", RADAR_DBG_SET_BAUD_VALUE);
    }
    else if ((s_dbg_setbaud_st == 3U) && ((m_u32Tickms - s_dbg_setbaud_ms) >= 2500U))
    {
        if (radar_frames_ok() != s_dbg_vfy_rep)
        {
            dbg_event_u32("baud verify OK ", RADAR_DBG_SET_BAUD_VALUE);
            s_dbg_setbaud_st = 5U;
        }
        else
        {
            /* 新波特率下收不到帧: 回退到切换前的波特率再看(模块可能没真正切换) */
            radar_port_set_baud(s_dbg_vfy_baud);
            radar_frame_init(&s_rx);
            s_dbg_vfy_rep    = radar_frames_ok();
            s_dbg_setbaud_ms = m_u32Tickms;
            s_dbg_setbaud_st = 4U;
            dbg_event_u32("no data at new baud, back to ", s_dbg_vfy_baud);
        }
    }
    else if ((s_dbg_setbaud_st == 4U) && ((m_u32Tickms - s_dbg_setbaud_ms) >= 2500U))
    {
        s_dbg_setbaud_st = 5U;
        if (radar_frames_ok() != s_dbg_vfy_rep)
        {
            dbg_event_u32("old baud still OK ", s_dbg_vfy_baud);
        }
        else
        {
            dbg_event("no data on either baud: check wiring");
        }
    }
#endif

    dbg_check_events();

    if ((m_u32Tickms - s_dbg_line_ms) >= RADAR_DBG_PERIOD_MS)
    {
        s_dbg_line_ms = m_u32Tickms;
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

void radar_dbg_note_u32(const char *tag, uint32_t v)
{
    (void)tag;
    (void)v;
}

void radar_dbg_poll(void)
{
}

#endif  /* RADAR_DBG_EN */
/* ============================== 调试输出段结束 ============================== */
