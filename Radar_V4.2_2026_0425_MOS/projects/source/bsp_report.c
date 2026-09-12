/*******************************************************************************
 * bsp_report.c
 * Build 0xAA Cmd 0x10 reply payload (3 bytes). See bsp_report.h for bit layout.
 * This file only assembles the payload: GPIO/radar by direct GPIO read,
 * workmode bit by bit via switch_decoder_pio_read.
 ******************************************************************************/
#include "main.h"        /* pulls in radar.h (RADAR_PORT..PIN), mode macros, decoder decl */
#include "bsp_report.h"

/* 声光报警进行中的判据对象(定义在 bsp_led.c / bsp_beep.c) */
extern LED_T  R_tLED;
extern BEEP_T g_tBeep;

/* low-active input -> active=1 */
#define LOW_ACTIVE_ON(x)   ((x) == PIN_RESET ? 1u : 0u)
/* high-active input -> active=1 */
#define HIGH_ACTIVE_ON(x)  ((x) == PIN_SET ? 1u : 0u)

uint8_t bsp_report_build(uint8_t *out)
{
    uint8_t gpio_in = 0u;
    uint8_t wm      = 0u;

    if (out == 0) { return 0u; }

    /* ---- Byte0: GPIO_IN bitmap ---- */
    if (HIGH_ACTIVE_ON(GPIO_ReadInputPins(RADAR_PORT0, RADAR_PIN0))) { gpio_in |= BSP_REPORT_BIT_RADAR1; }
    if (HIGH_ACTIVE_ON(GPIO_ReadInputPins(RADAR_PORT1, RADAR_PIN1))) { gpio_in |= BSP_REPORT_BIT_RADAR2; }
    if (HIGH_ACTIVE_ON(GPIO_ReadInputPins(RADAR_PORT2, RADAR_PIN2))) { gpio_in |= BSP_REPORT_BIT_RADAR3; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(AI_CAMERA)))           { gpio_in |= BSP_REPORT_BIT_GPIO_IN1;}
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(INput_RELAY)))         { gpio_in |= BSP_REPORT_BIT_GPIO_IN2;}
    out[0] = gpio_in;

    /* ---- Byte1: workmode bitmap (each switch read alone, low = switch on) ---- */
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(LIGHT_ON)))   { wm |= BSP_REPORT_BIT_LIGHT_ON; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(SYNC_MODE)))  { wm |= BSP_REPORT_BIT_SYNC_MODE; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(RADAR_MODE))) { wm |= BSP_REPORT_BIT_RADAR_MODE; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(AICAM_MODE))) { wm |= BSP_REPORT_BIT_AICAM_MODE; }
    if (LOW_ACTIVE_ON(switch_decoder_pio_read(EAS_MODE)))   { wm |= BSP_REPORT_BIT_EAS_MODE; }
    out[1] = wm;

    /* ---- Byte2: alarm_done: 1 = 本板正在声光报警(红灯或蜂鸣器在动作) ---- */
    out[2] = ((R_tLED.ucEnalbe != 0u) || (g_tBeep.ucEnalbe != 0u)) ? 1u : 0u;

    return BSP_REPORT_LEN;
}
