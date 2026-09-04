
/* Includes ------------------------------------------------------------------*/

#include "bsp.h"



alarm_pdu alarmboard;

extern IWDG_HandleTypeDef hiwdg;



int main(void)
{
  System_Init();
  bsp_Init();

  while (1)
  {
    Check_Uart_Pdu();
		
#if GET_RADAR_ENABLE	
	  Radar_thread();
#endif			

		
#if STM32F0_IWDG_ENABLE			
		HAL_IWDG_Refresh(&hiwdg);
#endif		
  }

}

