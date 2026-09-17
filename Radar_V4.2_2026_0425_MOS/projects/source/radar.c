/*******************************************************************************
 * radar.c -- LD2410C 雷达服务层实现
 *
 * 第 3 步(多口): radar.c 每口一份 + 按口循环。
 *   - 运行时状态(分帧器/ACK 事务/探测/链路监控/帧率/统计)**全部按口分开**;
 *   - 每个雷达口一个接收跳板 radar_rx_cb0/1/2 -> radar_on_bytes(port, ...);
 *   - radar_poll() 里 for (p = 0; p < RADAR_PORT_CNT; p++) 逐口推进;
 *   - 现场 Watch 仍是 3 个变量, 但变成 [RADAR_PORT_CNT] 数组(一口一个下标)。
 * 命令层(后续步骤): 所有命令走**非阻塞事务引擎**(见文件后部「命令层」一节):
 *   begin/poll 两段式, 由 radar_poll() 每拍推进一步, 主循环单次占用有界(不再 while 死等 ACK)。
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
#if ((RADAR_DL_SENS_EN != 0U) && (RADAR_DL_SENS_GATE_MAX > RADAR_GATE_MAX))
#error "RADAR_DL_SENS_GATE_MAX must not exceed RADAR_GATE_MAX (radar_proto.h)"
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
 *                  bit11..bit15 = **参数类逐口进度/结果码**。两个来源**编译期互斥**:
 *                                 ① 参数自动配置(RADAR_PARAM_EN != 0; 出厂关闭) -> 下表;
 *                                 ② 报警下行灵敏度(RADAR_PARAM_EN == 0 且 RADAR_DL_SENS_EN != 0):
 *                                    0 没下发过 / 1..10 已生效档位(= 下行值, 灵敏度 = 值×10) /
 *                                    11 正在写 / 12 写失败已放弃 / 13 该口未锁定(等它上线) /
 *                                    14 在等平静期(报警期间不动雷达)
 *                                 ① 的编码表:
 *                     0   未开始(闸门没过, 什么都还没发)
 *                     8   正在读回(0x0061 / 0x00AE 事务在跑)
 *                     9   正在写(0x0060 / 0x0064 / 0x00AD 事务在跑)
 *                     1   读回成功且与目标一致(不需要写, 一条命令都没发)
 *                     2   读回成功但与目标不一致(只读自检模式到此为止)
 *                     3   已写最大距离门 0x0060        4   已写各门灵敏度 0x0064
 *                     5   已写光感辅助 0x00AD          6   复检通过, 与目标一致 <- 成功终态
 *                     7   复检后仍不一致 <- 失败终态
 *                     16~19 读回失败 / 20~23 写失败 / 24~27 复检读回失败, 每组 4 个原因:
 *                           +0 超时(模块一直没回 ACK)
 *                           +1 模块回了但状态非 0(拒绝了这个命令)
 *                           +2 帧发不出去(等 TX 空闲超时 / 写口失败)
 *                           +3 ACK 格式不对(解析失败)
 *                     31  该口未锁定(不在线), 跳过
 *                     注: 这些失败码是"同一步重试 RADAR_PARAM_TRY_MAX(3) 次仍失败"后才出现的终态。
 *                         改造前只知道"读回失败", 现在是"哪一步 + 为什么", 现场不用调试器就能定位。
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
#if (RADAR_PROBE_FAILSAFE != 0U)
static uint8_t             s_probe_rescan[RADAR_PORT_CNT];  /* 1 = 本轮是"重扫"(不是上电首次探测) */
/* **靶向急救**的状态: 急救包(裸 0x00FE)只在"**我们自己**的配置事务把模块留在配置态"时才许发。
 * 客户手机 APP 把模块留在配置态**不归我们管** —— 它自己会收尾(APP 崩了也不该由我们去打断它)。
 * 这一条是"客户在参数页停留很久也不会报 6401"的关键。 */
static uint8_t             s_rescue_own[RADAR_PORT_CNT];    /* 1 = 是我们自己的事务把该口模块留在配置态 */
static uint8_t             s_rescue_cnt[RADAR_PORT_CNT];    /* 本轮急救已补发帧数(到 RADAR_RESCUE_SEND_MAX 收工) */
#if (RADAR_RESCUE_ABANDON_MS > 0UL)
static uint32_t            s_rescue_last_ms[RADAR_PORT_CNT]; /* 上次补发时刻(长时兜底的节流用) */
#endif
#define RADAR_RESCUE_SEND_MAX   (8U)                        /* 最多补发几帧(候选表 8 档, 一轮一帧) */
#endif
static uint8_t             s_probe_idx[RADAR_PORT_CNT];
static uint32_t            s_probe_t0[RADAR_PORT_CNT];
static uint32_t            s_rep_frames[RADAR_PORT_CNT];    /* 本档收到的上报帧数(换档清零) */
static uint32_t            s_ack_frames[RADAR_PORT_CNT];    /* 本档收到的 ACK 帧数(换档清零) */
#if (RADAR_BAUD_TARGET != 0UL)
#define RADAR_PROV_OP_NONE     (0U)                         /* 当前没有事务在跑 */
#define RADAR_PROV_OP_BAUD     (1U)                         /* 在等写波特率 0x00A1 */
#define RADAR_PROV_OP_RESTART  (2U)                         /* 在等重启模块 0x00A3 */

static uint8_t             s_prov_st[RADAR_PORT_CNT];       /* 产线配置状态: 0 待做 1 写入中 2 等重启 3 自检中 4 成功 5 失败 6 无法配置 */
static uint32_t            s_prov_t0[RADAR_PORT_CNT];
static uint8_t             s_prov_op[RADAR_PORT_CNT];       /* 正在跑哪笔事务: 0 无 / 1 写波特率 / 2 重启模块 */
#endif

/* ==================== 非阻塞命令事务引擎(每口一笔) ====================
 * 详见文件后部"命令层"的说明。这里只放类型与状态: 必须声明在 radar_init() 之前。 */
#define RADAR_TXN_ERR_BASE   (-100)         /* 内部错误码基数: 远离 LL_ERR_* 的取值域, 不会撞 */
#define RADAR_TXN_STEP_MAX   (3U)           /* 一笔事务最多 3 帧: 使能配置 / 业务命令 / 结束配置 */

/* 内部哨兵: 事务本身成功(收到 ACK), 但 ACK 的格式/解析不对 ——
 * 必须和"模块回了 status≠0(拒绝)"区分开, 否则现场分不清是模块拒了还是我们解析错了。
 * **必须定义在这个无条件编译的位置**: 用它的 *_poll 函数不受 RADAR_PARAM_EN 控制,
 * 放到参数块里会导致 PARAM_EN=0(出货配置) 直接编不过。 */
#define RADAR_ERR_PARSE      (RADAR_TXN_ERR_BASE + 0)

#define RADAR_TXN_IDLE       (0U)           /* 空闲 */
#define RADAR_TXN_WAIT_TX    (1U)           /* 等上一帧发完 */
#define RADAR_TXN_WAIT_ACK   (2U)           /* 已发, 等这一帧的 ACK */
#define RADAR_TXN_DONE       (3U)           /* 有结果, 等上层 poll 取走 */

typedef struct {
    uint8_t      st;                                            /* RADAR_TXN_* */
    uint8_t      step;                                          /* 当前帧序号 */
    uint8_t      nstep;                                         /* 总帧数 1~3 */
    uint8_t      res_step;                                      /* 结果取第几帧的 ACK */
    uint8_t      skip_mid;                                      /* 1 = 使能帧失败 -> 跳过中间帧直接补末帧(运行时置位) */
    uint32_t     t0;                                            /* 进入当前等待的时刻 */
    uint8_t      len[RADAR_TXN_STEP_MAX];
    uint16_t     cmd[RADAR_TXN_STEP_MAX];
    uint8_t      buf[RADAR_TXN_STEP_MAX][RADAR_TX_MAX];
    int32_t      ret;                                           /* 最终结果 */
    uint8_t      has_ack;
    radar_ack_t  ack;                                           /* 结果帧的 ACK */
} radar_txn_t;
static radar_txn_t s_txn[RADAR_PORT_CNT];

#if (RADAR_PARAM_EN != 0U)
/* 参数自动配置: 待写项掩码(读回一次算完, 再逐项写)。见 radar_param_tick()。 */
#define RADAR_PNEED_GATE       (0x0001U)                        /* 位0: 写 0x0060 */
#define RADAR_PNEED_SENS(g)    ((uint16_t)(1U << ((g) + 1U)))   /* 位1..9: 门 0..8 的灵敏度 */
#define RADAR_PNEED_AUX        (0x0400U)                        /* 位10: 写 0x00AD */

#define RADAR_POP_NONE         (0U)                             /* 当前没有事务在跑 */
#define RADAR_POP_PARAMS       (1U)                             /* 在等读参数 0x0061 */
#define RADAR_POP_AUX          (2U)                             /* 在等读辅助 0x00AE */
#define RADAR_POP_GATE         (3U)                             /* 在等写 0x0060 */
#define RADAR_POP_SENS         (4U)                             /* 在等写 0x0064 */
#define RADAR_POP_AUXSET       (5U)                             /* 在等写 0x00AD */

#define RADAR_PSTEP_IDLE       (0U)
#define RADAR_PSTEP_RD_PAR     (1U)
#define RADAR_PSTEP_RD_AUX     (2U)
#define RADAR_PSTEP_WRITE      (3U)
#define RADAR_PSTEP_RCHK_PAR   (4U)
#define RADAR_PSTEP_RCHK_AUX   (5U)

/* 同一个子步最多试几次: 命令事务是"要么成要么重来", 瞬时失败(模块还在稳定 / 与重扫撞车)
 * 不该让该口永久放弃 —— 一次失败就锁死是改造前的毛病。 */
#define RADAR_PARAM_TRY_MAX    (3U)

/* 状态码(补两个, 原来 0 既是"没开始"又是"正在读回", 现场分不清):
 *   8 = 正在读回(0x0061 / 0x00AE 事务在跑)
 *   9 = 正在写(0x0060 / 0x0064 / 0x00AD 事务在跑) */
