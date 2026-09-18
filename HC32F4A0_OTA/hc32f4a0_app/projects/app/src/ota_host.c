/**
 * @file ota_host.c
 * @brief OTA1 帧协议「上位机端」发送器实现（纯逻辑，零硬件依赖）
 */
#include <string.h>
#include "ota_host.h"
#include "ota_frame.h"   /* 复用平台无关核心：ota_frame_build / ota_frame_crc16 */

/* ---------- 小工具 ---------- */
static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static void ota_host_fail(ota_host_t *h, int err)
{
    h->phase  = OTA_HOST_FAILED;
    h->result = err;
}

static void ota_host_finish_enter(ota_host_t *h, uint32_t now)
{
    /* 进度已满：转入 FINISH，由该分支发一次探测帧兜底；
     * 设备此时自行 校验 -> 激活标志 -> 复位 */
    h->phase    = OTA_HOST_FINISH;
    h->tx_stage = 0u;
    h->t_phase  = now;
}

/* 发一帧；返回 0 成功 */
static int tx_frame(ota_host_t *h, uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t plen)
{
    uint8_t frame[OTA_HOST_TX_MAX];
    int flen;

    if (((uint32_t)plen + 9u + 2u) > OTA_HOST_TX_MAX) {
        return -1;
    }
    flen = ota_frame_build(type, seq, payload, plen, frame);
    if (flen < 0) {
        return -1;
    }
    return h->io.write(h->io.ctx, frame, (uint32_t)flen);
}

static int tx_probe(ota_host_t *h)
{
    return tx_frame(h, OTA_FRAME_TYPE_DATA, 0xFFFFu, NULL, 0u);
}

/* 发当前分块（off 处），不推进 off */
static int tx_chunk(ota_host_t *h)
{
    uint32_t chunk = min_u32((uint32_t)h->max_payload, h->total - h->off);
    uint16_t seq   = (uint16_t)(h->off / h->max_payload + 1u);

    if (chunk == 0u) {
        return -1;
    }
    if (tx_frame(h, OTA_FRAME_TYPE_DATA, seq, &h->pkg[h->off], (uint16_t)chunk) != 0) {
        return -1;
    }
    h->sent_len = (uint16_t)chunk;
    h->seq      = seq;
    return 0;
}

/* ---------- 设备回帧接收 ---------- */
static void trace_flush(ota_host_t *h)
{
    if (h->trace_len > 0u) {
        if (h->io.on_trace != NULL) {
            h->io.on_trace(h->io.ctx, h->trace_buf, (uint32_t)h->trace_len);
        }
        h->trace_len = 0u;
    }
}

static void trace_put(ota_host_t *h, uint8_t b)
{
    if (b == (uint8_t)'\n') {
        trace_flush(h);
        return;
    }
    if (b == 0u) {
        return;   /* 忽略填充字节 */
    }
    if (h->trace_len >= (uint8_t)sizeof(h->trace_buf)) {
        trace_flush(h);
    }
    h->trace_buf[h->trace_len++] = b;
}

static void rx_reset(ota_host_t *h)
{
    h->rx_len  = 0u;
    h->rx_sync = 0u;
    h->rx_have = 0u;
    h->rx_plen = 0u;
}

/* 解析已收满的帧；成功返回 1 */
static int rx_parse(ota_host_t *h, uint8_t *type_out, uint32_t *off_out)
{
    uint16_t crc_rx;
    uint16_t crc_calc;

    if (h->rx_len < 9u + 2u) {
        return 0;
    }
    if (h->rx_buf[0] != (uint8_t)OTA_FRAME_MAGIC0 || h->rx_buf[1] != (uint8_t)OTA_FRAME_MAGIC1 ||
        h->rx_buf[2] != (uint8_t)OTA_FRAME_MAGIC2 || h->rx_buf[3] != (uint8_t)OTA_FRAME_MAGIC3) {
        return 0;
    }
    if (h->rx_plen < 4u || h->rx_plen > (uint16_t)(OTA_HOST_RX_SMALL - 9u - 2u)) {
        return 0;
    }
    if (h->rx_len != (uint8_t)(9u + h->rx_plen + 2u)) {
        return 0;
    }
    crc_rx   = (uint16_t)(h->rx_buf[9u + h->rx_plen] | ((uint16_t)h->rx_buf[9u + h->rx_plen + 1u] << 8));
    crc_calc = ota_frame_crc16(h->rx_buf, (uint32_t)(9u + h->rx_plen));
    if (crc_rx != crc_calc) {
        return 0;
    }
    if (h->rx_buf[4] != (uint8_t)OTA_FRAME_TYPE_ACK && h->rx_buf[4] != (uint8_t)OTA_FRAME_TYPE_RESUME) {
        return 0;
    }
    if (type_out != NULL) { *type_out = h->rx_buf[4]; }
    if (off_out != NULL)  { *off_out  = get_u32(&h->rx_buf[9]); }
    return 1;
}

