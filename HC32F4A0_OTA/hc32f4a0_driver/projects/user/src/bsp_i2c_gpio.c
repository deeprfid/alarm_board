/*
*********************************************************************************************************
*
*	模块名称 : I2C总线驱动模块
*	文件名称 : bsp_i2c_gpio.c
*	版    本 : V1.0
*	说    明 : 用gpio模拟i2c总线, 适用于STM32F4系列CPU。该模块不包括应用层命令帧，仅包括I2C总线基本操作函数。
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01 armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	应用说明：
	在访问I2C设备前，请先调用 i2c_CheckDevice() 检测I2C设备是否正常，该函数会配置GPIO
*/

#include "bsp.h"
#include <time.h>
#include "bsp_i2c_gpio.h"
#include "string.h"
#include "stdlib.h"
void seconds_to_date(time_t secs, char *date);
/*
	安富莱STM32-V7开发板 i2c总线GPIO:
 		PB6/I2C1_SCL
 		PB9/I2C1_SDA
*/

/* 定义I2C总线连接的GPIO端口, 用户只需要修改下面4行代码即可任意改变SCL和SDA的引脚 */
#define I2C_SCL_GPIO	GPIO_PORT_D			/* 连接到SCL时钟线的GPIO */
#define I2C_SDA_GPIO	GPIO_PORT_D			/* 连接到SDA数据线的GPIO */

#define I2C_SCL_PIN		GPIO_PIN_00			/* 连接到SCL时钟线的GPIO */
#define I2C_SDA_PIN		GPIO_PIN_01			/* 连接到SDA数据线的GPIO */

#define ALL_I2C_GPIO_CLK_ENABLE()	__HAL_RCC_GPIOH_CLK_ENABLE()

/* 定义读写SCL和SDA的宏 */
#define I2C_SCL_1()  GPIO_SetPins  (GPIO_PORT_D,GPIO_PIN_00)				/* SCL = 1 */
#define I2C_SCL_0()  GPIO_ResetPins(GPIO_PORT_D,GPIO_PIN_00)			/* SCL = 0 */

#define I2C_SDA_1()  GPIO_SetPins  (GPIO_PORT_D,GPIO_PIN_01)				/* SDA = 1 */
#define I2C_SDA_0()  GPIO_ResetPins(GPIO_PORT_D,GPIO_PIN_01)				/* SDA = 0 */

#define I2C_SDA_READ()  (GPIO_ReadOutputPins(GPIO_PORT_D,GPIO_PIN_01))	/* 读SDA口线状态 */
#define I2C_SCL_READ()  (GPIO_ReadOutputPins(GPIO_PORT_D,GPIO_PIN_00))	/* 读SCL口线状态 */




#define BCD2HEX(x) (((x) >> 4) * 10 + ((x)&0x0F))
#define HEX2BCD(x) (((x) % 10) + ((((x) / 10) % 10) << 4))

// uint8_t twdata[9] = {0x00, 0x00, 0x00, 0x50, 0x14, 0x03, 0x01, 0x06, 0x24}; /*前2个数据用来设置状态寄存器，后 7 个用来设置时间寄存器 */
                                                   /*定义数组用来存储读取的时间数据 */
                                                     /*定义数组用来存储转换的 asc 码时间数据，供显示用 */
static volatile uint8_t ack = 0;


void IIC_SDA_SET(void) {
  I2C_SDA_1();
}

void IIC_SDA_RESET(void) {
 I2C_SDA_0();
}

void IIC_SCL_SET(void) {
  I2C_SCL_1() ;
}

void IIC_SCL_RESET(void) {
  I2C_SCL_0() ;
}

uint8_t IN_SDA(void) {
 
  if (I2C_SDA_READ()) {
    return 1;
  } else {
    return 0;
  }
}
/******************************************************************************
 * Function Name --> IIC启动
 * Description   --> SCL高电平期间，SDA由高电平突变到低电平时启动总线
 *                   SCL: __________
 *                                  \__________
 *                   SDA: _____
 *                             \_______________
 * Input         --> none
 * Output        --> none
 * Reaturn       --> none
 ******************************************************************************/
