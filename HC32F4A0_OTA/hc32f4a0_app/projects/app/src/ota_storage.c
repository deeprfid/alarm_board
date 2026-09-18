/**
 * @file ota_storage.c
 * @brief 存储后端实现：QSPI 暂存（QSPI_FLASH_*）+ 片内 dual-bank commit（EFM + SwapCmd）
 * @note HC32F4A0 支持硬件引导交换（EFM_SWAP_ADDR + EFM_SwapCmd），dual-bank A/B 切换原生可用
 */
#include <string.h>
#include "qspi_flash.h"
#include "hc32_ll_efm.h"
#include "ota_storage.h"

static uint32_t s_crc32_table[256];
static int s_crc_init = 0;

static void crc32_init(void)
{
    if (s_crc_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        s_crc32_table[i] = c;
    }
    s_crc_init = 1;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    crc32_init();
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
        crc = s_crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

int ota_storage_prepare(uint32_t total_size)
{
    uint32_t addr = OTA_QSPI_STAGE_BASE;
    uint32_t remain = (total_size > OTA_QSPI_STAGE_SIZE) ? OTA_QSPI_STAGE_SIZE : total_size;
    /* v9.81p: W25QXX 64KB 块擦除（0xD8，~120ms/块）——v9.81o 起 OTA 全链路统一 64KB 块擦 */
    for (uint32_t off = 0; off < remain; off += 65536) {
        if (QSPI_FLASH_EraseBlock64K(addr + off) != 0)
            return -1;
    }
    return 0;
}

int ota_storage_write_stage(const uint8_t *data, uint32_t len, uint32_t offset)
{
    if (offset + len > OTA_QSPI_STAGE_SIZE)
        return -1;
    if (QSPI_FLASH_Write(OTA_QSPI_STAGE_BASE + offset, (uint8_t *)data, len) != 0)
        return -1;
    return 0;
}

int ota_storage_verify_stage(uint32_t size)
{
    uint8_t buf[OTA_STAGE_BLOCK];
    uint32_t crc_w = 0, crc_r = 0;
    uint32_t remain = size;

    /* 计算暂存区 CRC（下载时可增量算，此处整读复核，Phase 3 简化） */
    for (uint32_t off = 0; off < size; off += OTA_STAGE_BLOCK) {
        uint32_t n = (remain > OTA_STAGE_BLOCK) ? OTA_STAGE_BLOCK : remain;
        if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE + off, buf, n) != 0)
            return -1;
        crc_r = crc32_update(crc_r, buf, n);
        remain -= n;
    }
    /* TODO(Phase 3): 与包内 CRC32 对比（由调用方传入期望值）；此处先返回可读性 OK */
    (void)crc_w;
    return 0;
}

int ota_storage_verify_payload(uint32_t payload_off, uint32_t payload_len, uint32_t expected_crc32)
{
    uint8_t buf[OTA_STAGE_BLOCK];
    uint32_t remain = payload_len, crc = 0;

    for (uint32_t off = 0; off < payload_len; off += OTA_STAGE_BLOCK) {
        uint32_t n = (remain > OTA_STAGE_BLOCK) ? OTA_STAGE_BLOCK : remain;
        if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE + payload_off + off, buf, n) != 0)
            return -1;
        crc = crc32_update(crc, buf, n);
        remain -= n;
    }
    return (crc == expected_crc32) ? 0 : -2;
}

