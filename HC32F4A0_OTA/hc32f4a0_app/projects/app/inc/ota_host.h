/**
 * @file ota_host.h
 * @brief OTA1 帧协议「上位机端」发送器（纯逻辑，零硬件依赖）
 *
 * 目标端：HC32F460 报警板（A/B 双槽 + Boot，见 docs/ota_boot_design.md v0.2）
 * 运行端：HC32F4A0 主机（取代 PC 上的 C# 上位机串口 OTA）
 *
 * 帧格式：[0:4]"OTA1" [4]type [5:7]seq(16LE) [7:9]len(16LE) [9:9+N]payload [末2]CRC16(LE)
 *   type  0x50=DATA(主机->设备) 0x51=ACK 0x52=RESUME（ACK/RESUME 载荷 = 4B LE 偏移）
 *   CRC16 CCITT-FALSE, poly 0x1021, init 0xFFFF, 覆盖 [0 : 9+len]
 *
 * 偏移语义 = 【串口语义】：含 82B 包头的绝对偏移，total = len(pkg)。
 *   切勿混用 USB-CDC/WinUSB 的「载荷相对偏移」语义（见 §9.2）。
 *
 * 控制流对齐 C# ReaderUI_v1_MCU/OtaUpdater.cs 的 Update()（串口分支）与 tools/ota_send.py，
 * 但实现为【非阻塞状态机】，由集成层周期调用 ota_host_poll() 推进：
 *   包头握手(只发一次,4s) -> 分块发送(停等,3s) -> 超时发 len=0 探测帧(2.5s) 跟随设备偏移
 *   -> 设备确认的最大偏移 10s 不推进则中止 -> 进度满后发一次探测帧兜底。
 */
#ifndef OTA_HOST_H
#define OTA_HOST_H

#include <stdint.h>

/* ---- 结果码 ---- */
#define OTA_HOST_BUSY          1    /* 仍在进行 */
#define OTA_HOST_OK            0    /* 全部发完 */
#define OTA_HOST_ERR_PARAM    (-1)
#define OTA_HOST_ERR_IO       (-2)
#define OTA_HOST_ERR_STALL    (-4)  /* 设备进度长时间不推进 */

/* ---- 阶段 ---- */
typedef enum {
    OTA_HOST_IDLE = 0,
    OTA_HOST_HANDSHAKE,
    OTA_HOST_STREAM,
    OTA_HOST_FINISH,
    OTA_HOST_DONE,
    OTA_HOST_FAILED
} ota_host_phase_t;

/* ---- I/O 回调：由集成层提供（F4A0 里就是 read/write(fd) + osKernelGetTickCount） ---- */
typedef struct {
    /* 写链路；返回 0 成功，<0 失败 */
    int      (*write)(void *ctx, const uint8_t *buf, uint32_t len);
    /* 非阻塞读；返回 >0 实际字节数，0 = 暂无数据，<0 错误 */
    int      (*read)(void *ctx, uint8_t *buf, uint32_t len);
    /* 单调毫秒 */
    uint32_t (*tick_ms)(void *ctx);
    /* 进度回调，可 NULL（报告的偏移已由设备确认） */
    void     (*on_progress)(void *ctx, uint32_t off, uint32_t total);
    /* 设备 TRACE 文本行（与 OTA 帧共用一条线），可 NULL */
    void     (*on_trace)(void *ctx, const uint8_t *line, uint32_t len);
    void     *ctx;
} ota_host_io_t;

/* ---- 默认参数（对齐 C# OtaUpdater.cs / tools/ota_send.py） ---- */
#define OTA_HOST_PKG_HDR_LEN      82u    /* 统一 OTA 包头长 */
#define OTA_HOST_MAX_PAYLOAD      256u   /* 分块上限：落在雷达板 512B DMA 窗口内 */
#define OTA_HOST_HANDSHAKE_MS     4000u  /* 首帧包头握手超时 */
#define OTA_HOST_ACK_MS           3000u  /* 每帧 ACK 超时 */
#define OTA_HOST_PROBE_MS         2500u  /* 探测帧超时 */
#define OTA_HOST_STALL_MS         10000u /* 设备确认偏移无推进则中止 */
#define OTA_HOST_FINISH_MS        500u   /* 收尾等待（设备随后自行校验+复位） */
#define OTA_HOST_RX_SMALL         64u    /* ACK/RESUME 帧长上限（plen<=4 → 15B） */
#define OTA_HOST_TX_MAX           (9u + OTA_HOST_MAX_PAYLOAD + 2u)

typedef struct {
    ota_host_phase_t phase;
    ota_host_io_t    io;

    const uint8_t   *pkg;
    uint32_t         total;        /* 含 82B 包头 */
    uint32_t         off;          /* 待确认的偏移（发送后不乐观推进） */
    uint32_t         acked_max;    /* 设备确认过的最大偏移（卡死判据） */
    uint16_t         seq;
    uint16_t         max_payload;
    uint16_t         sent_len;     /* 本帧发送长度（重发用） */

    uint8_t          tx_stage;     /* 0 = 需发送, 1 = 等待设备应答 */
    uint8_t          probed;       /* 本次超时是否已发过探测帧 */

    uint32_t         t_phase;      /* 当前等待的起始时刻 */
    uint32_t         t_progress;   /* 上次设备推进时刻 */
    int              result;

    /* 接收状态机（设备回帧；非帧字节按 TRACE 文本处理） */
    uint8_t          rx_buf[OTA_HOST_RX_SMALL];
    uint8_t          rx_len;
    uint8_t          rx_sync;      /* 1 = 已同步到 magic */
    uint8_t          rx_have;      /* 同步阶段已匹配的 magic 字节数 */
    uint16_t         rx_plen;
    uint8_t          trace_buf[64];
    uint8_t          trace_len;
} ota_host_t;

/* 初始化（io 结构体被浅拷贝；max_payload 传 0 用默认值） */
void ota_host_init(ota_host_t *h, const ota_host_io_t *io, uint16_t max_payload);

/* 开始一次下发；pkg 必须含 82B 包头（前 4 字节为 "OTA1"）。返回 OTA_HOST_BUSY 或错误码 */
int  ota_host_start(ota_host_t *h, const uint8_t *pkg, uint32_t pkg_len);

/* 非阻塞推进；返回 OTA_HOST_BUSY 需继续调用，其他值为最终结果（同时写入 h->result） */
int  ota_host_poll(ota_host_t *h);

/* 查询设备固件版本：RESUME(seq=0xFFFF, payload="VER1") -> ACK(4B 版本)。返回版本号或 -1 */
int  ota_host_query_version(ota_host_t *h, uint32_t *ver_out);

#endif /* OTA_HOST_H */
