/*******************************************************************************
 * radar_dbg.h -- 雷达驱动调试快照(临时模块, 只给 Keil Watch 看)
 *
 * 本板没有连电脑的串口, 所以**不做任何串口/printf/文本输出**:
 * 诊断信息放在结构体 g_radar_dbg 里(纯数值 + 一个 8 字节数组), 调试时在 Keil 的
 * Watch 窗口加入 g_radar_dbg 展开即可。
 *
 * 实现放在 radar.c 末尾的 "#if (RADAR_DBG_EN != 0U)" 段里 —— 不新建 .c 文件,
 * 不需要改 Keil 工程(Keil GUI 打开时会覆盖 .uvprojx)。
 *
 * 验证完成后: ① radar_cfg.h 里 RADAR_DBG_EN 置 0(空实现, 不占 Flash); 或
 *             ② 删本文件 + radar.c 里 "#if (RADAR_DBG_EN != 0U)" 到文件末尾的整段
 *                + main.c 的 radar_dbg_poll() + main.h 的 #include "radar_dbg.h"
 ******************************************************************************/
#ifndef __RADAR_DBG_H__
#define __RADAR_DBG_H__

#include "radar_cfg.h"

#define RADAR_DBG_RX_HEAD               (8U)    /* 每个波特率下记录的最前面几个字节 */

/* 快照: Keil Watch 里加 g_radar_dbg 后展开, 全是十进制数值 */
typedef struct {
    uint32_t ms;            /* 1ms 计数(在涨 = 主循环/SysTick 活着) */
    uint32_t probe_st;      /* 自适应探测阶段: 0 等待 / 1 已切档待发 / 2 判定 / 4 复检 / 9 结束 */
    uint32_t lock;          /* 是否找到模块波特率(1 = 找到; 0 = 8 档都不应答, 已回落) */
    uint32_t baud;          /* 当前波特率(探测中 = 正在试的那一档) */
    uint32_t prov_st;       /* 目标波特率配置状态: 4 成功/本来就是; 5 没切成(保持原波特率) */
    uint32_t rep;           /* 解析成功的目标上报数(持续增长 = 链路通) */
    uint32_t fer;           /* 分帧错误数(在涨 = 收到的不是合法帧, 波特率/接线有问题) */
    uint32_t rx;            /* 串口累计收到字节数(0 = 一个字节都没收到) */
    uint32_t st;            /* 目标状态: 0 无 / 1 运动 / 2 静止 / 3 动静 / 4~6 底噪 */
    uint32_t mv_dist;       /* 运动目标距离 cm */
    uint32_t mv_eng;        /* 运动目标能量 */
    uint32_t st_dist;       /* 静止目标距离 cm */
    uint32_t st_eng;        /* 静止目标能量 */
    uint32_t dd;            /* 探测距离 cm */
    uint32_t out;           /* 模块 OUT 脚电平 */
    uint32_t online;        /* 串口数据是否新鲜(1s 内) */
    uint32_t pre;           /* radar_presence(0) 最终判定 */
    uint8_t  rx_head[RADAR_DBG_RX_HEAD];    /* 本波特率下收到的最前面几个字节:
                                             * 以 F4 F3 F2 F1 开头 = 上报帧头(波特率对) */
} radar_dbg_snap_t;

#if (RADAR_DBG_EN != 0U)
extern volatile radar_dbg_snap_t g_radar_dbg;
#endif

/* 主循环每拍调用(内部按 RADAR_DBG_PERIOD_MS 刷新快照; RADAR_DBG_EN=0 时为空实现) */
void radar_dbg_poll(void);

#endif /* __RADAR_DBG_H__ */