#endif

#if (RADAR_DL_SENS_EN != 0U)
/* 报警下行下发的灵敏度(逐口一套待办)。取值与状态码见 radar_cfg.h 的 RADAR_DL_SENS_*。
 * 必须声明在 radar_init() 之前(那里要清零)。s_dl_need[port] 是 9 位掩码:
 * 位 i = 门 i 与目标不一致, 还需要写。 */
#define RADAR_DL_OP_READ       (1U)                             /* 在等读参数 0x0061 */
#define RADAR_DL_OP_WRITE      (2U)                             /* 在等写某一门 0x0064 */

static uint8_t  s_dl_sens;                      /* 目标动态灵敏度(0 = 还没下发过) */
static uint16_t s_dl_need[RADAR_PORT_CNT];      /* 待写门掩码 */
static uint8_t  s_dl_st[RADAR_PORT_CNT];        /* 0 空闲 / 1 待读回 / 2 逐门写 / 3 完成或放弃 */
static uint8_t  s_dl_try[RADAR_PORT_CNT];       /* 当前步骤连续失败次数 */
static uint8_t  s_dl_code[RADAR_PORT_CNT];      /* -> g_radar_comm 的 bit11..15 */
static uint8_t  s_dl_op[RADAR_PORT_CNT];        /* 正在跑哪笔事务: 0 无 / 1 读 / 2 写 */
static uint8_t  s_dl_gate[RADAR_PORT_CNT];      /* 正在写的门号 */
static radar_params_t s_dl_cur[RADAR_PORT_CNT]; /* 本口读回的工作副本 */
static uint32_t s_dl_last_req_ms;               /* 最近一次收到"带雷达参数的报警下行包"的时刻 */
#endif

/* 各口 OUT 脚(与串口同模块, 供串口/OUT 双路判定)。下标 = 口号:
 *   口0 -> RADAR_PORT0/PIN0 (PC14)   口1 -> RADAR_PORT1/PIN1 (PC13)   口2 -> RADAR_PORT2/PIN2 (PH2)
 * 与 bsp_report.c / common.c / bsp_gpio.c 读三路 OUT 的写法一致。 */
static const uint8_t  s_out_port[RADAR_PORT_CNT] = { RADAR_PORT0, RADAR_PORT1, RADAR_PORT2 };
static const uint16_t s_out_pin[RADAR_PORT_CNT]  = { RADAR_PIN0,  RADAR_PIN1,  RADAR_PIN2  };

/* ------------------------------ 前置声明(定义在本文件后部) ------------------------------ */
static void    radar_pump(uint8_t port);
static void    radar_switch_baud(uint8_t port, uint32_t baud);
#if (RADAR_PROBE_FAILSAFE != 0U)
static void    radar_probe_failsafe(uint8_t port);
#endif
static uint8_t radar_probe_done(uint8_t port);
static void    radar_txn_tick(uint8_t port);                    /* 非阻塞事务: 每拍推进一步 */
static int32_t radar_txn_poll(uint8_t port, radar_ack_t *ack);  /* 取事务结果(LL_ERR_BUSY = 未完) */
static int32_t radar_cfg_begin(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len);
#if (RADAR_PARAM_EN != 0U)
static void    radar_param_tick(uint8_t port);
#endif
#if (RADAR_DL_SENS_EN != 0U)
static void    radar_dl_sens_tick(uint8_t port);
#endif

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
    memset(s_txn, 0, sizeof(s_txn));    /* 所有口都没有在跑的事务 */
#if (RADAR_DL_SENS_EN != 0U)
    s_dl_sens         = 0U;             /* 还没收到过报警下行包 */
    s_dl_last_req_ms  = 0U;             /* 上电默认视为"平时"(反正 s_dl_sens = 0 时什么都不会做) */
#endif

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
#if (RADAR_DL_SENS_EN != 0U)
        s_dl_need[p] = 0U;
        s_dl_st[p]   = 0U;
        s_dl_try[p]  = 0U;
        s_dl_code[p] = 0U;
        s_dl_op[p]   = 0U;
        s_dl_gate[p] = 0U;
#endif
#if (RADAR_BAUD_TARGET != 0UL)
        s_prov_op[p] = 0U;
#endif

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
#if (RADAR_PROBE_FAILSAFE != 0U)
        /* =2 时上电首次探测也发急救包; =1 时首次保持纯监听(现场口径), 只有重扫才发 */
        s_probe_rescan[p] = (RADAR_PROBE_FAILSAFE == 2U) ? 1U : 0U;
#endif
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
#if (RADAR_PROBE_FAILSAFE != 0U)
    s_rescue_own[port] = 0U;    /* 锁上了 = 模块在正常上报, 没有卡在配置态: 撤销靶向急救 */
    s_rescue_cnt[port] = 0U;
#endif
}

/* 波特率自适应 —— **纯监听**: 一条命令都不发, 唯一的例外是重扫急救包(见下)。
 *   每切一档波特率, 只听模块周期性主动上报的帧(F4F3F2F1..F8F7F6F5), 收到 RADAR_BAUD_LOCK_FRAMES
 *   个合法帧就认定该档; RADAR_PROBE_LISTEN_MS 内听不到就换下一档。
 *
 * 为什么绝不发"使能配置 0x00FF": 它会让模块**进入配置态并停止上报**, 只要 0x00FE 晚发/丢失/被拒,
 *   模块就"哑"了 —— 现场已经踩过这个坑。探测阶段永远不发 0x00FF。
 * 唯一的例外是**重扫急救包**(裸 0x00FE, RADAR_PROBE_FAILSAFE): 它没有"进配置态"的语义,
 *   而且**只在两种情况下才发**(见 radar_probe_failsafe()): ① 是**我们自己**的事务把模块留在配置态
 *   (靶向); ② 该口彻底静默超过 RADAR_RESCUE_ABANDON_MS(长时兜底)。客户 APP 的会话不触发 ①,
 *   所以"客户在参数页停留多久"与我们无关。细节见 radar_cfg.h 的 RADAR_PROBE_FAILSAFE 一节。
 * 参数读写等命令仍按协议包"使能配置->命令->结束配置", 但那是**显式调用**时才发生。 */
/* 统一换档入口: 换波特率必须和『复位分帧器 + 清接收计数』一起做 ——
 * 换档瞬间线上那一帧会被拆开(前半截在旧档、后半截在新档或直接丢失),
 * 若不复位分帧器/不清计数, 就可能把两段拼成一帧或让旧档的帧计入新档。
 * radar_port_set_baud(port, ) 内部已完成: 关收发 -> USART_DeInit -> 重新初始化
 *   -> BRR 回读校验 -> 清环形缓冲; 这里补齐上层状态。
 * 前置条件: 必须在 TX 空闲时调用(换档会复位 USART, 发送到一半会被打断)。 */
static void radar_switch_baud(uint8_t port, uint32_t baud)
{
    radar_port_set_baud(port, baud);
#if (RADAR_PROBE_FAILSAFE != 0U)
    /* 换到本档之后补一帧裸 0x00FE: 但只对"**我们自己**把模块留在配置态"的那一口补(靶向)。
     * 这里只管"是不是重扫"; "要不要补"由 radar_probe_failsafe() 判(查靶向武装 s_rescue_own 与静默条件),
     * 见 radar_cfg.h 的 RADAR_PROBE_FAILSAFE / RADAR_FAILSAFE_SILENT_MS / RADAR_RESCUE_ABANDON_MS。 */
    if (s_probe_rescan[port] != 0U) { radar_probe_failsafe(port); }
#endif
    radar_frame_init(&s_rx[port]);      /* 分帧器状态清零: 不跨波特率拼接 */
    s_rep_frames[port] = 0U;
    s_ack_frames[port] = 0U;
}

