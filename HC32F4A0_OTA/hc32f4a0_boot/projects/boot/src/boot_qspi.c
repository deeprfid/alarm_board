/**
 * @file boot_qspi.c
 * @brief Bootloader 精简 QSPI：初始化 + 内存映射(XIP)读（bootloader 只读不写暂存区）
 *        配置与 App qspi_flash.c 一致（QUAD IO Fast Read，4 字节地址）。
 */
#include <string.h>
#include "hc32f4xx.h"
#include "hc32_ll_qspi.h"
#include "hc32_ll_gpio.h"
#include "hc32_ll_fcg.h"
#include "boot_cfg.h"

static void boot_qspi_enter_4byte(void);

/* 与 App qspi_flash.c 相同的引脚/模式配置 */
#define BOOT_QSPI_RD_MD          (QSPI_RD_MD_QUAD_IO_FAST_RD)
#define BOOT_QSPI_DUMMY          (QSPI_DUMMY_CYCLE6)
#define BOOT_QSPI_ADDR_WIDTH     (QSPI_ADDR_WIDTH_32BIT_INSTR_24BIT)
#define BOOT_QSPI_CS_PORT        (GPIO_PORT_C)
#define BOOT_QSPI_CS_PIN         (GPIO_PIN_07)
#define BOOT_QSPI_CS_FUNC        (GPIO_FUNC_18)
#define BOOT_QSPI_SCK_PORT       (GPIO_PORT_C)
#define BOOT_QSPI_SCK_PIN        (GPIO_PIN_06)
#define BOOT_QSPI_SCK_FUNC       (GPIO_FUNC_18)
#define BOOT_QSPI_IO0_PORT       (GPIO_PORT_D)
#define BOOT_QSPI_IO0_PIN        (GPIO_PIN_08)
#define BOOT_QSPI_IO0_FUNC       (GPIO_FUNC_18)
#define BOOT_QSPI_IO1_PORT       (GPIO_PORT_D)
#define BOOT_QSPI_IO1_PIN        (GPIO_PIN_09)
#define BOOT_QSPI_IO1_FUNC       (GPIO_FUNC_18)
#define BOOT_QSPI_IO2_PORT       (GPIO_PORT_D)
#define BOOT_QSPI_IO2_PIN        (GPIO_PIN_10)
#define BOOT_QSPI_IO2_FUNC       (GPIO_FUNC_18)
#define BOOT_QSPI_IO3_PORT       (GPIO_PORT_D)
#define BOOT_QSPI_IO3_PIN        (GPIO_PIN_11)
#define BOOT_QSPI_IO3_FUNC       (GPIO_FUNC_18)

void boot_qspi_init(void)
{
    stc_gpio_init_t stcGpioInit;
    stc_qspi_init_t stcQspiInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDrv = PIN_HIGH_DRV;
    (void)GPIO_Init(BOOT_QSPI_CS_PORT,  BOOT_QSPI_CS_PIN,  &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_SCK_PORT, BOOT_QSPI_SCK_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO0_PORT, BOOT_QSPI_IO0_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO1_PORT, BOOT_QSPI_IO1_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO2_PORT, BOOT_QSPI_IO2_PIN, &stcGpioInit);
    (void)GPIO_Init(BOOT_QSPI_IO3_PORT, BOOT_QSPI_IO3_PIN, &stcGpioInit);
    GPIO_SetFunc(BOOT_QSPI_CS_PORT,  BOOT_QSPI_CS_PIN,  BOOT_QSPI_CS_FUNC);
    GPIO_SetFunc(BOOT_QSPI_SCK_PORT, BOOT_QSPI_SCK_PIN, BOOT_QSPI_SCK_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO0_PORT, BOOT_QSPI_IO0_PIN, BOOT_QSPI_IO0_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO1_PORT, BOOT_QSPI_IO1_PIN, BOOT_QSPI_IO1_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO2_PORT, BOOT_QSPI_IO2_PIN, BOOT_QSPI_IO2_FUNC);
    GPIO_SetFunc(BOOT_QSPI_IO3_PORT, BOOT_QSPI_IO3_PIN, BOOT_QSPI_IO3_FUNC);

    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_QSPI, ENABLE);
    (void)QSPI_StructInit(&stcQspiInit);
    stcQspiInit.u32ClockDiv     = QSPI_CLK_DIV3;   /* v9.81cl: 60MHz->80MHz，与 app/driver 一致 */
    stcQspiInit.u32ReadMode     = BOOT_QSPI_RD_MD;
    stcQspiInit.u32PrefetchMode = QSPI_PREFETCH_MD_EDGE_STOP;
    stcQspiInit.u32DummyCycle   = BOOT_QSPI_DUMMY;
    stcQspiInit.u32AddrWidth    = BOOT_QSPI_ADDR_WIDTH;
    stcQspiInit.u32SetupTime    = QSPI_QSSN_SETUP_ADVANCE_QSCK1P5;
    stcQspiInit.u32ReleaseTime  = QSPI_QSSN_RELEASE_DELAY_QSCK1P5;
    stcQspiInit.u32IntervalTime = QSPI_QSSN_INTERVAL_QSCK2;
    (void)QSPI_Init(&stcQspiInit);
    boot_qspi_enter_4byte();   /* W25Q256 4 字节地址（与 App 一致） */
}

/* ---- 指令模式：写/读指令（4 字节地址模式切换用） ---- */
static void boot_qspi_write_instr(uint8_t instr, uint8_t *addr, uint32_t addrlen,
                                  uint8_t *data, uint32_t datalen)
{
    uint32_t i;
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(instr);
    if ((addr != NULL) && (addrlen != 0UL)) {
        for (i = 0; i < addrlen; i++) QSPI_WriteDirectCommValue(addr[i]);
    }
    if ((data != NULL) && (datalen != 0UL)) {
        for (i = 0; i < datalen; i++) QSPI_WriteDirectCommValue(data[i]);
    }
    QSPI_ExitDirectCommMode();
}

static void boot_qspi_wait_done(uint32_t timeout)
{
    uint8_t status;
    uint32_t cnt = timeout * (HCLK_VALUE / 20000UL);
    QSPI_EnterDirectCommMode();
    QSPI_WriteDirectCommValue(0x05U);   /* RD_STATUS_REG1 */
    while (cnt-- != 0UL) {
        status = QSPI_ReadDirectCommValue();
        if (0U == (status & 0x01U))     /* BUSY 清除 */
            break;
    }
    QSPI_ExitDirectCommMode();
}

static void boot_qspi_enter_4byte(void)
{
    /* WR_ENABLE(0x06) + 4BYTE_ADDR(0xB7)，与 App qspi_flash.c 一致（W25Q256） */
    boot_qspi_write_instr(0x06U, NULL, 0U, NULL, 0U);
    boot_qspi_write_instr(0xB7U, NULL, 0U, NULL, 0U);
    boot_qspi_wait_done(5000U);
}

int boot_qspi_read(uint32_t addr, uint8_t *buf, uint32_t size)
{
    const uint8_t *src;

    if ((buf == NULL) || (size == 0UL))
        return -1;
    if ((addr + size) > (BOOT_QSPI_STAGE_BASE + BOOT_QSPI_STAGE_MAX))
        return -1;
    /* 内存映射读（QSPI_Init 已配置 QUAD IO Fast Read，XIP 内存映射模式） */
    src = (const uint8_t *)(QSPI_ROM_BASE + addr);
    memcpy(buf, src, size);
    return 0;
}
