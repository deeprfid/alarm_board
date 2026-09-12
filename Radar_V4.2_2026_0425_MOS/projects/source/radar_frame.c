/*******************************************************************************
 * radar_frame.c -- LD2410C 串口分帧实现(纯逻辑, 无硬件依赖)
 *
 * 逐字节状态机: 找帧头(4B 滑窗) -> 收长度(2B 小端) -> 收数据(N) -> 收帧尾(4B)
 * 每字节只消费一次; 长度或帧尾非法 -> 丢帧重新找帧头并计数。
 ******************************************************************************/
#include "radar_frame.h"

static const uint8_t s_magic_ack[4]    = { 0xFD, 0xFC, 0xFB, 0xFA };
static const uint8_t s_magic_report[4] = { 0xF4, 0xF3, 0xF2, 0xF1 };
static const uint8_t s_tail_ack[4]     = { 0x04, 0x03, 0x02, 0x01 };
static const uint8_t s_tail_report[4]  = { 0xF8, 0xF7, 0xF6, 0xF5 };

#define RF_ST_SCAN      (0U)
#define RF_ST_LEN       (1U)
#define RF_ST_DATA      (2U)
#define RF_ST_TAIL      (3U)

#define RF_HDR_LEN      (6U)    /* 4 头 + 2 长度, 在 buf 中的固定位置 */

void radar_frame_init(radar_frame_rx_t *rx)
{
    uint16_t i;

    if (rx == 0) { return; }

    rx->state = RF_ST_SCAN;
    rx->kind = RADAR_FRAME_KIND_NONE;
    rx->idx = 0U;
    rx->data_len = 0U;
    rx->total = 0U;
    rx->last_byte_ms = 0U;
    rx->ok_cnt = 0U;
    rx->err_cnt = 0U;
    rx->resync_cnt = 0U;
    rx->timeout_cnt = 0U;

    for (i = 0U; i < RADAR_FRAME_MAX; i++) { rx->buf[i] = 0U; }
}

static void rf_reset(radar_frame_rx_t *rx)
{
    rx->state = RF_ST_SCAN;
    rx->kind = RADAR_FRAME_KIND_NONE;
    rx->idx = 0U;
    rx->data_len = 0U;
    rx->total = 0U;
}

/* 找帧头: buf[0..3] 组成 4 字节滑窗 */
static void rf_scan(radar_frame_rx_t *rx, uint8_t b)
{
    rx->buf[rx->idx] = b;
    rx->idx++;

    if (rx->idx < 4U) { return; }

    if ((rx->buf[0] == s_magic_ack[0]) && (rx->buf[1] == s_magic_ack[1]) &&
        (rx->buf[2] == s_magic_ack[2]) && (rx->buf[3] == s_magic_ack[3]))
    {
        rx->kind = RADAR_FRAME_KIND_ACK;
        rx->state = RF_ST_LEN;
        rx->idx = 0U;
        return;
    }

    if ((rx->buf[0] == s_magic_report[0]) && (rx->buf[1] == s_magic_report[1]) &&
        (rx->buf[2] == s_magic_report[2]) && (rx->buf[3] == s_magic_report[3]))
    {
        rx->kind = RADAR_FRAME_KIND_REPORT;
        rx->state = RF_ST_LEN;
        rx->idx = 0U;
        return;
    }

    /* 不是帧头: 窗口右移 1 字节继续找 */
    rx->buf[0] = rx->buf[1];
    rx->buf[1] = rx->buf[2];
    rx->buf[2] = rx->buf[3];
    rx->idx = 3U;
    rx->resync_cnt++;
}

int radar_frame_feed(radar_frame_rx_t *rx, uint8_t b, uint32_t now_ms, radar_frame_t *out)
{
    if ((rx == 0) || (out == 0)) { return 0; }

    rx->last_byte_ms = now_ms;

    for (;;)
    {
        switch (rx->state)
        {
            case RF_ST_SCAN:
                rf_scan(rx, b);
                return 0;                       /* 本字节已被消费 */

            case RF_ST_LEN:
                if (rx->idx == 0U) { rx->data_len = (uint16_t)b; }
                else               { rx->data_len |= (uint16_t)((uint16_t)b << 8); }
                rx->idx++;

                if (rx->idx < 2U) { return 0; } /* 还差 1 字节 */

                if (rx->data_len > (uint16_t)(RADAR_FRAME_MAX - RADAR_FRAME_OVERHEAD))
                {
                    rx->err_cnt++;              /* 长度非法 */
                    rf_reset(rx);
                    return 0;
                }

                rx->buf[4] = (uint8_t)(rx->data_len & 0xFFU);
                rx->buf[5] = (uint8_t)((rx->data_len >> 8) & 0xFFU);
                rx->total = (uint16_t)(rx->data_len + RADAR_FRAME_OVERHEAD);
                rx->idx = 0U;
                rx->state = (rx->data_len == 0U) ? RF_ST_TAIL : RF_ST_DATA;
                return 0;                       /* 本字节已被消费 */

            case RF_ST_DATA:
                rx->buf[RF_HDR_LEN + rx->idx] = b;
                rx->idx++;
                if (rx->idx >= rx->data_len)
                {
                    rx->state = RF_ST_TAIL;
                    rx->idx = 0U;
                }
                return 0;

            case RF_ST_TAIL:
                rx->buf[RF_HDR_LEN + rx->data_len + rx->idx] = b;
                rx->idx++;
                if (rx->idx >= 4U)
                {
                    const uint8_t *tail = (rx->kind == RADAR_FRAME_KIND_ACK) ? s_tail_ack : s_tail_report;
                    const uint8_t *p = &rx->buf[RF_HDR_LEN + rx->data_len];

                    if ((p[0] == tail[0]) && (p[1] == tail[1]) && (p[2] == tail[2]) && (p[3] == tail[3]))
                    {
                        uint16_t i;

                        out->kind = rx->kind;
                        out->data_len = rx->data_len;
                        for (i = 0U; i < rx->data_len; i++) { out->data[i] = rx->buf[RF_HDR_LEN + i]; }
                        rx->ok_cnt++;
                        rf_reset(rx);
                        return 1;
                    }

                    rx->err_cnt++;              /* 帧尾非法 */
                    rf_reset(rx);
                }
                return 0;

            default:
                rf_reset(rx);
                return 0;
        }
    }
}

void radar_frame_tick(radar_frame_rx_t *rx, uint32_t now_ms)
{
    if (rx == 0) { return; }
    if (rx->state == RF_ST_SCAN) { return; }

    if ((now_ms - rx->last_byte_ms) > RADAR_FRAME_GAP_MS)
    {
        rx->timeout_cnt++;
        rf_reset(rx);
    }
}
