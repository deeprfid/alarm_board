#include "bsp.h"



#define frameAaEn (1u)  /* 1=enable 0xAA variable frame (COM2..6 + COM1) */
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
					  LED_Start(&Port_1_LED, PORTLED_1, 10, 10, 1);
					  LED_Start(&Port_2_LED, PORTLED_2, 10, 10, 1);
					  LED_Start(&Port_3_LED, PORTLED_3, 10, 10, 1);
					  LED_Start(&Port_4_LED, PORTLED_4, 10, 10, 1);
					  LED_Start(&Port_5_LED, PORTLED_5, 10, 10, 1);
					 //BEEP_Start(10, 10, 1);
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

#if !frameAaEn  /* legacy count-32B COM1 rx (kept, used when AA pump disabled) */
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
#endif /* !frameAaEn */



#if GET_RADAR_ENABLE
static  uint32_t txcnt=0;
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
    txcnt += 5u;


}

uint8_t Chaneel_ID[9]={0};   /* [1..8]=通道号, [0] 未用 */

#if frameAaEn
/* ===== variable-length frame core (0xAA) - channel links only ===== */
#define frameHdrAa           (0xAAu)
#define frameHdrLegGpio     (0x55u)
#define frameHdrLegPdu      (0xFFu)
#define frameMaxPayload      (251u)
#define frameRxGuardMs      (20u)
#define frameTestPing        (0u)  /* 1=每秒向5口发0xAA回显自检ping(诊断用, 会叠加并发流量) */
#define frameVarTotalMax    (255u)
#define frameEvNone          (0)
#define frameEvLegacy        (1)
#define frameEvVar           (2)
#define frameEvVarBad       (3)
typedef struct
{
    uint8_t  state;
    uint8_t  len;
    uint16_t idx;
    uint32_t lastByteMs;
    uint8_t  buf[frameVarTotalMax];
} frameRx_t;
static uint16_t frCrc16(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i, j;
    for (i = 0u; i < n; i++)
    {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (j = 0u; j < 8u; j++)
        {
            if (crc & 0x8000u) { crc = (uint16_t)((crc << 1) ^ 0x1021u); }
            else { crc = (uint16_t)(crc << 1); }
        }
    }
    return crc;
}
static void frameRxInit(frameRx_t *rx)
{
    rx->state = 0u; rx->len = 0u; rx->idx = 0u; rx->lastByteMs = 0u;
}
static void frameRxGuard(frameRx_t *rx, uint32_t now)
{
    if ((rx->state != 0u) && ((now - rx->lastByteMs) > frameRxGuardMs))
    {
        rx->state = 0u; rx->len = 0u; rx->idx = 0u;
    }
}
static int frameRxFeed(frameRx_t *rx, uint8_t b)
{
    uint16_t t;
    uint16_t c;
    if (rx->state == 0u)
    {
        if ((b == frameHdrLegGpio) || (b == frameHdrLegPdu))
        {
            rx->buf[0] = b; rx->idx = 1u; rx->state = 1u;
        }
        else if (b == frameHdrAa)   { rx->buf[0] = b; rx->idx = 1u; rx->state = 2u; }
        return frameEvNone;
    }
    if (rx->state == 1u)
    {
        rx->buf[rx->idx++] = b;
        if (rx->idx >= 32u) { rx->state = 0u; rx->idx = 0u; return frameEvLegacy; }
        return frameEvNone;
    }
    if (rx->state == 2u)
    {
        rx->len = b;
        if ((rx->len < 2u) || (rx->len > (frameMaxPayload + 2u))) { rx->state = 0u; rx->idx = 0u; return frameEvNone; }
        rx->buf[rx->idx++] = b;
        rx->state = 3u;
        return frameEvNone;
    }
    rx->buf[rx->idx++] = b;
    t = (uint16_t)((uint16_t)rx->len + 4u);
    if (rx->idx >= t)
    {
        rx->state = 0u; rx->idx = 0u;
        c = frCrc16(rx->buf, (uint16_t)(t - 2u));
        if (((uint8_t)(c & 0xFFu) == rx->buf[t - 2u]) && ((uint8_t)(c >> 8) == rx->buf[t - 1u]))
        {
            return frameEvVar;
        }
        return frameEvVarBad;
    }
    return frameEvNone;
}
/* ===== end frame core part1 ===== */

