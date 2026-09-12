/*******************************************************************************
 * radar_frame.h -- LD2410C 串口分帧(纯逻辑, 不依赖硬件, 可在 PC 上单测)
 *
 * 两种帧(帧头/帧尾都是 4 字节魔术字, 按 datalen 变长):
 *   命令/ACK : FD FC FB FA + len(2,小端) + data + 04 03 02 01
 *   上报数据 : F4 F3 F2 F1 + len(2,小端) + data + F8 F7 F6 F5
 ******************************************************************************/
#ifndef __RADAR_FRAME_H__
#define __RADAR_FRAME_H__

#include "radar_cfg.h"

/* 帧类型 */
#define RADAR_FRAME_KIND_NONE       (0U)
#define RADAR_FRAME_KIND_ACK        (1U)    /* 命令/ACK 帧(FD FC FB FA ...) */
#define RADAR_FRAME_KIND_REPORT     (2U)    /* 雷达上报帧(F4 F3 F2 F1 ...) */

/* 帧内数据长度 + 帧尾长度上限保护 */
#define RADAR_FRAME_OVERHEAD        (10U)   /* 4(头) + 2(长度) + 4(尾) */

typedef struct {
    uint8_t  kind;                              /* RADAR_FRAME_KIND_x */
    uint16_t data_len;                          /* 帧内数据长度(N) */
    uint8_t  data[RADAR_FRAME_MAX];             /* 帧内数据(不含头/长度/尾) */
} radar_frame_t;

/* 分帧状态机 */
typedef struct {
    uint8_t  state;                             /* 0=找帧头 1=收长度 2=收数据 3=收帧尾 */
    uint8_t  kind;
    uint16_t idx;                               /* 当前状态已收字节数 */
    uint16_t data_len;
    uint16_t total;                             /* 本帧总长度 */
    uint8_t  buf[RADAR_FRAME_MAX];              /* 组装中的整帧 */
    uint32_t last_byte_ms;
    uint32_t ok_cnt;
    uint32_t err_cnt;                           /* 帧尾/长度非法 */
    uint32_t resync_cnt;                        /* 非帧头字节丢弃数 */
    uint32_t timeout_cnt;                       /* 半包超时复位次数 */
} radar_frame_rx_t;

void radar_frame_init(radar_frame_rx_t *rx);

/* 喂入 1 字节; 返回 1 表示 out 里得到一个完整帧, 否则 0 */
int radar_frame_feed(radar_frame_rx_t *rx, uint8_t b, uint32_t now_ms, radar_frame_t *out);

/* 周期调用(建议每拍 1ms~10ms): 半包超时复位 */
void radar_frame_tick(radar_frame_rx_t *rx, uint32_t now_ms);

#endif /* __RADAR_FRAME_H__ */
