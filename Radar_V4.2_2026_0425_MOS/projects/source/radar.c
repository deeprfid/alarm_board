/*******************************************************************************
 * radar.c -- LD2410C 雷达服务层实现
 *
 * 第 3 步(多口): radar.c 每口一份 + 按口循环。
 *   - 运行时状态(分帧器/ACK 事务/探测/链路监控/帧率/统计)**全部按口分开**;
 *   - 每个雷达口一个接收跳板 radar_rx_cb0/1/2 -> radar_on_bytes(port, ...);
 *   - radar_poll() 里 for (p = 0; p < RADAR_PORT_CNT; p++) 逐口推进;
 *   - 现场 Watch 仍是 3 个变量, 但变成 [RADAR_PORT_CNT] 数组(一口一个下标)。
 * 硬约束: 设备号 == 口号(s_dev[]/radar_dev()/radar_report() 用设备号, 内部用口号)。
 ******************************************************************************/
#include "radar.h"
#include "radar_port.h"       /* 硬件层: init/write/tx_busy/set_baud/poll */
#include <string.h>           /* memset */

/* 1ms 计数(定义在 bsp_trng.c, 由 bsp_exint.c 的 SysTick_Handler 递增) */
extern uint32_t m_u32Tickms;

#if (RADAR_DEV_CNT != RADAR_PORT_CNT)
#error "RADAR_DEV_CNT must equal RADAR_PORT_CNT (device index == port index)"
#endif
#if (RADAR_PORT_CNT != 3U)
#error "radar_rx_cb0/1/2 only cover 3 ports; add trampolines when RADAR_PORT_CNT changes"
#endif

/* ==================== 每口一份的运行时状态(第 3 步: 全部 [RADAR_PORT_CNT]) ==================== */
static radar_frame_rx_t s_rx[RADAR_PORT_CNT];               /* 分帧器(每口独立, 不跨口拼接) */
static radar_dev_t      s_dev[RADAR_DEV_CNT];               /* 设备状态(设备号 == 口号) */
static radar_ack_t      s_ack[RADAR_PORT_CNT];              /* 最近一条 ACK */
static volatile uint8_t s_ack_ready[RADAR_PORT_CNT];        /* ACK 就绪标志(命令事务用) */
static uint32_t         s_reports[RADAR_PORT_CNT];          /* 累计合法上报帧数 */
static uint8_t          s_baud_locked[RADAR_PORT_CNT];      /* 该口波特率是否已锁定 */

/* ============================ 现场 Watch 只用这 3 个数组 ============================
 * 本板没有调试串口, 在 Watch 里加一堆变量抄数字的做法已经废弃(现场结论: 没法调试)。
 * 只保留下面 3 个, 且**不许再加第 4 个** —— 需要更多信息就重新定义取值, 不要新增变量。
 * 第 3 步起三路雷达各跑各的, 所以这 3 个由单值变成 [RADAR_PORT_CNT] 数组:
 * 下标 0/1/2 = 雷达口 0/1/2(USART1/USART2/USART3)。位图/BRR 指纹/帧率编码与单口版**完全一样**,
 * 只是每个口各有一份, 现场在 Watch 里展开数组逐口看即可。
 *
 *   g_radar_lock[p] : 1 = 已锁定模块波特率(探测成功); 0 = 没锁住(8 档全试完仍失败)
 *   g_radar_baud[p] : 当前波特率(软件记录的档位; 是否真写进硬件见 g_radar_comm[p] 的 0x400)
 *   g_radar_comm[p] : 位图 + 标志位 + **BRR 整数分频指纹**, 一个数即可定位:
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
volatile uint32_t          g_radar_lock[RADAR_PORT_CNT];
volatile uint32_t          g_radar_baud[RADAR_PORT_CNT];
volatile uint32_t          g_radar_comm[RADAR_PORT_CNT];
static uint32_t            s_rep_last_ms[RADAR_PORT_CNT];   /* 最近一次解出合法上报帧的时刻 */
static uint32_t            s_probe_rx0[RADAR_PORT_CNT];     /* 进入当前候选档时的累计接收字节数 */
static uint32_t            s_comm_map[RADAR_PORT_CNT];      /* 各候选档是否收到过字节 -> g_radar_comm 的 bit0..7 */
static uint32_t            s_link_ms[RADAR_PORT_CNT];       /* 链路监控窗口起点 */
static uint32_t            s_link_bytes0[RADAR_PORT_CNT];   /* 窗口起点的累计接收字节数 */
static uint32_t            s_win_ack_cnt[RADAR_PORT_CNT];   /* 本窗口解出的 ACK 帧数(与上报帧一起算合法帧) */
static uint32_t            s_link_last_rx_ms[RADAR_PORT_CNT];   /* 最近一次收到字节的时刻 */
static uint32_t            s_link_sweep_ms[RADAR_PORT_CNT];     /* 上次兜底重扫的时刻 */
static uint32_t            s_fps_ms[RADAR_PORT_CNT];        /* 帧率统计窗口起点 */
static uint32_t            s_fps_cnt[RADAR_PORT_CNT];       /* 本窗口内收到的合法上报帧数 */
static uint32_t            s_fps[RADAR_PORT_CNT];           /* 上一秒的帧率(Hz) */
static uint8_t             s_presence_src[RADAR_PORT_CNT];  /* 每口的"有人"判定来源 */
static uint8_t             s_probe_st[RADAR_PORT_CNT];      /* 0=待启动 1=已切波特率待听 9=结束 */
static uint8_t             s_probe_idx[RADAR_PORT_CNT];
static uint32_t            s_probe_t0[RADAR_PORT_CNT];
static uint32_t            s_rep_frames[RADAR_PORT_CNT];    /* 本档收到的上报帧数(换档清零) */
static uint32_t            s_ack_frames[RADAR_PORT_CNT];    /* 本档收到的 ACK 帧数(换档清零) */
#if (RADAR_BAUD_TARGET != 0UL)
static uint8_t             s_prov_st[RADAR_PORT_CNT];       /* 产线配置状态: 0 待做 1 写入中 2 等重启 3 自检中 4 成功 5 失败 6 无法配置 */
static uint32_t            s_prov_t0[RADAR_PORT_CNT];
static uint8_t             s_prov_busy[RADAR_PORT_CNT];     /* 1 = 正在执行阻塞式命令(防重入兜底) */
#endif

