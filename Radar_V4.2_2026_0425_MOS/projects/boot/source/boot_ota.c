/**
 *******************************************************************************
 * @file  boot_ota.c
 * @brief F460 Bootloader 引导实现（A/B 双槽、无搬运）
 *
 *   契约（与 App 侧 ota_recv.c / ota_flash.c 一致，见 docs/ota_boot_design.md v0.2）：
 *     App 下载完 -> 写【非活动槽】+ 整镜像 CRC32 校验 -> 一次标志写入激活
 *                   (active 切目标槽 + TRIAL + NEED_CONFIRM) -> 复位
 *     Boot: 读双份标志 -> 选中槽校验(向量表 + 槽镜像 CRC32)
 *             RUNNABLE -> 直接跳
 *             TRIAL    -> boot_count++；超 OTA_FLAG_MAX_BOOT 则标 FAILED 并切另一槽
 *             无标志   -> 按 A -> B 取第一个有效槽
 *             两槽皆无效 -> 停在 Boot（不跳任何槽 = 不砖）
 *
 *   A/B 无搬运：Boot 不做任何拷贝，激活只是一个标志翻转 —— 没有搬运中断窗口。
 *   擦写代码（ota_flash.o / hc32_ll_efm.o / boot_ota.o）由 scatter 放 RAM 执行。
 *******************************************************************************
 */
#include <stdio.h>
#include "hc32_ll.h"
#include "hc32_ll_clk.h"     /* CLK_SetSysClockSrc / CLK_PLLCmd / CLK_SYSCLK_SRC_HRC */
#include "hc32_ll_efm.h"     /* EFM_SetWaitCycle / EFM_WAIT_CYCLE0 */
#include "boot_ota.h"
#include "ota_flash.h"       /* ota_flag_read/write, ota_img_check（App 同一个模块） */

extern void SWDT_FeedDog(void);

/* 主 SRAM：0x1FFF8000 起 188KB（另 0x200F0000 的 4KB 独立块不作栈区） */
#define BOOT_SRAM_LO        0x1FFF8000UL
#define BOOT_SRAM_HI        0x20027000UL

/* 槽向量表有效性：栈顶落在 SRAM 内 + 复位向量落在该槽内 */
static int32_t boot_vec_ok(uint32_t u32Slot)
{
    uint32_t u32Base = OTA_SLOT_BASE(u32Slot);
    uint32_t u32Sp   = *(volatile uint32_t *)(u32Base);
    uint32_t u32Pc   = *(volatile uint32_t *)(u32Base + 4UL);

    if ((u32Sp < BOOT_SRAM_LO) || (u32Sp > BOOT_SRAM_HI)) {
        return -1;                      /* 擦除态全 0xFF 即在此被挡掉 */
    }
    if (((u32Pc & ~1UL) < u32Base) || ((u32Pc & ~1UL) >= (u32Base + OTA_SLOT_SIZE))) {
        return -2;
    }
    return 0;
}

/* 跳转：不把配好的 PLL 交出去 —— 退回 HRC、关 PLL、等待周期归 0，让 App 自行初始化 */
static void boot_jump(uint32_t u32Slot)
{
    uint32_t u32Base = OTA_SLOT_BASE(u32Slot);
    uint32_t u32Sp;
    void   (*pfnApp)(void);

    __disable_irq();

    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_HRC);
    (void)CLK_PLLCmd(DISABLE);
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE0);

    SCB->VTOR = u32Base;
    __DSB();
    __ISB();

    u32Sp  = *(volatile uint32_t *)(u32Base);
    pfnApp = (void (*)(void))(*(volatile uint32_t *)(u32Base + 4UL));

    __set_MSP(u32Sp);
    pfnApp();

    for (;;) {                          /* 不应到达 */
        ;
    }
}

static int32_t boot_pick_valid(uint32_t *pu32Slot)
{
    uint32_t u32S;

    for (u32S = 0UL; u32S < 2UL; u32S++) {
        if ((boot_vec_ok(u32S) == 0) && (ota_img_check(u32S) == 0)) {
            *pu32Slot = u32S;
            return 0;
        }
    }
    return -1;
}

static void boot_halt(void)
{
    printf("BOOT: no runnable slot - halt (debugger recovery required)\r\n");
    for (;;) {
        SWDT_FeedDog();
    }
}

/**
 * @brief 引导入口（不返回）
 */
void BOOT_OTA_Run(void)
{
    ota_flag_t stcFlag;
    uint32_t   u32Slot;

    printf("=========== BOOT (A/B no-copy) ===========\r\n");

    if (ota_flag_read(&stcFlag) != 0) {
        /* 标志无效：按 A -> B 找第一个能跑的槽 */
        if (boot_pick_valid(&u32Slot) != 0) {
            boot_halt();
        }
        boot_jump(u32Slot);
    }

    u32Slot = (stcFlag.active == OTA_SLOT_B) ? OTA_SLOT_B : OTA_SLOT_A;

    /* 选中槽必须过校验，否则换另一槽 */
    if ((boot_vec_ok(u32Slot) != 0) || (ota_img_check(u32Slot) != 0)) {
        if (boot_pick_valid(&u32Slot) != 0) {
            boot_halt();
        }
        stcFlag.active = u32Slot;
        stcFlag.flags &= ~OTA_FLAG_NEED_CONFIRM;
        (void)ota_flag_write(&stcFlag);
        boot_jump(u32Slot);
    }

    /* 试运行：本次启动计数 +1；超限即认为该槽起不来，标 FAILED 并切另一槽 */
    if (((u32Slot == OTA_SLOT_A) && (stcFlag.state_a == (uint32_t)OTA_SLOT_TRIAL)) ||
        ((u32Slot == OTA_SLOT_B) && (stcFlag.state_b == (uint32_t)OTA_SLOT_TRIAL))) {

        stcFlag.boot_count += 1UL;

        if (stcFlag.boot_count > OTA_FLAG_MAX_BOOT) {
            uint32_t u32Other = OTA_SLOT_OTHER(u32Slot);

            if (u32Slot == OTA_SLOT_A) { stcFlag.state_a = (uint32_t)OTA_SLOT_FAILED; stcFlag.fail_a += 1UL; }
            else                       { stcFlag.state_b = (uint32_t)OTA_SLOT_FAILED; stcFlag.fail_b += 1UL; }
            stcFlag.boot_count = 0UL;
            stcFlag.flags &= ~OTA_FLAG_NEED_CONFIRM;

            if ((boot_vec_ok(u32Other) == 0) && (ota_img_check(u32Other) == 0)) {
                stcFlag.active = u32Other;
                if (u32Other == OTA_SLOT_A) { stcFlag.state_a = (uint32_t)OTA_SLOT_RUNNABLE; }
                else                        { stcFlag.state_b = (uint32_t)OTA_SLOT_RUNNABLE; }
                (void)ota_flag_write(&stcFlag);
                boot_jump(u32Other);
            }
            (void)ota_flag_write(&stcFlag);
            boot_halt();
        }

        (void)ota_flag_write(&stcFlag);
        SWDT_FeedDog();
        printf("BOOT: trial boot_count=%u\r\n", (unsigned int)stcFlag.boot_count);
        boot_jump(u32Slot);
    }

    /* RUNNABLE：直接跳，不再动 Flash */
    boot_jump(u32Slot);
}

/******************************************************************************
 * EOF
 *****************************************************************************/
