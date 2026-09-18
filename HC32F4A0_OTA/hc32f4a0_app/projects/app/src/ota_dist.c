/**
 * @file  ota_dist.c
 * @brief F4A0 侧固件分发集成层实现
 */
#include <string.h>
#include "hc32f46_driver.h"      /* read/write(fd) / Uart_RS485_send 所在头 */
#include "ota_dist.h"
#include "ota_host.h"

static ota_host_t s_host;
static int        s_fd = -1;
static int        s_active = 0;
static int        s_result = 0;

/* ---- ota_host 需要的 IO 回调 ---- */
static int io_write(void *ctx, const uint8_t *buf, uint32_t len)
{
    int fd = *(int *)ctx;
    if (fd < 0) { return -1; }
    return (write(fd, buf, len) < 0) ? -1 : 0;
}

static int io_read(void *ctx, uint8_t *buf, uint32_t len)
{
    int fd = *(int *)ctx;
    if (fd < 0) { return -1; }
    return read(fd, buf, len);          /* 非阻塞：0 = 暂无数据 */
}

static uint32_t io_tick(void *ctx)
{
    (void)ctx;
    return (uint32_t)osKernelGetTickCount();
}

int ota_dist_start(int fd, const uint8_t *pkg, uint32_t len)
{
    ota_host_io_t io;

    if (s_active) { return -1; }        /* 一次只允许一路（闸门是全局的） */
    if (fd < 0 || pkg == NULL || len == 0UL) { return -2; }

    (void)memset(&io, 0, sizeof(io));
    io.write   = io_write;
    io.read    = io_read;
    io.tick_ms = io_tick;
    io.ctx     = &s_fd;

    ota_host_init(&s_host, &io, OTA_HOST_MAX_PAYLOAD);

    s_fd = fd;
    s_result = OTA_HOST_BUSY;
    if (ota_host_start(&s_host, pkg, len) != OTA_HOST_BUSY) {
        s_fd = -1;
        s_result = OTA_HOST_ERR_PARAM;
        return -3;
    }
    s_active = 1;
    return 0;
}

int ota_dist_poll(void)
{
    int r;

    if (!s_active) { return 0; }

    r = ota_host_poll(&s_host);
    if (r == OTA_HOST_BUSY) { return 1; }

    /* 结束：结果固化后释放闸门，业务帧自动恢复 */
    s_result = r;
    s_active = 0;
    return 0;
}

int ota_dist_busy(void)
{
    return s_active;
}

uint32_t ota_dist_progress(void)
{
    return s_host.off;
}

uint32_t ota_dist_total(void)
{
    return s_host.total;
}

int ota_dist_result(void)
{
    return s_active ? 0 : s_result;
}

void ota_dist_abort(void)
{
    s_active = 0;
    s_fd     = -1;
    s_result = OTA_HOST_ERR_IO;
}