#define stmPortCnt (5u)
typedef struct
{
    frameRx_t rx;
    uint32_t lastRcvMs;
    uint8_t  radarVal;      /* 0x10 reply: 1=someone present (any radar bit) */
    uint8_t  gpioIn;        /* 0x10 reply Byte0: bit0..2=radar1..3, bit3=GPIO_IN1, bit4=GPIO_IN2 */
    uint8_t  workMode;      /* 0x10 reply Byte1: install mode switches */
    uint8_t  alarmDone;     /* 0x10 reply Byte2 */
    uint8_t  varCmd;        /* last received 0xAA cmd */
    uint8_t  varPlen;       /* last received var payload len */
} portRx_t;
static portRx_t sPorts[stmPortCnt];
static const COM_PORT_E sPortCom[stmPortCnt] = { COM6, COM2, COM3, COM4, COM5 };
static const uint8_t sPortCh[stmPortCnt][2] = { {1,0},{2,3},{4,5},{6,7},{8,0} };
static int stmVarSend(COM_PORT_E port, uint8_t cmd, uint8_t addr, const uint8_t *pl, uint8_t plen)
{
    uint8_t out[frameVarTotalMax];
    uint8_t lenv;
    uint16_t total, c;
    if (plen > frameMaxPayload) { return 0; }
    lenv = (uint8_t)(plen + 2u);
    total = (uint16_t)lenv + 4u;
    out[0] = frameHdrAa; out[1] = lenv; out[2] = cmd; out[3] = addr;
    if (plen > 0u) { memcpy(&out[4], pl, plen); }
    c = frCrc16(out, (uint16_t)(total - 2u));
    out[total - 2u] = (uint8_t)(c & 0xFFu);
    out[total - 1u] = (uint8_t)(c >> 8);
    (void)UartTxWait(port, 5u);   /* wait TX idle: DMA busy would silently drop the packet */
    comSendBuf(port, out, total);
    return (int)total;
}
static void stmHandleLegacy(portRx_t *pr, uint32_t now)
{
    uint16_t c;
    uint16_t r0;
    if (pr->rx.buf[0] != GPIOHEAD) { return; }
    c = ipcCrc(pr->rx.buf, APP_FRAME_LEN_MAX - 2u);
    if ((uint16_t)(pr->rx.buf[30] | ((uint16_t)pr->rx.buf[31] << 8)) != c) { return; }
    r0 = (uint16_t)(pr->rx.buf[10] | ((uint16_t)pr->rx.buf[11] << 8));
    pr->radarVal = (r0 == 1u) ? 1u : 0u;
    pr->lastRcvMs = now;
}
static void stmHandleVar(portRx_t *pr)
{
    pr->varCmd  = pr->rx.buf[2];
    pr->varPlen = (uint8_t)(pr->rx.len - 2u);

    /* Cmd 0x10 query reply: payload 3B = [gpioIn][workMode][alarmDone] */
    if ((pr->varCmd == 0x10u) && (pr->varPlen >= 3u))
    {
        pr->gpioIn    = pr->rx.buf[4];
        pr->workMode  = pr->rx.buf[5];
        pr->alarmDone = pr->rx.buf[6];
        pr->radarVal  = ((pr->gpioIn & 0x07u) != 0u) ? 1u : 0u;   /* any radar present=1 */
    }
}
#if frameTestPing
static void pingOne(uint8_t portIdx)
{
    static const uint8_t plens[4] = { 0u, 8u, 32u, 80u };
    uint8_t pl[80];
    uint8_t plen;
    uint8_t n;
    uint8_t i;
    n = (uint8_t)(sPingLen[portIdx] & 3u);
    plen = plens[n];
    for (i = 0u; i < plen; i++) { pl[i] = (uint8_t)(0xA0u + i); }
    (void)stmVarSend(sPortCom[portIdx], 0x01u, (uint8_t)(portIdx + 1u), pl, plen);
}
#endif
static void refresh_chaneel(uint32_t now)
{
    uint8_t i, v;
    memset(Chaneel_ID, 0, sizeof(Chaneel_ID));
    for (i = 0u; i < stmPortCnt; i++)
    {
        v = ((now - sPorts[i].lastRcvMs) <= 150u) ? sPorts[i].radarVal : 0u;
        Chaneel_ID[sPortCh[i][0]] = v;
        if (sPortCh[i][1] != 0u) { Chaneel_ID[sPortCh[i][1]] = v; }
    }
}
/* ===== end frame core part2 ===== */
#endif /* frameAaEn */

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
   static  uint32_t rxcnt=0;
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
					   rxcnt++;
					
				}	
				else if ((Res_Radar_Data.crc == crcdata) && (Res_Radar_Data.FrameHead == GPIOHEAD) && Res_Radar_Data.Radarcfg[0] == false)
				{
				    *alarm_done=0;
					   rxcnt++;
				}
        else
				{
				  comClearRxFifo(_ucPort);
				}					
	}


}




