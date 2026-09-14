/*******************************************************************************
 * radar.c -- LD2410C 雷达服务层实现
 ******************************************************************************/
#include "radar.h"
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

/* ============================ 现场 Watch 只用这 3 个单值 ============================
 * 本板没有调试串口, 在 Watch 里加一堆变量抄数字的做法已经废弃(现场结论: 没法调试)。
 * 只保留下面 3 个, 且**不许再加第 4 个** —— 需要更多信息就重新定义取值, 不要新增变量。
 *
 *   g_radar_lock : 1 = 已锁定模块波特率(探测成功); 0 = 没锁住(8 档全试完仍失败)
 *   g_radar_baud : 当前波特率(软件记录的档位; 是否真写进硬件见 g_radar_comm 的 0x400)
 *   g_radar_comm : 位图 + 标志位 + **BRR 整数分频指纹**, 一个数即可定位:
 *                  bit0..bit7 = 各候选档是否收到过字节, 顺序同 RADAR_BAUD_TABLE:
 *                     bit0=256000 bit1=460800 bit2=115200 bit3=9600
 *                     bit4=19200  bit5=38400  bit6=57600  bit7=230400
 *                  bit24..bit31 = **最近 1 秒收到的合法上报帧数**(即帧率, 单位 Hz)
 *                  0x100 = 曾解出过合法上报帧(乱码凑不出来)
 *                  0x200 = 最近 1 秒内仍有合法帧(正在正常通信)
 *                  0x400 = 锁定档位的 SetBaudrate/Init 返回 LL_OK(波特率真的写进硬件了)
 *                  bit16..bit23 = **当前 BRR 的整数分频**(硬件实际用的值), 用于反查真实波特率:
 *                     26(0x1A) -> 460800   48(0x30) -> 256000   108(0x6C) -> 115200
 *                     20(0x14) -> 9600(DIV64)                  其余档按公式反推
 *                  读法举例: 0x1A0701 = 460800 档收到字节 + 解出帧 + 现在还在通 + 硬件确认写入
 *                            0x300301 = 记录是 460800 但硬件 BRR 整数是 48 = **实际还在 256000**(写没生效)
 *                            0x00     = 8 档全程零字节 -> 模块没发/接线/卡配置态(先断电重启模块)
 * ================================================================================ */
