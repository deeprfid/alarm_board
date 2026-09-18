/**
 * @file ota_transport_uart.c
 * @brief 本地通道 transport 实现：OTA1 帧解析 → 懒擦除写 QSPI 暂存 → 进度 KV
 *        下载完成 → 载荷 CRC32 比对 → ota_mark_ready → system_reset（与网络路径同一闭环）
 * @note 帧 CRC16 仅做传输层校验；载荷完整性由统一 OTA 包内 CRC32 保证。
 *       整区擦除改为"懒擦除"（写到哪个扇区擦哪个），避免 4MB 一次性擦除阻塞串口。
 */
#include <string.h>
#include "qspi_flash.h"          /* 必须在 hc32f46_driver.h 之前（hc32f4a0.h:3630 依赖） */
#include "hc32f46_driver.h"
#include "ota_transport_uart.h"
#include "ota_frame.h"
#include "ota_storage.h"
#include "ota_state.h"
#include "ota_security.h"   /* v9.81p: 本地通道补 HMAC 验签（与网络路径一致） */
#include "app_conf.h"          /* OTA_FW_VERSION */

#define OTA_PKG_HDR_LEN     82   /* 统一 OTA 包头长（OTA1+ver+plat+app+len+crc+sha+hmac） */
#define OTA_PKG_VER_OFF     4
#define OTA_PKG_LEN_OFF     10
#define OTA_PKG_CRC_OFF     14
#define QSPI_SECTOR         4096 /* W25QXX 扇区 */

static int      s_ota_fd        = -1;
static int      s_ota_chan      = 0;  /* v9.80: 通道互斥已持有标志（串口会话） */
static uint32_t s_erased_upto   = 0;  /* 本会话已擦除字节边界（懒擦除游标） */
static int      s_session_first = 1;  /* 本会话尚未写过数据 */
static uint32_t s_pkg_total     = 0;  /* 统一 OTA 包总长（82+payload_len），解析到后置位 */
static uint32_t s_last_rx_tick  = 0;  /* v9.81cm: 会话空闲超时（中止会话后释放通道锁，防锁死 USB/HTTP） */

/* progress KV 8KB batch write: fdb_kv_set_blob full-partition scan ~230ms per call,
 * cache progress in RAM, sync KV every OTA_PROGRESS_KV_SYNC(8KB) bytes.
 * resume granularity 512B->8KB (protocol is offset-driven, no misalign). */
#define OTA_PROGRESS_KV_SYNC    (65536UL)   /* v9.81p: 32KB->64KB，与 64KB 块擦除对齐（断点粒度 64KB） */
static uint32_t s_prog     = 0;    /* RAM progress (latest) */
static uint32_t s_prog_kv  = 0;    /* last progress written to KV */
static int      s_prog_loaded = 0; /* loaded from KV yet */

/* CRC16 由 ota_frame 提供（平台无关核心） */
uint16_t ota_transport_uart_crc16(const uint8_t *buf, uint32_t len)
{
    return ota_frame_crc16(buf, len);
}

