/**
 * @file  boot_main.c
 * @brief HC32F460 报警板 Bootloader —— A/B 双槽、无搬运、选择器标志选槽
 *
 * 依据：docs/ota_boot_design.md v0.2 §5（启动流程）与 §14（实现约定）
 *
 * 启动决策：
 *   1. 读双份选择器标志（各自 CRC32，取 seq 更大且有效的一份）；
 *   2. 标志有效：看 active 槽状态
 *        RUNNABLE      -> 直接跳
 *        TRIAL         -> boot_count++；超过 OTA_FLAG_MAX_BOOT 视为该槽起不来，
 *                         标 FAILED 并切另一槽；否则写回计数后跳该槽
 *        EMPTY/FAILED  -> 试另一槽
 *   3. 标志无效：按 A -> B 顺序取第一个「向量表有效且槽镜像 CRC32 通过」的槽；
 *   4. 两槽皆无效：停在 Boot（不跳任何槽 = 不砖）。
 *
 * 为什么「两槽皆无效」在实践中不可达：A/B 无搬运下，下载只写【非活动槽】，
 * 运行槽在整镜像 CRC32 校验通过前一个字节都不会被动 —— 所以运行槽恒为有效，
 * Boot 恒有槽可跳。只有用 SWD 等外部手段烧进坏镜像才可能构造出该状态，
 * 那种情况需要调试器恢复（Boot 本身不带下载通道）。
 *
 * 本 Boot 刻意保持最小：不配时钟（沿用 DDL SystemInit）、不开串口、不开看门狗、
 * 不初始化任何外设 —— 只读标志、校验、跳转。
 */
#include "hc32_ll.h"
#include "ota_layout.h"
#include "ota_flash.h"
#include "boot_jump.h"

/* 两槽皆无效时的停靠点：不跳任何槽。调试器可在此处附加后读 ota_flag_read 结果 */
static void boot_panic(void)
{
    for (;;)
    {
        __NOP();
    }
}

/* 按 A -> B 顺序取第一个可运行的槽；找不到返回 0 */
static int32_t boot_pick_valid_slot(uint32_t *slot_out)
{
    uint32_t s;

    for (s = 0u; s < 2u; s++)
    {
        if ((0 == boot_app_vector_valid(s)) && (0 == ota_img_check(s)))
        {
            *slot_out = s;
            return 0;
        }
    }
    return -1;
}

int32_t main(void)
{
    ota_flag_t flag;
    uint32_t   slot;
    int32_t    have_flag;

    have_flag = ota_flag_read(&flag);

    if (0 == have_flag)
    {
        slot = (flag.active == OTA_SLOT_B) ? OTA_SLOT_B : OTA_SLOT_A;

        /* 选中槽的镜像必须过校验，否则换另一槽 */
        if ((0 != boot_app_vector_valid(slot)) || (0 != ota_img_check(slot)))
        {
            if (0 != boot_pick_valid_slot(&slot))
            {
                boot_panic();
            }
            /* 镜像在、标志说的不对：把 active 纠正过去再跳 */
            flag.active = slot;
            flag.flags &= ~OTA_FLAG_NEED_CONFIRM;
            (void)ota_flag_write(&flag);
            (void)boot_jump_to(slot);
            boot_panic();
        }

        if ((flag.state_a == (uint32_t)OTA_SLOT_TRIAL && slot == OTA_SLOT_A) ||
            (flag.state_b == (uint32_t)OTA_SLOT_TRIAL && slot == OTA_SLOT_B))
        {
            /* 试运行中：本次启动计数 +1 */
            flag.boot_count += 1UL;

            if (flag.boot_count > OTA_FLAG_MAX_BOOT)
            {
                /* 连续起不来：本槽标 FAILED，切另一槽 */
                uint32_t other = OTA_SLOT_OTHER(slot);

                if (slot == OTA_SLOT_A) { flag.state_a = (uint32_t)OTA_SLOT_FAILED; flag.fail_a += 1UL; }
                else                    { flag.state_b = (uint32_t)OTA_SLOT_FAILED; flag.fail_b += 1UL; }

                flag.boot_count = 0UL;
                flag.flags &= ~OTA_FLAG_NEED_CONFIRM;

                if ((0 == boot_app_vector_valid(other)) && (0 == ota_img_check(other)))
                {
                    flag.active = other;
                    if (other == OTA_SLOT_A) { flag.state_a = (uint32_t)OTA_SLOT_RUNNABLE; }
                    else                     { flag.state_b = (uint32_t)OTA_SLOT_RUNNABLE; }
                    (void)ota_flag_write(&flag);
                    (void)boot_jump_to(other);
                }
                (void)ota_flag_write(&flag);
                boot_panic();
            }

            (void)ota_flag_write(&flag);
            (void)boot_jump_to(slot);
            boot_panic();
        }

        /* RUNNABLE：直接跳（标志不改，减少一次 Flash 擦写） */
        (void)boot_jump_to(slot);
        boot_panic();
    }

    /* 标志无效：按 A -> B 找第一个能跑的槽 */
    if (0 != boot_pick_valid_slot(&slot))
    {
        boot_panic();
    }
    (void)boot_jump_to(slot);
    boot_panic();

    return 0;
}
