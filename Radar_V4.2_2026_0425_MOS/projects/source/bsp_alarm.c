/**
 *******************************************************************************
 * @file  usart/usart_uart_int/source/ring_buf.c
 * @brief This file provides firmware functions to manage the ring buffer.
 @verbatim
   Change Logs:
   Date             Author          Notes
   2022-03-31       CDT             First version
 @endverbatim
 *******************************************************************************
 * Copyright (C) 2022-2023, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"

extern stc_ring_buf_t g_AlarmRing;

void alarm_thread(void)
{

  if(BUF_UsedSize(&g_AlarmRing))
  {
     uint8_t alarmtype=MSG_Alarm_OFF;
     BUF_Read(&g_AlarmRing,&alarmtype,1);
   switch(alarmtype)
     {
       case MSG_485_TAG_RTU :
       case MSG_Relay_RX :          
            {
                Alarm_On();break;
            }
       case MSG_Alarm_OFF:  
            {
                Alarm_Off();break;  
            }
			 case MSG_NETWORK_OFFLINE:
			      {
						  Network_offline();break; 
						}	
      case MSG_LEDTEST:
			      {
			    	  GPIO_LED_test();break; 
						}							
       default:
            break;      
     }   
  } 

}
void switch_decoder_init(void)
{
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir   = PIN_DIR_IN;
    stcGpioInit.u16PullUp   = PIN_PU_OFF;
    (void)GPIO_Init(GPIO_PORT_A , GPIO_PIN_04 , &stcGpioInit);  //switch#---1
	  (void)GPIO_Init(GPIO_PORT_A , GPIO_PIN_05 , &stcGpioInit);  //switch#---2
	  (void)GPIO_Init(GPIO_PORT_A , GPIO_PIN_06 , &stcGpioInit);  //switch#---3
	  (void)GPIO_Init(GPIO_PORT_A , GPIO_PIN_07 , &stcGpioInit);  //switch#---4
	  (void)GPIO_Init(GPIO_PORT_B , GPIO_PIN_12 , &stcGpioInit);  //switch#---5
	  (void)GPIO_Init(GPIO_PORT_B , GPIO_PIN_00 , &stcGpioInit);  //switch#---11
	  (void)GPIO_Init(GPIO_PORT_B , GPIO_PIN_01 , &stcGpioInit);  //switch#---12


}

uint16_t switch_pio_readall(uint16_t channel)
{
  uint16_t key=0xFFFF;
	
	  key= (GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_04)   | \
	       GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_05)<<1 | \
         GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_06)<<2 | \
         GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_07)<<3 | \
	       GPIO_ReadInputPins(GPIO_PORT_B,GPIO_PIN_12)<<4 | \
	       GPIO_ReadInputPins(GPIO_PORT_B,GPIO_PIN_13)<<5 | \
	       GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_09)<<6 | \
	       GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_10)<<7 );
	
	 return key;
}

en_pin_state_t switch_decoder_pio_read(uint8_t channel)
{
  en_pin_state_t pstatus=PIN_SET;//PIN_RESET;   //PIN_SET
  switch(channel)
	{
	
		 case 1 :{pstatus=GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_04);break;} //Beep model
		 case 2 :{pstatus=GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_05);break;}
	   case 3 :{pstatus=GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_06);break;}
	   case 4 :{pstatus=GPIO_ReadInputPins(GPIO_PORT_A,GPIO_PIN_07);break;}
	   case 5 :{pstatus=GPIO_ReadInputPins(GPIO_PORT_B,GPIO_PIN_12);break;}
		 case 11:{pstatus=GPIO_ReadInputPins(GPIO_PORT_B,GPIO_PIN_00);break;}    //IN1---AICAM
		 case 12:{pstatus=GPIO_ReadInputPins(GPIO_PORT_B,GPIO_PIN_01);break;}    //IN2----Realy
	 default:
            break;  
	}

  return pstatus;

}




/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
