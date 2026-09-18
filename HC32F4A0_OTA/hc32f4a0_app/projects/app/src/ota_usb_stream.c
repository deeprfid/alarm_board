/**
 * @file ota_usb_stream.c
 * @brief USB stream OTA (v9.74): PC sends 82B package header + 4KB data frames
 *        back-to-back (no per-frame ACK). Device EFM-writes directly into the
 *        other bank (app area), copies current bootloader, verifies CRC32,
 *        writes NEED_CONFIRM flag on other bank, swaps and resets.
 *        Bypasses QSPI staging / FlashDB KV / bootloader commit copy.
 */
#include <string.h>
#include "qspi_flash.h"      /* before hc32f46_driver.h (hc32f4a0.h:3630) */
#include "hc32f46_driver.h"  /* TRACE, sleep_ms, system_reset */
#include "hc32_ll_efm.h"
#include "ota_storage.h"
#include "ota_flag.h"
#include "ota_frame.h"
#include "ota_state.h"       /* ota_mark_ready（统一升级流） */
#include "ota_security.h"   /* v9.81p: USB 通道补 HMAC 验签 */
#include "ota_usb_stream.h"
#include "app_conf.h"        /* OTA_FW_VERSION */

/* 升级统一走 bootloader commit（v9.80），不再在 App 侧直写另一 bank */

#define SS_MAX_PAYLOAD        4096
#define SS_MAX_FRAME          (9 + SS_MAX_PAYLOAD + 2)
#define SS_HDR_LEN            82
/* v9.82: 热路径 TRACE 开关（逐帧/逐批耗时打印，默认关——省 ~300ms 串口阻塞 + monitor 刷屏） */
#define SS_HOT_TRACE_EN       (0)

typedef enum { SS_IDLE, SS_STREAM } ss_state_t;
static ss_state_t s_state = SS_IDLE;
/* v9.81cj-f: OTA 握手后按需申请 4KB 帧缓冲——平时不占静态 RAM。
 * 探测期只用 s_syncbuf[4]（认 "OTA1" magic），sync 后 malloc(SS_MAX_FRAME)。 */
static uint8_t  s_syncbuf[4];
static uint8_t *s_buf = NULL;
static uint16_t s_buflen = 0;
static uint16_t s_need   = 0;
static int      s_sync   = 0;
static uint32_t s_total      = 0;   /* payload length (from header) */
static uint32_t s_crc_expect = 0;
static uint32_t s_off        = 0;   /* payload bytes written */
static uint32_t s_frame_cnt  = 0;   /* frames since last ACK (flow control) */
static uint32_t s_version    = 0;   /* v9.80: 包头版本（统一 mark_ready 用） */
static int      s_chan       = 0;   /* v9.80: 通道互斥已持有标志（USB 会话） */
static uint32_t s_erased_upto = 0;  /* v9.80: 懒擦除游标（写哪扇区擦哪，防整区擦 4s 饿死 USB RX） */
static uint32_t s_last_rx_tick = 0;
static uint32_t s_last_mm_off = 0xFFFFFFFFUL;  /* v9.81e: seq mismatch 降噪（同 off 只打一条） */
static uint32_t s_last_ack_tick = 0;   /* v1.31: 批处理耗时诊断（上次 ACK -> 本次 ACK = 设备处理 2 帧时间） */
static uint16_t s_restart_cnt = 0;  /* v9.81f: header burst 重复 restart 只打第一条 */
/* v9.81: ACK 帧缓冲必须 static——usb_deveptx 异步发送（仅记录指针即返回，
 * USB 中断/DMA 稍后读内存），栈上缓冲在 write() 返回后即被覆盖 → ACK 损坏
 * → 发送器每批等满 5s 超时（USB OTA 慢 60-90s 根因，串口同步发送无此问题） */
static uint8_t s_ackbuf[16];

/* v9.81p: 进度 KV 断点续传——与串口/HTTP 一致（RAM 进度 + 批量同步；USB 中断后重连可续传）
 * v9.82b: 64KB → 256KB——fdb_kv_set_blob 每次 ~600ms，400KB 包从 6 次降到 2 次 */
#define SS_KV_SYNC          (262144UL)
static uint32_t s_prog      = 0;   /* 总字节进度（82B 包头 + payload 偏移） */
static uint32_t s_prog_kv   = 0;   /* 上次写入 KV 的进度 */

/* v9.81cj-f: 会话结束释放按需申请的帧缓冲（完成路径 reboot 自然清零，无需调） */
static void ss_free_buf(void)
{
    if (s_buf != NULL) {
        free_hexp(s_buf);
        s_buf = NULL;
    }
    s_buflen = 0; s_sync = 0; s_need = 0;
}