/* ---------- 组帧发送 ---------- */
static int send_frame(int fd, uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t plen)
{
    /* v9.81cj-f: 只发 ACK/RESUME（plen<=4B，帧<=15B）——栈上小缓冲即可，
     * 不再常驻 4KB 静态（原 OTA_FRAME_MAX_LEN 是给大 payload 预留，此处用不到） */
    uint8_t frame[OTA_FRAME_HDR_LEN + 4 + OTA_FRAME_CRC_LEN];
    int flen;

    if (fd < 0)
        return -1;
    flen = ota_frame_build(type, seq, payload, plen, frame);
    if (flen < 0)
        return -1;
    if (write(fd, frame, (uint32_t)flen) < 0)
        return -1;
    return 0;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int ota_transport_uart_init(int fd)
{
    s_ota_fd = fd;
    s_last_rx_tick = (uint32_t)osKernelGetTickCount();
    s_prog        = ota_get_progress();
    s_prog_kv     = s_prog;
    s_prog_loaded = 1;
    return 0;
}

int ota_transport_uart_send_ack(int fd, uint32_t offset)
{
    uint8_t p[4];
    put_u32(p, offset);
    return send_frame(fd, OTA_FRAME_TYPE_ACK, 0, p, sizeof(p));
}

int ota_transport_uart_send_resume(int fd, uint32_t offset)
{
    uint8_t p[4];
    put_u32(p, offset);
    return send_frame(fd, OTA_FRAME_TYPE_RESUME, 0, p, sizeof(p));
}

/* ---------- 懒擦除：确保 [off, off+len) 覆盖的所有扇区已擦除 ----------
 * 游标 s_erased_upto 之后只前进不回头；续传时按"已写区视为已擦"初始化，
 * 避免重擦已写数据（扇区半满时不得再擦）或漏擦旧数据（跨新扇区必须擦）。 */
static int ensure_erased(uint32_t off, uint32_t len)
{
    uint32_t end = ((off + len + 65535U) / 65536U) * 65536U;   /* v9.81o: 64KB 块擦除 */

    if (end <= s_erased_upto)
        return 0;
    for (uint32_t a = s_erased_upto; a < end; a += 65536U) {
        if (QSPI_FLASH_EraseBlock64K(OTA_QSPI_STAGE_BASE + a) != 0)
            return -1;
    }
    s_erased_upto = end;
    return 0;
}

/* ---------- 从暂存区解析包头 → 包总长/版本/载荷CRC ---------- */
static int parse_pkg_hdr(uint32_t *total, uint32_t *version, uint32_t *payload_crc)
{
    uint8_t hdr[OTA_PKG_HDR_LEN];

    if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE, hdr, sizeof(hdr)) != 0)
        return -1;
    if (hdr[0] != 'O' || hdr[1] != 'T' || hdr[2] != 'A' || hdr[3] != '1')
        return -1;
    if (version)   *version    = get_u32(hdr + OTA_PKG_VER_OFF);
    if (payload_crc) *payload_crc = get_u32(hdr + OTA_PKG_CRC_OFF);
    if (total)     *total      = OTA_PKG_HDR_LEN + get_u32(hdr + OTA_PKG_LEN_OFF);
    return 0;
}

/* ---------- 下载完成：载荷 CRC32 比对 → mark_ready → 复位 ---------- */
static int local_ota_finish(void)
{
    uint32_t total, version, payload_crc, fw_len;
    int rc;

    /* 同步 RAM 进度到 KV(mark_ready/bootloader 依赖 progress>=size) */
    if (s_prog_loaded) {
        ota_set_progress(s_prog);
        s_prog_kv = s_prog;
    }
    if (parse_pkg_hdr(&total, &version, &payload_crc) != 0)
        return -1;
    /* 版本防线：包头版本 ≠ 当前固件版本 → 拒收 + 清进度（防旧包/跨版本残留 commit） */
    if (version < OTA_FW_VERSION) {    /* v9.81cm: 同版本允许重刷（CRC32+HMAC 校验兜底，安全）——仅拒降级，四通道可同包全测 */
        TRACE("ota: pkg ver 0x%08X <= fw 0x%08X, reject + reset progress\n",
              (unsigned)version, (unsigned)OTA_FW_VERSION);
        ota_set_progress(0);
        s_prog        = 0;
        s_prog_kv     = 0;
        s_session_first = 1;
        s_erased_upto   = 0;
        s_pkg_total     = 0;
        if (s_ota_chan) { ota_channel_release(); s_ota_chan = 0; }
        return -1;
    }
    fw_len = total - OTA_PKG_HDR_LEN;

    rc = ota_storage_verify_payload(OTA_PKG_HDR_LEN, fw_len, payload_crc);
    if (rc != 0) {
        TRACE("ota local verify fail rc:%d, reset progress\n", rc);
        ota_set_progress(0);
        s_prog        = 0;
        s_prog_kv     = 0;
      /* 载荷损坏 → 进度清零，重新下发 */
        ota_fail(1);
        s_session_first = 1;      /* 重置会话：下一帧按全新下载处理（擦除游标归零） */
        s_erased_upto   = 0;
        s_pkg_total     = 0;
        ota_channel_release();    /* v9.80: 释放通道，允许另一通道接管 */
        s_ota_chan = 0;
        return -1;
    }

    /* v9.81p: 包级 HMAC-SHA256 验签（与 ota_agent_run 网络路径一致；LEVEL1 全0签名兼容放行） */
    if (ota_security_verify_staged() != 0) {
        TRACE("ota local sec verify fail, reset progress\n");
        ota_set_progress(0);
        s_prog = 0; s_prog_kv = 0;
        ota_fail(1);
        s_session_first = 1; s_erased_upto = 0; s_pkg_total = 0;
        if (s_ota_chan) { ota_channel_release(); s_ota_chan = 0; }
        return -1;
    }
    TRACE("ota local done %luB ver:%lu, mark_ready...\n",
          (unsigned long)total, (unsigned long)version);
    if (ota_mark_ready(fw_len, version) != 0) {
        ota_fail(1);
        return -1;
    }
    sleep_ms(500);
    system_reset();
    return 0;
}

