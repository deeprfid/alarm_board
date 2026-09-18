/**
 * @file  boot_jump.c
 * @brief Boot 跳转实现
 */
#include "hc32_ll.h"
#include "hc32_ll_clk.h"   /* CLK_SetSysClockSrc / CLK_PLLCmd / CLK_SYSCLK_SRC_HRC */
#include "hc32_ll_efm.h"   /* EFM_SetWaitCycle / EFM_WAIT_CYCLE0 */
#include "ota_layout.h"
#include "boot_jump.h"

typedef void (*boot_fn_t)(void);

/* 主 SRAM：0x1FFF8000 起 188KB（另有 0x200F0000 的 4KB 独立块，不作栈区用） */
#define BOOT_SRAM_LO    0x1FFF8000UL
#define BOOT_SRAM_HI    0x20027000UL

int32_t boot_app_vector_valid(uint32_t slot)
{
    uint32_t base = OTA_SLOT_BASE(slot);
    uint32_t sp   = *(volatile uint32_t *)(base);
    uint32_t pc   = *(volatile uint32_t *)(base + 4u);

    /* 栈顶必须落在 SRAM 内（擦除态是全 0xFFFFFFFF，这一条即可挡掉空槽） */
    if ((sp < BOOT_SRAM_LO) || (sp > BOOT_SRAM_HI))
    {
        return -1;
    }
    /* 复位向量必须落在本槽内（Thumb 位已置，比对时清掉最低位） */
    if (((pc & ~1UL) < base) || ((pc & ~1UL) >= (base + OTA_SLOT_SIZE)))
    {
        return -2;
    }
    return 0;
}

void boot_prep_handoff(uint32_t slot)
{
    __disable_irq();

    /* 时钟退回默认：先切 HRC，再关 PLL，最后把 Flash 等待周期归 0。
     * 全程走 DDL 接口，不直接写受保护寄存器。 */
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_HRC);
    (void)CLK_PLLCmd(DISABLE);
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE0);

    /* 让 App 的第一条指令起就有正确的向量表（App 自身启动早期也会再设一次） */
    SCB->VTOR = OTA_SLOT_BASE(slot);
    __DSB();
    __ISB();
}

int32_t boot_jump_to(uint32_t slot)
{
    uint32_t base;
    uint32_t sp;
    boot_fn_t fn;

    if (0 != boot_app_vector_valid(slot))
    {
        return -1;
    }
    base = OTA_SLOT_BASE(slot);

    boot_prep_handoff(slot);

    sp = *(volatile uint32_t *)(base);
    fn = (boot_fn_t)(*(volatile uint32_t *)(base + 4u));

    __set_MSP(sp);
    fn();

    return -2;   /* 不应到达 */
}