/*
 * 从链路取字节喂进接收状态机。返回 1 = 收到一帧（type/off 输出），0 = 暂无完整帧。
 * 逐字节同步 "OTA1"；非帧字节按设备 TRACE 文本按行回调（与 C# ReadAck 一致）。
 */
static int rx_pump(ota_host_t *h, uint8_t *type_out, uint32_t *off_out)
{
    static const uint8_t magic[4] = {
        (uint8_t)OTA_FRAME_MAGIC0, (uint8_t)OTA_FRAME_MAGIC1,
        (uint8_t)OTA_FRAME_MAGIC2, (uint8_t)OTA_FRAME_MAGIC3
    };
    uint8_t b;
    int n;
    uint8_t i;

    for (;;) {
        n = h->io.read(h->io.ctx, &b, 1u);
        if (n <= 0) {
            return 0;   /* 无数据或错误：交给上层按超时处理 */
        }

        if (h->rx_sync == 0u) {
            if (b == magic[h->rx_have]) {
                h->rx_buf[h->rx_have] = b;
                h->rx_have++;
                if (h->rx_have >= 4u) {
                    h->rx_len  = 4u;
                    h->rx_sync = 1u;
                    h->rx_have = 0u;
                }
            } else {
                /* 已缓冲的部分匹配 + 当前字节都算 TRACE（按原顺序输出） */
                for (i = 0u; i < h->rx_have; i++) {
                    trace_put(h, h->rx_buf[i]);
                }
                h->rx_have = 0u;
                trace_put(h, b);
            }
            continue;
        }

        /* 已同步：收满 9 + plen + 2 */
        if (h->rx_len >= (uint8_t)sizeof(h->rx_buf)) {
            rx_reset(h);
            continue;
        }
        h->rx_buf[h->rx_len++] = b;

        if (h->rx_len == 9u) {
            h->rx_plen = (uint16_t)(h->rx_buf[7] | ((uint16_t)h->rx_buf[8] << 8));
            if (h->rx_plen > (uint16_t)(OTA_HOST_RX_SMALL - 9u - 2u)) {
                rx_reset(h);   /* 非 ACK/RESUME 帧长，丢弃重同步 */
                continue;
            }
        } else if (h->rx_len >= 11u && h->rx_len == (uint8_t)(9u + h->rx_plen + 2u)) {
            int r = rx_parse(h, type_out, off_out);
            rx_reset(h);
            if (r == 1) {
                return 1;
            }
            continue;
        }
    }
}

/* ---------- 对外 ---------- */
void ota_host_init(ota_host_t *h, const ota_host_io_t *io, uint16_t max_payload)
{
    memset(h, 0, sizeof(*h));
    if (io != NULL) {
        h->io = *io;
    }
    h->max_payload = (max_payload == 0u) ? (uint16_t)OTA_HOST_MAX_PAYLOAD : max_payload;
    if (h->max_payload > (uint16_t)OTA_HOST_MAX_PAYLOAD) {
        h->max_payload = (uint16_t)OTA_HOST_MAX_PAYLOAD;
    }
    h->phase  = OTA_HOST_IDLE;
    h->result = OTA_HOST_BUSY;
}

int ota_host_start(ota_host_t *h, const uint8_t *pkg, uint32_t pkg_len)
{
    uint32_t now;

    if (pkg == NULL || pkg_len < OTA_HOST_PKG_HDR_LEN) {
        return OTA_HOST_ERR_PARAM;
    }
    if (pkg[0] != (uint8_t)OTA_FRAME_MAGIC0 || pkg[1] != (uint8_t)OTA_FRAME_MAGIC1 ||
        pkg[2] != (uint8_t)OTA_FRAME_MAGIC2 || pkg[3] != (uint8_t)OTA_FRAME_MAGIC3) {
        return OTA_HOST_ERR_PARAM;
    }
    if (h->io.write == NULL || h->io.read == NULL || h->io.tick_ms == NULL) {
        return OTA_HOST_ERR_PARAM;
    }

    h->pkg        = pkg;
    h->total      = pkg_len;   /* 串口语义：含 82B 包头的绝对偏移 */
    h->off        = 0u;
    h->acked_max  = 0u;
    h->seq        = 0u;
    h->sent_len   = 0u;
    h->tx_stage   = 1u;
    h->probed     = 0u;
    h->result     = OTA_HOST_BUSY;
    h->trace_len  = 0u;
    rx_reset(h);

    /* 首帧：只发一次包头帧（串口设备没有重复包头保护，burst 会把包头写进载荷） */
    if (tx_frame(h, OTA_FRAME_TYPE_DATA, 0u, pkg, (uint16_t)OTA_HOST_PKG_HDR_LEN) != 0) {
        ota_host_fail(h, OTA_HOST_ERR_IO);
        return h->result;
    }
    now           = h->io.tick_ms(h->io.ctx);
    h->t_phase    = now;
    h->t_progress = now;
    h->phase      = OTA_HOST_HANDSHAKE;
    return OTA_HOST_BUSY;
}

