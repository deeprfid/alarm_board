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
#if STM32F0_IWDG_ENABLE
static void MX_IWDG_Init(void);      /* 定义也在这个条件里(见文件下部): 关狗时不留未引用告警 */
#endif

/* ==================== 复位原因(开机抓一次) ====================
 * **为什么必须有**: 开了独立看门狗之后, 板子卡死会被**悄悄重启** —— RAM 清空、现象消失, 现场
 *   **完全看不出故障发生过**(表现为"偶尔应答慢一下", 会被误当成 Linux 侧的问题)。
 *   RCC_CSR 的复位标志是**粘滞的**(要写 RMVF 才清), 所以开机读一次就知道"上一次为什么重启"。
 *
 * **读后必须清标志**, 否则下一次读到的是累积的老标志(分不清是哪一次复位)。
 *
 * 位定义(与 Linux 侧约定, 上报到 gpio_pdu 的 GPIO[1]):
 *   bit0 = 独立看门狗 IWDG      <- **重点看这个**: 不为 0 说明板子被狗咬过
 *   bit1 = 软件复位             bit2 = 上电/掉电复位
 *   bit3 = NRST 引脚复位        bit4 = 低功耗复位
 *   bit5 = 选项字节装载复位     bit6 = V18 域掉电复位
 * 0 = 未知/没抓到(理论上不会出现)。
 *
 * 局限: 只反映**最近一次**复位; 要统计"累计被咬几次"靠 Linux 侧累积日志, 别在 STM32 上写 Flash。 */
uint8_t g_resetCause = 0u;

void Reset_Cause_Capture(void)
{
    g_resetCause = 0u;

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)  != RESET) { g_resetCause |= 0x01u; }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)   != RESET) { g_resetCause |= 0x02u; }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)   != RESET) { g_resetCause |= 0x04u; }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)   != RESET) { g_resetCause |= 0x08u; }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)  != RESET) { g_resetCause |= 0x10u; }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST)   != RESET) { g_resetCause |= 0x20u; }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_V18PWRRST)!= RESET) { g_resetCause |= 0x40u; }

    __HAL_RCC_CLEAR_RESET_FLAGS();      /* 必须清: 否则下次读到的是累积的老标志 */
}

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

    /* 尽早抓复位原因(越早越保险, 免得被后面的代码清掉或改写) */
    Reset_Cause_Capture();

    /* Configure the system clock */
    SystemClock_Config();

}

void CM4_System_Reset(void)
{
  HAL_GPIO_WritePin(CM4RESET_GPIO_Port, CM4RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(500);
	HAL_GPIO_WritePin(CM4RESET_GPIO_Port, CM4RESET_Pin, GPIO_PIN_SET);
	
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

    if(idcode != 0x03852952)
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
    bsp_InitUart();
    BEEP_InitHard();
    bsp_InitLed();
    PIO_GPIOInit();
	  rd_idkey_fun();
#if STM32F0_IWDG_ENABLE
	  CM4_System_Reset();
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

/* 独立看门狗(IWDG)初始化。
 *
 * **只在 STM32F0_IWDG_ENABLE != 0(量产固件)时编译**: 调试期关掉是因为 STM32F0 的 IWDG 走内部 LSI,
 * **一旦启动就停不下来**, 调试器 halt 时它照样在数 -> 单步会被它复位, 所以只能用编译期宏区分。
 *
 * 超时 = (Reload + 1) x Prescaler / LSI(约 40kHz) = (624+1) x 64 / 40000 = **约 1.0 秒**
 *   (LSI 有容差, 30~50kHz 对应 0.8~1.33 秒)。
 * 主循环最长合法耗时实测约 **30ms**: radarQueryAll() 5 口 x UartTxWait(5ms) + ipcReportStatus()
 *   的 UartTxWait(COM1,5ms), 而且只在 TX 忙时才真的等 -> 1 秒留了 25 倍以上余量。
 *   (原来的 Prescaler=4 / Reload=4095 只有 0.41 秒, 对 30ms 只有 13 倍, 偏紧。)
 * Window = 4095 = **关闭窗口功能**(不限最早喂狗时刻), 否则喂早了也会复位。
 *
 * 喂狗点: main() 的 while(1) 末尾 —— **必须在主循环最外层**, 不能放定时器/中断里,
 *   否则主循环卡死而中断还在喂, 狗形同虚设。量产前务必做一次"故意卡死"验证。 */
#if STM32F0_IWDG_ENABLE
static void MX_IWDG_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;   /* 40kHz / 64 = 625Hz */
    hiwdg.Init.Window    = 4095;                /* 关窗口 */
    hiwdg.Init.Reload    = 624;                 /* (624+1)/625 = 1.0s */

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        Error_Handler();
    }
}
#endif

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

		/*Configure GPIO pin : CM4RESET_Pin */
    GPIO_InitStruct.Pin = CM4RESET_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CM4RESET_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(CM4RESET_GPIO_Port, CM4RESET_Pin, GPIO_PIN_SET);
		
		 /*Configure GPIO pin : Host_IRQ_Pin */
		 GPIO_InitStruct.Pin  = Host_IRQ_Pin;
		 GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
     GPIO_InitStruct.Pull = GPIO_PULLUP;
     HAL_GPIO_Init(Host_IRQ_GPIO_Port, &GPIO_InitStruct);
		 HAL_GPIO_WritePin(Host_IRQ_GPIO_Port, Host_IRQ_Pin, GPIO_PIN_RESET);   /* 触发信号=1, 空闲保持 0 */
		
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

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