void IIC_Start(void) {
  IIC_SDA_SET(); //为SDA下降启动做准备
  DDL_DelayUS(1);
  IIC_SCL_SET(); //在SCL高电平时，SDA为下降沿时候总线启动
  DDL_DelayUS(3);
  IIC_SDA_RESET(); //突变，总线启动
  DDL_DelayUS(3);
  IIC_SCL_RESET();
  DDL_DelayUS(2);
}
/******************************************************************************
 * Function Name --> IIC停止
 * Description   --> SCL高电平期间，SDA由低电平突变到高电平时停止总线
 *                   SCL: ____________________
 *                                  __________
 *                   SDA: _________/
 * Input         --> none
 * Output        --> none
 * Reaturn       --> none
 ******************************************************************************/
void IIC_Stop(void) {
  IIC_SCL_RESET();
  DDL_DelayUS(1);
  IIC_SCL_SET(); //在SCL高电平时，SDA为上升沿时候总线停止
  DDL_DelayUS(3);
  IIC_SDA_SET(); //突变，总线停止
  DDL_DelayUS(3);
}
/******************************************************************************
 * Function Name --> 主机向从机发送应答信号
 * Description   --> 每从 BM8563 读取一个字节数据后都要发送应答信号
 * Input         --> a：应答信号
 *                      0：应答信号
 *                      1：非应答信号
 * Output        --> none
 * Reaturn       --> none
 ******************************************************************************/
void IIC_Ack(uint8_t a) {
  if (a)
    IIC_SDA_SET(); //放上应答信号电平
  else
    IIC_SDA_RESET();
  DDL_DelayUS(3);
  IIC_SCL_SET(); //为SCL下降做准备
  DDL_DelayUS(3);
  IIC_SCL_RESET(); //突变，将应答信号发送过去
  DDL_DelayUS(2);
}
/******************************************************************************
 * Function Name --> 向IIC总线发送一个字节数据
 * Description   --> 向 BM8563 写一个字节的数据
 * Input         --> dat：要发送的数据
 * Output        --> none
 * Reaturn       --> ack：返回应答信号
 ******************************************************************************/
void IIC_Write_Byte(uint8_t dat) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    if ((dat << i) & 0x80) {
      IIC_SDA_SET(); //判断发送位，先发送高位
    } else {
      IIC_SDA_RESET();
    }
    DDL_DelayUS(1);
    IIC_SCL_SET(); //为SCL下降做准备
    DDL_DelayUS(3);
    IIC_SCL_RESET(); //突变，将数据位发送过去
  }                  //字节发送完成，开始接收应答信号
  DDL_DelayUS(2);
  IIC_SDA_SET(); //释放数据线
  DDL_DelayUS(2);
  IIC_SCL_SET(); //为SCL下降做准备
  DDL_DelayUS(4);
  if (IN_SDA()) //读取应答信号
  {
    ack = 0;
  } else {
    ack = 1;
  }
  IIC_SCL_RESET();
  DDL_DelayUS(2);
}

/******************************************************************************
 * Function Name --> 从IIC总线上读取一个字节数据
 * Description   --> none
 * Input         --> none
 * Output        --> none
 * Reaturn       --> x：读取到的数据
 ******************************************************************************/
uint8_t IIC_Read_Byte(void) {
  uint8_t i;
  uint8_t rect = 0;

  IIC_SDA_SET(); //首先置数据线为高电平

  for (i = 0; i < 8; i++) {
    DDL_DelayUS(1);
    IIC_SCL_RESET();
    DDL_DelayUS(3);
    IIC_SCL_SET();
    DDL_DelayUS(2);
    rect = rect << 1;
    if (IN_SDA()) {
      rect = rect + 1;
    }
    DDL_DelayUS(2);
  }
  IIC_SCL_RESET();
  DDL_DelayUS(2);
  return rect; //返回读取到的数据
}

