/*******************************************************************************
 * radar.h -- LD2410C 雷达服务层(设备状态 + 命令事务 + 有人判定)
 *
 * 用法:
 *   radar_init();                       // 上电初始化一次(含波特率自适应探测)
 *   while (1) { radar_poll(); ... }     // 主循环每拍调用(非阻塞)
 *   radar_read_params(&p);              // 读参数(阻塞式, 内部自行 poll, 有超时)
 *   radar_set_sensitivity(3, 40, 40);   // 设距离门 3 灵敏度
 ******************************************************************************/
#ifndef __RADAR_H__
#define __RADAR_H__

#include "radar_cfg.h"
#include "radar_frame.h"
#include "radar_proto.h"

/* "有人"判定来源 */
#define RADAR_SRC_OUT               (0U)    /* 只用模块 OUT 脚(现有行为) */
#define RADAR_SRC_UART              (1U)    /* 只用串口目标状态 */
#define RADAR_SRC_OUT_OR_UART       (2U)    /* 二者取或 */
#define RADAR_SRC_UART_FALLBACK_OUT (3U)    /* 串口在线用串口, 掉线回落 OUT */

/* 事件(可选回调) */
#define RADAR_EVT_REPORT            (1U)    /* 收到上报帧 */
#define RADAR_EVT_ACK               (2U)    /* 收到 ACK */
#define RADAR_EVT_FRAME_ERR         (3U)    /* 帧错误(长度/帧尾非法) */

typedef struct {
    uint8_t  out_present;                   /* 模块 OUT 脚电平 */
    uint8_t  uart_online;                   /* 串口数据是否新鲜 */
    uint32_t last_rx_ms;                    /* 最近一次有效上报时刻 */
    radar_report_t rep;                     /* 串口解析结果 */
} radar_dev_t;

/* ---------------- 生命周期 ---------------- */
int32_t radar_init(void);
void    radar_poll(void);

/* ---------------- 状态查询 ---------------- */
const radar_dev_t *radar_dev(uint8_t dev);
const radar_report_t *radar_report(uint8_t dev);
uint8_t radar_presence(uint8_t dev);                    /* 按策略给出有人/无人 */
uint8_t radar_uart_online(uint8_t dev);
uint8_t radar_baud_locked(void);                        /* 自适应探测是否命中 */
uint8_t radar_ready(void);                            /* 自适应探测是否已结束 */
uint32_t radar_get_baud(void);
void    radar_set_presence_src(uint8_t src);
uint8_t radar_presence_src(void);

/* ---------------- 统计 ---------------- */
uint32_t radar_frames_ok(void);
uint32_t radar_frames_err(void);
uint32_t radar_reports(void);

/* ---------------- 命令 ---------------- */
/* 通用命令(自动等待 ACK); 返回 LL_OK / LL_ERR / LL_ERR_TIMEOUT / LL_ERR_INVD_PARAM */
int32_t radar_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                  radar_ack_t *ack, uint32_t timeout_ms);

/* 语义化封装(内部自动"使能配置 -> 命令 -> 结束配置") */
int32_t radar_read_params(radar_params_t *out);                     /* 0x0061 */
int32_t radar_set_sensitivity(uint16_t gate, uint16_t move_sens, uint16_t still_sens); /* 0x0064 */
int32_t radar_set_max_gate(uint16_t move_gate, uint16_t still_gate, uint16_t no_body_sec); /* 0x0060 */
int32_t radar_set_resolution(uint8_t idx);                          /* 0x00AA */
int32_t radar_set_uart_baud_index(uint8_t idx);                     /* 0x00A1 */
int32_t radar_eng_mode(uint8_t on);                                 /* 0x0062 / 0x0063 */
int32_t radar_noise_start(uint16_t sec);                            /* 0x000B */
int32_t radar_noise_status(uint16_t *status);                       /* 0x001B: 0 未执行 1 执行中 2 完成 */

#endif /* __RADAR_H__ */