static void radar_probe_tick(uint8_t port)
{
    if (s_probe_st[port] >= 9U)              /* 探测已结束 */
    {
#if (RADAR_PROBE_FAILSAFE != 0U)
        s_probe_rescan[port] = 1U;           /* 首次探测已跑完: 之后的每一轮都是"重扫" */
#endif
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
/* 命令通道是否可用: **该口**探测已结束, 且产线配置(若有)已跑完 */
static uint8_t radar_link_ready(uint8_t port)
{
#if (RADAR_BAUD_TARGET != 0UL)
    if (s_prov_st[port] < 4U) { return 0U; }
#endif
    return radar_probe_done(port);
}
#endif

/* 只读信息汇总: **口 0 单实例**维护功能(参数配置已改为逐口, 不再共用它) */
static radar_dump_t     s_dump;
#if (RADAR_DUMP_ONCE != 0U)
static uint8_t          s_dump_done;
#endif

#if (RADAR_PARAM_EN != 0U)
/* 参数自动配置: **逐口一套状态 + 逐口一份读回工作副本**(不共用 s_dump)。
 * 每块模块的参数存在它自己的 flash 里, 所以必须逐口配。 */
static uint8_t          s_param_st[RADAR_PORT_CNT];    /* 0 待做 / 1 读回 / 2 写 / 3 复检 / 4 成功 / 5 失败 */
static radar_params_t   s_param_cur[RADAR_PORT_CNT];   /* 本口读回的参数工作副本 */
static radar_aux_t      s_param_aux[RADAR_PORT_CNT];   /* 本口读回的辅助控制工作副本 */
static uint8_t          s_param_code[RADAR_PORT_CNT];  /* -> g_radar_comm 的 bit11..15(编码表见文件头) */
static uint16_t         s_param_need[RADAR_PORT_CNT];   /* 待写项掩码(读回一次算完, 见 RADAR_PNEED_*) */
static uint8_t          s_param_op[RADAR_PORT_CNT];     /* 正在跑哪笔事务(见 RADAR_POP_*) */
static uint8_t          s_param_idx[RADAR_PORT_CNT];    /* 正在写的门号 */
static uint8_t          s_param_step[RADAR_PORT_CNT];   /* 子步(见 RADAR_PSTEP_*) */
static uint8_t          s_param_try[RADAR_PORT_CNT];    /* 当前子步已试次数(见 RADAR_PARAM_TRY_MAX) */
static const uint8_t    s_param_move_sens[RADAR_GATE_MAX + 1U]  = RADAR_PARAM_MOVE_SENS;
static const uint8_t    s_param_still_sens[RADAR_GATE_MAX + 1U] = RADAR_PARAM_STILL_SENS;
#endif

const radar_dump_t *radar_dump(void)
{
    return &s_dump;
}

/* ------------------------------ A. 探测行为参数 ------------------------------ */
/* 光感辅助控制(0x00AD): mode 0=关闭 / 1=光感<阈值 / 2=光感>阈值; out_level 0=默认低(有人=高)。
 * 两段式(begin 发起 / poll 取结果), 见文件后部"命令层: 非阻塞事务引擎"。 */
int32_t radar_set_aux_control_begin(uint8_t port, uint8_t mode, uint8_t threshold, uint8_t out_level)
{
    uint8_t v[4];

    if (mode > 2U)      { return LL_ERR_INVD_PARAM; }
    if (out_level > 1U) { return LL_ERR_INVD_PARAM; }

    v[0] = mode;
    v[1] = threshold;
    v[2] = out_level;
    v[3] = 0x00U;

    return radar_cfg_begin(port, RADAR_CMD_AUX_SET, v, (uint8_t)sizeof(v));
}

int32_t radar_set_aux_control_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}
/* 把 A 组参数与目标值比对: 需要写返回 1 并给出 which; idx 返回要写的门号(纯函数, 无副作用) */
#if (RADAR_PARAM_EN != 0U)
static uint8_t radar_param_diff(const radar_params_t *p, const radar_aux_t *a,
                                uint8_t *which, uint8_t *idx)
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
        if (p->move_sens[i] != s_param_move_sens[i]) { *which = 2U; *idx = i; return 1U; }
        if (i >= 2U)                            /* 门 0/1 的静止灵敏度不可设置 */
        {
            if (p->still_sens[i] != s_param_still_sens[i]) { *which = 2U; *idx = i; return 1U; }
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

/* 发起都发起不起来(该口已有一笔在跑 / 探测没结束 / 组帧失败): 也算一次失败,
 * 否则会静默无限重试、状态码一直停在 0 或 8, 现场完全看不出问题。base = 16/20/24。 */
static void radar_param_begin_failed(uint8_t port, uint8_t base)
{
    s_param_try[port]++;
    s_param_code[port] = (uint8_t)(base + 2U);          /* 原因 2 = 发不出去 */
    if (s_param_try[port] >= (uint8_t)RADAR_PARAM_TRY_MAX) { s_param_st[port] = 5U; }
}

/* 把事务/解析的错误码翻成"原因"(0 超时 / 1 状态非0 / 2 发不出去 / 3 格式不对) */
static uint8_t radar_param_reason(int32_t ret)
{
    if (ret == LL_ERR_TIMEOUT) { return 0U; }
    if (ret == LL_ERR)         { return 1U; }
    if (ret == LL_ERR_BUSY)    { return 2U; }
    return 3U;
}

/* 事务起来了: 把"正在读回 / 正在写"显示到状态码上
 * (改造前 0 既是"没开始"又是"正在读回", 现场分不清卡在哪) */
static void radar_param_mark(uint8_t port, uint8_t op)
{
    s_param_code[port] = ((op == RADAR_POP_GATE) || (op == RADAR_POP_SENS) ||
                          (op == RADAR_POP_AUXSET)) ? 9U : 8U;
}

/* 逐口幂等应用流程(全部两段式, 不再阻塞主循环):
 *   子步 IDLE -> 读 0x0061 -> 读 0x00AE -> 算待写掩码 -> 逐项写 -> 复检(读 0x0061 + 0x00AE)
 * **只对已锁定(在线)的口执行**; 没锁上的口标 31(跳过)但不锁死, 等它上线后下一拍自动接手。
 * 每步进度写进 s_param_code[port], 由 radar_link_tick() 落到 g_radar_comm 的 bit11..15。
 * 改造前是"每写一项就重读一遍"(因为阻塞版一拍只能做一件事); 现在读一次算完整掩码再逐项写,
 * 事务数从 ~3N 降到 N+3。 */
static void radar_param_tick(uint8_t port)
{
    uint8_t  op;
    uint8_t  i;
    int32_t  ret;

    if (s_param_st[port] >= 4U) { return; }                       /* 已完成/已失败: 不再动 */

    /* ============ ① 有事务在跑: **先收结果**(每拍最多做这一件事) ============
     * 这一段必须排在下面那些"能不能动它"的闸门**之前**: 已经发出去的事务必须收回来,
     * 否则链路一不新鲜(例如模块正被 0x00FF 停在配置态), 该口就永远占着事务槽收不回来。 */
    op = s_param_op[port];
    if (op != RADAR_POP_NONE)
    {
        switch (op)
        {
            case RADAR_POP_PARAMS: ret = radar_read_params_poll(port, &s_param_cur[port]);      break;
            case RADAR_POP_AUX:    ret = radar_read_aux_control_poll(port, &s_param_aux[port]); break;
            case RADAR_POP_GATE:   ret = radar_set_max_gate_poll(port);                         break;
            case RADAR_POP_SENS:   ret = radar_set_sensitivity_poll(port);                      break;
            default:               ret = radar_set_aux_control_poll(port);                      break;
        }

        if (ret == LL_ERR_BUSY) { return; }                       /* 还没做完: 下一拍再来 */
        s_param_op[port] = RADAR_POP_NONE;

        if (ret != LL_OK)                                         /* 失败: 先重试, 到次数才认输 */
        {
            s_param_try[port]++;
            if (s_param_try[port] < (uint8_t)RADAR_PARAM_TRY_MAX)
            {
                return;                                           /* 子步不变, 下一拍自动重来同一件事 */
            }

            /* 失败码 = 阶段基准 + 原因(见文件头码表):
             *   读回 16+r / 写 20+r / 复检读回 24+r, r = 0 超时 1 状态非0 2 发不出去 3 格式不对 */
            if ((op == RADAR_POP_PARAMS) || (op == RADAR_POP_AUX))
            {
                uint8_t base = (s_param_step[port] >= (uint8_t)RADAR_PSTEP_RCHK_PAR) ? 24U : 16U;
                s_param_code[port] = (uint8_t)(base + radar_param_reason(ret));
            }
            else
            {
                s_param_code[port] = (uint8_t)(20U + radar_param_reason(ret));
            }
            s_param_st[port] = 5U;
            return;
        }

        s_param_try[port] = 0;                                    /* 这一步成了: 重试计数清零 */

        switch (s_param_step[port])                               /* 成功: 推进子步 */
        {
            case RADAR_PSTEP_RD_PAR:
                s_param_step[port] = RADAR_PSTEP_RD_AUX;
                break;

            case RADAR_PSTEP_RD_AUX:                              /* 两次读回都到手: 算待写掩码 */
                s_param_need[port] = 0U;

                if ((s_param_cur[port].max_move_gate  != (uint8_t)RADAR_PARAM_MAX_MOVE_GATE) ||
                    (s_param_cur[port].max_still_gate != (uint8_t)RADAR_PARAM_MAX_STILL_GATE) ||
                    (s_param_cur[port].no_body_sec    != (uint16_t)RADAR_PARAM_NO_BODY_SEC))
                {
                    s_param_need[port] |= RADAR_PNEED_GATE;
                }

                for (i = 0U; i <= (uint8_t)RADAR_GATE_MAX; i++)
                {
                    if (s_param_cur[port].move_sens[i] != s_param_move_sens[i])
                    {
                        s_param_need[port] |= RADAR_PNEED_SENS(i);
                    }
                    else if ((i >= 2U) && (s_param_cur[port].still_sens[i] != s_param_still_sens[i]))
                    {
                        s_param_need[port] |= RADAR_PNEED_SENS(i);   /* 门 0/1 的静止灵敏度模块不认, 不比 */
                    }
                    else
                    {
                        /* 这一门已经对上了 */
                    }
                }

                if ((s_param_aux[port].mode      != (uint8_t)RADAR_PARAM_AUX_MODE) ||
                    (s_param_aux[port].threshold != (uint8_t)RADAR_PARAM_AUX_THRESHOLD) ||
                    (s_param_aux[port].out_level != (uint8_t)RADAR_PARAM_AUX_OUT_LEVEL))
                {
                    s_param_need[port] |= RADAR_PNEED_AUX;
                }

                if (s_param_need[port] == 0U)
                {
                    s_param_st[port]   = 4U;                      /* 已经一致: 一条命令都不发 */
                    s_param_code[port] = 1U;
                }
#if (RADAR_PARAM_EN == 1U)
                else
                {
                    s_param_st[port]   = 4U;                      /* 只读自检: 到此为止, 一个字节都不写 */
                    s_param_code[port] = 2U;
                }
#else
                else
                {
                    s_param_step[port] = RADAR_PSTEP_WRITE;
                }
#endif
                break;

            case RADAR_PSTEP_WRITE:                               /* 刚写完的那一项已经生效: 清位 */
                if (op == RADAR_POP_GATE)
                {
                    s_param_need[port] &= (uint16_t)(~RADAR_PNEED_GATE);
                    s_param_code[port]  = 3U;                     /* 已写最大距离门 */
                }
                else if (op == RADAR_POP_SENS)
                {
                    s_param_need[port] &= (uint16_t)(~RADAR_PNEED_SENS(s_param_idx[port]));
                    s_param_code[port]  = 4U;                     /* 已写某一门灵敏度 */
                }
                else
                {
                    s_param_need[port] &= (uint16_t)(~RADAR_PNEED_AUX);
                    s_param_code[port]  = 5U;                     /* 已写光感辅助 */
                }

                if (s_param_need[port] == 0U) { s_param_step[port] = RADAR_PSTEP_RCHK_PAR; }
                break;

            case RADAR_PSTEP_RCHK_PAR:
                s_param_step[port] = RADAR_PSTEP_RCHK_AUX;
                break;

            default:                                              /* RCHK_AUX: 复检判定 */
            {
                uint8_t which;
                uint8_t idx = 0U;

                if (radar_param_diff(&s_param_cur[port], &s_param_aux[port], &which, &idx) == 0U)
                {
                    s_param_st[port]   = 4U; s_param_code[port] = 6U;   /* 成功终态 */
                }
                else
                {
                    s_param_st[port]   = 5U; s_param_code[port] = 7U;   /* 写完仍不一致 */
                }
                break;
            }
        }
        return;
    }

    /* ============ ② 没有事务在跑: 先判"能不能动它", 再发起 ============ */
    if (radar_link_ready(port) == 0U) { return; }                 /* 该口探测还没结束(或产线配置没跑完) */

    if (s_baud_locked[port] == 0U)                                /* 扫完了但没锁上 = 该口不在线: 跳过 */
    {
        s_param_code[port] = 31U;
        return;
    }

    /* 链路健康互锁: 只有"最近 1 秒内还解出过合法上报帧"才动它 ——
     * 链路本来就不新鲜时发 0x00FF, 一旦 ACK 丢了模块就停在配置态(只能断电救)。 */
    if ((m_u32Tickms - s_rep_last_ms[port]) > RADAR_REPORT_STALE_MS) { return; }

    switch (s_param_step[port])
    {
        case RADAR_PSTEP_IDLE:
            s_param_step[port] = RADAR_PSTEP_RD_PAR;
            s_param_st[port]   = 1U;
            /* 落到下一个 case, 本拍就把读回发起出去 */
            /* fall through */

        case RADAR_PSTEP_RD_PAR:
            if (radar_read_params_begin(port) == LL_OK)
            {
                s_param_op[port] = RADAR_POP_PARAMS;
                radar_param_mark(port, RADAR_POP_PARAMS);
            }
            else { radar_param_begin_failed(port, 16U); }
            break;

        case RADAR_PSTEP_RD_AUX:
            if (radar_read_aux_control_begin(port) == LL_OK)
            {
                s_param_op[port] = RADAR_POP_AUX;
                radar_param_mark(port, RADAR_POP_AUX);
            }
            else { radar_param_begin_failed(port, 16U); }
            break;

        case RADAR_PSTEP_WRITE:                                   /* 按掩码最低位发起一项写 */
            if ((s_param_need[port] & RADAR_PNEED_GATE) != 0U)
            {
                if (radar_set_max_gate_begin(port, (uint16_t)RADAR_PARAM_MAX_MOVE_GATE,
                                             (uint16_t)RADAR_PARAM_MAX_STILL_GATE,
                                             (uint16_t)RADAR_PARAM_NO_BODY_SEC) == LL_OK)
                {
                    s_param_op[port] = RADAR_POP_GATE;
                    radar_param_mark(port, RADAR_POP_GATE);
                }
            }
            else
            {
                uint8_t g;

                for (g = 0U; g <= (uint8_t)RADAR_GATE_MAX; g++)
                {
                    if ((s_param_need[port] & RADAR_PNEED_SENS(g)) != 0U) { break; }
                }

                if (g <= (uint8_t)RADAR_GATE_MAX)
                {
                    s_param_idx[port] = g;
                    if (radar_set_sensitivity_begin(port, (uint16_t)g,
                            (uint16_t)s_param_move_sens[g],
                            (uint16_t)s_param_still_sens[g]) == LL_OK)
                    {
                        s_param_op[port] = RADAR_POP_SENS;
                        radar_param_mark(port, RADAR_POP_SENS);
                    }
                }
                else if ((s_param_need[port] & RADAR_PNEED_AUX) != 0U)
                {
                    if (radar_set_aux_control_begin(port, (uint8_t)RADAR_PARAM_AUX_MODE,
                            (uint8_t)RADAR_PARAM_AUX_THRESHOLD,
                            (uint8_t)RADAR_PARAM_AUX_OUT_LEVEL) == LL_OK)
                    {
                        s_param_op[port] = RADAR_POP_AUXSET;
                        radar_param_mark(port, RADAR_POP_AUXSET);
                    }
                }
                else
                {
                    s_param_step[port] = RADAR_PSTEP_RCHK_PAR;     /* 掩码空了: 直接复检 */
                }
            }
            break;

        case RADAR_PSTEP_RCHK_PAR:
            if (radar_read_params_begin(port) == LL_OK)
            {
                s_param_op[port] = RADAR_POP_PARAMS;
                radar_param_mark(port, RADAR_POP_PARAMS);
            }
            else { radar_param_begin_failed(port, 24U); }
            break;

        default:                                                  /* RADAR_PSTEP_RCHK_AUX */
            if (radar_read_aux_control_begin(port) == LL_OK)
            {
                s_param_op[port] = RADAR_POP_AUX;
                radar_param_mark(port, RADAR_POP_AUX);
            }
            else { radar_param_begin_failed(port, 24U); }
            break;
    }
}

/* 口 0 兼容入口; **逐口结果请看 g_radar_comm 的 bit11..15** */
uint8_t radar_param_state(void)
{
    return s_param_st[0];
}

uint8_t radar_param_state_port(uint8_t port)
{
    return (port < (uint8_t)RADAR_PORT_CNT) ? s_param_st[port] : 0U;
}
#else
uint8_t radar_param_state(void)
{
    return 0U;                                  /* 未启用参数自动配置 */
}

uint8_t radar_param_state_port(uint8_t port)
{
    (void)port;
    return 0U;
}
#endif

/* ------------------------------ 报警下行下发灵敏度(逐口一套) ------------------------------
 * 上游: common.c 的 Get_pdu_data() 每收到一条报警下行 PDU 就调一次 radar_set_downlink_range()。
 * 这里只登记目标值, 真正的串口命令由 radar_poll() 逐拍推进 —— RS485 收包上下文里绝不做阻塞事务。
 *
 * **为什么还要"等平静期"**: 读/写参数的整段事务期间模块会停上报(0x00FF 让它进配置态),
 * 而这条参数偏偏是跟报警包一起来的 —— 报警期间正需要雷达数据。所以本函数**每条 PDU 都记一次**
 * 时刻(用于判定"现在是不是报警期间"), 但真正的读/写要等到 RADAR_DL_SENS_QUIET_MS 之后才做。
 * 整套读+写两段式, 一层也不阻塞主循环。 */
#if (RADAR_DL_SENS_EN != 0U)
int32_t radar_set_downlink_range(uint8_t range)
{
    uint8_t p;
    uint8_t sens;

    s_dl_last_req_ms = m_u32Tickms;                 /* 不管值合不合法: 这条 PDU 本身就是"报警活动" */

    if (range == 0U) { return LL_OK; }              /* 0 = 不设置: 保持模块现状, 连待办都不建 */
    if (range > 10U) { return LL_ERR_INVD_PARAM; }  /* 0xFF 之类: 忽略 */

    sens = (uint8_t)(range * 10U);                  /* 1..10 -> 10..100 */

    if (sens == s_dl_sens)
    {
        return LL_OK;                               /* **重复值: 一条命令都不发**(模块自己存了 flash) */
    }

    s_dl_sens = sens;
    for (p = 0U; p < (uint8_t)RADAR_PORT_CNT; p++)
    {
        s_dl_st[p]   = 1U;                          /* 先读回, 再决定写哪几门 */
        s_dl_try[p]  = 0U;
        s_dl_need[p] = 0U;
        s_dl_op[p]   = 0U;
        s_dl_code[p] = 11U;                         /* 正在写 */
    }
    return LL_OK;
}

/* 逐口推进: **两段式, 每拍只做一件事**(发起一笔 或 收一笔的结果)。
 * 命令事务已经非阻塞, 所以三个口可以各自推进, 不需要再"每拍只轮转一个口"。 */
static void radar_dl_sens_tick(uint8_t port)
{
    uint8_t i;
    int32_t ret;

    if (s_dl_sens == 0U) { return; }                 /* 还没下发过 */
    if (s_dl_st[port] >= 3U) { return; }             /* 已完成 / 已放弃 */

    /* ============ ① 有事务在跑: **先收结果**(理由同参数配置: 已发出的事务必须收回来) ============ */
    if (s_dl_op[port] != 0U)
    {
        ret = (s_dl_op[port] == RADAR_DL_OP_READ) ? radar_read_params_poll(port, &s_dl_cur[port])
                                                  : radar_set_sensitivity_poll(port);
        if (ret == LL_ERR_BUSY) { return; }          /* 还没完: 下一拍再来 */
        s_dl_op[port] = 0U;

        if (ret != LL_OK)
        {
            s_dl_try[port]++;
            if (s_dl_try[port] >= (uint8_t)RADAR_DL_SENS_TRY_MAX)
            {
                s_dl_st[port]   = 3U;
                s_dl_code[port] = 12U;               /* 读/写不动: 放弃, 等下次下行值变化再试 */
            }
            return;
        }

        if (s_dl_st[port] == 1U)                     /* 读回完成 -> 逐门比对算出待写掩码 */
        {
            s_dl_need[port] = 0U;
            for (i = 0U; i <= (uint8_t)RADAR_DL_SENS_GATE_MAX; i++)
            {
                if (s_dl_cur[port].move_sens[i] != s_dl_sens)
                {
                    s_dl_need[port] |= (uint16_t)(1U << i);
                }
                else if ((i >= 2U) && (s_dl_cur[port].still_sens[i] != (uint8_t)RADAR_DL_SENS_STILL))
                {
                    s_dl_need[port] |= (uint16_t)(1U << i);   /* 门 0/1 的静止灵敏度模块不认, 不比 */
                }
                else
                {
                    /* 这一门已经对上了 */
                }
            }

            s_dl_try[port] = 0U;
            s_dl_st[port]  = 2U;

            if (s_dl_need[port] == 0U)               /* 模块里存的已经是目标值: 一门都不写 */
            {
                s_dl_st[port]   = 3U;
                s_dl_code[port] = (uint8_t)(s_dl_sens / 10U);
            }
        }
        else                                         /* 某一门写成功 -> 清位 */
        {
            s_dl_need[port] &= (uint16_t)(~(uint16_t)(1U << s_dl_gate[port]));
            s_dl_try[port]   = 0U;
            s_dl_code[port]  = 11U;

            if (s_dl_need[port] == 0U)               /* 最后一门也写完了 */
            {
                s_dl_st[port]   = 3U;
                s_dl_code[port] = (uint8_t)(s_dl_sens / 10U);   /* 1..10 = 已生效档位 */
            }
        }
        return;
    }

    /* ============ ② 没有事务在跑: 先判"能不能动它", 再发起 ============ */
    if (radar_probe_done(port) == 0U) { return; }    /* 该口探测还没结束 */

    if (s_baud_locked[port] == 0U)                   /* 该口不在线: 保持待办, 等它上线 */
    {
        s_dl_code[port] = 13U;
        return;
    }

    /* **平静期门控(核心)**: 参数是跟着报警包下来的, 而读/写事务期间模块停上报 ——
     * 报警期间正需要雷达数据, 所以这里一直等到"距最近一次报警下行 >= RADAR_DL_SENS_QUIET_MS"
     * 才动雷达。宁可晚生效, 也不在报警期间把雷达打哑(所以不设强制超时)。 */
    if ((m_u32Tickms - s_dl_last_req_ms) < RADAR_DL_SENS_QUIET_MS)
    {
        s_dl_code[port] = 14U;
        return;
    }

    /* 链路健康互锁(与参数配置同一判据): 只有"最近 1 秒内还解出过合法上报帧"才动它 */
    if ((m_u32Tickms - s_rep_last_ms[port]) > RADAR_REPORT_STALE_MS) { return; }

    if (s_dl_st[port] == 1U)
    {
        if (radar_read_params_begin(port) == LL_OK) { s_dl_op[port] = RADAR_DL_OP_READ; }
        return;
    }

    for (i = 0U; i <= (uint8_t)RADAR_DL_SENS_GATE_MAX; i++)   /* 写掩码里最低的那一门 */
    {
        if ((s_dl_need[port] & (uint16_t)(1U << i)) != 0U)
        {
            s_dl_gate[port] = i;
            if (radar_set_sensitivity_begin(port, (uint16_t)i, (uint16_t)s_dl_sens,
                                            (uint16_t)RADAR_DL_SENS_STILL) == LL_OK)
            {
                s_dl_op[port] = RADAR_DL_OP_WRITE;
            }
            return;
        }
    }

    s_dl_st[port]   = 3U;                            /* 掩码空了(理论上走不到) */
    s_dl_code[port] = (uint8_t)(s_dl_sens / 10U);
}
#else
int32_t radar_set_downlink_range(uint8_t range)
{
    (void)range;
    return LL_OK;                                    /* 本功能关闭: 不碰模块 */
}
#endif
/* ==================== 探测重扫的"急救包": 每档开听前补一帧裸 0x00FE ====================
 * 现场实测(2026-09-15, 数据见 docs/radar_baud_debug_notes.md §10):
 *   - 模块**正常态**收到裸 0x00FE -> 回 ACK 但 status!=0(判"无效"), **上报完全不受影响**(安全);
 *   - 模块**卡在配置态**收到裸 0x00FE -> 回 ACK 且 status=0, **恢复上报**(有效)。
 *   => 它**只在需要它的时候生效**, 这正是我们要的。
 *
 * 为什么每档都发: 模块卡住时我们不知道它在哪一档, 只能在每个候选波特率上各补一帧;
 *   发错档就是乱码, 模块当坏帧丢掉, 无害(帧头/帧尾魔术字也拦掉了)。
 * 为什么不发 0x00FF: **它才会把模块推进配置态**, 是现场明确禁止的; 0x00FE 没有这个语义。
 * 时序: 必须在 radar_port_set_baud() 之后调用(换档要求 TX 空闲), 发完立刻开始听 ——
 *   一帧 11 字节 @9600 也只要 11.5ms, 而本档监听窗口 RADAR_PROBE_LISTEN_MS = 300ms。
 *
 * **门控的两次迭代(现场数据把第一版否掉了, 必须记下来)**:
 *   ① 第一版 = "长时间全静默就补发"(RADAR_FAILSAFE_SILENT_MS = 30s)。**错的** —— 我当时的假设是
 *      "APP 会话期间线上一直有字节, 所以门控挡得住"。但客户在 APP 参数页**盯着不放**时, 线上是
 *      **一个字节都没有**的, 30 秒一到门控就放行, 急救包照样插进 APP 的会话把模块拉出配置态
 *      (现场: 参数页停留久 -> 6401 设置失败 + 雷达 lock=0)。
 *   ② 现在 = **靶向**: 先问"是**谁**把模块留在配置态的"。只有**我们自己的**配置事务(结束帧 0x00FE
 *      没被确认, 或 0x00FF 发出后事务中途崩了)才武装急救; 客户 APP 的会话**永远不会**武装它。
 *      于是"客户在参数页停留多久"跟我们无关: 我们一帧都不发。
 *      (APP 自己会收尾; 万一 APP 崩了把模块留在配置态, 那也不该由我们去打断它 —— 那是 APP 的事。)
 *   第二道才是"线上全静默", 用来避免在别人正常通信时插嘴。
 *
 * **两种武装**(实现见 radar_probe_failsafe):
 *   ① **靶向** —— 是**我们自己**的事务把模块留在配置态(结束帧 0x00FE 没被确认 / 事务中途中止);
 *   ② **长时兜底** —— 该口**彻底静默**超过 RADAR_RESCUE_ABANDON_MS: 现场实测"客户 APP 配完参数
 *      不退出页面直接最小化/关掉", BLE 会话断开, 0x00FE 永远不会来, 模块卡死到只能断电 —— 必须救。
 *      代价不对称: 误伤 = 客户下一条设置命令失败(重试即可); 漏救 = 一路雷达一直死到断电。
 *
 * 为什么要每档都发: 模块卡住时我们不知道它在哪一档, 只能在每个候选波特率上各补一帧;
 *   发错档就是乱码, 模块当坏帧丢掉, 无害(帧头/帧尾魔术字也拦掉了)。一轮最多补 RADAR_RESCUE_SEND_MAX 帧。
 * 为什么不发 0x00FF: **它才会把模块推进配置态**, 是现场明确禁止的; 0x00FE 没有这个语义。
 * 时序: 必须在 radar_port_set_baud() 之后调用(换档要求 TX 空闲), 发完立刻开始听 ——
 *   一帧 11 字节 @9600 也只要 11.5ms, 而本档监听窗口 RADAR_PROBE_LISTEN_MS = 300ms。 */
#if (RADAR_PROBE_FAILSAFE != 0U)
static void radar_probe_failsafe(uint8_t port)
{
    uint8_t  f[RADAR_TX_MAX];
    uint16_t n;
    uint32_t silent_ms;

    silent_ms = m_u32Tickms - s_link_last_rx_ms[port];

    /* 本轮要不要开(每轮最多 RADAR_RESCUE_SEND_MAX 帧, 每档一帧) */
    if (s_rescue_cnt[port] == 0U)
    {
        uint8_t start = 0U;

        /* ① 靶向(核心): 是**我们自己**的配置事务把模块留在配置态 —— 静默够久就补。
         *    武装点只有两处(radar_txn_step_result / radar_txn_abort): 结束帧 0x00FE 没被确认,
         *    或 0x00FF 发出后事务中途中止。客户 APP 的会话**不会**触发这一条。 */
        if ((s_rescue_own[port] != 0U) && (silent_ms >= RADAR_FAILSAFE_SILENT_MS))
        {
            start = 1U;
        }
        /* ② 长时兜底: 不是我们干的, 但模块**长时间彻底不说话** —— 见 radar_cfg.h 的
         *    RADAR_RESCUE_ABANDON_MS(客户 APP 配完参数直接最小化/关掉, 0x00FE 永远没发出来)。
         *    两轮之间至少隔 RADAR_RESCUE_ABANDON_MS, 免得往一条哑线上一直灌。 */
#if (RADAR_RESCUE_ABANDON_MS > 0UL)
        else if ((silent_ms >= RADAR_RESCUE_ABANDON_MS) &&
                 ((m_u32Tickms - s_rescue_last_ms[port]) >= RADAR_RESCUE_ABANDON_MS))
        {
            start = 1U;
        }
#endif
        else
        {
            /* 两条都不满足: 不发 */
        }

        if (start == 0U) { return; }
    }
    else if (s_rescue_cnt[port] >= (uint8_t)RADAR_RESCUE_SEND_MAX)   /* 一轮 8 档都补过了: 收工 */
    {
        s_rescue_cnt[port] = 0U;
        s_rescue_own[port] = 0U;
        return;
    }
    else
    {
        /* 本轮进行中: 继续在下一档补 */
    }

    if (radar_port_tx_busy(port) != 0U) { return; }         /* 换档要求 TX 空闲, 这里再兜一次 */
    n = radar_proto_build_cmd(RADAR_CMD_DISABLE_CFG, 0, 0U, f, sizeof(f));
    if (n != 0U)
    {
        (void)radar_port_write(port, f, n);
        s_rescue_cnt[port]++;
#if (RADAR_RESCUE_ABANDON_MS > 0UL)
        s_rescue_last_ms[port] = m_u32Tickms;
#endif
    }
}
#endif

/* ------------------------------ C. 只读 / 维护 ------------------------------ */
/* 上电读回一次只读信息到 s_dump(供 Keil Watch 查看), 或由 field 手动 radar_dump_start() 触发。
 * **逐拍推进**: 5 项依次"发起->等结果->间隔", 全部由 radar_poll() 驱动, 一次只做一件事。
 * 结果看 s_dump.ok(5 = 全部成功) 与各项 ret_*。 */
#if (RADAR_DUMP_ONCE != 0U)
#define RADAR_DUMP_ITEM_CNT   (5U)
#define RADAR_DUMP_GAP_MS     (50U)      /* 每项之间的间隔(模块连续命令间需要喘口气) */
#define RADAR_DUMP_TRY        (2U)       /* 每项最多试几次 */

static uint8_t  s_dump_done;
static uint8_t  s_dump_item;             /* 当前第几项 */
static uint8_t  s_dump_try;              /* 本项已试次数 */
static uint8_t  s_dump_wait;             /* 1 = 正在等项间间隔 */
static uint32_t s_dump_t0;

static int32_t radar_dump_begin_item(uint8_t item)
{
    switch (item)
    {
        case 0U:  return radar_read_params_begin(0U);
        case 1U:  return radar_read_resolution_begin(0U);
        case 2U:  return radar_read_aux_control_begin(0U);
        case 3U:  return radar_read_fw_version_begin(0U);
        default:  return radar_read_mac_begin(0U);
    }
}

static int32_t radar_dump_poll_item(uint8_t item)
{
    switch (item)
    {
        case 0U:  return radar_read_params_poll(0U, &s_dump.params);
        case 1U:  return radar_read_resolution_poll(0U, &s_dump.resolution);
        case 2U:  return radar_read_aux_control_poll(0U, &s_dump.aux);
        case 3U:  return radar_read_fw_version_poll(0U, &s_dump.fw);
        default:  return radar_read_mac_poll(0U, s_dump.mac, &s_dump.mac_len);
    }
}

static void radar_dump_store(uint8_t item, int32_t ret)
{
    switch (item)
    {
        case 0U:  s_dump.ret_params = ret; break;
        case 1U:  s_dump.ret_res    = ret; break;
        case 2U:  s_dump.ret_aux    = ret; break;
        case 3U:  s_dump.ret_fw     = ret; break;
        default:  s_dump.ret_mac    = ret; break;
    }
}

static void radar_dump_finish(void)
{
    uint8_t n = 0U;

    if (s_dump.ret_params == LL_OK) { n++; }
    if (s_dump.ret_res    == LL_OK) { n++; }
    if (s_dump.ret_aux    == LL_OK) { n++; }
    if (s_dump.ret_fw     == LL_OK) { n++; }
    if (s_dump.ret_mac    == LL_OK) { n++; }

    s_dump.ok       = (uint32_t)n;                  /* 5 = 全部成功 */
    s_dump.last_ret = s_dump.ret_mac;               /* 最后一项(兼容旧用法) */
    s_dump_done     = 1U;
}

/* 手动触发一次读回(非阻塞): 由 radar_poll() 逐拍做完, 结果看 s_dump / radar_dump() */
void radar_dump_start(void)
{
    s_dump_done = 0U;
    s_dump_item = 0U;
    s_dump_try  = 0U;
    s_dump_wait = 0U;
    s_dump.ok   = 0U;
}

static void radar_dump_tick(void)
{
#if (RADAR_PARAM_EN != 0U)
    if (s_param_st[0] < 4U) { return; }             /* 等口 0 的参数配置先做完 */
#endif
    if (s_dump_done != 0U) { return; }
    if (radar_link_ready(0U) == 0U) { return; }

    if (s_dump_wait != 0U)                          /* 项间间隔 */
    {
        if ((m_u32Tickms - s_dump_t0) < (uint32_t)RADAR_DUMP_GAP_MS) { return; }
        s_dump_wait = 0U;
    }

    if (radar_cmd_busy(0U) == 0U)
    {
        int32_t ret;

        if (s_dump_try == 0U)                       /* 本项还没发起 */
        {
            (void)radar_dump_begin_item(s_dump_item);
            s_dump_try = 1U;
            return;
        }

        ret = radar_dump_poll_item(s_dump_item);    /* 上一拍已发起: 取结果 */
        if (ret == LL_ERR_BUSY) { return; }         /* 还没完 */

        radar_dump_store(s_dump_item, ret);

        if (ret == LL_OK)                           /* 该项成功 -> 下一项 */
        {
            s_dump_item++;
            s_dump_try  = 0U;
            if (s_dump_item >= (uint8_t)RADAR_DUMP_ITEM_CNT) { radar_dump_finish(); return; }
            s_dump_wait = 1U;
            s_dump_t0   = m_u32Tickms;
            return;
        }

        s_dump_try++;                               /* 失败 -> 重试 */
        if (s_dump_try > (uint8_t)RADAR_DUMP_TRY)
        {
            s_dump_item++;
            s_dump_try = 0U;
            if (s_dump_item >= (uint8_t)RADAR_DUMP_ITEM_CNT) { radar_dump_finish(); return; }
            s_dump_wait = 1U;
            s_dump_t0   = m_u32Tickms;
        }
        return;
    }
}
#else
void radar_dump_start(void)
{
}
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
 * 结果见 radar_provision_state()。三个口各跑各的(状态按口分开)。
 * 第 1 步的两笔命令走非阻塞事务引擎(两段式), 不再长时间占住主循环。 */
static void radar_provision_tick(uint8_t port)
{
    int32_t ret;

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

        case 1U:                             /* 写波特率 -> 重启模块(两笔都走非阻塞事务) */
            if (s_prov_op[port] == RADAR_PROV_OP_BAUD)          /* 在等 0x00A1 的结果 */
            {
                ret = radar_set_uart_baud_index_poll(port);
                if (ret == LL_ERR_BUSY) { break; }
                s_prov_op[port] = RADAR_PROV_OP_NONE;
                if (ret != LL_OK) { s_prov_st[port] = 5U; break; }

                if (radar_restart_begin(port) == LL_OK) { s_prov_op[port] = RADAR_PROV_OP_RESTART; }
                break;
            }

            if (s_prov_op[port] == RADAR_PROV_OP_RESTART)       /* 在等 0x00A3 的结果 */
            {
                ret = radar_restart_poll(port);
                if (ret == LL_ERR_BUSY) { break; }
                s_prov_op[port] = RADAR_PROV_OP_NONE;
                if (ret != LL_OK) { s_prov_st[port] = 5U; break; }

                s_prov_t0[port] = m_u32Tickms;
                s_prov_st[port] = 2U;
                break;
            }

            if (radar_set_uart_baud_index_begin(port, radar_baud_to_index(RADAR_BAUD_TARGET)) == LL_OK)
            {
                s_prov_op[port] = RADAR_PROV_OP_BAUD;
            }
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

/* 底层泵: 只做"分帧超时 + 搬运该口收到的字节 + 发送泵", **不跑任何状态机**。
 * radar_poll() 每拍每口先调它 —— 非阻塞事务引擎随后就能从 s_ack_ready/s_ack 里看到刚到的 ACK。 */
static void radar_pump(uint8_t port)
{
    uint32_t n0 = radar_port_rx_bytes(port);

    radar_frame_tick(&s_rx[port], m_u32Tickms);
    radar_port_poll(port);
    radar_port_tx_watchdog(port, m_u32Tickms);   /* 发送完成通知没来时的兜底 */

    /* 线上只要来了新字节就刷新"最近活动时刻" —— 原来只在链路监控每秒结算时刷,
     * 粒度粗到 1 秒, 会让"全静默"判据失真(急救包门控与失联兜底都用它)。 */
    if (radar_port_rx_bytes(port) != n0) { s_link_last_rx_ms[port] = m_u32Tickms; }
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
#if (RADAR_PARAM_EN != 0U)
        comm |= ((uint32_t)s_param_code[port] & 0x1FUL) << 11;    /* 参数自动配置进度/结果码 */
#elif (RADAR_DL_SENS_EN != 0U)
        comm |= ((uint32_t)s_dl_code[port] & 0x1FUL) << 11;       /* 报警下行灵敏度进度/结果码 */
#endif
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

        /* 非阻塞命令事务: 每拍每口最多"发一帧 / 查一次 ACK"(见文件后部命令层引擎)。
         * 三个口各自独立推进 —— 事务不再阻塞, 所以不需要"每拍只轮转一个口"那种将就做法。 */
        radar_txn_tick(p);

        radar_probe_tick(p);
#if (RADAR_BAUD_TARGET != 0UL)
        radar_provision_tick(p);         /* 产线配置: 把该口模块波特率配成 RADAR_BAUD_TARGET */
#endif
#if (RADAR_PARAM_EN != 0UL)
        radar_param_tick(p);             /* A: 参数自动配置(幂等, 逐口) */
#endif
#if (RADAR_DL_SENS_EN != 0U)
        radar_dl_sens_tick(p);           /* 报警下行下发的灵敏度(幂等, 逐口) */
#endif
        radar_link_tick(p);              /* 链路监控 + 帧率结算 + g_radar_*[p] 刷新 */

        /* 与串口同一模块的 OUT 脚(可选判定源): 口 p 对应 RADAR_PORTp/PINp */
        s_dev[p].out_present = (GPIO_ReadInputPins(s_out_port[p], s_out_pin[p]) == PIN_SET) ? 1U : 0U;
        s_dev[p].uart_online = ((m_u32Tickms - s_dev[p].last_rx_ms) <= RADAR_REPORT_STALE_MS) ? 1U : 0U;
    }

    radar_dump_tick();                   /* C: 只读信息读回(逐拍推进, 不再阻塞) */
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
/* 该口波特率探测是否已结束(9 = 结束); 命令事务必须等它结束才允许发起 */
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

/* ==================== 命令层: 非阻塞事务引擎 ====================
 * 【为什么不能是阻塞调用】2026-09-15 现场教训: 原 radar_cmd_port() 是"发一帧然后 while 死等 ACK",
 *   单帧最多 200ms; 配置事务 = 使能配置 + 业务命令 + 结束配置, 最坏 600ms 主循环完全停摆。后果:
 *     ① STM32 每 20ms 问一次、200ms 就判「无人」-> 那一路直接漏报;
 *     ② 这段时间 Check_Uart_Pdu() 不跑, RS485 环形缓冲(2048B, 460800 下约 44ms)被写满后
 *        BUF_Write() 静默截断 -> 下行命令丢;
 *     ③ 声光报警(alarm_thread)被推迟。
 *   更糟的是它**恰恰在模块不正常时最长**(ACK 不来才吃满 200ms 超时), 平时根本看不出来。
 *
 * 【现在怎么跑】每口一笔事务, 由 radar_poll() 每拍推进一次, 每拍最多做一件事:
 *   检查 TX 空闲 / 发一帧 / 查一次 ACK —— 绝不进 while 等。主循环单次占用有界。
 *   三个口各自独立推进, 不再需要"每拍只轮转一个口"那种将就做法。
 *
 * 一笔事务 = 1~3 帧命令按序发出, 取指定那一帧的 ACK 当结果:
 *   单命令:   [命令]
 *   配置事务: [使能配置 0x00FF] [业务命令] [结束配置 0x00FE]     <- 结果取中间那帧
 * 使能帧失败时**仍要补发结束帧**(现场教训: 0x00FF 的 ACK 丢了不等于模块没进配置态; 不补发
 *   结束帧模块就停在配置态彻底不再上报, 只能断电救), 所以失败后跳过业务命令直接补末帧,
 *   本次事务的结果仍记为失败。
 *
 * 【对外形状】所有命令接口都是 begin/poll 两段式:
 *   xxx_begin(...)  发起(立即返回; 该口已有在跑的事务 -> LL_ERR_BUSY)
 *   xxx_poll(...)   取结果: LL_ERR_BUSY = 还没做完(下一拍再来); LL_OK = 成功; 其它 = 失败码
 */

static uint8_t radar_txn_active(uint8_t port)
{
    return (s_txn[port].st != RADAR_TXN_IDLE) ? 1U : 0U;
}

/* 往事务里塞一帧(组帧失败返回 0) */
static uint8_t radar_txn_put(uint8_t port, uint8_t idx, uint16_t cmd,
                             const uint8_t *val, uint8_t val_len)
{
    uint16_t n;

    if (idx >= RADAR_TXN_STEP_MAX) { return 0U; }

    n = radar_proto_build_cmd(cmd, val, val_len, s_txn[port].buf[idx], RADAR_TX_MAX);
    if (n == 0U) { return 0U; }

    s_txn[port].cmd[idx] = cmd;
    s_txn[port].len[idx] = (uint8_t)n;
    return 1U;
}

/* 注意 skip_mid 不在这里预置: 它只能由"使能帧真的失败"在运行时置位(见 radar_txn_step_result)。
 * 之前这里对所有配置事务都置 1, 结果无论使能帧成没成都把中间那帧(业务命令)跳过了 ——
 * 表现就是 has_ack=0 / ret=0, 上层拿着没填过的 ack 去解析, 三个口一起报"读回失败"。 */
static void radar_txn_begin(uint8_t port, uint8_t nstep, uint8_t res_step)
{
    radar_txn_t *t = &s_txn[port];

    t->st       = RADAR_TXN_WAIT_TX;
    t->step     = 0U;
    t->nstep    = nstep;
    t->res_step = res_step;
    t->skip_mid = 0U;
    t->ret      = LL_OK;
    t->has_ack  = 0U;
    t->t0       = m_u32Tickms;
}

/* 记结果: 只有"结果帧"的 ACK 决定返回值; 另外使能帧(第 0 帧)失败也算整笔失败。
 * 收尾帧(0x00FE)的结果**不影响**本次事务(与改造前行为一致)。 */
static void radar_txn_step_result(uint8_t port, int32_t ret)
{
    radar_txn_t *t = &s_txn[port];

    if (t->step == t->res_step)
    {
        t->ret = ret;
    }
    else if ((t->step == 0U) && (ret != LL_OK))
    {
        /* 使能帧失败: 记失败码, 并**只在这种情况下**跳过业务命令直接补结束帧
         * (现场教训: 0x00FF 的 ACK 丢了不等于模块没进配置态, 不补 0x00FE 模块会一直哑着) */
        if (t->ret == LL_OK) { t->ret = ret; }
        if (t->nstep == 3U)  { t->skip_mid = 1U; }
    }
    else if ((t->nstep == 3U) && (t->step == 2U))
    {
        /* 收尾帧(0x00FE)的结果**不影响本次事务**, 但它决定"模块有没有回到工作模式":
         * 收尾没被确认 -> 武装靶向急救(重扫时补发); 收尾确认了 -> 撤销武装。 */
#if (RADAR_PROBE_FAILSAFE != 0U)
        s_rescue_own[port] = (ret != LL_OK) ? 1U : 0U;
        s_rescue_cnt[port] = 0U;
#endif
    }
    else { /* 其它帧: 忽略 */ }
}

/* 整体结束(该口回空闲, 结果等上层 poll 取走) */
static void radar_txn_finish(uint8_t port)
{
    radar_txn_t *t = &s_txn[port];

    if (t->nstep > 1U)
    {
        /* 配置事务期间模块会短暂停上报: 把链路监控窗口整体后移, 免得被失联兜底误判成断链 */
        s_link_ms[port]         = m_u32Tickms;
        s_link_bytes0[port]     = radar_port_rx_bytes(port);
        s_link_last_rx_ms[port] = m_u32Tickms;
        s_win_ack_cnt[port]     = 0U;
    }

    t->st = RADAR_TXN_DONE;
}

/* 当前帧结束 -> 下一帧(或整体结束) */
static void radar_txn_next(uint8_t port)
{
    radar_txn_t *t = &s_txn[port];

    t->step++;
    if (t->step >= t->nstep) { radar_txn_finish(port); return; }

    /* 使能帧失败: 跳过业务命令, 直接补最后一帧(结束配置) */
    if ((t->skip_mid != 0U) && (t->step == 1U) && (t->nstep == 3U)) { t->step = 2U; }

    t->st = RADAR_TXN_WAIT_TX;
    t->t0 = m_u32Tickms;
}

/* 中止整笔(用于"连发都发不出去"): 记错误并直接收尾 */
static void radar_txn_abort(uint8_t port, int32_t ret)
{
    radar_txn_t *t = &s_txn[port];

    if (t->ret == LL_OK) { t->ret = ret; }
#if (RADAR_PROBE_FAILSAFE != 0U)
    /* 已经发过第 0 帧(使能配置 0x00FF)就中止 -> 模块可能停在配置态, 武装靶向急救 */
    if ((t->nstep == 3U) && (t->step >= 1U)) { s_rescue_own[port] = 1U; s_rescue_cnt[port] = 0U; }
#endif
    t->skip_mid = 0U;
    t->step     = (uint8_t)(t->nstep - 1U);      /* next() 里 ++ 之后正好 >= nstep */
    radar_txn_next(port);
}

/* 每拍推进一步(由 radar_poll() 逐口调用): **单次调用最多"发一帧"或"查一次"** */
static void radar_txn_tick(uint8_t port)
{
    radar_txn_t *t = &s_txn[port];
    int32_t      r;

    if ((t->st != RADAR_TXN_WAIT_TX) && (t->st != RADAR_TXN_WAIT_ACK)) { return; }

    if (t->st == RADAR_TXN_WAIT_TX)
    {
        if (radar_port_tx_busy(port) != 0U)                  /* 上一帧还在线上: 继续等(不阻塞) */
        {
            if ((m_u32Tickms - t->t0) > RADAR_TXN_TX_WAIT_MS) { radar_txn_abort(port, LL_ERR_BUSY); }
            return;
        }

        s_ack_ready[port] = 0U;
        if (radar_port_write(port, t->buf[t->step], t->len[t->step]) != LL_OK)
        {
            radar_txn_abort(port, LL_ERR);
            return;
        }

        t->t0 = m_u32Tickms;
        t->st = RADAR_TXN_WAIT_ACK;
        return;
    }

    /* RADAR_TXN_WAIT_ACK */
    if (s_ack_ready[port] != 0U)
    {
        if (RADAR_ACK_CMD_MATCH(s_ack[port].cmd, t->cmd[t->step]))    /* 只认本帧命令的 ACK */
        {
            r = (s_ack[port].status == 0U) ? LL_OK : LL_ERR;
            if (t->step == t->res_step) { t->ack = s_ack[port]; t->has_ack = 1U; }
            s_ack_ready[port] = 0U;
            radar_txn_step_result(port, r);
            radar_txn_next(port);
            return;
        }
        s_ack_ready[port] = 0U;                              /* 别的命令的 ACK: 丢弃继续等 */
    }

    if ((m_u32Tickms - t->t0) >= RADAR_CMD_TIMEOUT_MS)
    {
        radar_txn_step_result(port, LL_ERR_TIMEOUT);
        radar_txn_next(port);
    }
}

/* 取结果: LL_ERR_BUSY = 还在跑(下一拍再来); 其它 = 最终结果(取走后该口回空闲) */
static int32_t radar_txn_poll(uint8_t port, radar_ack_t *ack)
{
    radar_txn_t *t = &s_txn[port];

    if ((t->st == RADAR_TXN_WAIT_TX) || (t->st == RADAR_TXN_WAIT_ACK)) { return LL_ERR_BUSY; }
    if (t->st != RADAR_TXN_DONE) { return LL_ERR_NOT_RDY; }   /* 没有在跑的事务 */

    if (ack != 0)
    {
        /* 没收到结果帧的 ACK 时把出参清零 —— 上层(如参数解析)会因此明确失败,
         * 而不是拿着未初始化的栈内存去"解析成功" */
        if (t->has_ack != 0U) { *ack = t->ack; }
        else                  { memset(ack, 0, sizeof(*ack)); }
    }

    t->st = RADAR_TXN_IDLE;
    return t->ret;
}

/* 该口是否还有事务在跑(给上层状态机判断用) */
uint8_t radar_cmd_busy(uint8_t port)
{
    if (port >= (uint8_t)RADAR_PORT_CNT) { return 0U; }
    return radar_txn_active(port);
}

/* 裸命令(单帧, 不套使能/结束配置) */
int32_t radar_cmd_begin(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len)
{
    if (port >= (uint8_t)RADAR_PORT_CNT) { return LL_ERR_INVD_PARAM; }
    if (radar_probe_done(port) == 0U)    { return LL_ERR_NOT_RDY; }    /* 该口波特率探测未结束 */
    if (radar_txn_active(port) != 0U)    { return LL_ERR_BUSY; }
    if (radar_txn_put(port, 0U, cmd, val, val_len) == 0U) { return LL_ERR_INVD_PARAM; }

    radar_txn_begin(port, 1U, 0U);
    return LL_OK;
}

int32_t radar_cmd_poll(uint8_t port, radar_ack_t *ack)
{
    if (port >= (uint8_t)RADAR_PORT_CNT) { return LL_ERR_INVD_PARAM; }
    return radar_txn_poll(port, ack);
}

/* 配置事务: 使能配置 -> 业务命令 -> 结束配置(全部走同一个口) */
static int32_t radar_cfg_begin(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len)
{
    static const uint8_t en[2] = { 0x01U, 0x00U };              /* 0x00FF 的命令值 0x0001 */

    if (port >= (uint8_t)RADAR_PORT_CNT) { return LL_ERR_INVD_PARAM; }
    if (radar_probe_done(port) == 0U)    { return LL_ERR_NOT_RDY; }
    if (radar_txn_active(port) != 0U)    { return LL_ERR_BUSY; }

    if (radar_txn_put(port, 0U, RADAR_CMD_ENABLE_CFG, en, 2U) == 0U)   { return LL_ERR_INVD_PARAM; }
    if (radar_txn_put(port, 1U, cmd, val, val_len) == 0U)              { return LL_ERR_INVD_PARAM; }
    if (radar_txn_put(port, 2U, RADAR_CMD_DISABLE_CFG, 0, 0U) == 0U)   { return LL_ERR_INVD_PARAM; }

    radar_txn_begin(port, 3U, 1U);
    return LL_OK;
}

/* 32 位小端写入(0x0060/0x0064 的载荷用) */
static void radar_put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

/* ---------- 写: 全部是 begin/poll 两段式 ---------- */

/* 0x0064 距离门灵敏度。手册 2.2.7: 距离门 0~8、灵敏度 0~100(100 = 忽略该门); 超范围模块会返回失败。
 * 注: 门 0/1 的**静止**灵敏度不可设置(模块忽略之), 这里不拦 —— 调用方按门遍历时不该被打断。 */
int32_t radar_set_sensitivity_begin(uint8_t port, uint16_t gate, uint16_t move_sens, uint16_t still_sens)
{
    uint8_t v[18];

    if (gate > (uint16_t)RADAR_GATE_MAX) { return LL_ERR_INVD_PARAM; }
    if (move_sens  > 100U) { return LL_ERR_INVD_PARAM; }
    if (still_sens > 100U) { return LL_ERR_INVD_PARAM; }

    v[0] = 0x00U; v[1] = 0x00U;                         /* 距离门字 */
    radar_put_u32le(&v[2], (uint32_t)gate);
    v[6] = 0x01U; v[7] = 0x00U;                         /* 运动灵敏度字 */
    radar_put_u32le(&v[8], (uint32_t)move_sens);
    v[12] = 0x02U; v[13] = 0x00U;                       /* 静止灵敏度字 */
    radar_put_u32le(&v[14], (uint32_t)still_sens);

    return radar_cfg_begin(port, RADAR_CMD_SENSITIVITY, v, (uint8_t)sizeof(v));
}

int32_t radar_set_sensitivity_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x0060 最大距离门与无人持续时间。手册 2.2.3: 运动/静止距离门 2~8(无人持续时间 uint16 天然满足)。
 * 超范围的值模块会返回失败, 不如根本不发 —— 就地拦掉, 连帧都不组。 */
int32_t radar_set_max_gate_begin(uint8_t port, uint16_t move_gate, uint16_t still_gate, uint16_t no_body_sec)
{
    uint8_t v[18];

    if ((move_gate  < 2U) || (move_gate  > (uint16_t)RADAR_GATE_MAX)) { return LL_ERR_INVD_PARAM; }
    if ((still_gate < 2U) || (still_gate > (uint16_t)RADAR_GATE_MAX)) { return LL_ERR_INVD_PARAM; }

    v[0] = 0x00U; v[1] = 0x00U;                         /* 最大运动距离门字 */
    radar_put_u32le(&v[2], (uint32_t)move_gate);
    v[6] = 0x01U; v[7] = 0x00U;                         /* 最大静止距离门字 */
    radar_put_u32le(&v[8], (uint32_t)still_gate);
    v[12] = 0x02U; v[13] = 0x00U;                       /* 无人持续时间字 */
    radar_put_u32le(&v[14], (uint32_t)no_body_sec);

    return radar_cfg_begin(port, RADAR_CMD_MAX_GATE, v, (uint8_t)sizeof(v));
}

int32_t radar_set_max_gate_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x00AA 距离分辨率(要重启模块才生效)。手册: 0 = 0.75m/门, 1 = 0.2m/门 */
int32_t radar_set_resolution_begin(uint8_t port, uint8_t idx)
{
    uint8_t v[2];

    if (idx > 1U) { return LL_ERR_INVD_PARAM; }

    v[0] = idx; v[1] = 0x00U;
    return radar_cfg_begin(port, RADAR_CMD_RESOLUTION, v, (uint8_t)sizeof(v));
}

int32_t radar_set_resolution_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x00A1 串口波特率索引(要重启模块才生效): 0x0007 = 256000, 0x0008 = 460800 */
int32_t radar_set_uart_baud_index_begin(uint8_t port, uint8_t idx)
{
    uint8_t v[2];

    v[0] = idx; v[1] = 0x00U;
    return radar_cfg_begin(port, RADAR_CMD_UART_BAUD, v, (uint8_t)sizeof(v));
}

int32_t radar_set_uart_baud_index_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x0062 / 0x0063 工程模式 */
int32_t radar_eng_mode_begin(uint8_t port, uint8_t on)
{
    return radar_cfg_begin(port, (on != 0U) ? RADAR_CMD_ENG_MODE_ON : RADAR_CMD_ENG_MODE_OFF, 0, 0U);
}

int32_t radar_eng_mode_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x000B 底噪检测(秒) */
int32_t radar_noise_start_begin(uint8_t port, uint16_t sec)
{
    uint8_t v[2];

    v[0] = (uint8_t)(sec & 0xFFU);
    v[1] = (uint8_t)((sec >> 8) & 0xFFU);
    return radar_cfg_begin(port, RADAR_CMD_NOISE_START, v, (uint8_t)sizeof(v));
}

int32_t radar_noise_start_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x00A2 恢复出厂(模块重启后生效) */
int32_t radar_factory_reset_begin(uint8_t port)
{
    return radar_cfg_begin(port, RADAR_CMD_FACTORY_RESET, 0, 0U);
}

int32_t radar_factory_reset_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* 0x00A3 重启模块: 模块在"应答发送完成后"自动重启。
 * 需要重启才生效的配置: 串口波特率(0x00A1)、距离分辨率(0x00AA)、蓝牙、恢复出厂(0x00A2);
 * 灵敏度(0x0064)与最大距离门(0x0060)立即生效。本命令是**裸命令**(不套使能/结束配置)。 */
int32_t radar_restart_begin(uint8_t port)
{
    return radar_cmd_begin(port, RADAR_CMD_RESTART, 0, 0U);
}

int32_t radar_restart_poll(uint8_t port)
{
    return radar_txn_poll(port, 0);
}

/* ---------- 读: 全部是 begin/poll 两段式(poll 出来的结果已经解析好) ---------- */

int32_t radar_read_params_begin(uint8_t port)
{
    return radar_cfg_begin(port, RADAR_CMD_READ_PARAM, 0, 0U);
}

int32_t radar_read_params_poll(uint8_t port, radar_params_t *out)
{
    radar_ack_t ack;
    int32_t     ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_txn_poll(port, &ack);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_params(&ack, out) != 0) ? LL_OK : RADAR_ERR_PARSE;
}

int32_t radar_read_aux_control_begin(uint8_t port)
{
    return radar_cfg_begin(port, RADAR_CMD_AUX_GET, 0, 0U);
}

int32_t radar_read_aux_control_poll(uint8_t port, radar_aux_t *out)
{
    radar_ack_t ack;
    int32_t     ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_txn_poll(port, &ack);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_aux(&ack, out) != 0) ? LL_OK : RADAR_ERR_PARSE;
}

int32_t radar_read_resolution_begin(uint8_t port)
{
    return radar_cfg_begin(port, RADAR_CMD_RESOLUTION_GET, 0, 0U);
}

int32_t radar_read_resolution_poll(uint8_t port, uint8_t *idx)
{
    radar_ack_t ack;
    uint16_t    v;
    int32_t     ret;

    if (idx == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_txn_poll(port, &ack);
    if (ret != LL_OK) { return ret; }
    if (radar_proto_parse_u16(&ack, &v) == 0) { return RADAR_ERR_PARSE; }

    *idx = (uint8_t)(v & 0x00FFU);
    return LL_OK;
}

int32_t radar_read_fw_version_begin(uint8_t port)
{
    return radar_cfg_begin(port, RADAR_CMD_FW_VERSION, 0, 0U);
}

int32_t radar_read_fw_version_poll(uint8_t port, radar_fw_t *out)
{
    radar_ack_t ack;
    int32_t     ret;

    if (out == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_txn_poll(port, &ack);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_fw(&ack, out) != 0) ? LL_OK : RADAR_ERR_PARSE;
}

int32_t radar_read_mac_begin(uint8_t port)
{
    uint8_t v[2];

    v[0] = 0x01U; v[1] = 0x00U;                                 /* 命令值 0x0001 */
    return radar_cfg_begin(port, RADAR_CMD_MAC, v, (uint8_t)sizeof(v));
}

int32_t radar_read_mac_poll(uint8_t port, uint8_t *mac, uint8_t *len)
{
    radar_ack_t ack;
    int32_t     ret;
    uint8_t     got = 0U;

    if (mac == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_txn_poll(port, &ack);
    if (ret != LL_OK) { return ret; }
    if (radar_proto_parse_bytes(&ack, mac, 6U, &got) == 0) { return RADAR_ERR_PARSE; }

    if (len != 0) { *len = got; }
    return LL_OK;
}

int32_t radar_noise_status_begin(uint8_t port)
{
    return radar_cfg_begin(port, RADAR_CMD_NOISE_STATUS, 0, 0U);
}

int32_t radar_noise_status_poll(uint8_t port, uint16_t *status)
{
    radar_ack_t ack;
    int32_t     ret;

    if (status == 0) { return LL_ERR_INVD_PARAM; }

    ret = radar_txn_poll(port, &ack);
    if (ret != LL_OK) { return ret; }

    return (radar_proto_parse_u16(&ack, status) != 0) ? LL_OK : RADAR_ERR_PARSE;
}