#if frameAaEn
/* ===== 0xAA Cmd 0x10 radar status polling (5-port) ===== */
#define radarPollMs    (20u)

/* per-port tx/rx counters (kept from previous AA self-test) */
static volatile uint32_t uart3Tx=0, uart3Rx=0;
static volatile uint32_t uart4Tx=0, uart4Rx=0;
static volatile uint32_t uart5Tx=0, uart5Rx=0;
static volatile uint32_t uart6Tx=0, uart6Rx=0;
static uint8_t  sPumpInit = 0u;

static void ipcReportStatus(uint32_t now);   /* defined below Radar_thread */

static void radarCntTx(COM_PORT_E port)
{
    switch (port)
    {
        case COM3: uart3Tx++; break;
        case COM4: uart4Tx++; break;
        case COM5: uart5Tx++; break;
        case COM6: uart6Tx++; break;
        default: break;
    }
}
static void radarCntRx(COM_PORT_E port)
{
    switch (port)
    {
        case COM3: uart3Rx++; break;
        case COM4: uart4Rx++; break;
        case COM5: uart5Rx++; break;
        case COM6: uart6Rx++; break;
        default: break;
    }
}

/* broadcast Cmd 0x10 empty query to all 5 ports */
static void radarQueryAll(void)
{
    uint8_t i;
    for (i = 0u; i < stmPortCnt; i++)
    {
        (void)stmVarSend(sPortCom[i], 0x10u, (uint8_t)(i + 1u), 0, 0u);
        radarCntTx(sPortCom[i]);
    }
}

/* pump one port's FIFO through the var-frame state machine */
static void radarPumpPort(uint8_t i, uint32_t now)
{
    uint8_t b;
    int ev;
    frameRxGuard(&sPorts[i].rx, now);
    while (comGetChar(sPortCom[i], &b))
    {
        sPorts[i].rx.lastByteMs = now;   /* refresh liveness so guard cannot clear mid-frame */
        ev = frameRxFeed(&sPorts[i].rx, b);
        if (ev == frameEvLegacy)
        {
            stmHandleLegacy(&sPorts[i], now);
        }
        else if (ev == frameEvVar)
        {
            stmHandleVar(&sPorts[i]);
            if (sPorts[i].varCmd == 0x10u)   /* valid 0x10 reply */
            {
                sPorts[i].lastRcvMs = now;
                radarCntRx(sPortCom[i]);
            }
        }
    }
}

void Radar_thread(void)
{
    static uint32_t lastBeat = 0u;
    uint32_t now = HAL_GetTick();
    uint8_t i;

    if (sPumpInit == 0u)
    {
        for (i = 0u; i < stmPortCnt; i++)
        {
            frameRxInit(&sPorts[i].rx);
        }
        sPumpInit = 1u;
    }

    if ((now - lastBeat) < radarPollMs) { return; }
    lastBeat = now;

    /* 1) pump incoming bytes per port */
    for (i = 0u; i < stmPortCnt; i++)
    {
        radarPumpPort(i, now);
    }

    /* 1b) refresh Chaneel_ID from per-port radar state (internal only) */
    refresh_chaneel(now);

    /* 2) broadcast Cmd 0x10 query */
    radarQueryAll();

    /* 2b) report port/channel status to Linux (0x55 gpio_pdu 32B, on change) */
    ipcReportStatus(now);
}

