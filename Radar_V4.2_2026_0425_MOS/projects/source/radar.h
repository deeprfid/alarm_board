/*******************************************************************************
 * radar.h -- LD2410C 雷达服务层(设备状态 + 命令事务 + 有人判定)
 *
 * 用法:
 *   radar_init();                       // 上电初始化一次(含波特率自适应探测)
 *   while (1) { radar_poll(); ... }     // 主循环每拍调用(非阻塞)
 *   radar_read_params(&p);              // 读参数(阻塞式, 内部自行 poll, 有超时)
 *   radar_set_sensitivity(3, 40, 40);   // 设距离门 3 灵敏度
 *
 * 第 3 步(多口): 三路雷达(USART1/2/3)各跑各的。设备号 == 口号(0/1/2)。
 *   - 不带口号的旧接口一律是"口 0 兼容入口"(既有调用点不用改);
 *   - 需要指定口时用 radar_cmd_port() / radar_restart_port() 等 _port 版本。
 ******************************************************************************/
#ifndef __RADAR_H__
#define __RADAR_H__

#include "radar_cfg.h"
#include "radar_frame.h"
#include "radar_proto.h"
#include "radar_port.h"     /* RADAR_PORT_CNT + radar_port_* 硬件层接口 */

/* "有人"判定来源 */
#define RADAR_SRC_OUT               (0U)    /* 只用模块 OUT 脚(现有行为) */
#define RADAR_SRC_UART              (1U)    /* 只用串口目标状态 */
#define RADAR_SRC_OUT_OR_UART       (2U)    /* 二者取或 */
#define RADAR_SRC_UART_FALLBACK_OUT (3U)    /* 串口在线用串口, 掉线回落 OUT */

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
uint8_t radar_baud_locked(void);                        /* 自适应探测是否命中(口 0 兼容入口) */
uint8_t radar_ready(void);                              /* 自适应探测是否已结束(口 0 兼容入口) */
uint32_t radar_get_baud(void);                          /* 当前波特率(口 0 兼容入口) */
uint8_t radar_provision_state(void);                    /* 产线配置波特率状态: 4=已是目标值/成功, 5=失败已回退 */
void    radar_set_presence_src(uint8_t src);
uint8_t radar_presence_src(void);

/* ---------------- 统计 ---------------- */
uint32_t radar_frames_ok(void);
uint32_t radar_frames_err(void);
/* ---------------- C. 只读 / 维护 ---------------- */
int32_t radar_read_resolution(uint8_t *idx);                 /* 0x00AB: 0=0.75m/门 1=0.2m/门 */
int32_t radar_read_aux_control(radar_aux_t *out);            /* 0x00AE */
int32_t radar_read_aux_control_port(uint8_t port, radar_aux_t *out);   /* 0x00AE, 指定口 */
int32_t radar_read_fw_version(radar_fw_t *out);              /* 0x00A0 */
int32_t radar_read_mac(uint8_t *mac, uint8_t *len);          /* 0x00A5 */
int32_t radar_factory_reset(void);                           /* 0x00A2(重启后生效) */
int32_t radar_read_all(void);                                /* 依次读回全部只读信息到 s_dump */

/* 只读信息汇总: Keil Watch 里加 s_dump, 或看 radar_dump() */
typedef struct {
    uint32_t       ok;          /* 本次读回成功的项数(5 = 全部成功) */
    int32_t        last_ret;    /* 最后一项的返回码(LL_OK = 0 / -1 模块没回 / -6 忙 / -8 超时) */
    int32_t        ret_params;  /* 0x0061 读参数的返回码 */
    int32_t        ret_res;     /* 0x00AB 读距离分辨率 */
    int32_t        ret_aux;     /* 0x00AE 读辅助控制 */
    int32_t        ret_fw;      /* 0x00A0 读固件版本 */
    int32_t        ret_mac;     /* 0x00A5 读 MAC */
    radar_params_t params;      /* 0x0061: 最大门/各门灵敏度/无人持续时间 */
    uint8_t        resolution;  /* 0x00AB */
    radar_aux_t    aux;         /* 0x00AE */
    radar_fw_t     fw;          /* 0x00A0 */
    uint8_t        mac[6];      /* 0x00A5 */
    uint8_t        mac_len;
} radar_dump_t;

const radar_dump_t *radar_dump(void);

/* ---------------- A 参数自动配置(幂等) ---------------- */
uint8_t radar_param_state(void);        /* 口 0 兼容入口; 逐口状态请看 g_radar_comm 的 bit11..15 */
uint8_t radar_param_state_port(uint8_t port);   /* 指定口的自动配置状态 */

