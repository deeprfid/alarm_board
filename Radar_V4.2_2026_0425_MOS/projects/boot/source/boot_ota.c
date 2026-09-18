/**
 *******************************************************************************
 * @file  boot_ota.c
 * @brief F460 Bootloader 引导实现（A/B 双槽、无搬运）
 *
 *   契约（与 App 侧 ota_recv.c / ota_flash.c 一致，见 docs/ota_boot_design.md v0.2）：
 *     App 下载完 -> 写【非活动槽】+ 整镜像 CRC32 校验 -> 一次标志写入激活
 *                   (active 切目标槽 + TRIAL + NEED_CONFIRM) -> 复位
 *     Boot: 读双份标志 -> 选中槽可用性判定
 *             RUNNABLE -> 直接跳
 *             TRIAL    -> boot_count++；超 OTA_FLAG_MAX_BOOT 则标 FAILED 并切另一槽
 *             无标志   -> 按 A -> B 取第一个可用槽
 *             两槽皆不可用 -> 停在 Boot（不跳任何槽 = 不砖）
 *
 *   【调试手段】本板没有可接 printf 的调试口（唯一的串口是 RS485 业务口），
 *   故一切诊断改由 LED 编码输出，见 boot_led.h 的编码表。
 *
 *   A/B 无搬运：Boot 不做任何拷贝，激活只是一个标志翻转 —— 没有搬运中断窗口。
 *   擦写代码（ota_flash.o / hc32_ll_efm.o / boot_ota.o）由 scatter 放 RAM 执行。
 *******************************************************************************
 */
#include "hc32_ll.h"
#include "hc32_ll_clk.h"     /* CLK_SetSysClockSrc / CLK_PLLCmd / CLK_SYSCLK_SRC_HRC */
#include "hc32_ll_efm.h"     /* EFM_SetWaitCycle / EFM_WAIT_CYCLE0 */
#include "boot_ota.h"
#include "boot_led.h"
#include "ota_flash.h"       /* ota_flag_read/write, ota_img_check, ota_flash_read */

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

/* 槽是否可用：
 *   向量表必须有效（能挡掉擦除态：栈顶 0xFFFFFFFF 不在 SRAM 内）；
 *   槽尾元数据若【存在】则必须 CRC32 通过；若【为空】(=0xFF, 从未写过)则以向量表为准。
 *
 *   为什么允许「元数据为空」：trailer 只在 OTA 下载完成时由 App 写入，而产线/调试器
 *   直接烧录 App 时没人写它 —— 若强制要求 CRC，首次烧录的板子会永远卡在 Boot。
 *   OTA 路径安全性不受影响：App 激活前已用 OTA1 包头的 CRC32 校验过整镜像。 */
static int32_t boot_slot_usable(uint32_t u32Slot)
{
    uint8_t  u8Magic[4];
    uint32_t u32Magic;

    if (boot_vec_ok(u32Slot) != 0) {
        return -1;
    }

    ota_flash_read(OTA_SLOT_BASE(u32Slot) + OTA_IMG_TRAILER_OFF, u8Magic, 4UL);
    u32Magic = ((uint32_t)u8Magic[0]) | ((uint32_t)u8Magic[1] << 8U) |
               ((uint32_t)u8Magic[2] << 16U) | ((uint32_t)u8Magic[3] << 24U);
    if (u32Magic == 0xFFFFFFFFUL) {
        return 0;   /* 从未写过 trailer：以向量表为准 */
    }

    return (ota_img_check(u32Slot) == 0) ? 0 : -2;   /* 写了 trailer 就必须校验通过 */
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

    /* 【必须在跳转前恢复中断】
     * 本函数开头调了 __disable_irq(), 它置的是内核的 PRIMASK —— 而 PRIMASK【不会被跳转清掉】,
     * App 会带着「全程中断屏蔽」运行: SysTick 中断永不触发 -> m_u32Tickms 冻结 ->
     * LED_Pro()/BEEP_Pro() 再也不被推进 -> 谁开的灯一直亮、谁启的蜂鸣一直响。
     * 现场表现就是「三灯常亮(白) + 蜂鸣器长鸣」, 而 CPU 其实在正常跑主循环 —— 极难定位。
     * Boot 自己没有使能任何中断源, 此处不会有挂起中断, 直接开中断是安全的。 */
    __enable_irq();

    u32Sp  = *(volatile uint32_t *)(u32Base);
    pfnApp = (void (*)(void))(*(volatile uint32_t *)(u32Base + 4UL));

    __set_MSP(u32Sp);
    pfnApp();

    for (;;) {                          /* 不应到达 */
        ;
    }
}

