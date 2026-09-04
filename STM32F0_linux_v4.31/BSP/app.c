#include "bsp.h"



#define MSG_CRC_INIT		      (0xFFFF)
#define MSG_CCITT_CRC_POLY		(0x1021)

extern alarm_pdu alarmboard;
extern UART_HandleTypeDef IPC_huart1;// IPCOM
extern UART_HandleTypeDef CH2_huart2;// CHANNEL 2
extern UART_HandleTypeDef CH3_huart3;// CHANNEL 3
extern UART_HandleTypeDef CH4_huart4;// CHANNEL 4
extern UART_HandleTypeDef CH5_huart5;// CHANNEL 5
extern UART_HandleTypeDef CH1_huart6;// CHANNEL 1




extern LED_T Port_1_LED;//Board_LED_Green CH1
extern LED_T Port_2_LED;//Board_LED_RED   CH2-3
extern LED_T Port_3_LED;//Board_LED_BLUE  CH4-5
extern LED_T Port_4_LED;//Board_LED_Green CH6-7
extern LED_T Port_5_LED;//Board_LED_WHITE CH8 

static void CRC_calcCrc8(unsigned short *crcReg, unsigned short poly, unsigned short u8Data)
{
    unsigned char i;
    unsigned short xorFlag;
    unsigned short bit;
    unsigned short dcdBitMask = 0x80;

    for(i = 0; i < 8; i++)
    {

        xorFlag = *crcReg & 0x8000;

        *crcReg <<= 1;

        bit = ((u8Data & dcdBitMask) == dcdBitMask);
        *crcReg |= bit;

        if(xorFlag)
        {
            *crcReg = *crcReg ^ poly;
        }

        dcdBitMask >>= 1;
    }
}

unsigned short CalcCRC(unsigned char *msgbuf, int msglen)
{
    unsigned short calcCrc = MSG_CRC_INIT;
    unsigned short  k;

    for (k = 1; k < msglen; ++k)
    {
        CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, msgbuf[k]);
    }

    return calcCrc;
}

unsigned short ipcCrc(unsigned char *msgbuf, int msglen)
{
    unsigned short calcCrc = MSG_CRC_INIT;
    unsigned short  k;

    for (k = 0; k < msglen; ++k)
    {
        CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, msgbuf[k]);
    }

    return calcCrc;
}

void rfid_app(void)
{


//	memset(&alarmboard, 0, sizeof(alarmboard));
    alarmboard.FrameHead = PDUHEAD;
    alarmboard.Pdu_len = sizeof(alarmboard);
    //alarmboard.AntID = 0x01;
    alarmboard.Alarm_Duration[0] = 2;                              // LED_R
    alarmboard.Alarm_Duration[1] = 2;                              // LED_B
    alarmboard.Alarm_Duration[2] = ALARM_R_CODE;                   // LED_G  p->cmd
    alarmboard.Alarm_Duration[3] = 1;
    alarmboard.Alarm_Duration[4] = AUX_EAS_CODE;
    alarmboard.random_forest = 0;

    if(alarmboard.AntID++ > 8)
    {
        alarmboard.AntID = 1;
    }

    uint16_t crcdata = ipcCrc((uint8_t *)&alarmboard, sizeof(alarmboard) - 2);
    alarmboard.crc = crcdata;

}

