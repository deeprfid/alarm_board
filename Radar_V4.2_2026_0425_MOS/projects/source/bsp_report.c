/*******************************************************************************
 * bsp_report.c
 * 0xAA Cmd 0x10 查询应答 payload 组装（3 字节）。见 bsp_report.h 位图定义。
 * 本文件独立: GPIO_IN/radar 用 GPIO 读, workmode 只调 switch_decoder_pio_read。
 ******************************************************************************/
#include "main.h"        /* LIGHT_ON..EAS_MODE / AI_CAMERA / INput_RELAY / switch_decoder_pio_read */
#include "bsp_report.h"
#include "bsp_radar.h"   /* RADAR_PORT0..2 / RADAR_PIN0..2 */

/* 低有效输入 -> 有信号=1 */
#define LOW_ACTIVE_ON(x)   ((x) == PIN_RESET ? 1u : 0u)
/* 高有效输入 -> 有信号=1 */
#define HIGH_ACTIVE_ON(x)  ((x) == PIN_SET ? 1u : 0u)

uint8_t bsp_report_build(uint8_t *out)
{
    uint8_t gpio_in = 0u;
    uint8_t wm      = 0u;

    if (out == 0) { return 0u; }

    /* ---- Byte0: GPIO_IN 位图 ---- */
    if (HIGH_ACTIVE_ON(GPIO_ReadInputPins(RADAR_PORT0, RADAR_PIN0))) { gpio_in |= BSP_REPORT_BIT_RADAR1; }
    if (HIGH_ACTIVE_ON(GPIO_ReadInputPins(RADAR_PORT1, RADAR_PIN1))) { gpio_in |= BSP_REPORT_BIT_RADAR2; }
    if (HIGH_ACTIVE_ON(GPIO_ReadInputPins(RADAR_PORT2, RADAR_PIN2))) { gpio_in |= BSP_REPORT_BIT_RADAR3; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(AI_CAMERA)))           { gpio_in |= BSP_REPORT_BIT_GPIO_IN1; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(INput_RELAY)))         { gpio_in |= BSP_REPORT_BIT_GPIO_IN2; }
    out[0] = gpio_in;

    /* ---- Byte1: workmode 位图(各开关单独读, 低有效=开关接通) ---- */
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(LIGHT_ON)))   { wm |= BSP_REPORT_BIT_LIGHT_ON; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(SYNC_MODE)))  { wm |= BSP_REPORT_BIT_SYNC_MODE; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(RADAR_MODE))) { wm |= BSP_REPORT_BIT_RADAR_MODE; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(AICAM_MODE))) { wm |= BSP_REPORT_BIT_AICAM_MODE; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(EAS_MODE)))   { wm |= BSP_REPORT_BIT_EAS_MODE; }
    out[1] = wm;

    /* ---- Byte2: alarm_done: 执行本应答时固定上报 1 ---- */
    out[2] = 1u;

    return BSP_REPORT_LEN;
}
