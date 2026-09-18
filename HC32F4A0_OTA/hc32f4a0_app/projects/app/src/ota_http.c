/**
 * @file ota_http.c
 * @brief HTTP OTA independent channel (v9.81j): device is HTTP server,
 *        PC POSTs the whole .otapkg. Independent port + socket 2/3 + thread,
 *        only shares the staging area via ota_channel_* mutex.
 * Usage: PC ota_http_send.py  POST http://<dev-ip>:<port>/ota
 */
#include <string.h>
#include "qspi_flash.h"
#include "hc32f46_driver.h"
#include "ota_storage.h"
#include "ota_state.h"
#include "ota_http.h"
#include "ota_security.h"   /* v9.81p: HTTP 通道??HMAC 验签 */
#include "app_conf.h"          /* OTA_FW_VERSION（v9.81p: HTTP 通道补版本检查，与串??USB 一致） */
#include "hw_api.h"   /* v9.81ci: driver config access */

#define OTA_HTTP_HDR_LEN     82
#define OTA_HTTP_KV_SYNC     (65536UL)   /* v9.81p: 32KB->64KB，与 64KB 块擦除对??*/

static uint32_t s_http_prog = 0, s_http_kv = 0;
static uint32_t s_http_erased = 0;  /* v9.81l: 懒擦除游标（写哪擦哪，摊在传输中，替代全??4s??*/

/* find Content-Length in HTTP header; -1 if absent */
static long http_get_content_length(const char *hdr, int hdrlen)
{
    int i;
    for (i = 0; i < hdrlen - 15; i++) {
        if (strncmp(hdr + i, "Content-Length:", 15) == 0) {
            long v = 0;
            int j = i + 15;
            while (j < hdrlen && (hdr[j] == 32 || hdr[j] == 9)) j++;
            while (j < hdrlen && hdr[j] >= 48 && hdr[j] <= 57) { v = v * 10 + (hdr[j] - 48); j++; }
            return v;
        }
    }
    return -1;
}

/* v9.81o: 懒擦除——按 64KB 块擦除（0xD8，tBE~120ms），等效 16 ??4KB 扇区擦提??~5 ??*/
static int http_ensure_erased(uint32_t off, uint32_t len)
{
    uint32_t end = ((off + len + 65535U) / 65536U) * 65536U;
    if (end <= s_http_erased)
        return 0;
    for (uint32_t a = s_http_erased; a < end; a += 65536U) {
        if (QSPI_FLASH_EraseBlock64K(OTA_QSPI_STAGE_BASE + a) != 0)
            return -1;
    }
    s_http_erased = end;
    return 0;
}

/* stream body into staging (channel mutex + 32KB KV batching) */
static int http_recv_body(int fd, long total)
{
    uint8_t buf[4096];   /* v9.81m: 4KB 对齐块（与扇区一致），减少擦写切??+ TCP 流控停顿 */
    long remain = total;
    uint32_t t_start = (uint32_t)getSysTick(), t_erase = 0, t_write = 0, t_es, t_ws;  /* v9.81n 诊断 */

    if (ota_channel_try_acquire() != 0) {
        TRACE("ota_http: channel busy, reject\n");
        return -1;
    }
    /* v9.81l: HTTP 通道全新下载——清进度 + 懒擦除（写哪擦哪），??0 写入 */
    ota_set_progress(0);
    s_http_prog  = 0;
    s_http_kv    = 0;
    s_http_erased = 0;

    while (remain > 0) {
        int n = read(fd, buf, (uint32_t)((remain > (long)sizeof(buf)) ? sizeof(buf) : remain));
        if (n <= 0) {
            TRACE("ota_http: recv short %ld/%ld\n", (long)(total - remain), total);
            ota_channel_release();
            return -1;
        }
        t_es = (uint32_t)getSysTick();
        if (http_ensure_erased(s_http_prog, (uint32_t)n) != 0) {
            TRACE("ota_http: erase FAIL @%lu\n", (unsigned long)s_http_prog);
            ota_channel_release();
            return -1;
        }
        t_erase += (uint32_t)getSysTick() - t_es;
        t_ws = (uint32_t)getSysTick();
        if (ota_storage_write_stage(buf, (uint32_t)n, s_http_prog) != 0) {
            TRACE("ota_http: stage write fail @%lu\n", (unsigned long)s_http_prog);
            ota_channel_release();
            return -1;
        }
        t_write += (uint32_t)getSysTick() - t_ws;
        s_http_prog += (uint32_t)n;
        remain      -= n;
        if (s_http_prog - s_http_kv >= OTA_HTTP_KV_SYNC) {
            ota_set_progress(s_http_prog);
            s_http_kv = s_http_prog;
        }
    }
    TRACE("ota_http: t_erase=%lums t_write=%lums total=%lums\n",
          (unsigned long)t_erase, (unsigned long)t_write, (unsigned long)((uint32_t)getSysTick() - t_start));
    if (s_http_prog != s_http_kv) {
        ota_set_progress(s_http_prog);
        s_http_kv = s_http_prog;
    }
    ota_channel_release();
    return 0;
}