/********************************************************************
函 数 名： GetBM8563(void)
功 能：从 BM8563 的内部寄存器（时间、状态、报警等寄存器）读取数据
说 明：该程序函数用来读取 BM8563 的内部寄存器，譬如时间，报警，状态等寄存器
采用页写的方式，设置数据的个数为 no，no 参数设置为 1 就是单字节方式
调 用：Start_I2C()，SendByte()，RcvByte()，Ack_I2C()，Stop_I2C()
入口参数：sla（BM8563 从地址）， suba（BM8563 内部寄存器地址）
*s（设置读取数据存储的指针）， no（传输数据的个数）
返 回 值：有，用来鉴定传输成功否
***********************************************************************/
uint8_t GetBM8563(uint8_t sla, uint8_t suba, uint8_t *s, uint8_t no) {
  uint8_t i;
  IIC_Start();
  IIC_Write_Byte(sla);
  if (ack == 0)
    return (0);
  IIC_Write_Byte(suba);
  if (ack == 0)
    return (0);
  IIC_Start();
  IIC_Write_Byte(sla + 1);
  if (ack == 0)
    return (0);
  for (i = 0; i < no - 1; i++) {
    *s = IIC_Read_Byte();
    IIC_Ack(0);
    s++;
  }
  *s = IIC_Read_Byte();
  IIC_Ack(1);
  IIC_Stop(); //除最后一个字节外，其他都要从 MASTER 发应答。
  return (1);
}

/********************************************************************
函 数 名：SetBM8563(void)
功 能：设置 BM8563 的内部寄存器（时间，报警等寄存器）
说 明：该程序函数用来设置 BM8563 的内部寄存器，譬如时间，报警，状态等寄存器
采用页写的方式，设置数据的个数为 no，no 参数设置为 1 就是单字节方式
调 用：Start_I2C()，SendByte()，Stop_I2C()
入口参数：sla（BM8563 从地址）， suba（BM8563 内部寄存器地址）
*s（设置初始化数据的指针）， no（传输数据的个数）
返 回 值：有，用来鉴定传输成功否
***********************************************************************/
uint8_t SetBM8563(uint8_t sla, uint8_t suba, uint8_t *s, uint8_t no) {
  uint8_t i;
  IIC_Start();
  IIC_Write_Byte(sla);
  if (ack == 0)
    return (0);
  IIC_Write_Byte(suba);
  if (ack == 0)
    return (0);
  for (i = 0; i < no; i++) {
    IIC_Write_Byte(*s);
    if (ack == 0)
      return (0);
    s++;
  }
  IIC_Stop();
  return (1);
}

/********************************************************************
函 数 名：void Bcd2asc(void)
功 能：bcd 码转换成 asc 码，供液晶显示用
说 明：
调 用：
入口参数：
返 回 值：无
***********************************************************************/
void Bcd2asc(uint8_t *bcd,uint8_t * ascii) {
  uint8_t i;
//  uint8_t *ptr = (uint8_t *)(&BM8563time);
//  uint8_t asc[14] = {0}; 
  for ( i = 0; i < 7; i++) {
//    asc[j++] = (trdata[i] & 0xf0) >> 4 | 0x30; /*格式为: 秒 分 时 日 星期 月  年 */
//    asc[j++] = (trdata[i] & 0x0f) | 0x30;
    bcd[i] = BCD2HEX(bcd[i]);
    ascii[i] = bcd[i];
  }
}