/* ---------- 会话空闲超时：上位机中止后释放通道锁（v9.81cm） ----------
 * 与 USB 通道一致，阈值 8s（正常帧节奏 <1s）。仅释放锁与会话状态，
 * 进度已按 64KB 同步到 KV，下次会话由包头帧重置或续传。 */
#define OTA_UART_IDLE_MS    (8000UL)
int ota_transport_uart_idle_timeout(void)
{
    if (!s_ota_chan)
        return 0;
    if ((uint32_t)osKernelGetTickCount() - s_last_rx_tick > OTA_UART_IDLE_MS) {
        TRACE("ota uart: idle timeout, release channel\n");
        ota_channel_release();
        s_ota_chan      = 0;
        s_session_first = 1;
        s_pkg_total     = 0;
        s_erased_upto   = 0;
        return 1;
    }
    return 0;
}

/* ---------- 整帧处理核心（fd 指定回发通道） ---------- */
static int handle_frame_on(int fd, const uint8_t *frame, uint16_t flen)
{
    uint16_t plen;
    uint32_t progress, new_off;

    s_last_rx_tick = (uint32_t)osKernelGetTickCount();   /* v9.81cm: 任意完整帧即刷新会话心跳 */

    /* 1) 帧头/长度/CRC16 校验（平台无关核心） */
    {
        uint8_t ftype;
        uint16_t fseq;
        if (ota_frame_parse(frame, flen, &ftype, &fseq, &plen) != 0) {
            TRACE("ota local crc err, drop frame\n");   /* v9.81bt: no-resume */
            return -2;
        }
    }

    /* v9.81cm: 版本查询帧（RESUME 类型 len=4 payload="VER1"，旧固件忽略非 DATA 帧→安全）
     * → 回 ACK(OTA_FW_VERSION)，不改任何状态。必须在类型检查之前。 */
    if (plen == 4 &&
        frame[OTA_FRAME_HDR_LEN + 0] == 'V' && frame[OTA_FRAME_HDR_LEN + 1] == 'E' &&
        frame[OTA_FRAME_HDR_LEN + 2] == 'R' && frame[OTA_FRAME_HDR_LEN + 3] == '1') {
        ota_transport_uart_send_ack(fd, OTA_FW_VERSION);
        return 0;
    }

    /* ACK/RESUME 为设备→上位机方向，设备收到即忽略 */
    if (frame[4] != OTA_FRAME_TYPE_DATA)
        return 0;

    if (!s_prog_loaded) {           /* 首次懒加载(未走 init 的通道) */
        s_prog        = ota_get_progress();
        s_prog_kv     = s_prog;
        s_prog_loaded = 1;
    }
    progress = s_prog;

    /* 探测帧（len=0 DATA）：仅回 ACK(当前进度)，不写存储——上位机超时后查询设备进度用 */
    if (plen == 0) {
        ota_transport_uart_send_ack(fd, progress);
        /* 进度已满但未触发完成（断点续传/重跑场景）：读包头解析 total，
         * progress >= total 则触发 finish（s_pkg_total 为新会话静态值，不可依赖） */
        /* v9.81ag: probe never triggers finish - header-frame pkg-change handshake decides. */
        if (progress > 0) {
            /* residual progress is advisory only; pkg consistency decided by session-first header frame */
        }
        return 0;
    }

    /* 2) 会话首帧：上位机先发 82B 包头帧 → 与暂存包头比对大小：
     *    一致 → 请求续传（丢弃本帧）；不一致（旧包残留/包变更）→ 清进度全新下载。
     *    非包头帧且已有进度 → 直接续传（兼容旧上位机）。
     *    擦除游标：扇区边界上=该扇区未写过需擦；扇区中间=半满扇区已擦过不可再擦 */
    if (s_session_first) {
        s_session_first = 0;
        s_pkg_total     = 0;
        /* v9.81p: 续传游标改 64KB 块对齐（v9.81o 起 ensure_erased 按 64KB 步进；
         * 原按 4KB 扇区对齐 → 非 64KB 边界地址调 EraseBlock64K 会擦错块/擦掉已写数据） */
        if (progress % 65536UL == 0)
            s_erased_upto = progress;
        else
            s_erased_upto = ((progress / 65536UL) + 1) * 65536UL;

        /* v9.80: 包头帧判定（82B + OTA1 magic）→ 包一致性校验 */
        if (plen == OTA_PKG_HDR_LEN &&
            frame[OTA_FRAME_HDR_LEN + 0] == 'O' && frame[OTA_FRAME_HDR_LEN + 1] == 'T' &&
            frame[OTA_FRAME_HDR_LEN + 2] == 'A' && frame[OTA_FRAME_HDR_LEN + 3] == '1') {
            uint32_t new_total = OTA_PKG_HDR_LEN + get_u32(frame + OTA_FRAME_HDR_LEN + OTA_PKG_LEN_OFF);
            if (progress > 0) {
                TRACE("ota: no-resume clean restart, old prog=%lu\n", (unsigned long)progress);
            }
            ota_set_progress(0);
            s_prog        = 0;
            s_prog_kv     = 0;
            s_erased_upto = 0;
            s_pkg_total   = new_total;
            progress      = 0;      /* v9.81bt: no-resume, header always offset 0 */
        }
    }

    /* 3) 超尾防护：包头已解析时校验偏移 */
    if (s_pkg_total > 0 && progress + plen > s_pkg_total) {
        TRACE("ota local overflow, drop frame\n");   /* v9.81bt: no-resume */
        return -2;
    }

    /* 4) 懒擦除 + 写暂存 + 进度 KV（先写后解析，包头须等字节落盘） */
    /* v9.80: 通道互斥——会话首次实际写暂存前获取（USB CDC 进行中则拒绝） */
    if (!s_ota_chan) {
        if (ota_channel_try_acquire() != 0) {
            TRACE("ota: channel busy (usb in progress), reject\n");
            return -1;
        }
        s_ota_chan = 1;
    }
    if (ensure_erased(progress, plen) != 0)
        return -1;
    if (ota_storage_write_stage(frame + OTA_FRAME_HDR_LEN, plen, progress) != 0)
        return -1;
    new_off = progress + plen;
    s_prog  = new_off;
    if (new_off - s_prog_kv >= OTA_PROGRESS_KV_SYNC) {   /* 每 8KB 同步一次 KV */
        ota_set_progress(new_off);
        s_prog_kv = new_off;
    }

    /* 4b) 首次凑齐包头（≥82B）→ 解析包总长，用于超尾防护与完成判定 */
    if (s_pkg_total == 0 && new_off >= OTA_PKG_HDR_LEN) {
        if (parse_pkg_hdr(&s_pkg_total, NULL, NULL) != 0) {
            ota_fail(1);
            return -1;
        }
    }
    ota_transport_uart_send_ack(fd, new_off);

    /* 5) 下载完成 → 载荷校验 → mark_ready → 复位 */
    if (s_pkg_total > 0 && new_off >= s_pkg_total)
        return local_ota_finish();
    return 0;
}

/* 整帧入口（UART0 用，回发 s_ota_fd） */
int ota_transport_uart_handle_frame(const uint8_t *frame, uint16_t flen)
{
    return handle_frame_on(s_ota_fd, frame, flen);
}

/* ---------- 流式组帧（USB1 等字节流通道，平台无关状态机） ---------- */
static ota_frame_feed_t s_feed;

/* progress KV 8KB 批量写: fdb_kv_set_blob 每次全分区扫描 ~230ms,
 * RAM 缓存当前进度, 每 OTA_PROGRESS_KV_SYNC(8KB) 才同步一次 KV.
 * 断点续传粒度 512B->8KB(协议 offset 驱动, 无错位风险). */

int ota_transport_uart_feed(int fd, const uint8_t *data, uint32_t len)
{
    static uint8_t frame[OTA_FRAME_MAX_LEN];   /* v9.80: 2059B 移出栈（字节流通道用） */
    uint16_t flen;

    for (uint32_t i = 0; i < len; i++) {
        int r = ota_frame_feed(&s_feed, data[i], frame, &flen);
        if (r == 1)
            (void)handle_frame_on(fd, frame, flen);   /* 完整帧 */
    }
    return 0;
}
