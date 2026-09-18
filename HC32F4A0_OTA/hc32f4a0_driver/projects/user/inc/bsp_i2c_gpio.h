/*
*********************************************************************************************************
*
*	模块名称 : I2C总线驱动模块
*	文件名称 : bsp_i2c_gpio.h
*	版    本 : V1.0
*	说    明 : 头文件。
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_I2C_GPIO_H
#define _BSP_I2C_GPIO_H
#include "stdint.h"
typedef struct
{
	uint16_t Year;
	uint8_t Mon;
	uint8_t Day;	
	uint8_t Hour;		
	uint8_t Min;				
	uint8_t Sec;					
	uint8_t Week;	
}RTC_T;

typedef struct
{
	uint8_t rev[2];
  uint8_t tm_sec;  // seconds after the minute - [0, 59]
  uint8_t tm_min;  // minutes after the hour   - [0, 59]
  uint8_t tm_hour; // hours since midnight     - [0, 23]
  uint8_t tm_mday; // day of the month         - [1, 31]
  uint8_t tm_week; // week since Sunday        - [0, 6]
  uint8_t tm_mon;  // months since January     - [0, 11]
  uint8_t tm_year; // years since 1900         - [0, 99]
} rtc_time;




void bsp_InitI2C(void);
void Set_Start_BM8563(uint8_t* current_time);
void datajust(uint8_t *inbuf);
void Bcd2asc(uint8_t *bcd,uint8_t * ascii);
uint8_t SetBM8563(uint8_t sla, uint8_t suba, uint8_t *s, uint8_t no);
uint8_t GetBM8563(uint8_t sla, uint8_t suba, uint8_t *s, uint8_t no);
void rtc_get_realtime(rtc_time *time_date);
void Board_Rtc_init(void);
uint8_t isValidDateTime(const char *dateTime);
uint8_t UTCTime_Set(time_t utc);
uint8_t UTCTime_Show(time_t utc, char*localshow);
#endif