/********************************************************************
函 数 名：datajust(void)
功 能：将读出的时间数据的无关位屏蔽掉
说 明：BM8563 时钟寄存器中有些是无关位，可以将无效位屏蔽掉
调 用：
入口参数：
返 回 值：无
***********************************************************************/
void datajust(uint8_t *inbuf) {
  inbuf[0] = inbuf[0] & 0x7f;
  inbuf[1] = inbuf[1] & 0x7f;
  inbuf[2] = inbuf[2] & 0x3f;
  inbuf[3] = inbuf[3] & 0x3f;
  inbuf[4] = inbuf[4] & 0x07;
  inbuf[5] = inbuf[5] & 0x1f;
  inbuf[6] = inbuf[6] & 0xff;
}
/********************************************************************
函 数 名：Set_Start_BM8563(void)
功 能：配置启动BM8563
说 明：
调 用：
入口参数：
返 回 值：无
***********************************************************************/

void Set_Start_BM8563(uint8_t* current_time) {
  volatile uint8_t bm_status = 0; //如果BM时钟芯片无应答，则为0,。应答正常则为1.

  do {
    bm_status = SetBM8563(0xa2, 0x00, current_time, 0x09); //设置时间和日期

  } while (bm_status == 0);
}

time_t rtc_time_update(char *systime) {

  struct tm cfg_date_time;
	rtc_time BM8563time;
  rtc_get_realtime(&BM8563time);
  cfg_date_time.tm_year = BM8563time.tm_year + 100;  // Year: 2021 (started from 1900)
  cfg_date_time.tm_mon  = BM8563time.tm_mon ;        // Month: July (Started from 0)
  cfg_date_time.tm_mday = BM8563time.tm_mday;        // Day in Month: 12
  cfg_date_time.tm_hour = BM8563time.tm_hour;        // Hour: 16
  cfg_date_time.tm_min  = BM8563time.tm_min;         // Minute: 45
  cfg_date_time.tm_sec  = BM8563time.tm_sec;         // Second: 41

    time_t utc_time_stamp = mktime(&cfg_date_time);    // Convert date time to tick
		sprintf(systime, "%04d-%02d-%02d %02d:%02d:%02d", cfg_date_time.tm_year+1900, 
		cfg_date_time.tm_mon+1, cfg_date_time.tm_mday, cfg_date_time.tm_hour, cfg_date_time.tm_min, 
		cfg_date_time.tm_sec);
  return utc_time_stamp;
 
}
void rtc_get_realtime(rtc_time *time_date) {
  volatile uint8_t bm_status = 0;              //如果BM时钟芯片无应答，则为0,。应答正常则为1.
  uint8_t trdata[7] = {0}; 
  for(uint8_t readcnt=0;readcnt<5;readcnt++)
  {
       if(GetBM8563(0xa2, 0x02, trdata, 0x07)) //测试读取时间、日期
       {
          break;
       }
    
  }
  datajust(trdata);
  Bcd2asc(trdata,&time_date->tm_sec);
}




 
uint8_t isValidDateTime(const char *dateTime) {
    // 假设年份为4位，月份为2位，日期为2位，小时为2位，分钟为2位，秒为2位
    // 日期时间格式为 YYYY-MM-DD HH:MM:SS
    char year[5], month[3], day[3], hour[3], minute[3], second[3];
	  struct tm cfg_date_time;
    memcpy(year  ,dateTime,4); 
	  memcpy(month ,dateTime+5,2);
	  memcpy(day   ,dateTime+8,2);
	  memcpy(hour  ,dateTime+11,2);
	  memcpy(minute,dateTime+14,2);
	  memcpy(second,dateTime+17,2);
    // 验证年份、月份、日期、小时、分钟和秒是否在合理范围内
    int y = atoi(year);
    int m = atoi(month);
    int d = atoi(day);
    int h = atoi(hour);
    int mins = atoi(minute);
    int s = atoi(second);
 
    if (y < 2024 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31 || h < 0 || h > 24 || mins < 0 || mins > 60 || s < 0 || s > 60) {
        return false; // 数值不在合理范围内
    }
		cfg_date_time.tm_year = y-1900;     // Year: 2021 (started from 1900)
    cfg_date_time.tm_mon  = m-1;        // Month: July (Started from 0)
    cfg_date_time.tm_mday = d;          // Day in Month: 12
    cfg_date_time.tm_hour = h;          // Hour: 16
    cfg_date_time.tm_min  = mins;       // Minute: 45
    cfg_date_time.tm_sec  = s;          // Second: 41
		time_t utc_time_stamp = mktime(&cfg_date_time);
    UTCTime_Set(utc_time_stamp-8*3600);//UTC+8:00 BeiJing Chongqing HOnhKOng
    // 对于闰年等其他复杂情况，可以添加额外的逻辑来验证日期的合法性
    return true; // 是正确的格式
}