void ipc_hpm_message(uint8_t *upload, uint8_t dlen, uint8_t antid)
{

    /*       Port1  Port2  Port3 Port4  Port5
    *Type1:    1-----23-----45-----67-----8
     Type2:    12----34
     Type3:                        12-----34
    */
#if GET_RADAR_ENABLE	
		
    if(antid)
		{
		   Send_RadarStatus_to_Master(antid);
		}	
#endif	
    switch(antid)
    {

        case 	0x1:

        {
            comSendBuf(COM6, upload, dlen);     //mainboard CH1
					  LED_Start(&Port_1_LED, PORTLED_1, 10, 15, 3);
           // comSendBuf(COM4, upload, dlen);   //mainboard CH4
					 
            break;
        }


        case 	0x2:
        {
            comSendBuf(COM2, upload, dlen);		  //mainboard CH2
            //comSendBuf(COM4, upload, dlen);	  //mainboard CH4
					  LED_Start(&Port_2_LED, PORTLED_2, 10, 15, 3);

            break;

        }

        case 	0x3:
        {
            comSendBuf(COM2, upload, dlen);		  //mainboard CH2
					  LED_Start(&Port_2_LED, PORTLED_2, 10, 15, 3);
           // comSendBuf(COM5, upload, dlen);		 //mainboard CH5
            break;

        }


        case 	0x4:
        {
            comSendBuf(COM3, upload, dlen);    //mainboard CH3
					  LED_Start(&Port_3_LED, PORTLED_3, 10, 15, 3);
           // comSendBuf(COM5, upload, dlen);	 //mainboard CH5
            break;
        }

        case 	0x5:
        {
            comSendBuf(COM3, upload, dlen); //mainboard CH3
					  LED_Start(&Port_3_LED, PORTLED_3, 10, 15, 3);
            break;
        }

        case 	0x6:
        {
            comSendBuf(COM4, upload, dlen);	 //mainboard CH4
					  LED_Start(&Port_4_LED, PORTLED_4, 10, 15, 3);
            break;
        }

        case 	0x7:
        {

            comSendBuf(COM4, upload, dlen);	 //mainboard CH4
					  LED_Start(&Port_4_LED, PORTLED_4, 10, 15, 3);
            break;
        }

        case 	0x8:

        {

            comSendBuf(COM5, upload, dlen);		//mainboard CH5
					  LED_Start(&Port_5_LED, PORTLED_5, 10, 15, 3);
            break;
        }

        case  0x0:// network offline--GPIO LED TEST

        {
            comSendBuf(COM6, upload, dlen); //mainboard CH1
            comSendBuf(COM2, upload, dlen); //mainboard CH2
            comSendBuf(COM3, upload, dlen);	//mainboard CH3
            comSendBuf(COM4, upload, dlen); //mainboard CH4
            comSendBuf(COM5, upload, dlen);	//mainboard CH5
            break;
        }

        default  :
        {
            break;
        }


    }


}

void Alarm_CMD(void)
{

    rfid_app();
    ipc_hpm_message((uint8_t *)&alarmboard, sizeof(alarmboard), alarmboard.AntID);


}

void Check_Uart_Pdu(void)
{
    uint32_t tickcount = HAL_GetTick();
    extern LED_T Port_1_LED;

    if ((UartGetRxcnt(COM1) >= APP_FRAME_LEN_MAX))
        //if ((UartGetRxcnt(COM1)>=APP_FRAME_LEN_MAX) && (tickcount%20==0))
    {
        uart_recv(COM1, (uint8_t *)&alarmboard, sizeof(alarmboard));
        uint16_t crcdata = ipcCrc((uint8_t *)&alarmboard, sizeof(alarmboard) - 2);

        if((alarmboard.crc == crcdata) && (alarmboard.FrameHead == GPIOHEAD))
        {
            uint8_t gpi_val = 0;
            PIO_GpioRead(&gpi_val);
            PIO_GpioSet(0xF, alarmboard.reserved & 0xF);
            alarmboard.reserved = gpi_val;
            crcdata = ipcCrc((uint8_t *)&alarmboard, sizeof(alarmboard) - 2);
            alarmboard.crc = crcdata;
            comSendBuf(COM1, (uint8_t *)&alarmboard, sizeof(alarmboard));
        }

        if(alarmboard.crc == crcdata  && alarmboard.FrameHead == PDUHEAD)
        {

            ipc_hpm_message((uint8_t *)&alarmboard, sizeof(alarmboard), alarmboard.AntID);
            //BEEP_Start(20,10,2);
           
        }
        else
        {
            comClearRxFifo(COM1);
            return;
        }
    }

    if(Port_5_LED.ucEnalbe == 0)
    {
        LED_Start(&Port_5_LED, PORTLED_5, 5, 50, 1);
    }
}


#if GET_RADAR_ENABLE

