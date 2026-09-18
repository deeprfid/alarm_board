/**
 * @file ota_agent.c
 * @brief 升级 Agent：下载→校验→置标志→复位；启动时 commit（Phase 3 最小闭环）
 * @note Phase 4 将 commit 移入独立 bootloader；当前在 App 验证逻辑（commit 写 Bank B 非运行区，安全）
 */
#include <string.h>
#include "qspi_flash.h"
#include "hc32f46_driver.h"
#include "ota_download.h"
#include "ota_storage.h"
#include "ota_state.h"
#include "ota_flag.h"
#include "ota_security.h"
#include "hc32_ll_efm.h"      /* EFM_GetSwapStatus */
#include "ota_agent.h"

#define OTA_HEADER_LEN      82
#define OTA_HDR_VERSION_OFF 4
#define OTA_HDR_PAYLOAD_OFF 10
#define OTA_HDR_CRC_OFF     14

/* v9.81p: 新固件待自检确认状态（NEED_CONFIRM 延迟到业务就绪后确认） */
static int s_pending_confirm = 0;

/* 读暂存包头 + 校验 payload CRC；输出固件长度（payload）与版本 */
static int verify_staged_pkg(uint32_t *fw_len, uint32_t *version)
{
    uint8_t hdr[OTA_HEADER_LEN];
    uint32_t size, crc_expect;

    if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE, hdr, sizeof(hdr)) != 0)
        return -1;
    if (hdr[0] != 'O' || hdr[1] != 'T' || hdr[2] != 'A' || hdr[3] != '1')
        return -1;

    *version = ((uint32_t)hdr[OTA_HDR_VERSION_OFF]) | ((uint32_t)hdr[OTA_HDR_VERSION_OFF + 1] << 8) |
               ((uint32_t)hdr[OTA_HDR_VERSION_OFF + 2] << 16) | ((uint32_t)hdr[OTA_HDR_VERSION_OFF + 3] << 24);
    size = ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF]) | ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF + 1] << 8) |
           ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF + 2] << 16) | ((uint32_t)hdr[OTA_HDR_PAYLOAD_OFF + 3] << 24);
    *fw_len = size;

    /* 载荷 CRC32 与包头 crc32 字段比对（读回复核；与本地 UART 通道同一校验） */
    crc_expect = ((uint32_t)hdr[OTA_HDR_CRC_OFF]) | ((uint32_t)hdr[OTA_HDR_CRC_OFF + 1] << 8) |
                 ((uint32_t)hdr[OTA_HDR_CRC_OFF + 2] << 16) | ((uint32_t)hdr[OTA_HDR_CRC_OFF + 3] << 24);
    if (ota_storage_verify_payload(OTA_HEADER_LEN, size, crc_expect) != 0) {
        TRACE("ota payload crc fail\n");
        return -1;
    }
    /* Phase 3 续：包级验签（HMAC-SHA256）+ SHA256 完整性（LEVEL 控制） */
    if (ota_security_verify_staged() != 0) {
        TRACE("ota verify security fail\n");
        return -1;
    }
    return 0;
}

int ota_agent_run(const char *url)
{
    uint32_t fw_len = 0, version = 0;

    if (ota_download_start(url) != 0) {
        ota_fail(1);
        return -1;
    }
    if (verify_staged_pkg(&fw_len, &version) != 0) {
        ota_fail(1);
        return -1;
    }
    if (ota_mark_ready(fw_len, version) != 0) {
        ota_fail(1);
        return -1;
    }
    TRACE("ota_agent_run ready ver:%lu size:%lu, reset...\n",
          (unsigned long)version, (unsigned long)fw_len);
    sleep_ms(500);
    system_reset();
    return 0;
}

void ota_agent_boot(void)
{
    ota_flag_t f;
    int fr;

    /* v9.81t: 移除启动 banner 诊断打印 */
    fr = ota_flag_read(&f);
    if (fr != 0)
        return;                       /* 无有效标志：正常启动 */
    if (f.flags & OTA_FLAG_NEED_CONFIRM) {
        TRACE("ota_agent_boot: new fw v%lu pending confirm\n", (unsigned long)f.version);
        s_pending_confirm = 1;
    }
    /* NEED_COMMIT 由 bootloader 处理；此处 App 正常继续 */
}


void ota_agent_confirm(void)
{
    if (s_pending_confirm) {
        TRACE("ota_agent_confirm: self-check ok, confirm\n");
        ota_boot_confirm();
        ota_set_progress(0);   /* v9.81br */
        s_pending_confirm = 0;
    }
}
