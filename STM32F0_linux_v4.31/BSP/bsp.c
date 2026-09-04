/*
*********************************************************************************************************
*
*	模块名称 : BSP模块(For STM32H7)
*	文件名称 : bsp.c
*	版    本 : V1.0
*	说    明 : 这是硬件底层驱动程序的主文件。每个c文件可以 #include "bsp.h" 来包含所有的外设驱动模块。
*			   bsp = Borad surport packet 板级支持包
*	修改记录 :
*		版本号  日期         作者       说明
*		V1.0    2018-07-29  Eric2013   正式发布
*
*	Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"


IWDG_HandleTypeDef hiwdg;

static void MX_GPIO_Init(void);
static void SystemClock_Config(void);
static void MX_IWDG_Init(void);

/*
*********************************************************************************************************
*	函 数 名: System_Init
*	功能说明: 系统初始化，主要是MPU，Cache和系统时钟配置
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void System_Init(void)
{

    HAL_Init();


    /* Configure the system clock */
    SystemClock_Config();

}


uint32_t rd_idkey_fun(void)
{
    /* 检测CPU ID */
    static uint32_t CPU_Sn0, CPU_Sn1, CPU_Sn2, idcode;
    CPU_Sn0 =  HAL_GetUIDw0();
    CPU_Sn1 =  HAL_GetUIDw1();
    CPU_Sn2 =  HAL_GetUIDw2();
    idcode = (CPU_Sn0 ^ CPU_Sn1 ^ CPU_Sn2) * 0x12011201;
    __NOP();

    if(idcode != 0xE882F340)
    {
        extern LED_T Port_1_LED;
        __NOP();
        BEEP_Start(20, 10, 3);
        LED_Start(&Port_1_LED, PORTLED_1, 1, 0, 0);
        while(idcode);
    }
    else
    {
        BEEP_Start(20, 10, 2);

    }

    return idcode;

}

/*
*********************************************************************************************************
*	函 数 名: bsp_Init
*	功能说明: 初始化所有的硬件设备。该函数配置CPU寄存器和外设的寄存器并初始化一些全局变量。只需要调用一次
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/



void bsp_Init(void)
{

    MX_GPIO_Init();
    EXTI4_15_IRQHandler_Config();
    bsp_InitUart();
    BEEP_InitHard();
    bsp_InitLed();
    PIO_GPIOInit();

#if STM32F0_IWDG_ENABLE
    rd_idkey_fun();
    MX_IWDG_Init();

#endif
}

/*
*********************************************************************************************************
*	函 数 名: SystemClock_Config
*	功能说明: 初始化系统时钟
*            	System Clock source            = PLL (HSE)
*            	SYSCLK(Hz)                     = 400000000 (CPU Clock)
*           	HCLK(Hz)                       = 200000000 (AXI and AHBs Clock)
*            	AHB Prescaler                  = 2
*            	D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
*            	D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
*            	D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
*            	D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
*            	HSE Frequency(Hz)              = 25000000
*           	PLL_M                          = 5
*            	PLL_N                          = 160
*            	PLL_P                          = 2
*            	PLL_Q                          = 4
*            	PLL_R                          = 2
*            	VDD(V)                         = 3.3
*            	Flash Latency(WS)              = 4
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/*
*********************************************************************************************************
*	函 数 名: Error_Handler
*	形    参: file : 源代码文件名称。关键字 __FILE__ 表示源代码文件名。
*			  line ：代码行号。关键字 __LINE__ 表示源代码行号
*	返 回 值: 无
*		Error_Handler(__FILE__, __LINE__);
*********************************************************************************************************
*/

static void MX_IWDG_Init(void)
{

    /* USER CODE BEGIN IWDG_Init 0 */

    /* USER CODE END IWDG_Init 0 */

    /* USER CODE BEGIN IWDG_Init 1 */

    /* USER CODE END IWDG_Init 1 */
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_4;
    hiwdg.Init.Window = 4095;
    hiwdg.Init.Reload = 4095;

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN IWDG_Init 2 */

    /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

		  /*Configure GPIO pin : CMRESET_Pin */
    GPIO_InitStruct.Pin = CMRESET_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CMRESET_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(CMRESET_GPIO_Port, CMRESET_Pin, GPIO_PIN_SET);
		
		
		
    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, GPO_BZ3V3_Pin | LED_B_Pin | LED_R_Pin | LED_G_Pin
                      | GPO_BZ_Pin | GPO1_Pin | GPO2_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */


    /*Configure GPIO pins : GPI_IN1_Pin GPI_IN2_Pin GPI_IN3_Pin */
    GPIO_InitStruct.Pin = GPI_IN1_Pin | GPI_IN2_Pin | GPI_IN3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pin : GPI_IN4_Pin */
    GPIO_InitStruct.Pin = GPI_IN4_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPI_IN4_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : GPO_BZ3V3_Pin LED_B_Pin LED_R_Pin LED_G_Pin
                             GPO_BZ_Pin GPO1_Pin GPO2_Pin GPO3_Pin
                             GPO4_Pin */
    GPIO_InitStruct.Pin = GPO_BZ3V3_Pin | LED_B_Pin | LED_R_Pin | LED_G_Pin
                          | GPO_BZ_Pin | GPO1_Pin | GPO2_Pin | MCULED_5_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : MCULED_Pin */
    GPIO_InitStruct.Pin   = MCULED_1_Pin | MCULED_2_Pin | MCULED_3_Pin | MCULED_4_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MCULED_1_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(MCULED_5_GPIO_Port, MCULED_5_Pin, GPIO_PIN_SET);
    /*Configure GPIO pins : RESERVED_Pin Host_IRQ_Pin */

    /**/
    HAL_I2CEx_EnableFastModePlus(SYSCFG_CFGR1_I2C_FMP_PB6);

    /**/
    HAL_I2CEx_EnableFastModePlus(SYSCFG_CFGR1_I2C_FMP_PB7);

    /**/
    HAL_I2CEx_EnableFastModePlus(SYSCFG_CFGR1_I2C_FMP_PB8);

    /**/
    HAL_I2CEx_EnableFastModePlus(SYSCFG_CFGR1_I2C_FMP_PB9);


}



/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();

    while (1)
    {
    }

    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */

/*
*********************************************************************************************************
*	函 数 名: HAL_Delay
*	功能说明: 重定向毫秒延迟函数。替换HAL中的函数。因为HAL中的缺省函数依赖于Systick中断，如果在USB、SD卡
*             中断中有延迟函数，则会锁死。也可以通过函数HAL_NVIC_SetPriority提升Systick中断
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
/* 当前例子使用stm32h7xx_hal.c默认方式实现，未使用下面重定向的函数 */
void STM32F030_delay(__IO uint32_t nCount)
{
    __IO uint32_t index = 0;

    for(index = (100000 * nCount); index != 0; index--)
    {
    }
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