/* one connection: header -> body to staging -> 200 -> verify -> mark_ready -> reset */
void http_handle_conn(int fd)   /* v1.0: by ota_dispatch_task (was static) */
{
    char hdr[512];
    int  hlen = 0, n;
    long total;

    while (hlen < (int)sizeof(hdr) - 1) {
        n = read(fd, (uint8_t *)hdr + hlen, 1);
        if (n != 1) break;
        hlen++;
        if (hlen >= 4 && hdr[hlen-4] == 13 && hdr[hlen-3] == 10 && hdr[hlen-2] == 13 && hdr[hlen-1] == 10)
            break;
    }
    hdr[hlen] = 0;
    if (hlen < 4) { TRACE("ota_http: no hdr\n"); return; }

    total = http_get_content_length(hdr, hlen);
    if (total <= OTA_HTTP_HDR_LEN) {
        TRACE("ota_http: bad len %ld\n", total);
        return;
    }
    TRACE("ota_http: POST %ld bytes\n", total);

    if (http_recv_body(fd, total) != 0)
        return;

    {
        static const char resp[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
        (void)write(fd, resp, sizeof(resp) - 1);
    }

    {
        uint8_t h[OTA_HTTP_HDR_LEN];
        uint32_t ver, payload_len, crc_expect, fw_len;
        if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE, h, sizeof(h)) != 0 ||
            h[0] != 'O' || h[1] != 'T' || h[2] != 'A' || h[3] != '1') {
            TRACE("ota_http: no pkg hdr\n");
            return;
        }
        ver = (uint32_t)h[4] | ((uint32_t)h[5] << 8) | ((uint32_t)h[6] << 16) | ((uint32_t)h[7] << 24);
        payload_len = (uint32_t)h[10] | ((uint32_t)h[11] << 8) | ((uint32_t)h[12] << 16) | ((uint32_t)h[13] << 24);
        /* v9.81p: 版本防线——与串口/USB 一致：包头版本 ??当前固件版本 ??拒收 + 清进??*/
        if (ver < OTA_FW_VERSION) {    /* v9.81cm: 同版本允许重刷（仅拒降级），四通道可同包全??*/

            TRACE("ota_http: pkg ver 0x%08X != fw 0x%08X, reject + reset progress\n",
                  (unsigned)ver, (unsigned)OTA_FW_VERSION);
            ota_set_progress(0);
            return;
        }
        crc_expect = (uint32_t)h[14] | ((uint32_t)h[15] << 8) | ((uint32_t)h[16] << 16) | ((uint32_t)h[17] << 24);
        fw_len = payload_len;
        TRACE("ota_http: verify %luB crc=%08X\n", (unsigned long)fw_len, (unsigned)crc_expect);
        if (ota_storage_verify_payload(OTA_HTTP_HDR_LEN, fw_len, crc_expect) != 0) {
            TRACE("ota_http: crc FAIL\n");
            ota_set_progress(0);
            return;
        }
        /* v9.81p: 包级 HMAC-SHA256 验签（与网络路径一致） */
        if (ota_security_verify_staged() != 0) {
            TRACE("ota_http: sec verify FAIL\n");
            ota_set_progress(0);
            return;
        }
        TRACE("ota_http: crc ok, mark_ready...\n");
        if (ota_mark_ready(fw_len, ver) != 0) { TRACE("ota_http: mark_ready FAIL\n"); return; }
        TRACE("ota_http: mark_ready ok, reset...\n");
        sleep_ms(500);
        system_reset();
    }
}
