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

#if !FRAME_AA_EN  /* legacy count-32B COM1 rx (kept, used when AA pump disabled) */
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
#endif /* !FRAME_AA_EN */



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

uint8_t Chaneel_ID[16]={0};

#define FRAME_AA_EN (1u)  /* 1=启用 0xAA 不定长帧扩展(默认关, 用原版接收) */
#if FRAME_AA_EN
/* ===== variable-length frame core (0xAA) - channel links only ===== */
#define FRAME_HDR_AA           (0xAAu)
#define FRAME_HDR_LEG_GPIO     (0x55u)
#define FRAME_HDR_LEG_PDU      (0xFFu)
#define FRAME_MAX_PAYLOAD      (251u)
#define FRAME_RX_GUARD_MS      (50u)
#define FRAME_TEST_PING        (0u)  /* 1=每秒向5口发0xAA回显自检ping(诊断用, 会叠加并发流量) */
#define FRAME_VAR_TOTAL_MAX    (255u)
#define FRAME_EV_NONE          (0)
#define FRAME_EV_LEGACY        (1)
#define FRAME_EV_VAR           (2)
#define FRAME_EV_VAR_BAD       (3)
typedef struct
{
    uint8_t  state;
    uint8_t  len;
    uint16_t idx;
    uint32_t lastByteMs;
    uint8_t  buf[FRAME_VAR_TOTAL_MAX];
} frame_rx_t;
static uint16_t fr_crc16(const uint8_t *p, uint16_t n)
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
static void frame_rx_init(frame_rx_t *rx)
{
    rx->state = 0u; rx->len = 0u; rx->idx = 0u; rx->lastByteMs = 0u;
}
static void frame_rx_guard(frame_rx_t *rx, uint32_t now)
{
    if ((rx->state != 0u) && ((now - rx->lastByteMs) > FRAME_RX_GUARD_MS))
    {
        rx->state = 0u; rx->len = 0u; rx->idx = 0u;
    }
}
static int frame_rx_feed(frame_rx_t *rx, uint8_t b)
{
    uint16_t t;
    uint16_t c;
    if (rx->state == 0u)
    {
        if ((b == FRAME_HDR_LEG_GPIO) || (b == FRAME_HDR_LEG_PDU))
        {
            rx->buf[0] = b; rx->idx = 1u; rx->state = 1u;
        }
        else if (b == FRAME_HDR_AA)   { rx->buf[0] = b; rx->idx = 1u; rx->state = 2u; }
        return FRAME_EV_NONE;
    }
    if (rx->state == 1u)
    {
        rx->buf[rx->idx++] = b;
        if (rx->idx >= 32u) { rx->state = 0u; rx->idx = 0u; return FRAME_EV_LEGACY; }
        return FRAME_EV_NONE;
    }
    if (rx->state == 2u)
    {
        rx->len = b;
        if ((rx->len < 2u) || (rx->len > (FRAME_MAX_PAYLOAD + 2u))) { rx->state = 0u; rx->idx = 0u; return FRAME_EV_NONE; }
        rx->buf[rx->idx++] = b;
        rx->state = 3u;
        return FRAME_EV_NONE;
    }
    rx->buf[rx->idx++] = b;
    t = (uint16_t)((uint16_t)rx->len + 4u);
    if (rx->idx >= t)
    {
        rx->state = 0u; rx->idx = 0u;
        c = fr_crc16(rx->buf, (uint16_t)(t - 2u));
        if (((uint8_t)(c & 0xFFu) == rx->buf[t - 2u]) && ((uint8_t)(c >> 8) == rx->buf[t - 1u]))
        {
            return FRAME_EV_VAR;
        }
        return FRAME_EV_VAR_BAD;
    }
    return FRAME_EV_NONE;
}
/* ===== end frame core part1 ===== */

#define STM_PORT_CNT (5u)
typedef struct
{
    frame_rx_t rx;
    uint32_t lastRcvMs;
    uint8_t  radarVal;      /* 0x10 reply: 1=someone present (any radar bit) */
    uint8_t  gpioIn;        /* 0x10 reply Byte0: bit0..2=radar1..3, bit3=GPIO_IN1, bit4=GPIO_IN2 */
    uint8_t  workMode;      /* 0x10 reply Byte1: install mode switches */
    uint8_t  alarmDone;     /* 0x10 reply Byte2 */
    uint8_t  varCmd;        /* last received 0xAA cmd */
    uint8_t  varPlen;       /* last received var payload len */
} port_rx_t;
static port_rx_t s_ports[STM_PORT_CNT];
//static uint32_t s_ping_next[STM_PORT_CNT];
//static uint8_t  s_ping_len[STM_PORT_CNT];

