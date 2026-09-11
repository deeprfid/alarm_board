/*
*********************************************************************************************************
*
*	模块名称 : 串口中断+FIFO驱动模块
*	文件名称 : bsp_uart_fifo.h
*	说    明 : 头文件
*
*	Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _APP_H_
#define _APP_H_
#include "stdint.h"

#define NONE_EAS_CODE      (0x64)
#define AUX_EAS_CODE       (0x32)
#define PDUHEAD            (0xFF)   /* Linux 链路唯一帧头: 下行命令与上行响应(gpio_pdu)均用它 */
#define NONE_EAS_CODE      (0x64)
#define AUX_EAS_CODE       (0x32)
#define ALARM_G_CODE       (0xA5)
#define ALARM_NONE_CODE    (0x90)
#define ALARM_R_CODE       (0x5A)
#define ALARM_RELAY_CODE   (0x55)
#define APP_FRAME_LEN_MAX  (32U)

#define ACT_TAGDATA     (1)
#define ACT_GPICHANGE   (2)
#define ACT_TAGCOMING   (3)

#define MAX_EPCLEN         (16)
#define MAX_BOARD_CNT      (8)



typedef struct
{
    unsigned char FrameHead;
    unsigned char Pdu_len;
    unsigned char DeviceID;
    unsigned char AntID;
    unsigned char Alarm_Duration[6];
    uint16_t  Radarcfg[5];
    uint32_t  time_stamp;
    uint32_t  random_forest;
    uint16_t  reserved;
    uint16_t  crc;
} alarm_pdu;


typedef struct
{
    uint8_t  FrameHead;
    uint8_t  Pdu_len;
    uint8_t  DeviceID;
    uint8_t  AntID;
    uint8_t  Rad_Status[8];
    uint8_t  Alarm_Done[8];	
	  uint8_t  GPIO[10];
    uint16_t crc;
} gpio_pdu;


unsigned short ipcCrc(unsigned char *msgbuf, int msglen);
void rfid_app(void);
void ipc_hpm_message(uint8_t *upload, uint8_t dlen, uint8_t antid);
void Alarm_CMD(void);
void Radar_thread(void);
void Check_Uart_Pdu(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
