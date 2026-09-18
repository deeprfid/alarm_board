#include "pio.h"
#include "hc32f46_driver.h"
#include "bsp.h"

void RS485_set_send(void)
{
}

void RS485_set_rec(void)
{
}

void rfid_power_on(void)
{
    GPIO_ResetPins(GPIO_PORT_A, GPIO_PIN_08);
}

void rfid_power_off(void)
{
    GPIO_SetPins(GPIO_PORT_A, GPIO_PIN_08);
}

void ex_power_on(void)
{
}

void ex_power_off(void)
{
}

void WG_D1_set(uint8 value)
{
}

void WG_D0_set(uint8 value)
{
}

void beep_on(void)
{
    GPIO_SetPins(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
}

void beep_off(void)
{
    GPIO_ResetPins(BOARD_BEEP_PORT, BOARD_BEEP_PIN);
}

void led_on(void)
{
    GPIO_ResetPins(LED_PORT, LED_2_PIN);
}

void led_off(void)
{
    GPIO_SetPins(LED_PORT, LED_2_PIN);
}

void board_ledtoggle(void)
{
    GPIO_TogglePins(LED_PORT, LED_1_PIN);
}

int get_ipreset_key_value(void)
{
    return GPIO_ReadInputPins(IPRESET_PORT, IPRESET_PIN);
}

int pio_Gpioinit(void)
{
    GPIO_ResetPins(BOARD_GPO_PORT, BOARD_RELAY_PIN);
    GPIO_ResetPins(BOARD_GPO_PORT, BOARD_DC12V_PIN);
    return 0;
}

void gpo_set(uint8 gpoid, uint8 state)
{
    if (gpoid == 1)  // board GPO1
    {
        if (state == 1)
            GPIO_SetPins(BOARD_GPO_PORT, BOARD_RELAY_PIN);
        else
            GPIO_ResetPins(BOARD_GPO_PORT, BOARD_RELAY_PIN);
    }

    if (gpoid == 2)  // board GPO2
    {
        if (state == 1)
            GPIO_SetPins(BOARD_GPO_PORT, BOARD_DC12V_PIN);
        else
            GPIO_ResetPins(BOARD_GPO_PORT, BOARD_DC12V_PIN);
    }

    if (gpoid == 3)  // RLED
    {
        if (state == 1)
        {
            GPIO_SetPins(LED_RGB_PORT, LED_R_PIN);
            GPIO_ResetPins(LED_PORT, LED_3_PIN);
        }
        else
        {
            GPIO_ResetPins(LED_RGB_PORT, LED_R_PIN);
            GPIO_SetPins(LED_PORT, LED_3_PIN);
        }
    }

    if (gpoid == 4)  // GLED
    {
        if (state == 1)
        {
            GPIO_SetPins(LED_RGB_PORT, LED_G_PIN);
            GPIO_ResetPins(LED_PORT, LED_4_PIN);
        }
        else
        {
            GPIO_ResetPins(LED_RGB_PORT, LED_G_PIN);
            GPIO_SetPins(LED_PORT, LED_4_PIN);
        }
    }

    if (gpoid == 6)  // BLED
    {
        if (state == 1)
            GPIO_SetPins(LED_RGB_PORT, LED_B_PIN);
        else
            GPIO_ResetPins(LED_RGB_PORT, LED_B_PIN);
    }
    else if (gpoid == 5)  // Buzzer
    {
        if (state == 0)
            beep_off();
        else
            beep_on();
    }
}

uint8 gpi_get(uint8 gpoid)
{
    uint8 state;
    pio_GpioRead(&state);
    return (state >> (gpoid - 1)) & 0x01;
}

uint8 gpi_get_all()
{
    uint8 state;
    pio_GpioRead(&state);
    return state;
}

void pio_GpioRead(uint8 *vals)
{
    *vals = GPIO_ReadInputPins(BOARD_GPI_PORT, BOARD_GPI1_PIN) |
           (GPIO_ReadInputPins(BOARD_GPI_PORT, BOARD_GPI2_PIN) << 1) |
           (GPIO_ReadInputPins(BOARD_GPI_PORT, BOARD_GPI3_PIN) << 2) |
           (GPIO_ReadInputPins(BOARD_KEY_PORT, BOARD_KEY1_PIN) << 3) |
           (GPIO_ReadInputPins(BOARD_KEY_PORT, BOARD_KEY2_PIN) << 4);
}

void pio_GpioSet(uint8 mask, uint8 vals)
{
    if ((mask & 0x01) == 1)
    {
        if ((vals & 0x01) == 1)
            GPIO_SetPins(BOARD_GPO_PORT, BOARD_RELAY_PIN);
        else
            GPIO_ResetPins(BOARD_GPO_PORT, BOARD_RELAY_PIN);
    }
    
    if (((mask & 0x02) >> 1) == 1)
    {
        if (((vals & 0x02) >> 1) == 1)
            GPIO_SetPins(BOARD_GPO_PORT, BOARD_DC12V_PIN);
        else
            GPIO_ResetPins(BOARD_GPO_PORT, BOARD_DC12V_PIN);
    }
}

void set_BT_FSC_BT8_status_detect(void)
{
}

uint8 get_BT_FSC_BT8_connect_status(void)
{
    return 0;
}

uint8_t switch_pio_read(uint8_t channel)
{
    en_pin_state_t pstatus = PIN_SET;
    
    switch (channel)
    {
        case 1:  // BEEP_MODE
            pstatus = GPIO_ReadInputPins(GPIO_PORT_E, GPIO_PIN_00);
            break;
        case 2:  // SYNC_MODE
            pstatus = GPIO_ReadInputPins(GPIO_PORT_E, GPIO_PIN_01);
            break;
        case 3:  // RADAR_AICAM_MODE
            pstatus = GPIO_ReadInputPins(GPIO_PORT_E, GPIO_PIN_06);
            break;
        case 4:  // EAS_MODE
            pstatus = GPIO_ReadInputPins(GPIO_PORT_C, GPIO_PIN_13);
            break;
        case 5:  // LIGHT_ON
            pstatus = GPIO_ReadInputPins(GPIO_PORT_C, GPIO_PIN_02);
            break;
        case 6:  // Board_radar_1
            pstatus = GPIO_ReadInputPins(BOARD_RADAR1_PORT, BOARD_RADAR1_PIN);
            break;
        case 7:  // Board_radar_2
            pstatus = GPIO_ReadInputPins(BOARD_RADAR2_PORT, BOARD_RADAR2_PIN);
            break;
        case 8:  // Board_key_1
            pstatus = GPIO_ReadInputPins(BOARD_KEY_PORT, BOARD_KEY1_PIN);
            break;
        case 9:  // Board_key_2
            pstatus = GPIO_ReadInputPins(BOARD_KEY_PORT, BOARD_KEY2_PIN);
            break;
        case 10:  // BOARD_gpi1
            pstatus = GPIO_ReadInputPins(BOARD_GPI_PORT, BOARD_GPI1_PIN);
            break;
        case 11:  // BOARD_gpi2
            pstatus = GPIO_ReadInputPins(BOARD_GPI_PORT, BOARD_GPI2_PIN);
            break;
        case 12:  // BOARD_gpi3
            pstatus = GPIO_ReadInputPins(BOARD_GPI_PORT, BOARD_GPI3_PIN);
            break;
        case 13:  // BOARD_ipreset
            pstatus = GPIO_ReadInputPins(IPRESET_PORT, IPRESET_PIN);
            break;
        default:
            break;
    }

    return pstatus;
}