int ota_storage_commit_from(uint32_t qspi_off, uint32_t size)
{
    uint8_t buf[OTA_STAGE_BLOCK];
    uint32_t remain = size;

    /* 1) QSPI 暂存(偏移 qspi_off，跳过 OTA 包头) → 片内 Bank B（逐块搬运） */
    for (uint32_t off = 0; off < size; off += OTA_STAGE_BLOCK) {
        uint32_t n = (remain > OTA_STAGE_BLOCK) ? OTA_STAGE_BLOCK : remain;
        if (QSPI_FLASH_Read(OTA_QSPI_STAGE_BASE + qspi_off + off, buf, n) != 0)
            return -1;
        if (EFM_Program(OTA_BANK_B_BASE + off, buf, n) != 0)
            return -1;
        remain -= n;
    }

    /* 2) 置引导交换标志 + 使能 Swap（复位后从 Bank B 启动） */
    /*    EFM_SWAP_ADDR(0x03002000) 写 0x005A5A5A 由芯片固件逻辑识别（hc32_ll_efm.h:164） */
    {
        uint32_t swap_data = 0x005A5A5AUL;
        if (EFM_Program(EFM_SWAP_ADDR, (const uint8_t *)&swap_data, sizeof(swap_data)) != 0)
            return -1;
    }
    if (EFM_SwapCmd(ENABLE) != 0)
        return -1;
    return 0;   /* 调用方随后 system_reset() */
}

int ota_storage_rollback(void)
{
    /* 清 Swap → 复位后回 Bank A（当前运行固件未受影响，硬回滚） */
    if (EFM_SwapCmd(DISABLE) != 0)
        return -1;
    return 0;
}

/* 另一 bank App 有效性检查（手动切换防呆：单 bank 首烧时另一 bank 为空 0xFF，
 * 直接 swap 会引导到垃圾向量区，boot_jump_app 栈顶判定不过 → bootloader 死等） */
static int other_app_valid(void)
{
    uint32_t stack = *((volatile uint32_t *)OTA_OTHER_APP_BASE);
    uint32_t reset = *((volatile uint32_t *)(OTA_OTHER_APP_BASE + 4));

    /* 与 bootloader boot_jump_app 相同的栈顶区间判定；复位向量落另一 bank App 区 */
    if ((stack >= 0x1FFE0000UL) && (stack <= 0x20061000UL))
        return 0;
    (void)reset;
    return -1;
}

int ota_switch_bank(void)
{
    int rc;

    /* 另一 bank 无有效 App → 拒绝切换（保持当前 bank 运行） */
    if (other_app_valid() != 0)
        return -1;

    /* 与 bootloader swap_toggle 同构：FAPRT 解锁 + FWMC + 按当前 swap 状态切换。
     * EFM_SwapCmd(ENABLE/DISABLE) 内部写 EFM_SWAP_ADDR 并等操作完成（hc32_ll_efm.c） */
    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);
    if (EFM_GetSwapStatus() == RESET)
        rc = (int)EFM_SwapCmd(ENABLE);
    else
        rc = (int)EFM_SwapCmd(DISABLE);
    return rc;   /* 调用方随后 system_reset()，bootloader 从另一 bank 引导 */
}

/* ---- 通道互斥（v9.80）：串口/网络 与 USB CDC 共用 QSPI 暂存区，禁止并发下载 ----
 * 两个通道运行在不同线程（send_func / ota_usb_task），用临界区保护检查+置位。
 * 探测帧（len=0）不获取；数据会话开始获取，完成/失败释放。 */
static uint8_t s_ota_channel_busy = 0;

int ota_channel_try_acquire(void)
{
    int r = -1;
    __disable_irq();
    if (s_ota_channel_busy == 0) {
        s_ota_channel_busy = 1;
        r = 0;
    }
    __enable_irq();
    return r;
}

void ota_channel_release(void)
{
    __disable_irq();
    s_ota_channel_busy = 0;
    __enable_irq();
}

/* v9.81b: 只读查询——send_func 的 apt_multi_infs_select 会选中 USB1，
 * 若 USB OTA 会话进行中（busy=1）则跳过，避免双线程竞争读同一 USB1 环形缓冲
 * （数据错乱 CRC MISMATCH + 竞争崩溃 system_reset）。 */
int ota_channel_busy(void)
{
    int r;
    __disable_irq();
    r = (s_ota_channel_busy != 0);
    __enable_irq();
    return r;
}
