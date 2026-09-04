#include "main.h"



static int Relay_pins_init(void)
{
    GPIO_ResetPins(GPIO_PORT_A, GPIO_PIN_15);
    GPIO_SetPins(GPIO_PORT_B, GPIO_PIN_02);
    GPIO_SetPins(GPIO_PORT_B, GPIO_PIN_10);
    return 0;
}


void Relay_gpio_init(void)
{
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir   = PIN_DIR_IN;
    stcGpioInit.u16PullUp   = PIN_PU_OFF;
    GPIO_SetDebugPort(GPIO_PIN_TDI, DISABLE);
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_00, &stcGpioInit);    //IN1
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_01, &stcGpioInit);    //IN2
    stcGpioInit.u16PinDir   = PIN_DIR_OUT;
    stcGpioInit.u16PinDrv = PIN_MID_DRV;
    (void)GPIO_Init(GPIO_PORT_A, GPIO_PIN_15, &stcGpioInit);    //GPO1
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_02, &stcGpioInit);    //GPO2
    (void)GPIO_Init(GPIO_PORT_B, GPIO_PIN_10, &stcGpioInit);    //GPO3

    GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_02);
    GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_10);
    Relay_pins_init();
}




void gpo_set(uint8_t gpoid, uint8_t state)
{
    if(gpoid == GPO1) //Relay
    {
        if(state == 1)
            GPIO_SetPins(GPIO_PORT_A, GPIO_PIN_15);
        else
            GPIO_ResetPins(GPIO_PORT_A, GPIO_PIN_15);
    }

    if(gpoid == GPO2) // OPA out1
    {
        if (state == 1)
            GPIO_SetPins(GPIO_PORT_B,  GPIO_PIN_02);
        else
            GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_02);
    }

    if(gpoid == GPO3) // OPA out2
    {
        if (state == 1)
            GPIO_SetPins(GPIO_PORT_B, GPIO_PIN_10);
        else
            GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_10);
    }

    if(gpoid == BOARD_GLED)
    {
        if (state == 1)
            GPIO_SetPins(RADAR_BOARD_LED_G_PORT, RADAR_BOARD_LED_G_PIN);
        else
            GPIO_ResetPins(RADAR_BOARD_LED_G_PORT, RADAR_BOARD_LED_G_PIN);
    }
}

uint8_t gpi_get(uint8_t gpoid)
{
    uint8_t state;
    pio_GpioRead(&state);
    return (state >> (gpoid - 1)) & 0x01;
}

uint8_t gpi_get_all()
{
    uint8_t state;
    pio_GpioRead(&state);
    return state;
}


void pio_GpioRead(uint8_t *vals) //读输入IO口状态 ，IN1的值存放在vals的bit0位，IN2的值存放在vals的bit1位,Radar 01 02 03 的值存放在vals的bit2位,IN4的值存放在vals的bit3位,
{
    *vals = ( GPIO_ReadInputPins (GPIO_PORT_B, GPIO_PIN_00)
              | (GPIO_ReadInputPins(GPIO_PORT_B, GPIO_PIN_01) << 1) \
              | (GPIO_ReadInputPins(RADAR_PORT0, RADAR_PIN0) << 2) \
              | (GPIO_ReadInputPins(RADAR_PORT1, RADAR_PIN1) << 3) \
              | (GPIO_ReadInputPins(RADAR_PORT2, RADAR_PIN2) << 4)
            );
}

void pio_GpioSet(uint8_t mask, uint8_t vals) //设置输出IO口状态，mask的bit0位表示输出IO1,bit1表示输出IO2，
{
    //当bit0或bit1值为1时才表示要设置对应的IO口，设置的值为对应的vals的bit0与bit1的值

    if((mask & 0x01) == 1)
    {
        if((vals & 0x01) == 1)	         GPIO_SetPins(GPIO_PORT_A, GPIO_PIN_15);
        else      				           GPIO_ResetPins(GPIO_PORT_A, GPIO_PIN_15);
    }

    if(((mask & 0x02) >> 1) == 1)
    {
        if (((vals & 0x02) >> 1) == 1)	  GPIO_SetPins(GPIO_PORT_B,  GPIO_PIN_02);
        else      				          GPIO_ResetPins(GPIO_PORT_B,  GPIO_PIN_02);
    }

    if(((mask & 0x04) >> 2) == 1)
    {
        if (((vals & 0x04) >> 2) == 1)	  GPIO_SetPins(GPIO_PORT_B, GPIO_PIN_10);
        else      				          GPIO_ResetPins(GPIO_PORT_B, GPIO_PIN_10);
    }

}

void WDT_Config(void)
{
    stc_wdt_init_t stcWdtInit;
    RMU_ClearStatus();
    /* WDT configuration */
    stcWdtInit.u32CountPeriod   = WDT_CNT_PERIOD65536;
    stcWdtInit.u32ClockDiv      = WDT_CLK_DIV8192;
    stcWdtInit.u32RefreshRange  = WDT_RANGE_0TO100PCT;
    stcWdtInit.u32LPMCount      = WDT_LPM_CNT_STOP;
    stcWdtInit.u32ExceptionType = WDT_EXP_TYPE_RST;
    (void)WDT_Init(&stcWdtInit);
    WDT_FeedDog();
    DDL_DelayMS(10U);
}


