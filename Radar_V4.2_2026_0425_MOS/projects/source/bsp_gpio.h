#ifndef _bsp_gpio_H_
#define _bsp_gpio_H_

#include "stdint.h"

int  pio_Gpioinit(void);
void pio_GpioRead(uint8_t *vals);
void pio_GpioSet(uint8_t mask,uint8_t vals);
void Relay_gpio_init(void);
void gpo_set(uint8_t gpoid, uint8_t state);
uint8_t gpi_get(uint8_t gpoid);
void WDT_Config(void);

#define GPO1          (0x1)
#define GPO2          (0x2)
#define GPO3          (0x3)
#define BOARD_GLED    (0x4)

#define GPID1          (0x1)
#define GPID2          (0x2)
#define GPID3_RADAR0   (0x3)
#define GPID4_RADAR1   (0x4)
#define GPID5_RADAR2   (0x5)

/* 雷达板绿色指示灯 */
#define RADAR_BOARD_LED_G_PORT   (GPIO_PORT_B)
#define RADAR_BOARD_LED_G_PIN    (GPIO_PIN_03)

#endif