/* 按 A -> B 顺序取第一个可用槽；找不到返回 -1 */
static int32_t boot_pick_valid(uint32_t *pu32Slot)
{
    uint32_t u32S;

    for (u32S = 0UL; u32S < 2UL; u32S++) {
        if (boot_slot_usable(u32S) == 0) {
            *pu32Slot = u32S;
            return 0;
        }
    }
    return -1;
}

/* 找不到可启动槽：LED 反复闪 BOOT_LED_HALT 次（永不返回），并喂狗 */
static void boot_halt(void)
{
    uint32_t u32Guard = 0UL;

    for (;;) {
        boot_led_blink(BOOT_LED_HALT);
        /* 长停期间喂狗，避免被看门狗打断 LED 编码的可读性 */
        while (u32Guard++ < 200000UL) {
            SWDT_FeedDog();
        }
        u32Guard = 0UL;
    }
}

/**
 * @brief 引导入口（不返回）
 */
void BOOT_OTA_Run(void)
{
    ota_flag_t stcFlag;
    uint32_t   u32Slot;

    boot_led_init();
    boot_led_blink(BOOT_LED_BOOT);      /* 闪 1 次 = Boot 跑起来了 */

    if (ota_flag_read(&stcFlag) != 0) {
        /* 标志无效（首次烧录未写标志区）：按 A -> B 找第一个可用槽 */
        if (boot_pick_valid(&u32Slot) != 0) {
            boot_halt();
        }
        boot_led_signal((u32Slot == OTA_SLOT_A) ? BOOT_LED_JUMP_A : BOOT_LED_JUMP_B, 3u);
        boot_jump(u32Slot);
    }

    u32Slot = (stcFlag.active == OTA_SLOT_B) ? OTA_SLOT_B : OTA_SLOT_A;

    /* 选中槽不可用则换另一槽 */
    if (boot_slot_usable(u32Slot) != 0) {
        if (boot_pick_valid(&u32Slot) != 0) {
            boot_halt();
        }
        stcFlag.active = u32Slot;
        stcFlag.flags &= ~OTA_FLAG_NEED_CONFIRM;
        (void)ota_flag_write(&stcFlag);
        boot_led_signal((u32Slot == OTA_SLOT_A) ? BOOT_LED_JUMP_A : BOOT_LED_JUMP_B, 3u);
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

            if (boot_slot_usable(u32Other) == 0) {
                stcFlag.active = u32Other;
                if (u32Other == OTA_SLOT_A) { stcFlag.state_a = (uint32_t)OTA_SLOT_RUNNABLE; }
                else                        { stcFlag.state_b = (uint32_t)OTA_SLOT_RUNNABLE; }
                (void)ota_flag_write(&stcFlag);
                boot_led_signal((u32Other == OTA_SLOT_A) ? BOOT_LED_JUMP_A : BOOT_LED_JUMP_B, 3u);
                boot_jump(u32Other);
            }
            (void)ota_flag_write(&stcFlag);
            boot_halt();
        }

        (void)ota_flag_write(&stcFlag);
        SWDT_FeedDog();
        boot_led_signal((u32Slot == OTA_SLOT_A) ? BOOT_LED_JUMP_A : BOOT_LED_JUMP_B, 3u);
        boot_jump(u32Slot);
    }

    /* RUNNABLE：直接跳，不再动 Flash */
    boot_led_signal((u32Slot == OTA_SLOT_A) ? BOOT_LED_JUMP_A : BOOT_LED_JUMP_B, 3u);
    boot_jump(u32Slot);
}

/******************************************************************************
 * EOF
 *****************************************************************************/
