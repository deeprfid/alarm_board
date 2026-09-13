/*******************************************************************************
 * radar_dbg.h -- 雷达驱动"上板验证"用的调试输出(临时模块)
 *
 * 目的: 上板确认 LD2410C 串口链路是否通、收到的是不是 0x02 上报、目标状态与
 *       距离是否合理。业务代码不依赖本模块。
 *
 * 实现放在 radar.c 末尾的 "#if (RADAR_DBG_EN != 0U)" 段里 —— 不新建 .c 文件,
 * 因此不用改 Keil 工程(Keil GUI 打开时会把 .uvprojx 的改动覆盖掉)。
 *
 * 验证完成后:
 *   ① radar_cfg.h 里 RADAR_DBG_EN 置 0(空实现, 不占 Flash); 或
 *   ② 删本文件 + radar.c 里 "#if (RADAR_DBG_EN != 0U)" 到文件末尾的整段
 *      + main.c 的 radar_dbg_poll() + main.h 的 #include "radar_dbg.h"
 *
 * 输出通道(见 radar_cfg.h 的 RADAR_DBG_SINK_xxx):
 *   SINK_KEIL  : 结构体 g_radar_dbg(Keil Watch 里一眼看全) + 字符串 g_radar_dbg_line
 *   SINK_ITM   : SWO/ITM, Keil Debug printf Viewer
 *   SINK_RS485 : 从 RS485 主机口(USART4)打 ASCII 行(默认关, 会抢总线)
 ******************************************************************************/
#ifndef __RADAR_DBG_H__
#define __RADAR_DBG_H__

#include "radar_cfg.h"

#define RADAR_DBG_LINE_MAX              (160U)      /* 状态行缓冲(含结尾 \r\n) */
#define RADAR_DBG_EVT_MAX               (64U)       /* 事件行缓冲(含结尾 \r\n) */
#define RADAR_DBG_HEX_BYTES             (24U)       /* 抓多少字节原始数据 */
#define RADAR_DBG_HEX_MAX               (96U)       /* 十六进制缓冲 */

/* 最近一次刷新的状态快照(Keil Watch 里加 g_radar_dbg 即可, 全部是十进制整数) */
typedef struct {
    uint32_t ms;            /* 当前 1ms 计数(确认主循环/SysTick 在跑) */
    uint32_t rdy;           /* 波特率自适应是否已结束(1=结束) */
    uint32_t lock;          /* 自适应是否命中(1=命中, 0=全失败回落 256000) */
    uint32_t baud;          /* 当前波特率 */
    uint32_t rep;           /* 解析出的上报帧数 */
    uint32_t fok;           /* 分帧成功帧数 */
    uint32_t fer;           /* 分帧错误帧数 */
    uint32_t rx;            /* 串口累计收到字节数 */
    uint32_t drp;           /* 环形缓冲丢弃字节数 */
    uint32_t st;            /* 目标状态 0 无 / 1 运动 / 2 静止 / 3 动静 / 4~6 底噪 */
    uint32_t mv_dist;       /* 运动目标距离 cm */
    uint32_t mv_eng;        /* 运动目标能量 */
    uint32_t st_dist;       /* 静止目标距离 cm */
    uint32_t st_eng;        /* 静止目标能量 */
    uint32_t dd;            /* 探测距离 cm */
    uint32_t out;           /* 模块 OUT 脚电平 */
    uint32_t online;        /* 串口数据是否新鲜(1=在线) */
    uint32_t pre;           /* radar_presence(0) 最终判定 */
} radar_dbg_snap_t;

#if (RADAR_DBG_EN != 0U)
extern volatile radar_dbg_snap_t g_radar_dbg;
extern char                      g_radar_dbg_line[RADAR_DBG_LINE_MAX];  /* 最新状态行 */
extern char                      g_radar_dbg_evt[RADAR_DBG_EVT_MAX];    /* 最近事件 */
extern char                      g_radar_dbg_hex[RADAR_DBG_HEX_MAX];  /* 本波特率下收到的前几个字节(十六进制) */
extern uint32_t                  g_radar_dbg_cnt;                       /* 已刷新的状态行数 */
extern uint32_t                  g_radar_dbg_evt_cnt;                   /* 已记录的事件数 */
#endif

/* 主循环每拍调用(内部按 RADAR_DBG_PERIOD_MS 节流; RADAR_DBG_EN=0 时为空实现) */
void radar_dbg_poll(void);
/* 手工记一条事件(如参数读写结果), RADAR_DBG_EN=0 时为空实现 */
void radar_dbg_note(const char *tag);

#endif /* __RADAR_DBG_H__ */
