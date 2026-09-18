/**
 * @file ota_boot.c
 * @brief Bootloader 核心：标志页读写、QSPI 暂存校验、commit（写另一 bank）、
 *        boot count 自检回滚、跳 App。
 *
 * 引导决策（boot_run）：
 *   当前 bank 标志 NEED_COMMIT=1  -> 校验 QSPI 暂存 -> commit 到另一 bank -> 清标志 -> swap -> 复位
 *   当前 bank 标志 NEED_CONFIRM=1 -> boot_count++；超限 -> swap 回滚；否则跳 App
 *   否则 -> 跳 App（当前 bank）
 */
#include <string.h>
#include "hc32f4xx.h"
#include "hc32_ll.h"
#include "hc32_ll_efm.h"
#include "hc32_ll_utility.h"
#include "boot_cfg.h"
#include "ota_boot.h"
extern void boot_printf(const char *fmt, ...);

/* QSPI 读（boot_qspi.c） */
void boot_qspi_init(void);
int  boot_qspi_read(uint32_t addr, uint8_t *buf, uint32_t size);

/* ---- 本地 CRC32（IEEE 0xEDB88320，与统一 OTA 包一致） ---- */
static uint32_t s_crc_tab[256];
static int s_crc_ok = 0;
static void crc_tab_init(void)
{
    if (s_crc_ok) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        s_crc_tab[i] = c;
    }
    s_crc_ok = 1;
}
static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    crc_tab_init();
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++)
        crc = s_crc_tab[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* ---- EFM 辅助 ---- */
static int efm_sector_erase(uint32_t addr)
{
    return (int)EFM_SectorErase(addr);
}

static int efm_program(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    return (int)EFM_Program(addr, buf, len);
}

/* ---- 标志页 ---- */
static uint32_t flag_crc(const boot_flag_t *f)
{
    return crc32_update(0, (const uint8_t *)f, 20);
}

int boot_flag_read(boot_flag_t *f)
{
    if (f == NULL) return -1;
    memcpy(f, (const void *)BOOT_FLAG_BASE, sizeof(boot_flag_t));
    if (f->magic != BOOT_FLAG_MAGIC) return -1;
    if (f->crc32 != flag_crc(f))     return -1;
    return 0;
}

static int flag_write_at(uint32_t base, const boot_flag_t *f)
{
    boot_flag_t tmp = *f;
    tmp.crc32 = flag_crc(f);
    EFM_SingleSectorOperateCmd((uint8_t)(base / 0x2000UL), ENABLE);   /* AN: unprotect 8KB sector */
    if (efm_sector_erase(base) != 0)
        return -1;
    return efm_program(base, (const uint8_t *)&tmp, sizeof(tmp));
}

int boot_flag_write(const boot_flag_t *f)
{
    return flag_write_at(BOOT_FLAG_BASE, f);
}

int boot_flag_write_other(const boot_flag_t *f)
{
    return flag_write_at(BOOT_OTHER_FLAG_BASE, f);
}

/* ---- QSPI 暂存包校验：magic + payload CRC32 与包头比对 ---- */
static int verify_staged_pkg(uint32_t *fw_len, uint32_t *version, uint32_t *crc_out)
{
    uint8_t hdr[BOOT_OTA_HDR_LEN];
    uint8_t buf[1024];
    uint32_t size, remain, crc = 0, crc_expect;

    if (boot_qspi_read(BOOT_QSPI_STAGE_BASE, hdr, sizeof(hdr)) != 0) {
        boot_printf("BOOT: qspi hdr read FAIL\n");
        return -1;
    }
    if (hdr[0] != BOOT_OTA_MAGIC0 || hdr[1] != BOOT_OTA_MAGIC1 ||
        hdr[2] != BOOT_OTA_MAGIC2 || hdr[3] != BOOT_OTA_MAGIC3) {
        boot_printf("BOOT: bad magic %02X %02X %02X %02X\n",
                    (unsigned)hdr[0], (unsigned)hdr[1], (unsigned)hdr[2], (unsigned)hdr[3]);
        return -1;
    }

    *version = ((uint32_t)hdr[BOOT_OTA_VER_OFF]) | ((uint32_t)hdr[BOOT_OTA_VER_OFF + 1] << 8) |
               ((uint32_t)hdr[BOOT_OTA_VER_OFF + 2] << 16) | ((uint32_t)hdr[BOOT_OTA_VER_OFF + 3] << 24);
    size = ((uint32_t)hdr[BOOT_OTA_LEN_OFF]) | ((uint32_t)hdr[BOOT_OTA_LEN_OFF + 1] << 8) |
           ((uint32_t)hdr[BOOT_OTA_LEN_OFF + 2] << 16) | ((uint32_t)hdr[BOOT_OTA_LEN_OFF + 3] << 24);
    crc_expect = ((uint32_t)hdr[BOOT_OTA_CRC_OFF]) | ((uint32_t)hdr[BOOT_OTA_CRC_OFF + 1] << 8) |
                 ((uint32_t)hdr[BOOT_OTA_CRC_OFF + 2] << 16) | ((uint32_t)hdr[BOOT_OTA_CRC_OFF + 3] << 24);
    if (crc_out) *crc_out = crc_expect;   /* v9.81cl: 供 swap 前复核另一 bank */

    if (size == 0 || size > BOOT_QSPI_STAGE_MAX) {
        boot_printf("BOOT: bad size %lu\n", (unsigned long)size);
        return -1;
    }
    *fw_len = size;

    remain = size;
    for (uint32_t off = BOOT_OTA_HDR_LEN; off < BOOT_OTA_HDR_LEN + size; off += sizeof(buf)) {
        uint32_t n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        if (boot_qspi_read(BOOT_QSPI_STAGE_BASE + off, buf, n) != 0) {
            boot_printf("BOOT: qspi data read FAIL off=%lu\n", (unsigned long)off);
            return -1;
        }
        crc = crc32_update(crc, buf, n);
        remain -= n;
    }
    if (crc != crc_expect) {
        boot_printf("BOOT: crc fail calc=%08X exp=%08X\n", (unsigned)crc, (unsigned)crc_expect);
        return -2;
    }
    boot_printf("BOOT: verify OK len=%lu\n", (unsigned long)size);
    return 0;
}

static int commit_to_other_bank(uint32_t fw_len, uint32_t version)
{
    uint8_t buf[1024];
    uint32_t remain;
    boot_flag_t f;

    /* 1) bootloader 自复制：当前 bank 0x0 (64KB) -> 另一 bank 0x00100000（4KB 扇区对齐擦） */
    remain = BOOT_BOOT_SIZE;
    for (uint32_t off = 0; off < BOOT_BOOT_SIZE; off += sizeof(buf)) {
        uint32_t n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        memcpy(buf, (const void *)(BOOT_BOOT_BASE + off), n);
        if ((off & 0x1FFFUL) == 0UL) {   /* 8KB 扇区对齐（EFM_SectorErase 需要） */
            EFM_SingleSectorOperateCmd((uint8_t)((BOOT_OTHER_BOOT_BASE + off) / 0x2000UL), ENABLE);
            if (efm_sector_erase(BOOT_OTHER_BOOT_BASE + off) != 0)
                return -1;
        }
        if (efm_program(BOOT_OTHER_BOOT_BASE + off, buf, n) != 0)
            return -1;
        remain -= n;
    }

    /* 2) App：QSPI 暂存(82B 后) -> 另一 bank App 区（4KB 扇区对齐擦） */
    remain = fw_len;
    for (uint32_t off = 0; off < fw_len; off += sizeof(buf)) {
        uint32_t n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        if (boot_qspi_read(BOOT_QSPI_STAGE_BASE + BOOT_OTA_HDR_LEN + off, buf, n) != 0)
            return -1;
        if ((off & 0x1FFFUL) == 0UL) {   /* 8KB 扇区对齐 */
            EFM_SingleSectorOperateCmd((uint8_t)((BOOT_OTHER_APP_BASE + off) / 0x2000UL), ENABLE);
            if (efm_sector_erase(BOOT_OTHER_APP_BASE + off) != 0)
                return -1;
        }
        if (efm_program(BOOT_OTHER_APP_BASE + off, buf, n) != 0)
            return -1;
        remain -= n;
    }

    /* 3) 另一 bank 标志：NEED_CONFIRM（新固件待自检） */
    memset(&f, 0, sizeof(f));
    f.magic      = BOOT_FLAG_MAGIC;
    f.flags      = BOOT_FLAG_NEED_CONFIRM;
    f.size       = fw_len;
    f.version    = version;
    f.boot_count = 0;
    if (boot_flag_write_other(&f) != 0)
        return -1;

    /* 4) 清当前 bank 标志（NEED_COMMIT 已处理，防回滚死循环） */
    memset(&f, 0, sizeof(f));
    f.magic = BOOT_FLAG_MAGIC;
    if (boot_flag_write(&f) != 0)
        return -1;

    return 0;
}

/* ---- v9.81cl: swap 前校验另一 bank ----
 * commit 写的另一 bank boot 副本/App 若在写入瞬间损坏（位翻转/读碰撞/磨损），
 * swap 后 CPU 从坏 boot 启动，回滚逻辑也在 boot 里 → 直接变砖。
 * 因此 swap 前必须逐字节比对 boot 副本 + 复核 App CRC + 向量表有效性，
 * 任何不一致都放弃 swap，保留旧固件运行。 */
static int verify_other_bank(uint32_t fw_len, uint32_t crc_expect)
{
    uint8_t buf[1024];
    uint32_t crc = 0, remain;
    uint32_t off, n;
    uint32_t stack;

    /* 1) boot 副本：逐块与当前 bank boot 比对（自复制必须逐字节一致） */
    remain = BOOT_BOOT_SIZE;
    for (off = 0; off < BOOT_BOOT_SIZE; off += sizeof(buf)) {
        n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        if (memcmp((const void *)(BOOT_BOOT_BASE + off),
                   (const void *)(BOOT_OTHER_BOOT_BASE + off), n) != 0) {
            boot_printf("BOOT: verify other-boot MISMATCH off=%lu\n", (unsigned long)off);
            return -1;
        }
        remain -= n;
    }

    /* 2) App 区：CRC32 与 OTA 包头期望值复核（不依赖暂存校验结果） */
    remain = fw_len;
    for (off = 0; off < fw_len; off += sizeof(buf)) {
        n = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        memcpy(buf, (const void *)(BOOT_OTHER_APP_BASE + off), n);
        crc = crc32_update(crc, buf, n);
        remain -= n;
    }
    if (crc != crc_expect) {
        boot_printf("BOOT: verify other-app CRC fail calc=%08X exp=%08X\n",
                    (unsigned)crc, (unsigned)crc_expect);
        return -2;
    }

    /* 3) 新 App 向量表有效性（与 boot_jump_app 相同判定） */
    stack = *((volatile uint32_t *)BOOT_OTHER_APP_BASE);
    if ((stack < 0x1FFE0000UL) || (stack > 0x20061000UL)) {
        boot_printf("BOOT: verify other-app vector bad SP=%08X\n", (unsigned)stack);
        return -3;
    }
    return 0;
}

/* ---- swap 切换：下次复位从另一 bank 启动 ---- */
static int swap_toggle(void)
{
    int rc;

    /* RM 7.9.1：写任意 EFM 寄存器会重新锁定 FAPRT；commit 过程已多次操作 EFM 寄存器，
     * 此处必须重新解锁 FAPRT/KEY1，否则 EFM_SwapCmd 设置 PEMOD 失败 → 写 0x03002000 不生效 */
    EFM_REG_Unlock();
    EFM_FWMC_Cmd(ENABLE);
    /* 引导交换 = 对 FLASH 地址 0x03002000 编程（RM 7.8），不涉及 OTP 数据区 */
    if (EFM_GetSwapStatus() == RESET) {
        rc = (int)EFM_SwapCmd(ENABLE);
    } else {
        rc = (int)EFM_SwapCmd(DISABLE);
    }
    return rc;
}

/* ---- 跳 App ---- */
void boot_jump_app(void)
{
    uint32_t app = BOOT_APP_BASE;
    uint32_t stack = *((volatile uint32_t *)app);
    void (*reset)(void) = (void (*)(void))(*((volatile uint32_t *)(app + 4)));

    /* 栈顶应在 SRAM 范围（F4A0 主体 SRAM 0x1FFE0000-0x20061000，516KB） */
    if ((stack >= 0x1FFE0000UL) && (stack <= 0x20061000UL)) {
        __disable_irq();
        __set_MSP(stack);
			  DDL_DelayMS(1);
			  boot_printf("BOOT: boot_jump_app\n");
        reset();
    }
    /* 非法则停留（等看门狗/人工复位） */
    for (;;) {
    }
}

/* ---- 引导决策 ---- */
void boot_run(void)
{
    boot_flag_t f;
    uint32_t fw_len = 0, version = 0;

    if (boot_flag_read(&f) != 0) {
        /* 无有效标志 -> 直接跳 App */
        boot_jump_app();
        return;
    }

    if (f.flags & BOOT_FLAG_NEED_COMMIT) {
        /* App 已下载完：校验暂存 -> commit -> 校验另一 bank -> swap -> 复位 */
        uint32_t crc_expect = 0;
        boot_printf("BOOT: NEED_COMMIT ver=0x%08X\n", (unsigned)f.version);
        if (verify_staged_pkg(&fw_len, &version, &crc_expect) == 0) {
            boot_printf("BOOT: verify OK len=%lu\n", (unsigned long)fw_len);
            int cr = commit_to_other_bank(fw_len, version);
            boot_printf("BOOT: commit ret=%d\n", cr);
            if (cr == 0) {
                /* v9.81cl: swap 前校验另一 bank，任何不一致放弃 swap（防变砖） */
                int vr = verify_other_bank(fw_len, crc_expect);
                boot_printf("BOOT: verify other-bank ret=%d\n", vr);
                if (vr == 0) {
                    int sr = swap_toggle();
                    boot_printf("BOOT: swap ret=%d\n", sr);
                    if (sr == 0) {
                        NVIC_SystemReset();
                    }
                } else {
                    /* 校验失败：清另一 bank 标志，防未来误 swap 引导坏镜像 */
                    memset(&f, 0, sizeof(f));
                    f.magic = BOOT_FLAG_MAGIC;
                    (void)boot_flag_write_other(&f);
                }
            }
        } else {
            boot_printf("BOOT: verify FAIL\n");
        }
        /* 校验/commit 失败：清标志，保留旧固件继续跑 */
        memset(&f, 0, sizeof(f));
        f.magic = BOOT_FLAG_MAGIC;
        (void)boot_flag_write(&f);
        boot_jump_app();
        return;
    }

    if (f.flags & BOOT_FLAG_NEED_CONFIRM) {
        /* 新固件待自检：boot_count 累加 */
        f.boot_count++;
        if (f.boot_count > BOOT_MAX_BOOT_COUNT) {
            /* 自检失败超限 -> 回滚到另一 bank（旧固件） */
            memset(&f, 0, sizeof(f));
            f.magic = BOOT_FLAG_MAGIC;
            (void)boot_flag_write(&f);
            if (swap_toggle() == 0)
                NVIC_SystemReset();
            return;
        }
        (void)boot_flag_write(&f);
        boot_jump_app();
        return;
    }

    /* 正常：跳 App */
    boot_jump_app();
}
