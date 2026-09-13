/*******************************************************************************
 * radar_dbg.h -- 雷达驱动"上板验证"调试快照(临时模块, 只给 Keil Watch 看)
 *
 * 本板没有连电脑的串口, 所以本模块**不做任何串口/printf/文本输出**:
 * 所有诊断信息都放在结构体 g_radar_dbg 里(纯数值 + 少量字节数组), 调试时在
 * Keil 的 Watch 窗口加入 g_radar_dbg 展开即可。
 *
 * 实现放在 radar.c 末尾的 "#if (RADAR_DBG_EN != 0U)" 段里 —— 不新建 .c 文件,
 * 不需要改 Keil 工程(Keil GUI 打开时会覆盖 .uvprojx)。
 *
 * 验证完成后:
 *   ① radar_cfg.h 里 RADAR_DBG_EN 置 0(空实现, 不占 Flash); 或
 *   ② 删本文件 + radar.c 里 "#if (RADAR_DBG_EN != 0U)" 到文件末尾的整段
 *      + main.c 的 radar_dbg_poll() + main.h 的 #include "radar_dbg.h"
 ******************************************************************************/
#ifndef __RADAR_DBG_H__
#define __RADAR_DBG_H__

#include "radar_cfg.h"

#define RADAR_DBG_EVT_DEPTH             (4U)    /* 事件环形记录条数(最近 4 条) */
#define RADAR_DBG_RX_HEAD               (8U)    /* 每个波特率下记录的最前面几个字节 */
#define RADAR_DBG_ACK_DATA              (8U)    /* 最近一帧 ACK 的原始数据字节数 */

/* 事件码: 记录在 g_radar_dbg.evt_code[] / evt_val[] / evt_ms[] 里(环形, 深度 4) */
#define RADAR_DBG_EV_NONE               (0U)
#define RADAR_DBG_EV_BOOT               (1U)    /* val = 0 */
#define RADAR_DBG_EV_BAUD_CHANGE        (2U)    /* val = 新波特率 */
#define RADAR_DBG_EV_TRY_BAUD           (3U)    /* val = 正在试的波特率 */
#define RADAR_DBG_EV_ACK_VERIFY         (4U)    /* val = 波特率(收到 ACK, 进入上报帧验证) */
#define RADAR_DBG_EV_LOCK_ACK           (5U)    /* val = 波特率(ACK + 上报帧验证通过) */
#define RADAR_DBG_EV_LOCK_FRAMES        (6U)    /* val = 波特率(仅靠上报帧认定) */
#define RADAR_DBG_EV_NO_REPORT          (7U)    /* val = 波特率(有 ACK 但收不到上报帧) */
#define RADAR_DBG_EV_PROBE_FAIL         (8U)    /* val = 回落的波特率 */
#define RADAR_DBG_EV_FRAME_ERR          (9U)    /* val = 累计错误帧数 */
#define RADAR_DBG_EV_ST_CHANGE          (10U)   /* val = 新的目标状态 */
#define RADAR_DBG_EV_RX_STALL           (11U)   /* val = 停滞时的累计收字节数 */
#define RADAR_DBG_EV_SETBAUD_OK         (12U)   /* 产线配置: 0x00A1 写入成功, val = 波特率索引 */
#define RADAR_DBG_EV_SETBAUD_FAIL       (13U)   /* val = 返回码 */
#define RADAR_DBG_EV_RESTART_SENT       (14U)   /* 产线配置: 0x00A3 已发, val = 目标波特率 */
#define RADAR_DBG_EV_RESTART_FAIL       (15U)   /* val = 返回码 */
#define RADAR_DBG_EV_DRIVER_BAUD        (16U)   /* 产线配置: 模块已重启, 开始重新探测复检 */
#define RADAR_DBG_EV_VERIFY_OK          (17U)   /* val = 波特率(自检通过) */
#define RADAR_DBG_EV_VERIFY_FALLBACK    (18U)   /* val = 回退到的波特率 */

/* 快照: Keil Watch 里加 g_radar_dbg 后展开, 全是十进制数值 */
typedef struct {
    uint32_t ms;            /* 当前 1ms 计数(确认主循环/SysTick 在跑) */
    uint32_t rdy;           /* 波特率自适应是否已结束(1=结束) */
    uint32_t lock;          /* 自适应是否命中(1=命中, 0=8 档都没应答, 已回落) */
    uint32_t baud;          /* 当前波特率 */
    uint32_t probe_st;      /* 探测状态机状态(0 待启动 / 1 已切档待发 / 2 判定 / 4 验证 / 9 结束) */
    uint32_t probe_idx;     /* 当前候选档序号(0..7) */
    uint32_t prov_st;       /* 产线配置状态(RADAR_BAUD_TARGET 启用时): 4=成功/已是目标值 5=失败已回退 */
    uint32_t rep;           /* 解析成功的目标上报数 */
    uint32_t repf;          /* 收到的上报帧数(F4F3F2F1, 按档清零) */
    uint32_t ackf;          /* 收到的 ACK 帧数(FDFCFBFA, 按档清零) */
    uint32_t fok;           /* 总分帧成功数 */
    uint32_t fer;           /* 总分帧错误数 */
    uint32_t rx;            /* 串口累计收到字节数 */
    uint32_t drp;           /* 接收环形缓冲丢弃字节数 */
    uint32_t st;            /* 目标状态 0 无 / 1 运动 / 2 静止 / 3 动静 / 4~6 底噪 */
    uint32_t mv_dist;       /* 运动目标距离 cm */
    uint32_t mv_eng;        /* 运动目标能量 */
    uint32_t st_dist;       /* 静止目标距离 cm */
    uint32_t st_eng;        /* 静止目标能量 */
    uint32_t dd;            /* 探测距离 cm */
    uint32_t out;           /* 模块 OUT 脚电平 */
    uint32_t online;        /* 串口数据是否新鲜(1=在线) */
    uint32_t pre;           /* radar_presence(0) 最终判定 */
    uint32_t ack_cmd;       /* 最近一帧 ACK 的命令字(原始, 未做低字节匹配) */
    uint32_t ack_status;    /* 最近一帧 ACK 的状态字 */
    uint32_t ack_len;       /* 最近一帧 ACK 的数据长度 */
    uint8_t  ack_data[RADAR_DBG_ACK_DATA];  /* 最近一帧 ACK 的原始数据字节 */
    uint8_t  rx_head[RADAR_DBG_RX_HEAD];    /* 本波特率下收到的最前面几个字节 */
    uint32_t evt_cnt;                       /* 事件总数(环形下标 = cnt % 4) */
    uint32_t evt_code[RADAR_DBG_EVT_DEPTH]; /* 最近 4 条事件码 */
    uint32_t evt_val[RADAR_DBG_EVT_DEPTH];  /* 对应的事件值(多是波特率/计数) */
    uint32_t evt_ms[RADAR_DBG_EVT_DEPTH];   /* 对应的事件时刻(ms) */
} radar_dbg_snap_t;

#if (RADAR_DBG_EN != 0U)
extern volatile radar_dbg_snap_t g_radar_dbg;
#endif

/* 主循环每拍调用(内部按 RADAR_DBG_PERIOD_MS 刷新快照; RADAR_DBG_EN=0 时为空实现) */
void radar_dbg_poll(void);
/* 记一条事件(数值形式), RADAR_DBG_EN=0 时为空实现 */
void radar_dbg_note_u32(uint8_t code, uint32_t val);

#endif /* __RADAR_DBG_H__ */