static uint32_t ss_get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- v9.81o 懒擦除：按 64KB 块擦除（0xD8，~120ms/块，等效 16 次 4KB 扇区擦提速 ~5 倍） ---- */
static int ss_ensure_erased(uint32_t off, uint32_t len)
{
    uint32_t end = ((off + len + 65535U) / 65536U) * 65536U;
    if (end <= s_erased_upto)
        return 0;
    for (uint32_t a = s_erased_upto; a < end; a += 65536U) {
        if (QSPI_FLASH_EraseBlock64K(OTA_QSPI_STAGE_BASE + a) != 0)
            return -1;
    }
    s_erased_upto = end;
    return 0;
}

/* ---- one complete frame ---- */
static int ss_handle_frame(int fd, const uint8_t *f, uint16_t flen)
{
    uint16_t plen = (uint16_t)(f[7] | ((uint16_t)f[8] << 8));
    const uint8_t *payload = f + 9;

    /* v9.81cm: 版本查询帧（RESUME 类型 len=4 payload="VER1"，旧固件忽略非 DATA 帧→安全）
     * → 回 ACK(OTA_FW_VERSION)，不改任何状态。必须在类型检查之前。 */
    if (plen == 4 && payload[0] == 'V' && payload[1] == 'E' && payload[2] == 'R' && payload[3] == '1') {
        uint8_t p[4];
        int flen2;
        uint32_t v = OTA_FW_VERSION;
        p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF);
        p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)((v >> 24) & 0xFF);
        flen2 = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, p, 4, s_ackbuf);
        if (flen2 > 0)
            (void)write(fd, s_ackbuf, (uint32_t)flen2);
        return 0;
    }

    if (f[4] != OTA_FRAME_TYPE_DATA)
        return 0;
    if (flen != 9 + plen + 2)
        return 0;

    /* header frame recognized in ANY state -> (re)start session */
    if (plen == SS_HDR_LEN &&
        payload[0] == 'O' && payload[1] == 'T' && payload[2] == 'A' && payload[3] == '1') {
        if (ss_get_u32(payload + 4) < OTA_FW_VERSION) {    /* v9.81cm: 同版本允许重刷（仅拒降级），四通道可同包全测 */
            TRACE("ss: ver reject 0x%08X\n", (unsigned)ss_get_u32(payload + 4));
            return 0;
        }
        /* v9.80 fix: 流式会话已进行（数据已写）收到重复包头帧（上位机 header burst）→
         * 不重置会话——否则中途重置会清掉已写数据致 CRC 失败。
         * v1.3 root-fix: 重复 header【不再回 ACK】——burst 5 帧若每帧回 ACK(0)，
         * 上位机只读 1 个，残留 4 个 ACK(0) 污染数据流读取（ReadAck 误读 ACK(0)
         * → devOff 回退 0 → 重发已处理帧 → seq mismatch 风暴 → 每批 3s 超时，60s vs 5s） */
        if (s_state == SS_STREAM) {
            return 0;
        }
        /* v9.80: 通道互斥——串口 OTA 进行中则拒绝 USB 会话 */
        if (!s_chan) {
            if (ota_channel_try_acquire() != 0) {
                TRACE("ss: channel busy, reject\n");   /* v9.81cm: 占用方可能为 uart/usb/http，不再误导 */
                return -1;
            }
            s_chan = 1;
        }
        s_version   = ss_get_u32(payload + 4);
        s_total      = ss_get_u32(payload + 10);
        s_crc_expect = ss_get_u32(payload + 14);
        if (s_total == 0 || s_total > 0x000E0000UL) {
            TRACE("ss: bad size %lu\n", (unsigned long)s_total);
            return 0;
        }
        s_off       = 0;
        s_frame_cnt = 0;
        s_prog      = SS_HDR_LEN;   /* v9.81p: 包头已写 */
        s_prog_kv   = s_prog;
        s_erased_upto = 0;   /* v9.80: 新会话从 0 懒擦除 */
        s_state     = SS_IDLE;
        if (s_restart_cnt++ == 0) {   /* v9.81f: header burst 重复帧只打第一条 */
            TRACE("ss: (re)start total=%lu\n", (unsigned long)s_total);
        }
        /* v9.80: 包头写入暂存 [0:82]（bootloader verify 依赖包头 magic/len/crc）——
         * 懒擦除覆盖扇区0 后写入，替代原整区擦除 prepare */
        if (ss_ensure_erased(0, SS_HDR_LEN) != 0) {
            TRACE("ss: erase hdr FAIL\n");
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
        if (ota_storage_write_stage(payload, SS_HDR_LEN, 0) != 0) {
            TRACE("ss: hdr write FAIL\n");
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
        /* v9.82d: 全量预擦暂存区——流式期间不再有块擦停顿（每批 R 恒定 ~16ms，
         * 输出干净）。代价：ACK(0) 晚 ~1.2s（一次性，flash 物理时间，总时长不变） */
        {
            uint32_t need = ((SS_HDR_LEN + s_total + 65535UL) / 65536UL) * 65536UL;
            while (s_erased_upto < need) {
                if (QSPI_FLASH_EraseBlock64K(OTA_QSPI_STAGE_BASE + s_erased_upto) != 0) {
                    TRACE("ss: pre-erase FAIL @%lu\n", (unsigned long)s_erased_upto);
                    s_state = SS_IDLE;
                    ss_free_buf();
                    ota_channel_release();
                    s_chan = 0;
                    return -1;
                }
                s_erased_upto += 65536UL;
            }
        }
        s_state = SS_STREAM;
        /* ACK(0) to tell PC it is ready for streaming */
        {
            uint8_t zero[4] = {0, 0, 0, 0};
            int flen = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, zero, 4, s_ackbuf);
            if (flen > 0)
                (void)write(fd, s_ackbuf, (uint32_t)flen);
        }
        return 0;
    }

    if (s_state == SS_IDLE)
        return 0;   /* non-header frame in idle: ignore */

    /* SS_STREAM: sequential data */
    if (plen == 0) {                       /* probe: report current offset */
        uint8_t p[4];
        int flen;
        p[0] = (uint8_t)(s_off & 0xFF);
        p[1] = (uint8_t)((s_off >> 8) & 0xFF);
        p[2] = (uint8_t)((s_off >> 16) & 0xFF);
        p[3] = (uint8_t)((s_off >> 24) & 0xFF);
        flen = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, p, 4, s_ackbuf);
        if (flen > 0)
            (void)write(fd, s_ackbuf, (uint32_t)flen);
        return 0;
    }
    /* v9.81e: seq 校验（仅数据帧，probe 帧 plen==0 已在上方处理）——
     * 帧 seq 必须等于当前进度期望 (s_off/4096 + 1)。发送器按内容偏移计算 seq；
     * 设备无此校验时丢失帧会让后续帧错位写入 → verify CRC32 失败。 */
    {
        uint16_t fseq = (uint16_t)(f[5] | ((uint16_t)f[6] << 8));
        uint32_t expect = (s_off / SS_MAX_PAYLOAD) + 1U;
        if (fseq != (uint16_t)expect) {
            /* v9.81e: 降噪——同 off 的重复 mismatch 只打一条（发送器重传风暴） */
            if (s_off != s_last_mm_off) {
                TRACE("ss: seq mismatch got=%u exp=%lu off=%lu, ignore\n",
                      (unsigned)fseq, (unsigned long)expect, (unsigned long)s_off);
                s_last_mm_off = s_off;
            }
            {
                uint8_t p[4];
                int flen;
                p[0] = (uint8_t)(s_off & 0xFF);
                p[1] = (uint8_t)((s_off >> 8) & 0xFF);
                p[2] = (uint8_t)((s_off >> 16) & 0xFF);
                p[3] = (uint8_t)((s_off >> 24) & 0xFF);
                flen = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, p, 4, s_ackbuf);
                if (flen > 0)
                    (void)write(fd, s_ackbuf, (uint32_t)flen);
            }
            return 0;
        }
    }
    if (s_off + plen > s_total) {
        TRACE("ss: dup/over off=%lu plen=%u total=%lu, ignore\n",
              (unsigned long)s_off, (unsigned)plen, (unsigned long)s_total);
        {
            uint8_t p[4];
            int flen;
            p[0] = (uint8_t)(s_off & 0xFF);
            p[1] = (uint8_t)((s_off >> 8) & 0xFF);
            p[2] = (uint8_t)((s_off >> 16) & 0xFF);
            p[3] = (uint8_t)((s_off >> 24) & 0xFF);
            flen = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, p, 4, s_ackbuf);
            if (flen > 0)
                (void)write(fd, s_ackbuf, (uint32_t)flen);
        }
        return 0;
    }
    /* v9.80: 懒擦除（写哪扇区擦哪）——避免整区擦除饿死 USB RX */
    {
        /* v1.33: 帧处理耗时（擦除+写暂存，不含等待数据）——定位 acksapn 268ms 是否设备处理慢 */
        uint32_t t_f0 = (uint32_t)osKernelGetTickCount();
        if (ss_ensure_erased(SS_HDR_LEN + s_off, plen) != 0) {
            TRACE("ss: erase FAIL off=%lu\n", (unsigned long)s_off);
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
        if (ota_storage_write_stage(payload, plen, SS_HDR_LEN + s_off) != 0) {
            TRACE("ss: write FAIL off=%lu len=%u\n", (unsigned long)s_off, (unsigned)plen);
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
#if SS_HOT_TRACE_EN
        TRACE("ss: frame=%lums off=%lu\n",
              (unsigned long)((uint32_t)osKernelGetTickCount() - t_f0), (unsigned long)s_off);
#endif
    }
    s_off += plen;
    s_frame_cnt++;
        /* v9.81p: 进度 KV 断点续传——总进度=包头82B+payload 偏移；每 64KB 同步一次 KV */
    s_prog = SS_HDR_LEN + s_off;
    if (s_prog - s_prog_kv >= SS_KV_SYNC) {
        ota_set_progress(s_prog);
        s_prog_kv = s_prog;
    }

    /* v9.81cl: flow control ACK every 2 frames (8KB)——batch 4 已回退（治本方向=bInterval 等描述符参数，非减批次数） */
    if ((s_frame_cnt % 2UL) == 0UL) {
        uint8_t p[4];
        int flen;
        p[0] = (uint8_t)(s_off & 0xFF);
        p[1] = (uint8_t)((s_off >> 8) & 0xFF);
        p[2] = (uint8_t)((s_off >> 16) & 0xFF);
        p[3] = (uint8_t)((s_off >> 24) & 0xFF);
        flen = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, p, 4, s_ackbuf);
        if (flen > 0) {
            /* v1.31: 批处理耗时诊断——上次 ACK 到本次 ACK（设备处理 2 帧总时间，含写暂存） */
            uint32_t t_now = (uint32_t)osKernelGetTickCount();
#if SS_HOT_TRACE_EN
            if (s_last_ack_tick != 0)
                TRACE("ss: ackspan=%lums\n", (unsigned long)(t_now - s_last_ack_tick));
#endif
            s_last_ack_tick = t_now;
            (void)write(fd, s_ackbuf, (uint32_t)flen);
        }
    }

    if (s_off >= s_total) {
        /* v9.81p: 完成前最终同步 KV（最后一块可能不足 64KB 未写） */
        s_prog = SS_HDR_LEN + s_off;
        if (s_prog != s_prog_kv) {
            ota_set_progress(s_prog);
            s_prog_kv = s_prog;
        }
        /* v9.82: 完成前补发最终 ACK——让发送端立即得知收满，免 3s 超时探测 */
        {
            uint8_t p[4];
            int flen;
            p[0] = (uint8_t)(s_off & 0xFF);
            p[1] = (uint8_t)((s_off >> 8) & 0xFF);
            p[2] = (uint8_t)((s_off >> 16) & 0xFF);
            p[3] = (uint8_t)((s_off >> 24) & 0xFF);
            flen = ota_frame_build(OTA_FRAME_TYPE_ACK, 0, p, 4, s_ackbuf);
            if (flen > 0)
                (void)write(fd, s_ackbuf, (uint32_t)flen);
        }
        TRACE("ss: done %lu, verify...\n", (unsigned long)s_off);
        if (ota_storage_verify_payload(SS_HDR_LEN, s_total, s_crc_expect) != 0) {
            TRACE("ss: crc FAIL exp=%08X\n", (unsigned)s_crc_expect);
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
        /* v9.81p: 包级 HMAC-SHA256 验签（与网络路径一致） */
        if (ota_security_verify_staged() != 0) {
            TRACE("ss: sec verify FAIL\n");
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
        /* v9.80: 统一升级流——与串口 OTA 同一闭环：
         * mark_ready(暂存/标志/KV) -> 复位 -> bootloader commit -> swap -> 新固件自检 -> confirm */
        TRACE("ss: crc ok, mark_ready...\n");
        if (ota_mark_ready(s_total, s_version) != 0) {
            TRACE("ss: mark_ready FAIL\n");
            s_state = SS_IDLE;
            ss_free_buf();
            ota_channel_release();
            s_chan = 0;
            return -1;
        }
        TRACE("ss: mark_ready ok, reset...\n");
        sleep_ms(200);
        system_reset();
        return 1;
    }
    return 0;
}

/* ---- byte-level frame assembler (self-contained, 4KB frames) ---- */
static int ss_feed_byte(int fd, uint8_t b)
{
    if (!s_sync) {
        switch (s_buflen) {
            case 0: if (b == 'O') { s_syncbuf[0] = b; s_buflen = 1; } break;
            case 1: if (b == 'T') { s_syncbuf[1] = b; s_buflen = 2; } else s_buflen = 0; break;
            case 2: if (b == 'A') { s_syncbuf[2] = b; s_buflen = 3; } else s_buflen = 0; break;
            case 3: if (b == '1') { s_syncbuf[3] = b; s_buflen = 4; s_sync = 1; } else s_buflen = 0; break;
            default: s_buflen = 0; break;
        }
        if (s_sync) {
            /* OTA magic 握手成功——按需申请 4KB 帧缓冲（平时不占） */
            if (s_buf == NULL) {
                s_buf = (uint8_t *)malloc_hexp(SS_MAX_FRAME);
                if (s_buf == NULL) {
                    TRACE("ss: no mem for s_buf\n");
                    s_buflen = 0; s_sync = 0;
                    return -1;
                }
            }
            memcpy(s_buf, s_syncbuf, 4);
        }
        return 0;
    }
    if (s_buflen >= SS_MAX_FRAME) {
        s_buflen = 0; s_sync = 0; s_need = 0;
        return -1;
    }
    s_buf[s_buflen++] = b;
    if (s_buflen == 9) {
        s_need = (uint16_t)(s_buf[7] | ((uint16_t)s_buf[8] << 8));
        if (s_need > SS_MAX_PAYLOAD) {
            s_buflen = 0; s_sync = 0; s_need = 0;
            return -1;
        }
        s_need += 2;   /* + CRC */
    }
    if (s_need > 0 && s_buflen == (uint16_t)(9 + s_need)) {
        uint16_t crc_rx = (uint16_t)(s_buf[s_buflen - 2] | ((uint16_t)s_buf[s_buflen - 1] << 8));
        int r = 0;
        /* v9.81f: 正常帧不再逐帧 TRACE（probe len=0 与数据 4096 交替导致刷屏），
         * 仅保留错误路径（crc MISMATCH 下方打印）+ 关键事件（done/verify/mark_ready）*/
        uint16_t crc_calc = ota_frame_crc16(s_buf, s_buflen - 2);
        if (crc_rx == crc_calc) {
            r = ss_handle_frame(fd, s_buf, s_buflen);
        } else {
            /* v9.81: 原静默丢弃——打印期望值定位丢帧来源（USB RX 溢出/上位机字节错） */
            TRACE("ss: crc MISMATCH len=%u got=%04X exp=%04X off=%lu\n",
                  (unsigned)(s_need - 2), (unsigned)crc_rx, (unsigned)crc_calc,
                  (unsigned long)s_off);
        }
        s_buflen = 0; s_sync = 0; s_need = 0;
        return r;
    }
    return 0;
}

int ota_usb_stream_feed(int fd, const uint8_t *data, uint32_t len)
{
    s_last_rx_tick = (uint32_t)osKernelGetTickCount();
    for (uint32_t i = 0; i < len; i++) {
        int r = ss_feed_byte(fd, data[i]);
        if (r != 0)
            return r;
    }
    return 0;
}

/* v9.81cl: send_func 让路查询——1.5s 内有 OTA 数据则视为会话活动（含 sync 前握手期），
 * 此时 USB1 由 ota_usb_task 独占，send_func 不得竞争读。 */
int ota_usb_stream_active(void)
{
    uint32_t now = (uint32_t)osKernelGetTickCount();
    return ((now - s_last_rx_tick) < 1500U);
}

/* v9.81: 会话空闲超时——上位机放弃/掉线后释放通道锁，避免锁永久卡死串口通道。
 * 阈值 8s（发送器正常节奏 < 1s/帧，probe 探测间隔 2.5s，远小于阈值）。 */
int ota_usb_stream_idle_timeout(void)
{
    uint32_t now;
    if (s_state != SS_STREAM)
        return 0;
    now = (uint32_t)osKernelGetTickCount();
    if ((now - s_last_rx_tick) > 8000U) {
        TRACE("ss: idle timeout %lums, release channel\n", (unsigned long)(now - s_last_rx_tick));
        s_state = SS_IDLE;
        ss_free_buf();
        if (s_chan) { ota_channel_release(); s_chan = 0; }
        return 1;
    }
    return 0;
}
