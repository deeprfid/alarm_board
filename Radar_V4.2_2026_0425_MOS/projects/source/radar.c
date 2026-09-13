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
#if (RADAR_PROVISION_BAUD != 0UL)
static uint8_t          s_prov_st;          /* 产线配置状态: 0 待做 1 写入中 2 等重启 3 自检中 4 成功 5 失败 6 无法配置 */
static uint32_t         s_prov_t0;
static uint32_t         s_prov_rep;
static uint32_t         s_prov_baud;        /* 配置前的波特率(失败时回退用) */
#endif

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
        radar_dbg_note_u32(RADAR_DBG_EV_PROBE_FAIL, RADAR_BAUD_FALLBACK);
        return;
    }

    radar_port_set_baud(radar_probe_baud(s_probe_idx));
    radar_frame_init(&s_rx);
    s_rep_frames = 0U;
    s_ack_frames = 0U;
    s_probe_t0   = m_u32Tickms;
    s_probe_st   = 1U;
    radar_dbg_note_u32(RADAR_DBG_EV_TRY_BAUD, radar_probe_baud(s_probe_idx));
}

/* 认定本档为模块真实波特率 */
static void radar_probe_accept(void)
{
    s_baud_locked = 1U;
    s_probe_st    = 9U;
    radar_dbg_note_u32(RADAR_DBG_EV_LOCK_ACK, radar_port_get_baud());
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
                radar_dbg_note_u32(RADAR_DBG_EV_TRY_BAUD, radar_probe_baud(0U));
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
                radar_dbg_note_u32(RADAR_DBG_EV_LOCK_FRAMES, radar_port_get_baud());
                radar_probe_accept();
            }
            else if (s_ack_frames != 0U)
            {
                /* 有应答只说明"这档有反应", 还要验证: 退出配置态后必须能收到上报帧 */
                radar_send_raw(RADAR_CMD_DISABLE_CFG, 0, 0U);
                s_probe_base = s_rep_frames;
                s_probe_t0   = m_u32Tickms;
                s_probe_st   = 4U;
                radar_dbg_note_u32(RADAR_DBG_EV_ACK_VERIFY, radar_port_get_baud());
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
                radar_dbg_note_u32(RADAR_DBG_EV_LOCK_ACK, radar_port_get_baud());
                radar_probe_accept();
            }
            else if ((m_u32Tickms - s_probe_t0) >= RADAR_PROBE_VERIFY_MS)
            {
                radar_dbg_note_u32(RADAR_DBG_EV_NO_REPORT, radar_port_get_baud());
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

/* ------------------------------ 产线配置: 模块波特率 ------------------------------ */
#if (RADAR_PROVISION_BAUD != 0UL)
/* 波特率 -> 协议表 6 的索引(0x00A1 用); 不在表里返回 0 */
static uint8_t radar_baud_to_index(uint32_t baud)
{
    static const uint32_t idx_tab[] = RADAR_BAUD_IDX_TABLE;
    uint8_t i;

    for (i = 0U; i < (uint8_t)RADAR_BAUD_IDX_TABLE_CNT; i++)
    {
        if (idx_tab[i] == baud) { return (uint8_t)(i + 1U); }
    }

    return 0U;
}

/* 把模块波特率配置成 RADAR_PROVISION_BAUD(掉电保存), 幂等:
 *   0 等探测结束 -> 若已是目标值则直接结束(什么都不发)
 *   1 写配置(0x00A1) -> 重启模块(0x00A3, 配置重启后生效) -> 等 RADAR_PROVISION_RESTART_MS
 *   2 驱动切到目标波特率
 *   3 自检 RADAR_PROVISION_VERIFY_MS: 收到上报帧 = 成功; 否则回退到原波特率继续工作
 * 结果见 radar_provision_state() 与 g_radar_dbg.prov_st */
static void radar_provision_tick(void)
{
    if (s_prov_st >= 4U) { return; }         /* 已结束 */

    switch (s_prov_st)
    {
        case 0U:                             /* 等自适应探测结束 */
            if ((radar_ready() == 0U) || (radar_baud_locked() == 0U)) { break; }
            if (radar_get_baud() == RADAR_PROVISION_BAUD)
            {
                s_prov_st = 4U;              /* 已经是目标值: 幂等, 不发任何命令 */
                break;
            }
            if (radar_baud_to_index(RADAR_PROVISION_BAUD) == 0U)
            {
                s_prov_st = 6U;              /* 目标值不在协议表 6 里 */
                break;
            }
            s_prov_st = 1U;
            break;

        case 1U:                             /* 写配置 + 重启模块 */
            s_prov_baud = radar_get_baud();
            if (radar_set_uart_baud_index(radar_baud_to_index(RADAR_PROVISION_BAUD)) != LL_OK)
            {
                radar_dbg_note_u32(RADAR_DBG_EV_SETBAUD_FAIL, RADAR_PROVISION_BAUD);
                s_prov_st = 5U;
                break;
            }
            radar_dbg_note_u32(RADAR_DBG_EV_SETBAUD_OK, (uint32_t)radar_baud_to_index(RADAR_PROVISION_BAUD));
            if (radar_restart() != LL_OK)
            {
                radar_dbg_note_u32(RADAR_DBG_EV_RESTART_FAIL, RADAR_PROVISION_BAUD);
                s_prov_st = 5U;
                break;
            }
            radar_dbg_note_u32(RADAR_DBG_EV_RESTART_SENT, RADAR_PROVISION_BAUD);
            s_prov_t0 = m_u32Tickms;
            s_prov_st = 2U;
            break;

        case 2U:                             /* 等模块按新波特率重启完成, 驱动再切过去 */
            if ((m_u32Tickms - s_prov_t0) >= RADAR_PROVISION_RESTART_MS)
            {
                s_prov_rep = radar_reports();
                radar_port_set_baud(RADAR_PROVISION_BAUD);
                radar_frame_init(&s_rx);
                radar_dbg_note_u32(RADAR_DBG_EV_DRIVER_BAUD, RADAR_PROVISION_BAUD);
                s_prov_t0 = m_u32Tickms;
                s_prov_st = 3U;
            }
            break;

        case 3U:                             /* 自检: 新波特率下能否收到上报 */
            if (radar_reports() != s_prov_rep)
            {
                radar_dbg_note_u32(RADAR_DBG_EV_VERIFY_OK, RADAR_PROVISION_BAUD);
                s_prov_st = 4U;
            }
            else if ((m_u32Tickms - s_prov_t0) >= RADAR_PROVISION_VERIFY_MS)
            {
                /* 新波特率下没数据(模块可能没真正切换): 回退原波特率继续工作, 不影响业务 */
                radar_port_set_baud(s_prov_baud);
                radar_frame_init(&s_rx);
                radar_dbg_note_u32(RADAR_DBG_EV_VERIFY_FALLBACK, s_prov_baud);
                s_prov_st = 5U;
            }
            else
            {
                /* 继续等 */
            }
            break;

        default:
            s_prov_st = 6U;
            break;
    }
}

uint8_t radar_provision_state(void)
{
    return s_prov_st;
}
#else
uint8_t radar_provision_state(void)
{
    return 0U;
}
#endif

void radar_poll(void)
{
    radar_frame_tick(&s_rx, m_u32Tickms);
    radar_port_poll();
    radar_probe_tick();
#if (RADAR_PROVISION_BAUD != 0UL)
    radar_provision_tick();              /* 产线配置: 把模块波特率配成 RADAR_PROVISION_BAUD */
#endif

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
 * 雷达调试快照(上板验证用, 临时) —— 总开关: radar_cfg.h 的 RADAR_DBG_EN
 *
 * 本板没有连电脑的串口, 因此这里**不做任何串口/printf/文本输出**:
 * 所有信息都写进结构体 g_radar_dbg(纯数值), Keil 调试时 Watch 窗口加 g_radar_dbg 即可。
 *
 * 实现放在本文件末尾是为了不动 Keil 工程(新建 .c 需要手工加入, 且 Keil GUI 打开时会
 * 用内存里的工程覆盖 .uvprojx)。验证通过后: RADAR_DBG_EN 置 0 或删掉本段。
 * ========================================================================================== */
#if (RADAR_DBG_EN != 0U)

volatile radar_dbg_snap_t g_radar_dbg;

static uint32_t s_dbg_upd_ms;           /* 上次刷新快照的时刻 */
static uint32_t s_dbg_rx_last;          /* 上次的收字节数 */
static uint32_t s_dbg_rx_ms;            /* 收字节数最近一次变化的时刻 */
static uint32_t s_dbg_fer_last;         /* 上次的分帧错误数 */
static uint32_t s_dbg_baud_last;        /* 上次的波特率(变化则重抓头部字节) */
static uint32_t s_dbg_st_last;          /* 上次的目标状态 */
static uint8_t  s_dbg_head_n;           /* 已抓的头部字节数 */
static uint8_t  s_dbg_booted;
static uint8_t  s_dbg_stalled;

/* ------------------------------ 事件(数值, 环形 4 条) ------------------------------ */
static void dbg_note(uint8_t code, uint32_t val)
{
    uint32_t i = g_radar_dbg.evt_cnt % (uint32_t)RADAR_DBG_EVT_DEPTH;

    g_radar_dbg.evt_code[i] = (uint32_t)code;
    g_radar_dbg.evt_val[i]  = val;
    g_radar_dbg.evt_ms[i]   = m_u32Tickms;
    g_radar_dbg.evt_cnt++;
}

void radar_dbg_note_u32(uint8_t code, uint32_t val)
{
    dbg_note(code, val);
}

/* 记录"本波特率下收到的最前面几个字节": 判断模块真实波特率/接线是否通。
 * 判读: 开头 F4 F3 F2 F1 = 上报帧头 / FD FC FB FA = ACK, 说明波特率正确。 */
static void radar_dbg_capture(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == 0) { return; }

    for (i = 0U; (i < len) && (s_dbg_head_n < (uint8_t)RADAR_DBG_RX_HEAD); i++)
    {
        g_radar_dbg.rx_head[s_dbg_head_n] = data[i];
        s_dbg_head_n++;
    }
}

/* 记录最近一帧 ACK 的解析值与原始数据字节(核对 ACK 真实字段布局用) */
static void radar_dbg_ack_dump(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t n = len;

    g_radar_dbg.ack_cmd    = (uint32_t)s_ack.cmd;
    g_radar_dbg.ack_status = (uint32_t)s_ack.status;
    g_radar_dbg.ack_len    = (uint32_t)len;

    if (data == 0) { return; }
    if (n > (uint16_t)RADAR_DBG_ACK_DATA) { n = (uint16_t)RADAR_DBG_ACK_DATA; }

    for (i = 0U; i < n; i++)
    {
        g_radar_dbg.ack_data[i] = data[i];
    }
}

/* ------------------------------ 快照刷新 ------------------------------ */
static void dbg_update_snap(void)
{
    const radar_dev_t    *d = radar_dev(0);
    const radar_report_t *r = radar_report(0);

    g_radar_dbg.ms        = m_u32Tickms;
    g_radar_dbg.rdy       = (uint32_t)radar_ready();
    g_radar_dbg.lock      = (uint32_t)radar_baud_locked();
    g_radar_dbg.baud      = radar_get_baud();
    g_radar_dbg.probe_st  = (uint32_t)s_probe_st;
    g_radar_dbg.probe_idx = (uint32_t)s_probe_idx;
#if (RADAR_PROVISION_BAUD != 0UL)
    g_radar_dbg.prov_st   = (uint32_t)s_prov_st;
#endif
    g_radar_dbg.rep       = radar_reports();
    g_radar_dbg.repf      = s_rep_frames;
    g_radar_dbg.ackf      = s_ack_frames;
    g_radar_dbg.fok       = radar_frames_ok();
    g_radar_dbg.fer       = radar_frames_err();
    g_radar_dbg.rx        = radar_rx_bytes();
    g_radar_dbg.drp       = radar_rx_drop();
    g_radar_dbg.out       = (d != 0) ? (uint32_t)d->out_present : 0U;
    g_radar_dbg.online    = (d != 0) ? (uint32_t)d->uart_online : 0U;
    g_radar_dbg.pre       = (uint32_t)radar_presence(0);
    g_radar_dbg.st        = (r != 0) ? (uint32_t)r->target_state : 0U;
    g_radar_dbg.mv_dist   = (r != 0) ? (uint32_t)r->moving_distance_cm : 0U;
    g_radar_dbg.mv_eng    = (r != 0) ? (uint32_t)r->moving_energy : 0U;
    g_radar_dbg.st_dist   = (r != 0) ? (uint32_t)r->still_distance_cm : 0U;
    g_radar_dbg.st_eng    = (r != 0) ? (uint32_t)r->still_energy : 0U;
    g_radar_dbg.dd        = (r != 0) ? (uint32_t)r->detect_distance_cm : 0U;
}

/* ------------------------------ 事件检测 ------------------------------ */
static void dbg_check_events(void)
{
    const radar_dev_t *d = radar_dev(0);
    uint32_t           rx;
    uint32_t           fer;

    if (d == 0) { return; }

    rx  = radar_rx_bytes();
    fer = radar_frames_err();

    if (s_dbg_booted == 0U)
    {
        s_dbg_booted = 1U;
        s_dbg_rx_ms  = m_u32Tickms;
        dbg_note(RADAR_DBG_EV_BOOT, 0U);
    }

    if (radar_get_baud() != s_dbg_baud_last)
    {
        s_dbg_baud_last    = radar_get_baud();
        s_dbg_head_n       = 0U;
        g_radar_dbg.rx_head[0] = 0U;
        dbg_note(RADAR_DBG_EV_BAUD_CHANGE, s_dbg_baud_last);
    }

    if (d->rep.target_state != s_dbg_st_last)
    {
        s_dbg_st_last = d->rep.target_state;
        dbg_note(RADAR_DBG_EV_ST_CHANGE, (uint32_t)s_dbg_st_last);
    }

    if (fer != s_dbg_fer_last)
    {
        s_dbg_fer_last = fer;
        dbg_note(RADAR_DBG_EV_FRAME_ERR, fer);
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
        dbg_note(RADAR_DBG_EV_RX_STALL, rx);
    }
}

/* ------------------------------ 对外接口 ------------------------------ */
void radar_dbg_poll(void)
{
    dbg_check_events();

    if ((m_u32Tickms - s_dbg_upd_ms) >= RADAR_DBG_PERIOD_MS)
    {
        s_dbg_upd_ms = m_u32Tickms;
        dbg_update_snap();
    }
}

#else   /* RADAR_DBG_EN == 0: 关闭调试, 保留空实现, 调用方不必加 #if */

void radar_dbg_note_u32(uint8_t code, uint32_t val)
{
    (void)code;
    (void)val;
}

void radar_dbg_poll(void)
{
}

#endif  /* RADAR_DBG_EN */
/* ============================== 调试段结束 ============================== */