static const COM_PORT_E s_portCom[STM_PORT_CNT] = { COM6, COM2, COM3, COM4, COM5 };
static const uint8_t s_portCh[STM_PORT_CNT][2] = { {1,0},{2,3},{4,5},{6,7},{8,0} };
static int stm_var_send(COM_PORT_E port, uint8_t cmd, uint8_t addr, const uint8_t *pl, uint8_t plen)
{
    uint8_t out[FRAME_VAR_TOTAL_MAX];
    uint8_t lenv;
    uint16_t total, c;
    if (plen > FRAME_MAX_PAYLOAD) { return 0; }
    lenv = (uint8_t)(plen + 2u);
    total = (uint16_t)lenv + 4u;
    out[0] = FRAME_HDR_AA; out[1] = lenv; out[2] = cmd; out[3] = addr;
    if (plen > 0u) { memcpy(&out[4], pl, plen); }
    c = fr_crc16(out, (uint16_t)(total - 2u));
    out[total - 2u] = (uint8_t)(c & 0xFFu);
    out[total - 1u] = (uint8_t)(c >> 8);
    comSendBuf(port, out, total);
    return (int)total;
}
static void stm_handle_legacy(port_rx_t *pr, uint32_t now)
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
static void stm_handle_var(port_rx_t *pr)
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
#if FRAME_TEST_PING
static void ping_one(uint8_t port_idx)
{
    static const uint8_t plens[4] = { 0u, 8u, 32u, 80u };
    uint8_t pl[80];
    uint8_t plen;
    uint8_t n;
    uint8_t i;
    n = (uint8_t)(s_ping_len[port_idx] & 3u);
    plen = plens[n];
    for (i = 0u; i < plen; i++) { pl[i] = (uint8_t)(0xA0u + i); }
    (void)stm_var_send(s_portCom[port_idx], 0x01u, (uint8_t)(port_idx + 1u), pl, plen);
}
#endif
static void refresh_chaneel(uint32_t now)
{
    uint8_t i, v;
    memset(Chaneel_ID, 0, sizeof(Chaneel_ID));
    for (i = 0u; i < STM_PORT_CNT; i++)
    {
        v = ((now - s_ports[i].lastRcvMs) <= 150u) ? s_ports[i].radarVal : 0u;
        Chaneel_ID[s_portCh[i][0]] = v;
        if (s_portCh[i][1] != 0u) { Chaneel_ID[s_portCh[i][1]] = v; }
    }
}
/* ===== end frame core part2 ===== */
#endif /* FRAME_AA_EN */

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




#if FRAME_AA_EN
/* ===== 0xAA Cmd 0x10 radar status polling (5-port) ===== */
#define RADAR_POLL_MS    (20u)

/* per-port tx/rx counters (kept from previous AA self-test) */
static volatile uint32_t uart3_tx=0, uart3_rx=0;
static volatile uint32_t uart4_tx=0, uart4_rx=0;
static volatile uint32_t uart5_tx=0, uart5_rx=0;
static volatile uint32_t uart6_tx=0, uart6_rx=0;
static uint8_t  s_pump_init = 0u;

static void ipc_report_status(uint32_t now);   /* defined below Radar_thread */

static void radar_cnt_tx(COM_PORT_E port)
{
    switch (port)
    {
        case COM3: uart3_tx++; break;
        case COM4: uart4_tx++; break;
        case COM5: uart5_tx++; break;
        case COM6: uart6_tx++; break;
        default: break;
    }
}
static void radar_cnt_rx(COM_PORT_E port)
{
    switch (port)
    {
        case COM3: uart3_rx++; break;
        case COM4: uart4_rx++; break;
        case COM5: uart5_rx++; break;
        case COM6: uart6_rx++; break;
        default: break;
    }
}

/* broadcast Cmd 0x10 empty query to all 5 ports */
static void radar_query_all(void)
{
    uint8_t i;
    for (i = 0u; i < STM_PORT_CNT; i++)
    {
        (void)stm_var_send(s_portCom[i], 0x10u, (uint8_t)(i + 1u), 0, 0u);
        radar_cnt_tx(s_portCom[i]);
    }
}

/* pump one port's FIFO through the var-frame state machine */
static void radar_pump_port(uint8_t i, uint32_t now)
{
    uint8_t b;
    int ev;
    frame_rx_guard(&s_ports[i].rx, now);
    while (comGetChar(s_portCom[i], &b))
    {
        ev = frame_rx_feed(&s_ports[i].rx, b);
        if (ev == FRAME_EV_LEGACY)
        {
            stm_handle_legacy(&s_ports[i], now);
        }
        else if (ev == FRAME_EV_VAR)
        {
            stm_handle_var(&s_ports[i]);
            if (s_ports[i].varCmd == 0x10u)   /* valid 0x10 reply */
            {
                s_ports[i].lastRcvMs = now;
                radar_cnt_rx(s_portCom[i]);
            }
        }
    }
}