void Broadcast_Get_Radar_Status(void)
{
    alarm_pdu Get_Radar_Data;
    memset(&Get_Radar_Data, 0, sizeof(Get_Radar_Data));
    Get_Radar_Data.FrameHead  = GPIOHEAD;
	  Get_Radar_Data.Pdu_len    = sizeof(Get_Radar_Data);
    Get_Radar_Data.Radarcfg[0]= 0xFF;
    Get_Radar_Data.crc= ipcCrc((uint8_t *)&Get_Radar_Data, sizeof(Get_Radar_Data) -2);
	
    comSendBuf(COM6, (uint8_t *)&Get_Radar_Data,sizeof(Get_Radar_Data));  //mainboard CH1
    comSendBuf(COM2, (uint8_t *)&Get_Radar_Data,sizeof(Get_Radar_Data));  //mainboard CH2
    comSendBuf(COM3, (uint8_t *)&Get_Radar_Data,sizeof(Get_Radar_Data));	//mainboard CH3
    comSendBuf(COM4, (uint8_t *)&Get_Radar_Data,sizeof(Get_Radar_Data));  //mainboard CH4
    comSendBuf(COM5, (uint8_t *)&Get_Radar_Data,sizeof(Get_Radar_Data));	//mainboard CH5



}

uint8_t Chaneel_ID[16]={0};

void Send_RadarStatus_to_Master(uint8_t antid)
{
	 radar_pdu  report_radar;
	 memset(&report_radar,0,   sizeof(report_radar));
	 report_radar.FrameHead  = GPIOHEAD;
	 report_radar.Pdu_len    = sizeof(report_radar);
	 report_radar.channel    = antid; 
	 report_radar.alarm_done = Chaneel_ID[antid];
	 report_radar.crc        = ipcCrc((uint8_t *)&report_radar, sizeof(report_radar) - 2);
	 comSendBuf(COM1,(uint8_t *)&report_radar,sizeof(report_radar));
}

void Check_RadarStatus(COM_PORT_E _ucPort,uint8_t *alarm_done)
{

	
  if ((UartGetRxcnt(_ucPort) >= APP_FRAME_LEN_MAX))
	{
		    alarm_pdu Res_Radar_Data;
	      memset(&Res_Radar_Data,0, sizeof(Res_Radar_Data));
	      uart_recv(_ucPort, (uint8_t *)&Res_Radar_Data, sizeof(Res_Radar_Data));
        uint16_t crcdata = ipcCrc((uint8_t *)&Res_Radar_Data, sizeof(Res_Radar_Data) - 2);

        if((Res_Radar_Data.crc == crcdata) && (Res_Radar_Data.FrameHead == GPIOHEAD) && Res_Radar_Data.Radarcfg[0] == true)
        {
					  *alarm_done=1;
					
				}	
				else if ((Res_Radar_Data.crc == crcdata) && (Res_Radar_Data.FrameHead == GPIOHEAD) && Res_Radar_Data.Radarcfg[0] == false)
				{
				    *alarm_done=0;
				}
        else
				{
				  comClearRxFifo(_ucPort);
				}					
	}


}



void Radar_thread(void)
{
	 static uint32_t timeout_get=0,timeout_send=0;
   uint32_t        now= HAL_GetTick();
    
	 if(now-timeout_get > 80  || (now < timeout_get))
	 {
	   timeout_get=now;	 
	 }
	 
	 if(now-timeout_send > 50  || (now < timeout_send))
	 {
		 timeout_send=now;
		 memset(Chaneel_ID,0,sizeof(Chaneel_ID)); 
		 Check_RadarStatus(COM6,&Chaneel_ID[1]);//mainboard CH1
		 Check_RadarStatus(COM2,&Chaneel_ID[2]);//mainboard CH2
		 Chaneel_ID[3]=Chaneel_ID[2];
     Check_RadarStatus(COM3,&Chaneel_ID[4]);//mainboard CH3
		 Chaneel_ID[5]=Chaneel_ID[4];
     Check_RadarStatus(COM4,&Chaneel_ID[6]);//mainboard CH4
		 Chaneel_ID[7]=Chaneel_ID[6];
     Check_RadarStatus(COM5,&Chaneel_ID[8]);//mainboard CH5
	   Broadcast_Get_Radar_Status();
	 
	 
	 }


}	
#endif
