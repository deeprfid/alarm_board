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
#define PDUHEAD            (0xFF)
#define GPIOHEAD           (0x55)
#define NONE_EAS_CODE      (0x64)
#define AUX_EAS_CODE       (0x32)
#define ALARM_G_CODE       (0xA5)
#define ALARM_NONE_CODE    (0x90)
#define ALARM_R_CODE       (0x5A)
#define ALARM_RELAY_CODE   (0x55)
#define APP_FRAME_LEN_MAX  (32U)
#define RADAR_PDU_LEN      (8U)

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
    unsigned char FrameHead;
    unsigned char Pdu_len;
    unsigned char channel;
    unsigned char alarm_done;
	  unsigned char alarm_r;
		unsigned char alarm_g;
    uint16_t  crc;
} radar_pdu;


unsigned short ipcCrc(unsigned char *msgbuf, int msglen);
void rfid_app(void);
void ipc_hpm_message(uint8_t *upload, uint8_t dlen, uint8_t antid);
void Alarm_CMD(void);
void Send_RadarStatus_to_Master(uint8_t antid);
void Radar_thread(void);
void Check_Uart_Pdu(void);
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
