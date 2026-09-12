/*******************************************************************************
 * radar.c -- LD2410C 雷达服务层实现
 ******************************************************************************/
#include "radar.h"
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

/* ------------------------------ 收字节 -> 分帧 -> 解析 ------------------------------ */
static void radar_on_bytes(const uint8_t *data, uint16_t len)
{
    radar_frame_t f;
    uint16_t i;

    for (i = 0U; i < len; i++)
    {
        if (radar_frame_feed(&s_rx, data[i], m_u32Tickms, &f) != 0)
        {
            if (f.kind == RADAR_FRAME_KIND_ACK)
            {
                if (radar_proto_parse_ack(&f, &s_ack) != 0) { s_ack_ready = 1U; }
            }
            else if (f.kind == RADAR_FRAME_KIND_REPORT)
            {
                if (radar_proto_parse_report(&f, &s_dev[0].rep) != 0)
                {
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

    s_probe_st = 0U;                 /* 波特率自适应由 radar_poll() 推进(非阻塞) */

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

static void radar_probe_tick(void)
{
    static const uint32_t baud_tab[] = RADAR_BAUD_TABLE;
    uint8_t v[2];

    if (s_probe_st >= 9U) { return; }        /* 已结束 */

    v[0] = 0x01U;
    v[1] = 0x00U;

    switch (s_probe_st)
    {
        case 0U:                             /* 启动: 试第 1 个波特率 */
            s_probe_idx = 0U;
            radar_port_set_baud(baud_tab[0]);
            radar_frame_init(&s_rx);
            s_probe_t0 = m_u32Tickms;
            s_probe_st = 1U;
            break;

        case 1U:                             /* 发"使能配置" */
            if ((m_u32Tickms - s_probe_t0) >= 5U)
            {
                radar_send_raw(RADAR_CMD_ENABLE_CFG, v, 2U);
                s_probe_t0 = m_u32Tickms;
                s_probe_st = 2U;
            }
            break;

        case 2U:                             /* 等 ACK */
            if ((s_ack_ready != 0U) && (s_ack.cmd == RADAR_CMD_ENABLE_CFG) && (s_ack.status == 0U))
            {
                s_baud_locked = 1U;
                s_probe_t0 = m_u32Tickms;
                s_probe_st = 3U;
            }
            else if ((m_u32Tickms - s_probe_t0) >= RADAR_BAUD_PROBE_TIMEOUT_MS)
            {
                s_probe_idx++;
                if (s_probe_idx >= RADAR_BAUD_TABLE_CNT)
                {
                    radar_port_set_baud(RADAR_BAUD_FALLBACK);   /* 全部失败: 回落默认 */
                    radar_frame_init(&s_rx);
                    s_probe_st = 9U;
                }
                else
                {
                    radar_port_set_baud(baud_tab[s_probe_idx]);
                    radar_frame_init(&s_rx);
                    s_probe_t0 = m_u32Tickms;
                    s_probe_st = 1U;
                }
            }
            break;

        case 3U:                             /* 探测成功收尾: 退出配置态 */
            if ((m_u32Tickms - s_probe_t0) >= 5U)
            {
                radar_send_raw(RADAR_CMD_DISABLE_CFG, 0, 0U);
                s_probe_st = 9U;
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
            if (s_ack.cmd == cmd)                       /* 只认本条命令的 ACK */
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
