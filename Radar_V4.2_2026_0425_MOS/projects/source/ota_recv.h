/**
 * @file  ota_recv.h
 * @brief 报警板侧 OTA1 接收端（HC32F460，A/B 双槽、无搬运）—— ota_host 的对端
 *
 * 对端：HC32F4A0 主机的 ota_host.c（上位机端发送器）。
 * 帧：与 ota_frame.h 同一套（"OTA1" + type + seq(2LE) + len(2LE) + payload + CRC16(2LE)）。
 * 偏移语义：【串口语义】含 82B 包头的绝对偏移（与 ota_host 严格一致）。
 *
 * 行为（对齐 C# OtaUpdater 的串口对端语义）：
 *   - 包头帧（DATA seq=0, payload=82B）：校验包一致性 -> 选定【非活动槽】为目标 -> 回 ACK(82)
 *   - 数据帧：按页粒度惰性擦写目标槽 -> 每块回读校验 -> 回 ACK(已写绝对偏移)
 *   - 帧 CRC16 错：回 RESUME(当前偏移) 请求重发
 *   - 收满：整镜像 CRC32 校验 -> 写选择器标志（active 切到目标槽 + TRIAL + NEED_CONFIRM）
 *           -> 回 ACK(total) -> 置 DONE，由上层决定复位
 *   - 会话空闲超时：退出升级模式，恢复业务
 *
 * 注意：包 payload 即【槽镜像】（含 ota_layout.h 定义的槽镜像头，TargetSlot 须与本槽一致）；
 *       主机须先查得目标槽再选对应镜像下发 —— 可用 VER1 / SLOT 查询帧。
 */
#ifndef OTA_RECV_H
#define OTA_RECV_H

#include <stdint.h>
#include "ota_layout.h"

/* 分块上限：与 ota_host 的 OTA_HOST_MAX_PAYLOAD 一致（落在 512B DMA 窗口内） */
#define OTA_RX_MAX_PAYLOAD      256u
#define OTA_RX_PKG_HDR_LEN      82u
#define OTA_RX_FRAME_MAX        (9u + OTA_RX_MAX_PAYLOAD + 2u)
#define OTA_RX_IDLE_MS          10000u   /* 会话空闲超时 */

#ifndef OTA_RX_FW_VERSION
#define OTA_RX_FW_VERSION       0u       /* 由工程侧覆盖（版本查询用） */
#endif

typedef enum
{
    OTA_RX_OFF = 0,     /* 未在升级模式（业务态） */
    OTA_RX_WAIT_HDR,    /* 升级模式，等包头帧 */
    OTA_RX_DATA,        /* 正在写非活动槽 */
    OTA_RX_DONE,        /* 校验通过、标志已写，待上层复位 */
    OTA_RX_FAIL         /* 失败 */
} ota_rx_state_t;

/* 结果：0 = 无；1 = 升级完成待复位；-1 = 失败 */
#define OTA_RX_RESULT_NONE      0
#define OTA_RX_RESULT_OK        1
#define OTA_RX_RESULT_FAIL      (-1)

void           ota_recv_init(void);
/* 进入/退出升级模式（进入后上层必须停掉该链路全部业务帧） */
void           ota_recv_enter(void);
void           ota_recv_exit(void);
uint8_t        ota_recv_busy(void);
ota_rx_state_t ota_recv_state(void);
uint32_t       ota_recv_progress(void);   /* 已写偏移（含 82B 包头） */
uint32_t       ota_recv_total(void);      /* 包总长（0 = 未知） */
int            ota_recv_result(void);     /* 见 OTA_RX_RESULT_* */
void           ota_recv_result_clear(void);

/* 喂入链路收到的字节（升级模式下由主循环调用） */
void           ota_recv_feed(const uint8_t *buf, uint32_t len);
/* 会话心跳（周期调用，传毫秒时基） */
void           ota_recv_tick(uint32_t now_ms);
/* 立刻回一帧 ACK（调试/上报用） */
void           ota_recv_send_ack(uint32_t offset);

#endif /* OTA_RECV_H */
