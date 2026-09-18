/**
 * @file ota_download.c
 * @brief 下载 Agent 实现：复用现有 HTTP 基础设施（socket/TLS/http_parser）
 *        GET + Range 续传 → ota_storage_write_stage（QSPI 暂存）→ 进度 KV
 * @note 独立 http_parser 实例（不影响 http_callback 的上报解析）
 */
#include <stdio.h>
#include <string.h>
#include "qspi_flash.h"      /* QSPI_FLASH_EraseBlock64K（v9.81p 懒擦除） */
#include "hc32f46_driver.h"
#include "http_parser.h"
#include "http_callback.h"
#include "reader_msg.h"
#include "ota_download.h"
#include "ota_state.h"
#include "ota_storage.h"

#define OTA_HTTP_SEND(fd, buf, slen)  do { if (write_n(fd, buf, slen) != slen) return -1; } while (0)

/* v9.81i: 与串口/USB 一致——进度 KV 32KB 批处理（fdb_kv_set_blob 全分区扫描 ~230ms，
 * 每块都写会拖慢下载；RAM 缓存进度，每 32KB 同步一次 KV，断点粒度 32KB） */
#define OTA_PROGRESS_KV_SYNC    (65536UL)   /* v9.81p: 32KB->64KB，与 64KB 块擦除对齐（断点粒度 64KB） */
static uint32_t s_dl_prog    = 0;    /* RAM progress (latest) */
static uint32_t s_dl_prog_kv = 0;    /* last progress written to KV */
static uint32_t s_dl_erased  = 0;    /* v9.81p: 懒擦除游标（写哪块擦哪块，64KB 步进） */


static http_parser_settings s_ota_parse_set;
static http_parser         s_ota_parser;
static int      s_dl_err   = 0;

/* v9.81p: 懒擦除——64KB 块步进，确保 [off, off+len) 覆盖的块已擦（与串口/USB/HTTP-server 一致） */
static int dl_ensure_erased(uint32_t off, uint32_t len)
{
    uint32_t end = ((off + len + 65535UL) / 65536UL) * 65536UL;
    if (end <= s_dl_erased)
        return 0;
    for (uint32_t a = s_dl_erased; a < end; a += 65536UL) {
        if (QSPI_FLASH_EraseBlock64K(OTA_QSPI_STAGE_BASE + a) != 0)
            return -1;
    }
    s_dl_erased = end;
    return 0;
}

/* v9.81ch: HTTP session 状态走 http_callback 接口（http_get_hostname/http_handshake_fin 等） */

static int ota_OnBody(http_parser *parser, const char *at, size_t length)
{
    uint32_t base = s_dl_prog;                   /* v9.81i: RAM 进度（续传基点） */
    if (dl_ensure_erased(base, (uint32_t)length) != 0) {   /* v9.81p: 先擦后写（防旧数据残留） */
        s_dl_err = 1;
        return -1;
    }
    if (ota_storage_write_stage((const uint8_t *)at, (uint32_t)length, base) != 0) {
        s_dl_err = 1;
        return -1;
    }
    s_dl_prog = base + (uint32_t)length;
    /* v9.81i: 与串口/USB 一致——每 32KB 同步一次 KV（原来每块 ~1KB 写一次 → 慢） */
    if (s_dl_prog - s_dl_prog_kv >= OTA_PROGRESS_KV_SYNC) {
        ota_set_progress(s_dl_prog);
        s_dl_prog_kv = s_dl_prog;
    }
    return 0;
}
static int ota_OnMessageComplete(http_parser *parser)
{
    return 0;
}