/* ===== Linux IPC (COM1) 上行: 0x55 GPIOHEAD gpio_pdu 定长 32B (Linux 给定格式) =====
 * 变化即报 + ipcReportIdleMs 无变化心跳; HC32 侧 0xAA Cmd 0x10 查询/3B 应答完全不变
 */
#define ipcReportIdleMs      (1000u)  /* 无变化时的心跳周期(ms) */
#define ipcReportDeviceId    (0x00u)  /* TODO: Linux 侧 DeviceID 语义待确认 */
#define ipcReportAntId       (0x00u)  /* 0 = 整机聚合上报(不分通道) */
#define ipcReportVar20En     (0u)     /* 1 = 启用旧的 0xAA Cmd 0x20 (5口x3B) 聚合帧 */

/* 编译期校验: gpio_pdu 必须正好 32B(无结构体填充), 与 APP_FRAME_LEN_MAX 一致 */
typedef char ipcGpioPduSizeChk[(sizeof(gpio_pdu) == APP_FRAME_LEN_MAX) ? 1 : -1];

static gpio_pdu sIpcGpioPdu;                     /* 上行帧(Linux 给定格式) */
static uint8_t  sIpcReportLast[stmPortCnt][3];   /* last reported per-port 3B */
static uint32_t sIpcReportMs = 0u;

static void ipcReportStatus(uint32_t now)
{
    uint8_t changed = 0u;
    uint8_t i, ch;

    memset(&sIpcGpioPdu, 0, sizeof(sIpcGpioPdu));
    sIpcGpioPdu.FrameHead = GPIOHEAD;
    sIpcGpioPdu.Pdu_len   = (uint8_t)sizeof(sIpcGpioPdu);
    sIpcGpioPdu.DeviceID  = ipcReportDeviceId;
    sIpcGpioPdu.AntID     = ipcReportAntId;

    for (i = 0u; i < stmPortCnt; i++)
    {
        /* 一个口可带 1~2 个通道: 通道号 1..8 -> Rad_Status/Alarm_Done 下标 0..7 */
        ch = (uint8_t)(sPortCh[i][0] - 1u);
        sIpcGpioPdu.Rad_Status[ch] = sPorts[i].radarVal;
        sIpcGpioPdu.Alarm_Done[ch] = sPorts[i].alarmDone;
        if (sPortCh[i][1] != 0u)
        {
            ch = (uint8_t)(sPortCh[i][1] - 1u);
            sIpcGpioPdu.Rad_Status[ch] = sPorts[i].radarVal;
            sIpcGpioPdu.Alarm_Done[ch] = sPorts[i].alarmDone;
        }

        if ((sPorts[i].gpioIn    != sIpcReportLast[i][0]) ||
            (sPorts[i].workMode  != sIpcReportLast[i][1]) ||
            (sPorts[i].alarmDone != sIpcReportLast[i][2]))
        {
            changed = 1u;
        }
    }
    /* GPIO[10]: Linux 侧字段语义待确认, 暂填 0 */

    sIpcGpioPdu.crc = ipcCrc((uint8_t *)&sIpcGpioPdu, sizeof(sIpcGpioPdu) - 2);

    if ((changed != 0u) || ((now - sIpcReportMs) >= ipcReportIdleMs))
    {
        (void)UartTxWait(COM1, 5u);
        comSendBuf(COM1, (uint8_t *)&sIpcGpioPdu, sizeof(sIpcGpioPdu));
        for (i = 0u; i < stmPortCnt; i++)
        {
            sIpcReportLast[i][0] = sPorts[i].gpioIn;
            sIpcReportLast[i][1] = sPorts[i].workMode;
            sIpcReportLast[i][2] = sPorts[i].alarmDone;
        }
        sIpcReportMs = now;
    }
}

#if ipcReportVar20En
/* ===== 旧的 0xAA Cmd 0x20 (5口 x 3B = 15B) 聚合上报, 已被 gpio_pdu 取代, 代码保留 =====
 * 需要改回旧格式时: 置 ipcReportVar20En=1 并把 Radar_thread 里的 ipcReportStatus()
 * 换成 ipcReportStatusVar20()
 */