/* 各口 OUT 脚(与串口同模块, 供串口/OUT 双路判定)。
 * 口 0 沿用 radar_cfg.h 的 RADAR_UART_DEV_OUT_PORT/PIN(= RADAR_PORT0/PIN0, 与旧单口行为逐位一致);
 * 口 1/口 2 用 RADAR_PORT1/PIN1、RADAR_PORT2/PIN2 —— 与 bsp_report.c / common.c / bsp_gpio.c
 * 里读三路 OUT(PC14/PC13/PH2)的写法一致。 */
static const uint8_t  s_out_port[RADAR_PORT_CNT] = { RADAR_PORT0, RADAR_PORT1, RADAR_PORT2 };
static const uint16_t s_out_pin[RADAR_PORT_CNT]  = { RADAR_PIN0,  RADAR_PIN1,  RADAR_PIN2  };

/* ------------------------------ 前置声明(定义在本文件后部) ------------------------------ */
static void    radar_pump(uint8_t port);
static void    radar_switch_baud(uint8_t port, uint32_t baud);
static int32_t radar_cfg_cmd(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len,
                             radar_ack_t *ack, uint32_t timeout_ms);
static uint8_t radar_probe_done(uint8_t port);

/* ------------------------------ 收字节 -> 分帧 -> 解析(按口) ------------------------------ */

static void radar_on_bytes(uint8_t port, const uint8_t *data, uint16_t len)
{
    radar_frame_t f;
    uint16_t i;

    if (port >= (uint8_t)RADAR_PORT_CNT) { return; }

    for (i = 0U; i < len; i++)
    {
        if (radar_frame_feed(&s_rx[port], data[i], m_u32Tickms, &f) != 0)
        {
            if (f.kind == RADAR_FRAME_KIND_ACK)
            {
                if (radar_proto_parse_ack(&f, &s_ack[port]) != 0)
                {
                    s_ack_frames[port]++;
                    s_win_ack_cnt[port]++;
                    s_ack_ready[port] = 1U;
                }
            }
            else if (f.kind == RADAR_FRAME_KIND_REPORT)
            {
                if (radar_proto_parse_report(&f, &s_dev[port].rep) != 0)
                {
                    s_rep_frames[port]++;
                    s_dev[port].last_rx_ms = m_u32Tickms;
                    s_rep_last_ms[port] = m_u32Tickms;
                    s_fps_cnt[port]++;
                    s_reports[port]++;
                }
            }
        }
    }
}

/* 接收跳板: radar_port 的接收回调按口登记, 但签名只带 data/len、不带口号,
 * 所以每口一个跳板把口号补上(回调签名见 radar_port.h 的 radar_port_set_rx_handler)。 */