uint8_t UTCTime_Set(time_t utc) {
    // 假设年份为4位，月份为2位，日期为2位，小时为2位，分钟为2位，秒为2位
    // 日期时间格式为 YYYY-MM-DD HH:MM:SS
	  rtc_time UTCtime;
	  struct tm *mytime = localtime(&utc);
    int y = mytime->tm_year;
    int m = mytime->tm_mon;
    int d = mytime->tm_mday;
    int h = mytime->tm_hour;
    int mins = mytime->tm_min;
    int s = mytime->tm_sec;
 
    if ((y+1900) < 2000 || (y+1900) > 2100 || m < 0 || m > 12 || d < 1 || d > 31 || h < 0 || h > 24 || mins < 0 || mins > 60 || s < 0 || s > 60) {
        return false; // 数值不在合理范围内
    }
		UTCtime.rev[0]=0;
		UTCtime.rev[1]=0;
    UTCtime.tm_year=HEX2BCD(y-100);
		UTCtime.tm_mon =HEX2BCD(m);
		UTCtime.tm_mday=HEX2BCD(d);
		UTCtime.tm_hour=HEX2BCD(h);
		UTCtime.tm_min =HEX2BCD(mins);
		UTCtime.tm_sec =HEX2BCD(s);
		
		Set_Start_BM8563((uint8_t*)&UTCtime);
    // 对于闰年等其他复杂情况，可以添加额外的逻辑来验证日期的合法性
    return true; // 是正确的格式
}

uint8_t UTCTime_Show(time_t utc, char*localshow) {
    // 假设年份为4位，月份为2位，日期为2位，小时为2位，分钟为2位，秒为2位
    // 日期时间格式为 YYYY-MM-DD HH:MM:SS

	  struct tm *mytime = localtime(&utc);
    int y = mytime->tm_year;
    int m = mytime->tm_mon;
    int d = mytime->tm_mday;
    int h = mytime->tm_hour;
    int mins = mytime->tm_min;
    int s = mytime->tm_sec;
 
	sprintf(localshow,"%04d-%02d-%02d %02d:%02d:%02d",y+1900,m+1,d,h,mins,s);
    if ((y+1900) < 2000 || (y+1900) > 2100 || m < 0 || m > 12 || d < 1 || d > 31 || h < 0 || h > 24 || mins < 0 || mins > 60 || s < 0 || s > 60) {
        return false; // 数值不在合理范围内
    }

    // 对于闰年等其他复杂情况，可以添加额外的逻辑来验证日期的合法性
    return true; // 是正确的格式
}
/*
*********************************************************************************************************
*	函 数 名: bsp_InitI2C
*	功能说明: 配置I2C总线的GPIO，采用模拟IO的方式实现
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitI2C(void)
{
	   stc_gpio_init_t stcGpioInit;

    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState      = PIN_STAT_RST;
    stcGpioInit.u16PinDir        = PIN_DIR_OUT;
	  stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
	  stcGpioInit.u16PullUp        = PIN_PU_OFF;
	 
	 (void)GPIO_Init(I2C_SCL_GPIO, I2C_SCL_PIN, &stcGpioInit);
	 (void)GPIO_Init(I2C_SDA_GPIO, I2C_SDA_PIN, &stcGpioInit);

	/* 给一个停止信号, 复位I2C总线上的所有设备到待机模式 */
	IIC_Stop();
}

void Board_Rtc_init(void)
{
  char systime[32];	
 	bsp_InitI2C();
	rtc_time_update(systime);

}	

