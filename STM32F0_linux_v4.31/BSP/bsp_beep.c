/*
*********************************************************************************************************
*
*	模块名称 : 蜂鸣器驱动模块
*	文件名称 : bsp_beep.c
*	版    本 : V1.1
*	说    明 : 驱动蜂鸣器.
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2014-10-20 armfly  正式发布
*		V1.1    2015-10-06 armfly  增加静音函数。用于临时屏蔽蜂鸣器发声。
*
*	Copyright (C), 2015-2020,  www.autobma.com
*
*********************************************************************************************************
*/

#include "bsp.h"

 //  #define BEEP_ENABLE()	 HAL_GPIO_WritePin(GPO_BZ_GPIO_Port,GPO_BZ_Pin,GPIO_PIN_SET)			/* 使能蜂鸣器鸣叫 */
//	 #define BEEP_DISABLE()	 HAL_GPIO_WritePin(GPO_BZ_GPIO_Port,GPO_BZ_Pin,GPIO_PIN_RESET)			/* 禁止蜂鸣器鸣叫 */

	 #define BEEP_ENABLE()	 HAL_GPIO_WritePin(GPO_BZ3V3_GPIO_Port,GPO_BZ3V3_Pin,GPIO_PIN_SET)			/* 使能蜂鸣器鸣叫 */
	 #define BEEP_DISABLE()	 HAL_GPIO_WritePin(GPO_BZ3V3_GPIO_Port,GPO_BZ3V3_Pin,GPIO_PIN_RESET)			/* 禁止蜂鸣器鸣叫 */


BEEP_T g_tBeep;		/* 定义蜂鸣器全局结构体变量 */

static volatile uint8_t mutex_beep = 0;

void mutex_beep_lock(void) {

  mutex_beep = 1;
  HAL_SuspendTick(); 
}

void mutex_beep_unlock(void) {
  mutex_beep = 0;
  HAL_ResumeTick();   
}
/*
*********************************************************************************************************
*	函 数 名: BEEP_InitHard
*	功能说明: 初始化蜂鸣器硬件
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_InitHard(void)
{
//    stc_gpio_init_t stcGpioInit;
//    (void)GPIO_StructInit(&stcGpioInit);
//    stcGpioInit.u16PinState = PIN_STAT_RST;
//    stcGpioInit.u16PinDir = PIN_DIR_OUT;
//    stcGpioInit.u16PinDrv=PIN_HIGH_DRV;
//    (void)GPIO_Init(BEEP_PORT , BEEP_PIN , &stcGpioInit);
	
	g_tBeep.ucMute = 0;	/* 关闭静音 */
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Start
*	功能说明: 启动蜂鸣音。
*	形    参: _usBeepTime : 蜂鸣时间，单位10ms; 0 表示不鸣叫
*			  _usStopTime : 停止时间，单位10ms; 0 表示持续鸣叫
*			  _usCycle : 鸣叫次数， 0 表示持续鸣叫
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_Start(uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle)
{
	if (_usBeepTime == 0 || g_tBeep.ucMute == 1 || g_tBeep.ucEnalbe==1)
	{
		return;
	}

	g_tBeep.usBeepTime = _usBeepTime;
	g_tBeep.usStopTime = _usStopTime;
	g_tBeep.usCycle = _usCycle;
	g_tBeep.usCount = 0;
	g_tBeep.usCycleCount = 0;
	g_tBeep.ucState = 0;
	g_tBeep.ucEnalbe = 1;	/* 设置完全局参数后再使能发声标志 */

	BEEP_ENABLE();			/* 开始发声 */
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Stop
*	功能说明: 停止蜂鸣音。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_Stop(void)
{
  mutex_beep_lock(); 
  
  g_tBeep.ucEnalbe=0;
  g_tBeep.ucMute=0;
  g_tBeep.ucState=0;    
  g_tBeep.usBeepTime=0;
  g_tBeep.usCount=0;
  g_tBeep.usCycle=0; 
  g_tBeep.usCycleCount=0;
  g_tBeep.usStopTime=0;    
  BEEP_DISABLE();
	mutex_beep_unlock();  
//	if ((g_tBeep.usStopTime == 0) || (g_tBeep.usCycle == 0))
//	{
//		BEEP_DISABLE();	/* 必须在清控制标志后再停止发声，避免停止后在中断中又开启 */
//	}
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Pause
*	功能说明: 由于TIM冲突等原因，临时屏蔽蜂鸣音。通过 BEEP_Resume 恢复
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_Pause(void)
{
	BEEP_Stop();
	
	g_tBeep.ucMute = 1;		/* 静音 */
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Resume
*	功能说明: 恢复蜂鸣器正常功能
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_Resume(void)
{
	BEEP_Stop();
	
	g_tBeep.ucMute = 0;		/* 静音 */
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_KeyTone
*	功能说明: 发送按键音
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_KeyTone(void)
{
	BEEP_Start(5, 1, 1);	/* 鸣叫50ms，停10ms， 1次 */
}

/*
*********************************************************************************************************
*	函 数 名: BEEP_Pro
*	功能说明: 每隔10ms调用1次该函数，用于控制蜂鸣器发声。该函数在 bsp_timer.c 中被调用。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BEEP_Pro(void)
{
	if ((g_tBeep.ucEnalbe == 0) || (g_tBeep.usStopTime == 0) || (g_tBeep.ucMute == 1))
	{
		return;
	}

	if (g_tBeep.ucState == 0)
	{
		if (g_tBeep.usStopTime > 0)	/* 间断发声 */
		{
			if (++g_tBeep.usCount >= g_tBeep.usBeepTime)
			{
				BEEP_DISABLE();		/* 停止发声 */
				
				g_tBeep.usCount = 0;
				g_tBeep.ucState = 1;
			}
		}
		else
		{
			;	/* 不做任何处理，连续发声 */
		}
	}
	else if (g_tBeep.ucState == 1)
	{
		if (++g_tBeep.usCount >= g_tBeep.usStopTime)
		{
			/* 连续发声时，直到调用stop停止为止 */
			if (g_tBeep.usCycle > 0)
			{
				if (++g_tBeep.usCycleCount >= g_tBeep.usCycle)
				{
					/* 循环次数到，停止发声 */
					g_tBeep.ucEnalbe = 0;
				}

				if (g_tBeep.ucEnalbe == 0)
				{
					g_tBeep.usStopTime = 0;
					return;
				}
			}

			g_tBeep.usCount = 0;
			g_tBeep.ucState = 0;

			BEEP_ENABLE();			/* 开始发声 */
		}
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