static void radar_rx_cb0(const uint8_t *data, uint16_t len) { radar_on_bytes(0U, data, len); }
static void radar_rx_cb1(const uint8_t *data, uint16_t len) { radar_on_bytes(1U, data, len); }
static void radar_rx_cb2(const uint8_t *data, uint16_t len) { radar_on_bytes(2U, data, len); }

static void (*const s_rx_cb_tab[RADAR_PORT_CNT])(const uint8_t *data, uint16_t len) = {
    radar_rx_cb0, radar_rx_cb1, radar_rx_cb2
};

/* ------------------------------ 生命周期 ------------------------------ */
int32_t radar_init(void)
{
    uint8_t p;

    memset(s_dev, 0, sizeof(s_dev));
    memset(s_ack, 0, sizeof(s_ack));

    for (p = 0U; p < (uint8_t)RADAR_PORT_CNT; p++)
    {
        radar_frame_init(&s_rx[p]);
        s_ack_ready[p]    = 0U;
        s_reports[p]      = 0U;
        s_baud_locked[p]  = 0U;
        s_comm_map[p]     = 0U;
        s_rep_last_ms[p]  = 0U;
        s_probe_rx0[p]    = 0U;
        s_presence_src[p] = RADAR_SRC_OUT;

        s_link_ms[p]         = m_u32Tickms;     /* 链路监控窗口起点 */
        s_link_bytes0[p]     = 0U;
        s_win_ack_cnt[p]     = 0U;
        s_link_last_rx_ms[p] = m_u32Tickms;
        s_link_sweep_ms[p]   = m_u32Tickms;     /* 上电先允许一次兜底扫, 不影响正常锁定 */
        s_fps_ms[p]          = m_u32Tickms;
        s_fps_cnt[p]         = 0U;
        s_fps[p]             = 0U;

        s_dev[p].out_present = 0U;
        s_dev[p].uart_online = 0U;

#if (RADAR_BAUD_INIT_FIXED != 0UL)
        /* 调试: 上电即固定波特率 -> 跳过自适应, 也不写模块波特率 */
        s_probe_st[p]    = 9U;
        s_baud_locked[p] = 1U;
#if (RADAR_BAUD_TARGET != 0UL)
        s_prov_st[p]     = 4U;              /* 视为产线配置已完成: 不写模块 */
#endif
        s_rep_frames[p]  = 0U;
        s_ack_frames[p]  = 0U;
#else
        s_probe_st[p]   = 0U;               /* 波特率自适应由 radar_poll() 推进(非阻塞) */
        s_probe_t0[p]   = m_u32Tickms;      /* 启动延时基准: 等模块上电启动完成再探测 */
        s_rep_frames[p] = 0U;
        s_ack_frames[p] = 0U;
#endif

        /* 硬件层按口初始化。
         * 注意: radar_port_init() 内部会把该口的接收回调清零, 所以登记回调必须在它之后。 */
        (void)radar_port_init(p);
        radar_port_set_rx_handler(p, s_rx_cb_tab[p]);
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
static void radar_probe_next(uint8_t port)
{
    if (radar_port_rx_bytes(port) != s_probe_rx0[port]) { s_comm_map[port] |= (1UL << s_probe_idx[port]); }
    s_ack_ready[port]  = 0U;
    s_probe_idx[port]++;

    if (s_probe_idx[port] >= (uint8_t)RADAR_BAUD_TABLE_CNT)
    {
        radar_switch_baud(port, RADAR_BAUD_FALLBACK);
        s_probe_rx0[port] = radar_port_rx_bytes(port);
        s_probe_t0[port]  = m_u32Tickms;   /* 重扫计时起点 */
        s_probe_st[port]  = 9U;
        return;
    }

    radar_switch_baud(port, radar_probe_baud(s_probe_idx[port]));
    s_probe_rx0[port] = radar_port_rx_bytes(port);
    s_probe_t0[port]  = m_u32Tickms;
    s_probe_st[port]  = 1U;
}

/* 认定本档为模块真实波特率 */
static void radar_probe_accept(uint8_t port)
{
    if (radar_port_rx_bytes(port) != s_probe_rx0[port]) { s_comm_map[port] |= (1UL << s_probe_idx[port]); }
    s_baud_locked[port] = 1U;
    s_probe_st[port]    = 9U;
}

/* 波特率自适应 —— **纯监听, 一个字节都不发**:
 *   每切一档波特率, 只听模块周期性主动上报的帧(F4F3F2F1..F8F7F6F5), 收到 RADAR_BAUD_LOCK_FRAMES
 *   个合法帧就认定该档; RADAR_PROBE_LISTEN_MS 内听不到就换下一档。
 *
 * 为什么不再发"使能配置 0x00FF / 结束配置 0x00FE":
 *   0x00FF 会让模块**进入配置态并停止上报**, 只要 0x00FE 晚发/丢失/被拒, 模块就"哑"了 ——
 *   现场已经踩过这个坑, 明确要求探测阶段**绝不发这两个包**。
 * 参数读写等命令仍按协议包"使能配置->命令->结束配置", 但那是**显式调用**时才发生,
 * 上电自动流程一条命令都不发。 */
/* 统一换档入口: 换波特率必须和『复位分帧器 + 清接收计数』一起做 ——
 * 换档瞬间线上那一帧会被拆开(前半截在旧档、后半截在新档或直接丢失),
 * 若不复位分帧器/不清计数, 就可能把两段拼成一帧或让旧档的帧计入新档。
 * radar_port_set_baud(port, ) 内部已完成: 关收发 -> USART_DeInit -> 重新初始化
 *   -> BRR 回读校验 -> 清环形缓冲; 这里补齐上层状态。
 * 前置条件: 必须在 TX 空闲时调用(换档会复位 USART, 发送到一半会被打断)。 */
static void radar_switch_baud(uint8_t port, uint32_t baud)
{
    radar_port_set_baud(port, baud);
    radar_frame_init(&s_rx[port]);      /* 分帧器状态清零: 不跨波特率拼接 */
    s_rep_frames[port] = 0U;
    s_ack_frames[port] = 0U;
}

static void radar_probe_tick(uint8_t port)
{
    if (s_probe_st[port] >= 9U)              /* 探测已结束 */
    {
        /* 一整轮 8 档都没锁定 -> 隔 RADAR_PROBE_RETRY_MS 重新扫一轮。
         * 否则会一直停在 fallback 波特率(lock=0)直到断电重启 —— 模块比本板上电晚,
         * 或某一轮刚好错过上报时, 就再也追不上了。 */
        if ((s_baud_locked[port] == 0U) && ((m_u32Tickms - s_probe_t0[port]) >= RADAR_PROBE_RETRY_MS))
        {
            s_probe_st[port] = 0U;           /* 重新武装: 状态 0 自带启动延时 */
            s_probe_t0[port] = m_u32Tickms;
        }
        return;
    }

    switch (s_probe_st[port])
    {
        case 0U:                             /* 等模块启动完成, 再试第 1 档 */
            if ((m_u32Tickms - s_probe_t0[port]) >= RADAR_PROBE_BOOT_MS)
            {
                s_probe_idx[port] = 0U;
                radar_switch_baud(port, radar_probe_baud(0U));   /* 从头按候选表扫(第0档=256000) */
                s_probe_rx0[port] = radar_port_rx_bytes(port);
                s_probe_t0[port]  = m_u32Tickms;
                s_probe_st[port]  = 1U;
            }
            break;

        case 1U:                             /* 只听本档 */
            if (s_rep_frames[port] >= (uint32_t)RADAR_BAUD_LOCK_FRAMES)
            {
                radar_probe_accept(port);
            }
            else if ((m_u32Tickms - s_probe_t0[port]) >= RADAR_PROBE_LISTEN_MS)
            {
                radar_probe_next(port);
            }
            else
            {
                /* 继续听 */
            }
            break;

        default:
            s_probe_st[port] = 9U;
            break;
    }
}

/* ------------------------------ A/C: 参数配置 · 只读维护 ------------------------------ */
/* radar_link_ready 在"参数自动配置"或"上电读回"任一打开时都要有 —— 两者都关时整段不编译,
 * 否则 ARMCC 会报 #177-D(声明未使用)。 */
#if ((RADAR_PARAM_EN != 0U) || (RADAR_DUMP_ONCE != 0U))
/* 命令通道是否可用: 自适应探测已结束, 且产线配置(若有)已跑完 */
static uint8_t radar_link_ready(void)
{
#if (RADAR_BAUD_TARGET != 0UL)
    if (s_prov_st[0] < 4U) { return 0U; }
#endif
    return radar_ready();
}
#endif

/* 只读信息汇总(Keil Watch 里看 s_dump)。
 * 注意: 参数读写/只读回读/产线 dump 目前仍是**单实例**(走口 0), 不随雷达口分份 ——
 *       第 3 步只把"每口并行的运行态"拆开, 维护类功能按需再扩。 */
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

    return radar_cfg_cmd(0U, RADAR_CMD_AUX_SET, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
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

    ret = radar_cfg_cmd(0U, RADAR_CMD_RESOLUTION_GET, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
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

    ret = radar_cfg_cmd(0U, RADAR_CMD_AUX_GET, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_aux(&ack, out) != 0) ? LL_OK : LL_ERR;
}

int32_t radar_read_fw_version(radar_fw_t *out)
{
    radar_ack_t ack;
    int32_t     ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(0U, RADAR_CMD_FW_VERSION, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
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
    ret = radar_cfg_cmd(0U, RADAR_CMD_MAC, v, sizeof(v), &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }
    if (radar_proto_parse_bytes(&ack, mac, 6U, &got) == 0) { return LL_ERR; }

    if (len != 0) { *len = got; }
    return LL_OK;
}

int32_t radar_factory_reset(void)
{
    return radar_cfg_cmd(0U, RADAR_CMD_FACTORY_RESET, 0, 0U, 0, RADAR_CMD_TIMEOUT_MS);
}

/* 延时(期间继续泵该口的字节, 不干等) */
static void radar_delay_ms(uint8_t port, uint32_t ms)
{
    uint32_t t0 = m_u32Tickms;

    while ((m_u32Tickms - t0) < ms) { radar_pump(port); }
}

/* 单项读取(带重试): item 0 参数 / 1 分辨率 / 2 辅助控制 / 3 固件版本 / 4 MAC */
static int32_t radar_read_one(uint8_t port, uint8_t item)
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

        radar_delay_ms(port, (uint32_t)RADAR_READ_GAP_MS);
    }

    return ret;
}

/* 依次读回所有只读信息到 s_dump(每项带重试并逐项记录返回码, 结果看 Keil Watch 的 s_dump)。
 * 仍走口 0(单实例维护功能)。 */
int32_t radar_read_all(void)
{
    uint8_t n = 0U;

    s_dump.ret_params = radar_read_one(0U, 0U);  radar_delay_ms(0U, (uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_res    = radar_read_one(0U, 1U);  radar_delay_ms(0U, (uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_aux    = radar_read_one(0U, 2U);  radar_delay_ms(0U, (uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_fw     = radar_read_one(0U, 3U);  radar_delay_ms(0U, (uint32_t)RADAR_READ_GAP_MS);
    s_dump.ret_mac    = radar_read_one(0U, 4U);

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
 * 结果见 radar_provision_state()。第 3 步起三个口各跑各的(状态按口分开)。 */
static void radar_provision_tick(uint8_t port)
{
    if (s_prov_busy[port] != 0U) { return; }  /* 阻塞命令执行中(兜底, 正常路径不会进) */
    if (s_prov_st[port] >= 4U) { return; }    /* 已结束 */

    switch (s_prov_st[port])
    {
        case 0U:                             /* 等自适应探测结束 */
            if ((radar_probe_done(port) == 0U) || (s_baud_locked[port] == 0U)) { break; }
            if (radar_port_get_baud(port) == RADAR_BAUD_TARGET)
            {
                s_prov_st[port] = 4U;        /* 已经是目标值: 幂等, 不发任何命令 */
                break;
            }
            if (radar_baud_to_index(RADAR_BAUD_TARGET) == 0U)
            {
                s_prov_st[port] = 6U;        /* 目标值不在协议表 6 里 */
                break;
            }
            s_prov_st[port] = 1U;
            break;

        case 1U:                             /* 写配置 + 重启模块(下面两步是阻塞的, 见 radar_cmd_port) */
            s_prov_busy[port] = 1U;

            if (radar_set_uart_baud_index_port(port, radar_baud_to_index(RADAR_BAUD_TARGET)) != LL_OK)
            {
                s_prov_st[port] = 5U;
            }
            else if (radar_restart_port(port) != LL_OK)
            {
                s_prov_st[port] = 5U;
            }
            else
            {
                s_prov_t0[port] = m_u32Tickms;
                s_prov_st[port] = 2U;
            }

            s_prov_busy[port] = 0U;
            break;

        case 2U:                             /* 等模块重启, 然后重新跑一遍自适应探测来复检 */
            if ((m_u32Tickms - s_prov_t0[port]) >= RADAR_PROV_RESTART_MS)
            {
                s_probe_st[port]    = 0U;    /* 重新武装探测: 状态 0 里自带 RADAR_PROBE_BOOT_MS 启动延时 */
                s_probe_t0[port]    = m_u32Tickms;
                s_probe_idx[port]   = 0U;
                s_rep_frames[port]  = 0U;
                s_ack_frames[port]  = 0U;
                s_baud_locked[port] = 0U;
                s_prov_st[port]     = 3U;
            }
            break;

        case 3U:                             /* 复检: 探测锁定在目标波特率 = 成功 */
            if (radar_probe_done(port) == 0U) { break; }

            if ((s_baud_locked[port] != 0U) && (radar_port_get_baud(port) == RADAR_BAUD_TARGET))
            {
                s_prov_st[port] = 4U;
            }
            else
            {
                /* 模块没切成(或写配置无效): 保持探测找到的波特率继续工作, 不影响业务 */
                s_prov_st[port] = 5U;
            }
            break;

        default:
            s_prov_st[port] = 6U;
            break;
    }
}

uint8_t radar_provision_state(void)
{
    return s_prov_st[0];                    /* 单值兼容接口: 返回口 0 的产线配置状态 */
}
#else
uint8_t radar_provision_state(void)
{
    return 0U;
}
#endif

/* 底层泵: 只做"分帧超时 + 搬运该口收到的字节", **不跑探测/产线配置状态机**。
 * 阻塞式命令(radar_cmd_port)在等 ACK 期间必须调它而不是 radar_poll():
 * 否则 radar_cmd -> radar_poll -> radar_provision_tick -> radar_cmd 会无限递归。 */
static void radar_pump(uint8_t port)
{
    radar_frame_tick(&s_rx[port], m_u32Tickms);
    radar_port_poll(port);
    radar_port_tx_watchdog(port, m_u32Tickms);   /* 发送完成通知没来时的兜底 */
}

/* 链路监控 + 帧率结算 + Watch 输出刷新 —— 每口一份, 由 radar_poll() 逐口调用。
 * 判据与单口版**完全一致**, 只是下标换成 port。 */
static void radar_link_tick(uint8_t port)
{
    /* 链路监控: 有字节进来却一个合法帧都解不出 => 波特率被改了(乱码), 立刻重扫。
     * 不用发任何命令, 纯监听。判据(现场定): 最近 1 秒 字节增量 >= 32 且 合法帧(上报+ACK) == 0。
     * 注意两点:
     *   ① '字节增量'条件自动排除'模块进配置模式且安静'(那时一个字节都没有) -> 不误扫;
     *   ② '合法帧'把 ACK 也算进来, 所以 APP 正在配置(有 ACK)时也不会误扫。
     * 重扫顺序: 一律从候选表第 0 档开始按顺序扫完 8 档 —— 与上电探测行为完全一致。
     * 已知未覆盖: 模块被换到'连乱码都收不到'的大跨档(如 38400 换到 460800 时通道全静默),
     *             此时字节增量为 0, 本判据不触发 -> 由下面的"失联兜底"覆盖。 */
    if ((m_u32Tickms - s_link_ms[port]) >= 1000U)
    {
        uint32_t dBytes = radar_port_rx_bytes(port) - s_link_bytes0[port];
        uint32_t dFrames = s_fps_cnt[port] + s_win_ack_cnt[port];   /* 本窗口合法帧(上报+ACK) */

        if ((s_baud_locked[port] != 0U) && (dBytes >= 32U) && (dFrames == 0U))
        {
            s_baud_locked[port] = 0U;
            s_probe_st[port]    = 0U;                     /* 重新走探测(状态 0 自带启动延时) */
            s_probe_t0[port]    = m_u32Tickms;
            s_probe_idx[port]   = 0U;
        }

        if (dBytes != 0U)
        {
            s_link_last_rx_ms[port] = m_u32Tickms;        /* 线上还有字节: 记下最近活动时刻 */
        }
        else if ((s_baud_locked[port] != 0U) &&
                 ((m_u32Tickms - s_link_last_rx_ms[port]) >= RADAR_LINK_SILENT_MS) &&
                 ((m_u32Tickms - s_link_sweep_ms[port]) >= RADAR_LINK_SWEEP_MIN_MS))
        {
            /* 兜底: 锁定着却长时间一个字节都没有(模块被换到连乱码都收不到的档/长时间静默) */
            s_baud_locked[port] = 0U;
            s_probe_st[port]    = 0U;
            s_probe_t0[port]    = m_u32Tickms;
            s_probe_idx[port]   = 0U;
            s_link_sweep_ms[port] = m_u32Tickms;
        }
        s_link_bytes0[port] = radar_port_rx_bytes(port);
        s_win_ack_cnt[port] = 0U;
        s_link_ms[port]     = m_u32Tickms;
    }

    if ((m_u32Tickms - s_fps_ms[port]) >= 1000U)                   /* 每秒结算一次帧率 */
    {
        s_fps[port]     = s_fps_cnt[port];
        s_fps_cnt[port] = 0U;
        s_fps_ms[port]  = m_u32Tickms;
    }

    /* 现场只看这 3 个(现在每口一份), 位图/BRR 指纹/帧率编码与单口版一致 */
    g_radar_lock[port] = (uint32_t)s_baud_locked[port];
    g_radar_baud[port] = radar_port_baud_actual(port);            /* 只信硬件实际值 */

    {
        uint32_t comm = s_comm_map[port];

        if (s_reports[port] != 0U)                        { comm |= 0x100UL; }
        if ((m_u32Tickms - s_rep_last_ms[port]) <= 1000U) { comm |= 0x200UL; }
        if (radar_port_baud_ok(port) != 0U)               { comm |= 0x400UL; }
        comm |= ((radar_port_brr(port) >> 8) & 0xFFUL) << 16;     /* BRR 整数分频指纹 */
        comm |= (s_fps[port] & 0xFFUL) << 24;                     /* 最近 1 秒的合法帧数 = 帧率 */

        g_radar_comm[port] = comm;
    }
}

void radar_poll(void)
{
    uint8_t p;

    for (p = 0U; p < (uint8_t)RADAR_PORT_CNT; p++)
    {
        radar_pump(p);
        radar_probe_tick(p);
#if (RADAR_BAUD_TARGET != 0UL)
        radar_provision_tick(p);         /* 产线配置: 把该口模块波特率配成 RADAR_BAUD_TARGET */
#endif
        radar_link_tick(p);              /* 链路监控 + 帧率结算 + g_radar_*[p] 刷新 */

        /* 与串口同一模块的 OUT 脚(可选判定源): 口 p 对应 RADAR_PORTp/PINp */
        s_dev[p].out_present = (GPIO_ReadInputPins(s_out_port[p], s_out_pin[p]) == PIN_SET) ? 1U : 0U;
        s_dev[p].uart_online = ((m_u32Tickms - s_dev[p].last_rx_ms) <= RADAR_REPORT_STALE_MS) ? 1U : 0U;
    }

    /* 下面两项是单实例维护功能(共享 s_dump/s_param_*), 不随口循环 */
#if (RADAR_PARAM_EN != 0UL)
    radar_param_tick();                  /* A: 把探测行为参数写成目标值(幂等) */
#endif
    radar_dump_tick();                   /* C: 上电读回一次只读信息到 s_dump */
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

    switch (s_presence_src[dev])
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

/* 判定来源仍是"整机一个策略": 一次设置对所有雷达口生效(逐口写一份, 保持旧 API 语义) */
void radar_set_presence_src(uint8_t src)
{
    uint8_t p;

    for (p = 0U; p < (uint8_t)RADAR_PORT_CNT; p++) { s_presence_src[p] = src; }
}

uint8_t radar_presence_src(void)
{
    return s_presence_src[0];
}

uint8_t radar_baud_locked(void)
{
    return s_baud_locked[0];                /* 单值兼容接口: 口 0; 多口看 g_radar_lock[] */
}

uint32_t radar_get_baud(void)
{
    return radar_port_get_baud(0U);         /* 单值兼容接口: 口 0; 多口看 g_radar_baud[] */
}

uint32_t radar_frames_ok(void)
{
    return s_rx[0].ok_cnt;
}

uint32_t radar_frames_err(void)
{
    return s_rx[0].err_cnt;
}

uint32_t radar_reports(void)
{
    return s_reports[0];
}

uint32_t radar_rx_bytes(void)
{
    return radar_port_rx_bytes(0U);
}

uint32_t radar_rx_drop(void)
{
    return radar_port_rx_drop(0U);
}

/* ------------------------------ 命令 ------------------------------ */
/* 单口兼容入口(旧 API): 等价于 radar_cmd_port(0U, ...) */
int32_t radar_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                  radar_ack_t *ack, uint32_t timeout_ms)
{
    return radar_cmd_port(0U, cmd, val, val_len, ack, timeout_ms);
}

/* 按口命令事务: 命令/ACK 全部落在该口(s_ack[port]/s_ack_ready[port]), 泵也只泵该口。 */
int32_t radar_cmd_port(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len,
                       radar_ack_t *ack, uint32_t timeout_ms)
{
    uint8_t  frame[RADAR_TX_MAX];
    uint16_t flen;
    uint32_t t0;

    if (port >= (uint8_t)RADAR_PORT_CNT) { return LL_ERR_INVD_PARAM; }
    if (radar_probe_done(port) == 0U) { return LL_ERR_NOT_RDY; }   /* 该口波特率探测未结束 */

    flen = radar_proto_build_cmd(cmd, val, val_len, frame, sizeof(frame));
    if (flen == 0U) { return LL_ERR_INVD_PARAM; }

    /* 等上一条发完(最多 50ms) */
    t0 = m_u32Tickms;
    while (radar_port_tx_busy(port) != 0U)
    {
        radar_pump(port);
        if ((m_u32Tickms - t0) > 50U) { return LL_ERR_BUSY; }
    }

    s_ack_ready[port] = 0U;
    if (radar_port_write(port, frame, flen) != LL_OK) { return LL_ERR; }

    t0 = m_u32Tickms;
    while ((m_u32Tickms - t0) < timeout_ms)
    {
        radar_pump(port);

        if (s_ack_ready[port] != 0U)
        {
            if (RADAR_ACK_CMD_MATCH(s_ack[port].cmd, cmd))    /* 只认本条命令的 ACK(比低字节) */
            {
                if (ack != 0) { *ack = s_ack[port]; }
                return (s_ack[port].status == 0U) ? LL_OK : LL_ERR;
            }
            s_ack_ready[port] = 0U;                           /* 别的命令的 ACK, 丢弃继续等 */
        }
    }

    return LL_ERR_TIMEOUT;
}

/* 该口探测是否已结束(9 = 结束) */
static uint8_t radar_probe_done(uint8_t port)
{
    if (port >= (uint8_t)RADAR_PORT_CNT) { return 0U; }
    return (s_probe_st[port] >= 9U) ? 1U : 0U;
}

/* 兼容接口: 口 0 的探测是否已结束 */
uint8_t radar_ready(void)
{
    return radar_probe_done(0U);
}

/* 配置事务: 使能配置 -> 命令 -> 结束配置(全部走同一个口) */
static int32_t radar_cfg_cmd(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len,
                             radar_ack_t *ack, uint32_t timeout_ms)
{
    uint8_t en[2];
    int32_t ret;

    en[0] = 0x01U;
    en[1] = 0x00U;

    ret = radar_cmd_port(port, RADAR_CMD_ENABLE_CFG, en, 2U, 0, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    ret = radar_cmd_port(port, cmd, val, val_len, ack, timeout_ms);

    (void)radar_cmd_port(port, RADAR_CMD_DISABLE_CFG, 0, 0U, 0, RADAR_CMD_TIMEOUT_MS);

    return ret;
}

int32_t radar_read_params(radar_params_t *out)
{
    radar_ack_t ack;
    int32_t ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(0U, RADAR_CMD_READ_PARAM, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
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

    return radar_cfg_cmd(0U, RADAR_CMD_SENSITIVITY, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
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

    return radar_cfg_cmd(0U, RADAR_CMD_MAX_GATE, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_set_resolution(uint8_t idx)
{
    uint8_t v[2];

    v[0] = idx; v[1] = 0x00U;                           /* 0 = 0.75m, 1 = 0.2m */

    return radar_cfg_cmd(0U, RADAR_CMD_RESOLUTION, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

/* 0x00A3: 模块在"应答发送完成后"自动重启。
 * 需要重启才生效的配置: 串口波特率(0x00A1)、距离分辨率(0x00AA)、蓝牙(0x00A4)、
 * 蓝牙密码(0x00A9)、恢复出厂(0x00A2); 灵敏度(0x0064)与最大距离门(0x0060)立即生效。 */
int32_t radar_restart(void)
{
    return radar_restart_port(0U);
}

int32_t radar_restart_port(uint8_t port)
{
    return radar_cmd_port(port, RADAR_CMD_RESTART, 0, 0U, 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_set_uart_baud_index(uint8_t idx)
{
    return radar_set_uart_baud_index_port(0U, idx);
}

int32_t radar_set_uart_baud_index_port(uint8_t port, uint8_t idx)
{
    uint8_t v[2];

    v[0] = idx; v[1] = 0x00U;                           /* 0x0007=256000, 0x0008=460800 */

    return radar_cfg_cmd(port, RADAR_CMD_UART_BAUD, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_eng_mode(uint8_t on)
{
    return radar_cfg_cmd(0U, (on != 0U) ? RADAR_CMD_ENG_MODE_ON : RADAR_CMD_ENG_MODE_OFF,
                         0, 0U, 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_noise_start(uint16_t sec)
{
    uint8_t v[2];

    v[0] = (uint8_t)(sec & 0xFFU);
    v[1] = (uint8_t)((sec >> 8) & 0xFFU);

    return radar_cfg_cmd(0U, RADAR_CMD_NOISE_START, v, sizeof(v), 0, RADAR_CMD_TIMEOUT_MS);
}

int32_t radar_noise_status(uint16_t *status)
{
    radar_ack_t ack;
    int32_t ret;

    if (status == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_cfg_cmd(0U, RADAR_CMD_NOISE_STATUS, 0, 0U, &ack, RADAR_CMD_TIMEOUT_MS);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_u16(&ack, status) != 0) ? LL_OK : LL_ERR;
}