int ota_download_start(const char *url)
{
    int fd = COMMON_INTERFACE_SOCKET0;
    int rtimeout = 20000;
    char headerbuf[200];
    uint8_t recvbuf[1024];


    /* v9.81i: 通道互斥——与串口/USB 一致（共用 QSPI 暂存区，禁止并发下载） */
    if (ota_channel_try_acquire() != 0) {
        TRACE("ota dl: channel busy (uart/usb in progress), reject\n");
        return -1;
    }

    /* v9.81bt: 去掉远程断点续传——总是从 0 全新下载（残留暂存被覆盖重写，
     * 规避续传无版本比对 → 新旧固件拼接损坏风险；与串口/USB/HTTP 一致 no-resume） */

    s_dl_prog    = 0;
    s_dl_prog_kv = 0;
    s_dl_erased  = 0;

    /* W5100S/PHY 复位后未就绪：立即 connect 会致首次 ARP 失败（rv=-13/DHAR=FF，2026-08-18 真机定案）。
     * 首次网络操作前等待就绪（仅本上电周期一次；后续续传/重试不再等待） */
    {
        static int s_net_ready = 0;
        if (s_net_ready == 0) {
            sleep_ms(2000);
            s_net_ready = 1;
        }
    }

    /* 1) 解析 URL（复用 url_get_domain：domain/port/路径/是否 https） */
    {
        char domain[100];
        unsigned short port;
        int ishttps = 0;
        if (url_get_domain((char *)url, domain, &port, http_abspathpos_ptr(), &ishttps) != 0) {
            ota_channel_release();
            return -1;
        }
    }

    /* 2) TCP 连接（以太网 W5100S；4G/WiFi 走 UART1 由 usr_mod 拨号，Phase 3 先支持以太网） */
    if (net_is_connected() == 0) {
        disconnect(fd);
        close(fd);
        CheckServerConnection();      /* reader_msg.c：TCP 连服务器 */
    }
    ioctl(fd, COMMON_INTERFACE_SET_TIMEOUT, &rtimeout);
    ioctl(fd, COMMON_INTERFACE_CLEAR_REVBUF, NULL);

    /* 3) TLS 握手（若 https） */
    if (http_is_tls() == 1 && http_handshake_fin() == 0) {
        if (mbedtls_handshake() != 0) {
            net_set_connected(0);
            ota_channel_release();
            return -1;
        }
        http_set_handshake_fin(1);
    }

    /* 4) GET + Range（断点续传） */
    sprintf(headerbuf, "GET %s HTTP/1.1\r\n", url + http_get_abspathpos());
    OTA_HTTP_SEND(fd, headerbuf, strlen(headerbuf));
    sprintf(headerbuf, "Host: %s\r\n", http_get_hostname());
    OTA_HTTP_SEND(fd, headerbuf, strlen(headerbuf));
    sprintf(headerbuf, "Accept: */*\r\n");
    OTA_HTTP_SEND(fd, headerbuf, strlen(headerbuf));
    /* v9.81bt: no-resume - no Range header */
    OTA_HTTP_SEND(fd, "Connection: close\r\n\r\n", 22);

    /* 5) 接收循环（独立 parser，on_body 写 QSPI） */
    http_parser_init(&s_ota_parser, HTTP_RESPONSE);
    http_parser_settings_init(&s_ota_parse_set);
    s_ota_parse_set.on_body = ota_OnBody;
    s_ota_parse_set.on_message_complete = ota_OnMessageComplete;
    s_dl_err = 0;

    while (1) {
        int nrecv = read(fd, recvbuf, sizeof(recvbuf));
        if (nrecv <= 0) {               /* 流式下载完成由服务器 close 触发（Connection: close） */
            break;
        }
        {
            int nparsed = http_parser_execute(&s_ota_parser, &s_ota_parse_set,
                                              (char *)recvbuf, nrecv);
            if (nparsed != nrecv || s_ota_parser.http_errno != HPE_OK || s_dl_err) {
                TRACE("ota download parse err:%d\n", s_ota_parser.http_errno);
                ota_channel_release();
                return -1;
            }
        }
    }
    disconnect(fd);
    close(fd);
    net_set_connected(0);

    if (s_dl_err) {
        ota_channel_release();
        return -1;
    }

    /* 6) 完成：读暂存包头取 CRC/长度 → 校验 payload CRC（TODO(Phase 3): 与包内 crc32 比对）
     *    验签 → ota_mark_ready（由上层统一触发） */
    {
        uint32_t size;
        /* v9.81i: 下载完成最终同步一次 KV（最后一块可能不足 32KB 未写）——
         * 与串口/USB local_ota_finish 一致，确保断点续传/校验用完整进度 */
        if (s_dl_prog != s_dl_prog_kv) {
            ota_set_progress(s_dl_prog);
            s_dl_prog_kv = s_dl_prog;
        }
        size = s_dl_prog;   /* 用 RAM 进度（= 实际写入字节）而非 KV 滞后值 */
        TRACE("ota download done, %lu bytes\n", (unsigned long)size);
        {
            int rc = (size > 82) ? 0 : -1;   /* 至少含 OTA1 头 */
            ota_channel_release();
            return rc;
        }
    }
}