volatile uint32_t          g_radar_lock;
volatile uint32_t          g_radar_baud;
volatile uint32_t          g_radar_comm;
static uint32_t            s_rep_last_ms;   /* 最近一次解出合法上报帧的时刻 */
static uint32_t            s_probe_rx0;     /* 进入当前候选档时的累计接收字节数 */
static uint32_t            s_comm_map;      /* 各候选档是否收到过字节 -> g_radar_comm 的 bit0..7 */
static uint32_t            s_fps_ms;        /* 帧率统计窗口起点 */
static uint32_t            s_fps_cnt;       /* 本窗口内收到的合法上报帧数 */
static uint32_t            s_fps;           /* 上一秒的帧率(Hz) */
static uint8_t          s_presence_src = RADAR_SRC_OUT;
static uint8_t          s_probe_st = 0U;      /* 0=待启动 1=已切波特率待发 2=等 ACK 3=收尾 9=结束 */
static uint8_t          s_probe_idx;
static uint32_t         s_probe_t0;
static uint32_t         s_rep_frames;      /* 本档收到的上报帧数(换档清零) */
static uint32_t         s_ack_frames;      /* 本档收到的 ACK 帧数(换档清零) */
#if (RADAR_BAUD_TARGET != 0UL)
static uint8_t          s_prov_st;          /* 产线配置状态: 0 待做 1 写入中 2 等重启 3 自检中 4 成功 5 失败 6 无法配置 */
static uint32_t         s_prov_t0;
static uint8_t          s_prov_busy;        /* 1 = 正在执行阻塞式命令(防重入兜底) */
#endif

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
                if (radar_proto_parse_ack(&f, &s_ack) != 0)
                {
                    s_ack_frames++;
                    s_ack_ready = 1U;
                }
            }
            else if (f.kind == RADAR_FRAME_KIND_REPORT)
            {
                if (radar_proto_parse_report(&f, &s_dev[0].rep) != 0)
                {
                    s_rep_frames++;
                    s_dev[0].last_rx_ms = m_u32Tickms;
                    s_rep_last_ms = m_u32Tickms;
                    s_fps_cnt++;
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

#if (RADAR_BAUD_INIT_FIXED != 0UL)
    /* 调试: 上电即固定波特率 -> 跳过自适应, 也不写模块波特率 */
    s_probe_st    = 9U;
    s_baud_locked = 1U;
#if (RADAR_BAUD_TARGET != 0UL)
    s_prov_st     = 4U;              /* 视为产线配置已完成: 不写模块 */
#endif
    s_rep_frames  = 0U;
    s_ack_frames  = 0U;
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
    if (radar_port_rx_bytes() != s_probe_rx0) { s_comm_map |= (1UL << s_probe_idx); }
    s_ack_ready  = 0U;
    s_probe_idx++;

    if (s_probe_idx >= (uint8_t)RADAR_BAUD_TABLE_CNT)
    {
        radar_port_set_baud(RADAR_BAUD_FALLBACK);
        s_probe_rx0 = radar_port_rx_bytes();
        radar_frame_init(&s_rx);
        s_rep_frames = 0U;
        s_ack_frames = 0U;
        s_probe_st   = 9U;
        return;
    }

    radar_port_set_baud(radar_probe_baud(s_probe_idx));
    s_probe_rx0 = radar_port_rx_bytes();
    radar_frame_init(&s_rx);
    s_rep_frames = 0U;
    s_ack_frames = 0U;
    s_probe_t0   = m_u32Tickms;
    s_probe_st   = 1U;
}

/* 认定本档为模块真实波特率 */
static void radar_probe_accept(void)
{
    if (radar_port_rx_bytes() != s_probe_rx0) { s_comm_map |= (1UL << s_probe_idx); }
    s_baud_locked = 1U;
    s_probe_st    = 9U;
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
/* 波特率自适应 —— **纯监听, 一个字节都不发**:
 *   每切一档波特率, 只听模块周期性主动上报的帧(F4F3F2F1..F8F7F6F5), 收到 RADAR_BAUD_LOCK_FRAMES
 *   个合法帧就认定该档; RADAR_PROBE_LISTEN_MS 内听不到就换下一档。
 *
 * 为什么不再发"使能配置 0x00FF / 结束配置 0x00FE":
 *   0x00FF 会让模块**进入配置态并停止上报**, 只要 0x00FE 晚发/丢失/被拒, 模块就"哑"了 ——
 *   现场已经踩过这个坑, 明确要求探测阶段**绝不发这两个包**。
 * 参数读写等命令仍按协议包"使能配置->命令->结束配置", 但那是**显式调用**时才发生,
 * 上电自动流程一条命令都不发。 */
static void radar_probe_tick(void)
{
    if (s_probe_st >= 9U) { return; }        /* 已结束 */

    switch (s_probe_st)
    {
        case 0U:                             /* 等模块启动完成, 再试第 1 档 */
            if ((m_u32Tickms - s_probe_t0) >= RADAR_PROBE_BOOT_MS)
            {
                s_probe_idx  = 0U;
                s_rep_frames = 0U;
                s_ack_frames = 0U;
                radar_port_set_baud(radar_probe_baud(0U));
                s_probe_rx0 = radar_port_rx_bytes();
                radar_frame_init(&s_rx);
                s_probe_t0 = m_u32Tickms;
                s_probe_st = 1U;
            }
            break;

        case 1U:                             /* 只听本档 */
            if (s_rep_frames >= (uint32_t)RADAR_BAUD_LOCK_FRAMES)
            {
                radar_probe_accept();
            }
            else if ((m_u32Tickms - s_probe_t0) >= RADAR_PROBE_LISTEN_MS)
            {
                radar_probe_next();
            }
            else
            {
                /* 继续听 */
            }
            break;

        default:
            s_probe_st = 9U;
            break;
    }
}

#if ((RADAR_PARAM_EN != 0U) || (RADAR_DUMP_ONCE != 0U))
/* 命令通道是否可用: 自适应探测已结束, 且产线配置(若有)已跑完 */
static uint8_t radar_link_ready(void)
{
#if (RADAR_BAUD_TARGET != 0UL)
    if (s_prov_st < 4U) { return 0U; }
#endif
    return radar_ready();
}
#endif

/* ------------------------------ A/C: 参数配置 · 只读维护 ------------------------------ */
/* 前向声明: 底层泵与命令事务(定义在本文件后部) */
static void radar_pump(void);
/* 前向声明: radar_cfg_cmd 定义在文件后部(命令事务: 使能配置->命令->结束配置) */
static int32_t radar_cfg_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                             radar_ack_t *ack, uint32_t timeout_ms);
/* 只读信息汇总(Keil Watch 里看 s_dump) */
static radar_dump_t     s_dump;
#if (RADAR_PARAM_EN != 0U)
static uint8_t          s_param_st;
static uint8_t          s_param_idx;
static uint8_t          s_param_done;
#endif
#if (RADAR_DUMP_ONCE != 0U)
static uint8_t          s_dump_done;
#endif

#if (RADAR_PARAM_EN != 0U)
static const uint8_t s_param_move_sens[RADAR_GATE_MAX + 1U]  = RADAR_PARAM_MOVE_SENS;
static const uint8_t s_param_still_sens[RADAR_GATE_MAX + 1U] = RADAR_PARAM_STILL_SENS;
#endif

const radar_dump_t *radar_dump(void)
{
    return &s_dump;
}

/* ------------------------------ A. 探测行为参数 ------------------------------ */
/* 光感辅助控制(0x00AD): mode 0=关闭 / 1=光感<阈值 / 2=光感>阈值; out_level 0=默认低(有人=高) */
int32_t radar_set_aux_control(uint8_t mode, uint8_t threshold, uint8_t out_level)
{
    uint8_t v[4];

    if (mode > 2U) { return LL_ERR_INVD_PARAM; }
    if (out_level > 1U) { return LL_ERR_INVD_PARAM; }

    v[0] = mode;
    v[1] = threshold;
    v[2] = out_level;
    v[3] = 0x00U;

    return radar_cfg_cmd(RADAR_CMD_AUX_SET, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

/* 把 A 组参数与目标值比对: 需要写返回 1, 否则 0 */
#if (RADAR_PARAM_EN != 0U)
static uint8_t radar_param_diff(const radar_params_t *p, const radar_aux_t *a, uint8_t *which)
{
    uint8_t i;

    *which = 0U;                                /* 0 = 无差异 */

    if ((p->max_move_gate != (uint8_t)RADAR_PARAM_MAX_MOVE_GATE) ||
        (p->max_still_gate != (uint8_t)RADAR_PARAM_MAX_STILL_GATE) ||
        (p->no_body_sec != (uint16_t)RADAR_PARAM_NO_BODY_SEC))
    {
        *which = 1U;                            /* 需要写 0x0060 */
        return 1U;
    }

    for (i = 0U; i <= (uint8_t)RADAR_GATE_MAX; i++)
    {
        if (p->move_sens[i] != s_param_move_sens[i]) { *which = 2U; s_param_idx = i; return 1U; }
        if (i >= 2U)                            /* 门 0/1 的静止灵敏度不可设置 */
        {
            if (p->still_sens[i] != s_param_still_sens[i]) { *which = 2U; s_param_idx = i; return 1U; }
        }
    }

    if ((a->mode != (uint8_t)RADAR_PARAM_AUX_MODE) ||
        (a->threshold != (uint8_t)RADAR_PARAM_AUX_THRESHOLD) ||
        (a->out_level != (uint8_t)RADAR_PARAM_AUX_OUT_LEVEL))
    {
        *which = 3U;                            /* 需要写 0x00AD */
        return 1U;
    }

    return 0U;
}

/* 幂等应用流程: 0 等链路 -> 1 读回 -> 2 逐项写 -> 3 复检 -> 4 成功/本来就一致, 5 失败 */
static void radar_param_tick(void)
{
    int32_t ret;
    uint8_t which;

    if (s_param_st >= 4U) { return; }
    if (radar_link_ready() == 0U) { return; }

    switch (s_param_st)
    {
        case 0U:
            s_param_st = 1U;
            break;

        case 1U:                                /* 读回当前配置 */
            ret = radar_read_params(&s_dump.params);
            if (ret != LL_OK) { s_param_st = 5U; s_dump.last_ret = ret; break; }

            ret = radar_read_aux_control(&s_dump.aux);
            if (ret != LL_OK) { s_param_st = 5U; s_dump.last_ret = ret; break; }

            if (radar_param_diff(&s_dump.params, &s_dump.aux, &which) == 0U)
            {
                s_param_st = 4U;                /* 已经一致: 一条命令都不发 */
                break;
            }
            s_param_st = 2U;
            break;

        case 2U:                                /* 逐项写(每拍只写一条命令, 不长时间占住主循环) */
            if (s_param_done == 0U)
            {
                ret = radar_read_params(&s_dump.params);
                if (ret == LL_OK)
                {
                    ret = radar_read_aux_control(&s_dump.aux);
                }
                if (ret != LL_OK) { s_param_st = 5U; s_dump.last_ret = ret; break; }

                if (radar_param_diff(&s_dump.params, &s_dump.aux, &which) == 0U)
                {
                    s_param_st = 3U;            /* 都写完了 -> 复检 */
                    break;
                }

                if (which == 1U)
                {
                    ret = radar_set_max_gate((uint16_t)RADAR_PARAM_MAX_MOVE_GATE,
                                             (uint16_t)RADAR_PARAM_MAX_STILL_GATE,
                                             (uint16_t)RADAR_PARAM_NO_BODY_SEC);
                }
                else if (which == 2U)
                {
                    ret = radar_set_sensitivity((uint16_t)s_param_idx,
                                                (uint16_t)s_param_move_sens[s_param_idx],
                                                (uint16_t)s_param_still_sens[s_param_idx]);
                }
                else
                {
                    ret = radar_set_aux_control((uint8_t)RADAR_PARAM_AUX_MODE,
                                                (uint8_t)RADAR_PARAM_AUX_THRESHOLD,
                                                (uint8_t)RADAR_PARAM_AUX_OUT_LEVEL);
                }

                s_dump.last_ret = ret;
                if (ret != LL_OK) { s_param_st = 5U; }
            }
            break;

        case 3U:                                /* 复检: 再读回一遍 */
            ret = radar_read_params(&s_dump.params);
            if (ret == LL_OK) { ret = radar_read_aux_control(&s_dump.aux); }
            s_dump.last_ret = ret;
            if (ret != LL_OK) { s_param_st = 5U; break; }

            if (radar_param_diff(&s_dump.params, &s_dump.aux, &which) == 0U) { s_param_st = 4U; }
            else                                                            { s_param_st = 5U; }
            break;

        default:
            s_param_st = 5U;
            break;
    }
}

uint8_t radar_param_state(void)
{
    return s_param_st;
}
#else
uint8_t radar_param_state(void)
{
    return 0U;                                  /* 未启用参数自动配置 */
}
#endif

/* ------------------------------ C. 只读 / 维护 ------------------------------ */
int32_t radar_read_resolution(uint8_t *idx)
{
    radar_ack_t ack;
    uint16_t    v;
    int32_t     ret;

    if (idx == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(RADAR_CMD_RESOLUTION_GET, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }
    if (radar_proto_parse_u16(&ack, &v) == 0) { return LL_ERR; }

    *idx = (uint8_t)(v & 0x00FFU);
    return LL_OK;
}

int32_t radar_read_aux_control(radar_aux_t *out)
{
    radar_ack_t ack;
    int32_t     ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(RADAR_CMD_AUX_GET, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_aux(&ack, out) != 0) ? LL_OK : LL_ERR;
}

int32_t radar_read_fw_version(radar_fw_t *out)
{
    radar_ack_t ack;
    int32_t     ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(RADAR_CMD_FW_VERSION, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_fw(&ack, out) != 0) ? LL_OK : LL_ERR;
}

int32_t radar_read_mac(uint8_t *mac, uint8_t *len)
{
    radar_ack_t ack;
    uint8_t     v[2];
    int32_t     ret;
    uint8_t     got = 0U;

    if (mac == 0) { return LL_ERR_INVD_PARAM; }

    v[0] = 0x01U; v[1] = 0x00U;                 /* 命令值 0x0001 */
    ret = radar_cfg_cmd(RADAR_CMD_MAC, v, sizeof(v), &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }
    if (radar_proto_parse_bytes(&ack, mac, 6U, &got) == 0) { return LL_ERR; }

    if (len != 0) { *len = got; }
    return LL_OK;
}

int32_t radar_factory_reset(void)
{
    return radar_cfg_cmd(RADAR_CMD_FACTORY_RESET, 0, 0U, 0, RADAR_CMD_TIMEOUT_MS);
}

/* 延时(期间继续泵字节, 不干等) */
static void radar_delay_ms(uint32_t ms)
{
    uint32_t t0 = m_u32Tickms;

    while ((m_u32Tickms - t0) < ms) { radar_pump(); }
}

/* 单项读取(带重试): item 0 参数 / 1 分辨率 / 2 辅助控制 / 3 固件版本 / 4 MAC */
static int32_t radar_read_one(uint8_t item)
{
    int32_t ret = LL_ERR;
    uint8_t n;

    for (n = 0U; n < (uint8_t)RADAR_READ_TRY; n++)
    {
        switch (item)
        {
            case 0U: ret = radar_read_params(&s_dump.params);           break;
            case 1U: ret = radar_read_resolution(&s_dump.resolution);   break;
            case 2U: ret = radar_read_aux_control(&s_dump.aux);         break;
            case 3U: ret = radar_read_fw_version(&s_dump.fw);           break;
            default: ret = radar_read_mac(s_dump.mac, &s_dump.mac_len); break;
        }

        if (ret == LL_OK) { return ret; }

        radar_delay_ms((uint32_t)RADAR_READ_GAP_MS);
    }

    return ret;
}

/* 依次读回所有只读信息到 s_dump(每项带重试并逐项记录返回码, 结果看 Keil Watch 的 s_dump) */
int32_t radar_read_all(void)
{
    uint8_t n = 0U;

    s_dump.ret_params = radar_read_one(0U);  radar_delay_ms((uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_res    = radar_read_one(1U);  radar_delay_ms((uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_aux    = radar_read_one(2U);  radar_delay_ms((uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_fw     = radar_read_one(3U);  radar_delay_ms((uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_mac    = radar_read_one(4U);

    if (s_dump.ret_params == LL_OK) { n++; }
    if (s_dump.ret_res    == LL_OK) { n++; }
    if (s_dump.ret_aux    == LL_OK) { n++; }
    if (s_dump.ret_fw     == LL_OK) { n++; }
    if (s_dump.ret_mac    == LL_OK) { n++; }

    s_dump.ok       = (uint32_t)n;                  /* 5 = 全部成功 */
    s_dump.last_ret = s_dump.ret_mac;               /* 最后一项(兼容旧用法) */

    return s_dump.last_ret;
}

/* 上电后把只读信息读回一次(供 Keil Watch 查看); 也可在调试器里手动调 radar_read_all() */
#if (RADAR_DUMP_ONCE != 0U)
static void radar_dump_tick(void)
{
#if (RADAR_PARAM_EN != 0U)
    if (s_param_st < 4U) { return; }            /* 等参数配置先做完 */
#endif
    if (s_dump_done != 0U) { return; }
    if (radar_link_ready() == 0U) { return; }

    s_dump_done = 1U;
    (void)radar_read_all();
}
#else
static void radar_dump_tick(void)
{
}
#endif

/* ------------------------------ 产线配置: 模块波特率 ------------------------------ */
#if (RADAR_BAUD_TARGET != 0UL)
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

/* 把模块波特率配置成 RADAR_BAUD_TARGET(掉电保存), 幂等:
 *   0 等探测结束 -> 若已是目标值则直接结束(什么都不发)
 *   1 写配置(0x00A1) -> 重启模块(0x00A3, 配置重启后生效) -> 等 RADAR_PROV_RESTART_MS
 *   2 等模块重启(RADAR_PROV_RESTART_MS)后**重新跑一遍自适应探测**
 *   3 复检: 探测锁定在目标波特率 = 成功; 锁定在别的值 = 模块没切成, 保持该波特率继续用
 * 结果见 radar_provision_state() */
static void radar_provision_tick(void)
{
    if (s_prov_busy != 0U) { return; }       /* 阻塞命令执行中(兜底, 正常路径不会进) */
    if (s_prov_st >= 4U) { return; }         /* 已结束 */

    switch (s_prov_st)
    {
        case 0U:                             /* 等自适应探测结束 */
            if ((radar_ready() == 0U) || (radar_baud_locked() == 0U)) { break; }
            if (radar_get_baud() == RADAR_BAUD_TARGET)
            {
                s_prov_st = 4U;              /* 已经是目标值: 幂等, 不发任何命令 */
                break;
            }
            if (radar_baud_to_index(RADAR_BAUD_TARGET) == 0U)
            {
                s_prov_st = 6U;              /* 目标值不在协议表 6 里 */
                break;
            }
            s_prov_st = 1U;
            break;

        case 1U:                             /* 写配置 + 重启模块(下面两步是阻塞的, 见 radar_cmd) */
            s_prov_busy = 1U;

            if (radar_set_uart_baud_index(radar_baud_to_index(RADAR_BAUD_TARGET)) != LL_OK)
            {
                s_prov_st = 5U;
            }
            else if (radar_restart() != LL_OK)
            {
                s_prov_st = 5U;
            }
            else
            {
                s_prov_t0 = m_u32Tickms;
                s_prov_st = 2U;
            }

            s_prov_busy = 0U;
            break;

        case 2U:                             /* 等模块重启, 然后重新跑一遍自适应探测来复检 */
            if ((m_u32Tickms - s_prov_t0) >= RADAR_PROV_RESTART_MS)
            {
                s_probe_st    = 0U;          /* 重新武装探测: 状态 0 里自带 RADAR_PROBE_BOOT_MS 启动延时 */
                s_probe_t0    = m_u32Tickms;
                s_probe_idx   = 0U;
                s_rep_frames  = 0U;
                s_ack_frames  = 0U;
                s_baud_locked = 0U;
                s_prov_st     = 3U;
            }
            break;

        case 3U:                             /* 复检: 探测锁定在目标波特率 = 成功 */
            if (radar_ready() == 0U) { break; }

            if ((radar_baud_locked() != 0U) && (radar_get_baud() == RADAR_BAUD_TARGET))
            {
                s_prov_st = 4U;
            }
            else
            {
                /* 模块没切成(或写配置无效): 保持探测找到的波特率继续工作, 不影响业务 */
                s_prov_st = 5U;
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

/* 底层泵: 只做"分帧超时 + 搬运收到的字节", **不跑探测/产线配置状态机**。
 * 阻塞式命令(radar_cmd)在等 ACK 期间必须调它而不是 radar_poll():
 * 否则 radar_cmd -> radar_poll -> radar_provision_tick -> radar_cmd 会无限递归。 */
static void radar_pump(void)
{
    radar_frame_tick(&s_rx, m_u32Tickms);
    radar_port_poll();
    radar_port_tx_watchdog(m_u32Tickms);     /* 发送完成中断没来时的兜底 */
}

void radar_poll(void)
{
    radar_pump();
    radar_probe_tick();
#if (RADAR_BAUD_TARGET != 0UL)
    radar_provision_tick();              /* 产线配置: 把模块波特率配成 RADAR_BAUD_TARGET */
#endif
#if (RADAR_PARAM_EN != 0UL)
    radar_param_tick();                  /* A: 把探测行为参数写成目标值(幂等) */
#endif
    radar_dump_tick();                   /* C: 上电读回一次只读信息到 s_dump */

    /* 与串口同一模块的 OUT 脚(可选判定源) */
    s_dev[0].out_present = (GPIO_ReadInputPins(RADAR_UART_DEV_OUT_PORT, RADAR_UART_DEV_OUT_PIN) == PIN_SET) ? 1U : 0U;
    s_dev[0].uart_online = ((m_u32Tickms - s_dev[0].last_rx_ms) <= RADAR_REPORT_STALE_MS) ? 1U : 0U;

    /* 现场只看这 3 个单值(定义见文件头) */
    g_radar_lock = (uint32_t)s_baud_locked;
    g_radar_baud = radar_port_baud_actual();   /* 只信硬件实际值 */
    /* 现场只看这 3 个单值(定义与读法见文件头) */
    g_radar_lock = (uint32_t)s_baud_locked;
    g_radar_baud = radar_port_baud_actual();   /* 只信硬件实际值 */
    g_radar_comm = s_comm_map;
    if (s_reports != 0U)                        { g_radar_comm |= 0x100UL; }
    if ((m_u32Tickms - s_rep_last_ms) <= 1000U) { g_radar_comm |= 0x200UL; }
    if (radar_port_baud_ok() != 0U)             { g_radar_comm |= 0x400UL; }
    if ((m_u32Tickms - s_fps_ms) >= 1000U)                      /* 每秒结算一次帧率 */
    {
        s_fps     = s_fps_cnt;
        s_fps_cnt = 0U;
        s_fps_ms  = m_u32Tickms;
    }
    g_radar_comm |= ((radar_port_brr() >> 8) & 0xFFUL) << 16;   /* BRR 整数分频指纹 */
    g_radar_comm |= (s_fps & 0xFFUL) << 24;                     /* 最近 1 秒的合法帧数 = 帧率 */
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
        radar_pump();
        if ((m_u32Tickms - t0) > 50U) { return LL_ERR_BUSY; }
    }

    s_ack_ready = 0U;
    if (radar_port_write(frame, flen) != LL_OK) { return LL_ERR; }

    t0 = m_u32Tickms;
    while ((m_u32Tickms - t0) < timeout_ms)
    {
        radar_pump();

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