uint32_t radar_reports(void);

/* ---------------- 报警下行下发雷达灵敏度 ----------------
 * 入参 = 报警下行包 alarm_pdu 的 Alarm_Duration[1]:
 *   0 = 不设置(保持模块现状) / 1~10 -> 动态(运动)灵敏度 10~100, 静态灵敏度恒 100。
 * **非阻塞**: 只登记目标值, 真正的"先读回、只写不一致的门"由 radar_poll() 逐拍幂等推进;
 * 重复值一条命令都不发。返回 LL_OK / LL_ERR_INVD_PARAM(值 > 10)。
 * 取值与状态码说明见 radar_cfg.h 的 RADAR_DL_SENS_*。 */
int32_t radar_set_downlink_range(uint8_t range);

/* 现场 Watch 只看这 3 个。
 * 第 3 步起每口一份: 仍是 3 个变量, 但各自是 [RADAR_PORT_CNT] 数组, 下标 0/1/2 = 雷达口 0/1/2。
 * 位图/BRR 指纹/帧率编码与单口版**完全一致**, 定义与读法见 radar.c 文件头。 */
extern volatile uint32_t g_radar_lock[RADAR_PORT_CNT];
extern volatile uint32_t g_radar_baud[RADAR_PORT_CNT];
extern volatile uint32_t g_radar_comm[RADAR_PORT_CNT];
uint32_t radar_rx_bytes(void);                         /* 串口累计收到字节数(诊断, 口 0) */
uint32_t radar_rx_drop(void);                          /* 接收缓冲丢弃字节数(诊断, 口 0) */

/* ---------------- 命令 ---------------- */
/* 通用命令(自动等待 ACK); 返回 LL_OK / LL_ERR / LL_ERR_TIMEOUT / LL_ERR_INVD_PARAM */
int32_t radar_cmd(uint16_t cmd, const uint8_t *val, uint8_t val_len,
                  radar_ack_t *ack, uint32_t timeout_ms);

/* 按口通用命令: 命令/ACK 事务只落在该口。radar_cmd() 等价于本函数 port=0。 */
int32_t radar_cmd_port(uint8_t port, uint16_t cmd, const uint8_t *val, uint8_t val_len,
                       radar_ack_t *ack, uint32_t timeout_ms);

/* 语义化封装(内部自动"使能配置 -> 命令 -> 结束配置") */
int32_t radar_read_params(radar_params_t *out);                     /* 0x0061 */
int32_t radar_set_sensitivity(uint16_t gate, uint16_t move_sens, uint16_t still_sens); /* 0x0064 */
int32_t radar_set_max_gate(uint16_t move_gate, uint16_t still_gate, uint16_t no_body_sec); /* 0x0060 */
int32_t radar_set_resolution(uint8_t idx);                          /* 0x00AA */
int32_t radar_set_uart_baud_index(uint8_t idx);                     /* 0x00A1 */
int32_t radar_set_uart_baud_index_port(uint8_t port, uint8_t idx);  /* 0x00A1, 指定口 */
int32_t radar_restart(void);                                        /* 0x00A3: 应答后模块自动重启 */
int32_t radar_restart_port(uint8_t port);                           /* 0x00A3, 指定口 */
int32_t radar_eng_mode(uint8_t on);                                 /* 0x0062 / 0x0063 */
int32_t radar_noise_start(uint16_t sec);                            /* 0x000B */
int32_t radar_noise_status(uint16_t *status);                       /* 0x001B: 0 未执行 1 执行中 2 完成 */
int32_t radar_set_aux_control(uint8_t mode, uint8_t threshold, uint8_t out_level);  /* 0x00AD */

/* 语义化封装的**按口**版本(参数逐口配置用); 不带口号的同名函数等价于 port = 0。
 * 注意: 分辨率(0x00AA/0x00AB)、固件版本、MAC、恢复出厂、工程模式、底噪检测目前**只有口 0 版本**。 */
int32_t radar_read_params_port(uint8_t port, radar_params_t *out);                  /* 0x0061 */
int32_t radar_set_max_gate_port(uint8_t port, uint16_t move_gate,
                                uint16_t still_gate, uint16_t no_body_sec);         /* 0x0060 */
int32_t radar_set_sensitivity_port(uint8_t port, uint16_t gate,
                                   uint16_t move_sens, uint16_t still_sens);        /* 0x0064 */
int32_t radar_set_aux_control_port(uint8_t port, uint8_t mode,
                                   uint8_t threshold, uint8_t out_level);           /* 0x00AD */

#endif /* __RADAR_H__ */