#define ipcReportCmd       (0x20u)
#define ipcReportAddr      (0x00u)
#define ipcReportPort3b    (3u)

static void ipcReportStatusVar20(uint32_t now)
{
    uint8_t payload[stmPortCnt * 3u];
    uint8_t changed = 0u;
    uint8_t i;

    for (i = 0u; i < stmPortCnt; i++)
    {
        payload[i * 3u + 0u] = sPorts[i].gpioIn;
        payload[i * 3u + 1u] = sPorts[i].workMode;
        payload[i * 3u + 2u] = sPorts[i].alarmDone;

        if ((payload[i * 3u + 0u] != sIpcReportLast[i][0]) ||
            (payload[i * 3u + 1u] != sIpcReportLast[i][1]) ||
            (payload[i * 3u + 2u] != sIpcReportLast[i][2]))
        {
            changed = 1u;
        }
    }

    if ((changed != 0u) || ((now - sIpcReportMs) >= ipcReportIdleMs))
    {
        (void)stmVarSend(COM1, ipcReportCmd, ipcReportAddr, payload, sizeof(payload));
        for (i = 0u; i < stmPortCnt; i++)
        {
            sIpcReportLast[i][0] = payload[i * 3u + 0u];
            sIpcReportLast[i][1] = payload[i * 3u + 1u];
            sIpcReportLast[i][2] = payload[i * 3u + 2u];
        }
        sIpcReportMs = now;
    }
}
#endif

/* ===== Linux IPC (COM1) frame pump: 0x55/0xFF legacy 32B + 0xAA variable ===== */
static frameRx_t sIpcRx;
static uint8_t   sIpcInited = 0u;

/* 32B fixed frame from Linux: PDUHEAD dispatch; GPIOHEAD query disabled for now */
static void ipcHandleLegacy(void)
{
    uint16_t c;
    uint16_t crcRcv;

    if ((sIpcRx.buf[0] != PDUHEAD) && (sIpcRx.buf[0] != GPIOHEAD)) { return; }
    c = ipcCrc(sIpcRx.buf, APP_FRAME_LEN_MAX - 2u);
    crcRcv = (uint16_t)(sIpcRx.buf[30] | ((uint16_t)sIpcRx.buf[31] << 8));
    if (crcRcv != c) { return; }   /* drop bad frame; state machine resyncs on next header */

    memcpy(&alarmboard, sIpcRx.buf, sizeof(alarm_pdu));

    if (alarmboard.FrameHead == PDUHEAD)
    {
        ipc_hpm_message((uint8_t *)&alarmboard, sizeof(alarmboard), alarmboard.AntID);
        return;
    }

#if 0 /* GPIOHEAD query reply disabled for now */
    if (alarmboard.FrameHead == GPIOHEAD)
    {
        uint8_t gpi_val = 0;
        PIO_GpioRead(&gpi_val);
        PIO_GpioSet(0xF, alarmboard.reserved & 0xF);
        alarmboard.reserved = gpi_val;
        alarmboard.crc = ipcCrc((uint8_t *)&alarmboard, sizeof(alarmboard) - 2);
        comSendBuf(COM1, (uint8_t *)&alarmboard, sizeof(alarmboard));
    }
#endif
}

/* 0xAA variable frame from Linux - received, business reserved for future */
static void ipcHandleVar(void)
{
    /* reserved: parse Len/CRC verified in frameRxFeed; no business yet */
}

void Check_Uart_Pdu(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t b;
    int ev;
    extern LED_T Port_1_LED;

    if (sIpcInited == 0u) { frameRxInit(&sIpcRx); sIpcInited = 1u; }

    frameRxGuard(&sIpcRx, now);

    while (comGetChar(COM1, &b))
    {
        sIpcRx.lastByteMs = now;   /* refresh liveness so guard cannot clear mid-frame */
        ev = frameRxFeed(&sIpcRx, b);
        if (ev == frameEvLegacy)  { ipcHandleLegacy(); }
        else if (ev == frameEvVar) { ipcHandleVar(); }
    }

    if (Port_5_LED.ucEnalbe == 0)
    {
        LED_Start(&Port_5_LED, PORTLED_5, 5, 50, 1);
    }
}

#endif /* frameAaEn */
#endif /* GET_RADAR_ENABLE */