void Radar_thread(void)
{
    static uint32_t last_beat = 0u;
    uint32_t now = HAL_GetTick();
    uint8_t i;

    if (s_pump_init == 0u)
    {
        for (i = 0u; i < STM_PORT_CNT; i++)
        {
            frame_rx_init(&s_ports[i].rx);
        }
        s_pump_init = 1u;
    }

    if ((now - last_beat) < RADAR_POLL_MS) { return; }
    last_beat = now;

    /* 1) pump incoming bytes per port */
    for (i = 0u; i < STM_PORT_CNT; i++)
    {
        radar_pump_port(i, now);
    }

    /* 1b) refresh Chaneel_ID from per-port radar state (internal only) */
    refresh_chaneel(now);

    /* 2) broadcast Cmd 0x10 query */
    radar_query_all();

    /* 2b) report 5-port status to Linux on change (Cmd 0x20, 0xAA var frame) */
    ipc_report_status(now);
}

/* ===== Linux IPC (COM1) report: 0xAA Cmd 0x20, 5x3B payload, send on change ===== */
#define IPC_REPORT_CMD       (0x20u)
#define IPC_REPORT_ADDR      (0x00u)
#define IPC_REPORT_PORT3B    (3u)
#define IPC_REPORT_IDLE_MS   (1000u)   /* keepalive fallback when no change */

static uint8_t  s_ipc_report_last[STM_PORT_CNT][3];   /* last reported per-port 3B */
static uint32_t s_ipc_report_ms = 0u;

static void ipc_report_status(uint32_t now)
{
    uint8_t payload[STM_PORT_CNT * 3u];
    uint8_t changed = 0u;
    uint8_t i;

    for (i = 0u; i < STM_PORT_CNT; i++)
    {
        payload[i * 3u + 0u] = s_ports[i].gpioIn;
        payload[i * 3u + 1u] = s_ports[i].workMode;
        payload[i * 3u + 2u] = s_ports[i].alarmDone;

        if ((payload[i * 3u + 0u] != s_ipc_report_last[i][0]) ||
            (payload[i * 3u + 1u] != s_ipc_report_last[i][1]) ||
            (payload[i * 3u + 2u] != s_ipc_report_last[i][2]))
        {
            changed = 1u;
        }
    }

    if ((changed != 0u) || ((now - s_ipc_report_ms) >= IPC_REPORT_IDLE_MS))
    {
        (void)stm_var_send(COM1, IPC_REPORT_CMD, IPC_REPORT_ADDR, payload, sizeof(payload));
        for (i = 0u; i < STM_PORT_CNT; i++)
        {
            s_ipc_report_last[i][0] = payload[i * 3u + 0u];
            s_ipc_report_last[i][1] = payload[i * 3u + 1u];
            s_ipc_report_last[i][2] = payload[i * 3u + 2u];
        }
        s_ipc_report_ms = now;
    }
}

/* ===== Linux IPC (COM1) frame pump: 0x55/0xFF legacy 32B + 0xAA variable ===== */
static frame_rx_t s_ipc_rx;
static uint8_t   s_ipc_inited = 0u;

/* 32B fixed frame from Linux: PDUHEAD dispatch; GPIOHEAD query disabled for now */
static void ipc_handle_legacy(void)
{
    uint16_t c;
    uint16_t crc_rcv;

    if ((s_ipc_rx.buf[0] != PDUHEAD) && (s_ipc_rx.buf[0] != GPIOHEAD)) { return; }
    c = ipcCrc(s_ipc_rx.buf, APP_FRAME_LEN_MAX - 2u);
    crc_rcv = (uint16_t)(s_ipc_rx.buf[30] | ((uint16_t)s_ipc_rx.buf[31] << 8));
    if (crc_rcv != c) { comClearRxFifo(COM1); return; }

    memcpy(&alarmboard, s_ipc_rx.buf, sizeof(alarm_pdu));

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
static void ipc_handle_var(void)
{
    /* reserved: parse Len/CRC verified in frame_rx_feed; no business yet */
}

void Check_Uart_Pdu(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t b;
    int ev;
    extern LED_T Port_1_LED;

    if (s_ipc_inited == 0u) { frame_rx_init(&s_ipc_rx); s_ipc_inited = 1u; }

    frame_rx_guard(&s_ipc_rx, now);

    while (comGetChar(COM1, &b))
    {
        ev = frame_rx_feed(&s_ipc_rx, b);
        if (ev == FRAME_EV_LEGACY)  { ipc_handle_legacy(); }
        else if (ev == FRAME_EV_VAR) { ipc_handle_var(); }
    }

    if (Port_5_LED.ucEnalbe == 0)
    {
        LED_Start(&Port_5_LED, PORTLED_5, 5, 50, 1);
    }
}

#endif /* FRAME_AA_EN */
#endif /* GET_RADAR_ENABLE */