int ota_host_poll(ota_host_t *h)
{
    uint8_t  type    = 0u;
    uint32_t dev_off = 0u;
    uint32_t now;
    int      got;

    if (h->phase == OTA_HOST_IDLE || h->phase == OTA_HOST_DONE || h->phase == OTA_HOST_FAILED) {
        return h->result;
    }
    now = h->io.tick_ms(h->io.ctx);

    /* 本拍把已到达的设备帧全部消化，只取最后一帧的偏移 */
    got = 0;
    while (rx_pump(h, &type, &dev_off) == 1) {
        got = 1;
    }

    switch (h->phase) {
    case OTA_HOST_HANDSHAKE:
        if (got != 0) {
            /* ACK(0)=设备就绪无进度 -> 从包头之后开始；RESUME=设备已有进度 */
            h->off = (dev_off < OTA_HOST_PKG_HDR_LEN) ? (uint32_t)OTA_HOST_PKG_HDR_LEN : dev_off;
            if (h->off > h->acked_max) { h->acked_max = h->off; }
            h->t_progress = now;
            h->t_phase    = now;
            h->probed     = 0u;
            if (h->off >= h->total) {
                ota_host_finish_enter(h, now);
            } else {
                h->phase    = OTA_HOST_STREAM;
                h->tx_stage = 0u;
            }
        } else if ((now - h->t_phase) >= OTA_HOST_HANDSHAKE_MS) {
            /* 无响应：从 0 起发（首数据帧含包头字节），与 C#/Python 同款兜底 */
            h->off        = 0u;
            h->t_progress = now;
            h->t_phase    = now;
            h->probed     = 0u;
            h->phase      = OTA_HOST_STREAM;
            h->tx_stage   = 0u;
        }
        break;

    case OTA_HOST_STREAM:
        if (h->tx_stage == 0u) {
            /* 需发送：要么重发，要么发下一分块 */
            if (h->off >= h->total) {
                ota_host_finish_enter(h, now);
                break;
            }
            if (tx_chunk(h) != 0) {
                ota_host_fail(h, OTA_HOST_ERR_IO);
                break;
            }
            h->tx_stage = 1u;
            h->t_phase  = now;
            break;
        }

        /* 等待设备应答 */
        if (got != 0) {
            if (dev_off > h->acked_max) {
                h->acked_max  = dev_off;
                h->t_progress = now;      /* 只有真实推进才刷新卡死计时 */
            }
            /* 始终跟随设备偏移（可进可退），与 C#/Python 一致 */
            h->off      = dev_off;
            h->probed   = 0u;
            h->tx_stage = 0u;
            h->t_phase  = now;
            if (h->io.on_progress != NULL) {
                h->io.on_progress(h->io.ctx, h->off, h->total);
            }
            break;
        }

        if ((now - h->t_progress) >= OTA_HOST_STALL_MS) {
            ota_host_fail(h, OTA_HOST_ERR_STALL);
            break;
        }
        if (h->probed == 0u) {
            if ((now - h->t_phase) >= OTA_HOST_ACK_MS) {
                (void)tx_probe(h);        /* 超时：发探测帧查询设备真实进度 */
                h->probed  = 1u;
                h->t_phase = now;
            }
        } else {
            if ((now - h->t_phase) >= OTA_HOST_PROBE_MS) {
                h->probed   = 0u;
                h->tx_stage = 0u;         /* 探测也无响应：重发当前分块 */
                h->t_phase  = now;
            }
        }
        break;

    case OTA_HOST_FINISH:
        if (h->tx_stage == 0u) {
            (void)tx_probe(h);
            h->tx_stage = 1u;
            h->t_phase  = now;
            break;
        }
        /* 设备随后自行 校验+激活+复位，不必久等 */
        if (got != 0 || (now - h->t_phase) >= OTA_HOST_FINISH_MS) {
            h->phase  = OTA_HOST_DONE;
            h->result = OTA_HOST_OK;
        }
        break;

    default:
        ota_host_fail(h, OTA_HOST_ERR_PARAM);
        break;
    }

    return h->result;
}

int ota_host_query_version(ota_host_t *h, uint32_t *ver_out)
{
    uint8_t  type = 0u;
    uint32_t off  = 0u;
    uint32_t t0;
    uint8_t  payload[4];

    if (h->io.write == NULL || h->io.read == NULL || h->io.tick_ms == NULL) {
        return -1;
    }
    /* 用 RESUME 类型：旧固件忽略非 DATA 帧 -> 查询安全（不会误写槽/触发重启） */
    payload[0] = (uint8_t)'V'; payload[1] = (uint8_t)'E';
    payload[2] = (uint8_t)'R'; payload[3] = (uint8_t)'1';
    if (tx_frame(h, OTA_FRAME_TYPE_RESUME, 0xFFFFu, payload, 4u) != 0) {
        return -1;
    }
    t0 = h->io.tick_ms(h->io.ctx);
    while ((h->io.tick_ms(h->io.ctx) - t0) < OTA_HOST_ACK_MS) {
        if (rx_pump(h, &type, &off) == 1) {
            if (ver_out != NULL) { *ver_out = off; }
            return (int)off;
        }
    }
    return -1;
}
