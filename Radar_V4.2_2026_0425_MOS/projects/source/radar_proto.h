/*******************************************************************************
 * radar_proto.h -- LD2410C 协议层: 组命令帧 / 解析 ACK / 解析上报数据
 *
 * 帧格式见 docs/LD2410C 串口通信协议 V1.09.pdf
 *   命令 : FD FC FB FA + len + [cmd(2,LE) + 命令值(N)] + 04 03 02 01
 *   ACK  : FD FC FB FA + len + [cmd(2,LE) + 状态(2,LE) + 返回值(N)] + 04 03 02 01
 *   上报 : F4 F3 F2 F1 + len + [类型(1) + 0xAA + 目标数据 + 0x55 0x00] + F8 F7 F6 F5
 ******************************************************************************/
#ifndef __RADAR_PROTO_H__
#define __RADAR_PROTO_H__

#include "radar_cfg.h"
#include "radar_frame.h"

/* ------------------------------ 命令字 ------------------------------ */
#define RADAR_CMD_ENABLE_CFG        (0x00FFU)   /* 使能配置(值 0x0001) */
#define RADAR_CMD_DISABLE_CFG       (0x00FEU)   /* 结束配置 */
#define RADAR_CMD_MAX_GATE          (0x0060U)   /* 最大距离门 + 无人持续时间 */
#define RADAR_CMD_READ_PARAM        (0x0061U)   /* 读取参数 */
#define RADAR_CMD_ENG_MODE_ON       (0x0062U)   /* 使能工程模式 */
#define RADAR_CMD_ENG_MODE_OFF      (0x0063U)   /* 关闭工程模式 */
#define RADAR_CMD_SENSITIVITY       (0x0064U)   /* 距离门灵敏度 */
#define RADAR_CMD_FW_VERSION        (0x00A0U)   /* 固件版本 */
#define RADAR_CMD_UART_BAUD         (0x00A1U)   /* 串口波特率 */
#define RADAR_CMD_FACTORY_RESET     (0x00A2U)   /* 恢复出厂 */
#define RADAR_CMD_RESTART           (0x00A3U)   /* 重启模块 */
#define RADAR_CMD_MAC               (0x00A5U)   /* MAC 地址 */
#define RADAR_CMD_RESOLUTION        (0x00AAU)   /* 距离分辨率 0=0.75m 1=0.2m */
#define RADAR_CMD_RESOLUTION_GET    (0x00ABU)   /* 查询距离分辨率 */
#define RADAR_CMD_NOISE_START       (0x000BU)   /* 开始底噪检测+灵敏度自动配置(值=秒) */
#define RADAR_CMD_NOISE_STATUS      (0x001BU)   /* 查询底噪检测状态 0/1/2 */

/* 上报数据类型 */
#define RADAR_REPORT_TYPE_BASIC     (0x02U)     /* 目标基本信息 */
#define RADAR_REPORT_TYPE_ENGINEER  (0x01U)     /* 工程模式(含 9 门能量/光感) */

/* 目标状态 */
#define RADAR_STATE_NONE            (0x00U)     /* 无目标 */
#define RADAR_STATE_MOVING          (0x01U)     /* 运动目标 */
#define RADAR_STATE_STILL           (0x02U)     /* 静止目标 */
#define RADAR_STATE_MOVING_STILL    (0x03U)     /* 运动+静止 */
#define RADAR_STATE_NOISE_RUNNING   (0x04U)     /* 底噪检测中 */
#define RADAR_STATE_NOISE_OK        (0x05U)     /* 底噪检测成功 */
#define RADAR_STATE_NOISE_FAIL      (0x06U)     /* 底噪检测失败 */

#define RADAR_GATE_MAX              (8U)        /* 距离门 0..8 */
#define RADAR_ACK_RET_MAX           (32U)

/* ------------------------------ 数据结构 ------------------------------ */
typedef struct {
    uint16_t cmd;                               /* 命令字 */
    uint16_t status;                            /* 0 = 成功, 其它 = 失败 */
    uint8_t  ret[RADAR_ACK_RET_MAX];            /* 返回值(状态之后的部分) */
    uint8_t  ret_len;
} radar_ack_t;

typedef struct {
    uint8_t  target_state;                      /* RADAR_STATE_x */
    uint16_t moving_distance_cm;
    uint8_t  moving_energy;
    uint16_t still_distance_cm;
    uint8_t  still_energy;
    uint16_t detect_distance_cm;
    uint8_t  eng_mode;                          /* 1 = 本帧含工程模式数据 */
    uint8_t  max_move_gate;                     /* 工程模式 */
    uint8_t  max_still_gate;
    uint8_t  move_gate_energy[RADAR_GATE_MAX + 1U];
    uint8_t  still_gate_energy[RADAR_GATE_MAX + 1U];
    uint8_t  light_sensor;                      /* 光感值 0..255 */
    uint8_t  out_pin;                           /* 模块 OUT 脚状态 */
} radar_report_t;

typedef struct {
    uint8_t  max_gate;                          /* 最大距离门 N */
    uint8_t  max_move_gate;
    uint8_t  max_still_gate;
    uint8_t  move_sens[RADAR_GATE_MAX + 1U];
    uint8_t  still_sens[RADAR_GATE_MAX + 1U];
    uint16_t no_body_sec;                       /* 无人持续时间(秒) */
} radar_params_t;

/* ------------------------------ API ------------------------------ */
/* 组一帧命令, 返回整帧长度(0 = 参数错误) */
uint16_t radar_proto_build_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                               uint8_t *out, uint16_t out_cap);

int radar_proto_parse_ack(const radar_frame_t *f, radar_ack_t *out);
int radar_proto_parse_report(const radar_frame_t *f, radar_report_t *out);
int radar_proto_parse_params(const radar_ack_t *ack, radar_params_t *out);
int radar_proto_parse_u16(const radar_ack_t *ack, uint16_t *out);   /* 取 2 字节返回值 */

#endif /* __RADAR_PROTO_H__ */
