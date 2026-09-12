/*******************************************************************************
 * radar_proto.c -- LD2410C 协议层实现
 ******************************************************************************/
#include "radar_proto.h"

#define RP_HDR_LEN      (6U)    /* FD FC FB FA + len(2) */

static uint16_t rp_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint16_t radar_proto_build_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                               uint8_t *out, uint16_t out_cap)
{
    uint16_t total;
    uint8_t  len;
    uint8_t  i;

    if (out == 0) { return 0U; }

    len = (uint8_t)(val_len + 2U);
    total = (uint16_t)len + RADAR_FRAME_OVERHEAD;   /* 4 头 + 2 长度 + len + 4 尾 */
    if (total > out_cap) { return 0U; }

    out[0] = 0xFDU;
    out[1] = 0xFCU;
    out[2] = 0xFBU;
    out[3] = 0xFAU;
    out[4] = len;
    out[5] = 0x00U;
    out[6] = (uint8_t)(cmd & 0xFFU);
    out[7] = (uint8_t)((cmd >> 8) & 0xFFU);

    for (i = 0U; i < val_len; i++) { out[8U + i] = val[i]; }

    out[8U + val_len]      = 0x04U;
    out[8U + val_len + 1U] = 0x03U;
    out[8U + val_len + 2U] = 0x02U;
    out[8U + val_len + 3U] = 0x01U;

    return total;
}

int radar_proto_parse_ack(const radar_frame_t *f, radar_ack_t *out)
{
    uint8_t i;

    if ((f == 0) || (out == 0)) { return 0; }
    if (f->kind != RADAR_FRAME_KIND_ACK) { return 0; }
    if (f->data_len < 4U) { return 0; }             /* cmd(2) + status(2) */

    out->cmd = rp_u16_le(&f->data[0]);
    out->status = rp_u16_le(&f->data[2]);
    out->ret_len = (uint8_t)(f->data_len - 4U);
    if (out->ret_len > RADAR_ACK_RET_MAX) { out->ret_len = RADAR_ACK_RET_MAX; }

    for (i = 0U; i < out->ret_len; i++) { out->ret[i] = f->data[4U + i]; }

    return 1;
}

int radar_proto_parse_u16(const radar_ack_t *ack, uint16_t *out)
{
    if ((ack == 0) || (out == 0)) { return 0; }
    if (ack->ret_len < 2U) { return 0; }

    *out = rp_u16_le(&ack->ret[0]);
    return 1;
}

int radar_proto_parse_params(const radar_ack_t *ack, radar_params_t *out)
{
    uint8_t  n;
    uint8_t  i;
    uint8_t  need;

    if ((ack == 0) || (out == 0)) { return 0; }
    if (ack->ret_len < 4U) { return 0; }
    if (ack->ret[0] != 0xAAU) { return 0; }         /* 数据头 */

    n = ack->ret[1];
    if (n > RADAR_GATE_MAX) { return 0; }

    /* 0xAA + N + max_move + max_still + move[0..N] + still[0..N] + no_body(2) */
    need = (uint8_t)(4U + 2U * (uint8_t)(n + 1U) + 2U);
    if (ack->ret_len < need) { return 0; }

    out->max_gate = n;
    out->max_move_gate = ack->ret[2];
    out->max_still_gate = ack->ret[3];

    for (i = 0U; i <= (uint8_t)RADAR_GATE_MAX; i++)
    {
        out->move_sens[i] = 0U;
        out->still_sens[i] = 0U;
    }

    for (i = 0U; i <= n; i++)
    {
        out->move_sens[i] = ack->ret[4U + i];
        out->still_sens[i] = ack->ret[4U + (uint8_t)(n + 1U) + i];
    }

    out->no_body_sec = rp_u16_le(&ack->ret[4U + 2U * (uint8_t)(n + 1U)]);

    return 1;
}

int radar_proto_parse_report(const radar_frame_t *f, radar_report_t *out)
{
    const uint8_t *d;
    uint8_t  type;
    uint8_t  i;
    uint8_t  idx;

    if ((f == 0) || (out == 0)) { return 0; }
    if (f->kind != RADAR_FRAME_KIND_REPORT) { return 0; }
    if (f->data_len < 13U) { return 0; }

    d = f->data;
    type = d[0];
    if (d[1] != 0xAAU) { return 0; }
    if ((d[f->data_len - 2U] != 0x55U) || (d[f->data_len - 1U] != 0x00U)) { return 0; }

    /* 基本信息(两种模式都有) */
    out->target_state = d[2];
    out->moving_distance_cm = rp_u16_le(&d[3]);
    out->moving_energy = d[5];
    out->still_distance_cm = rp_u16_le(&d[6]);
    out->still_energy = d[8];
    out->detect_distance_cm = rp_u16_le(&d[9]);

    out->eng_mode = 0U;
    out->max_move_gate = 0U;
    out->max_still_gate = 0U;
    out->light_sensor = 0U;
    out->out_pin = 0U;
    for (i = 0U; i <= (uint8_t)RADAR_GATE_MAX; i++)
    {
        out->move_gate_energy[i] = 0U;
        out->still_gate_energy[i] = 0U;
    }

    if (type == RADAR_REPORT_TYPE_BASIC)
    {
        return 1;
    }

    if (type != RADAR_REPORT_TYPE_ENGINEER)
    {
        return 0;
    }

    /* 工程模式: 基本信息之后追加 最大运动门/最大静止门 + 9+9 门能量 + 光感 + OUT */
    if (f->data_len < 35U) { return 0; }

    idx = 11U;
    out->max_move_gate = d[idx]; idx++;
    out->max_still_gate = d[idx]; idx++;

    for (i = 0U; i <= (uint8_t)RADAR_GATE_MAX; i++) { out->move_gate_energy[i] = d[idx]; idx++; }
    for (i = 0U; i <= (uint8_t)RADAR_GATE_MAX; i++) { out->still_gate_energy[i] = d[idx]; idx++; }

    out->light_sensor = d[idx]; idx++;
    out->out_pin = d[idx]; idx++;
    out->eng_mode = 1U;

    return 1;
}
