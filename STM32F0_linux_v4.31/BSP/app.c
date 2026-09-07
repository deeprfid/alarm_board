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

#define FRAME_AA_EN (0u)  /* 1=启用 0xAA 不定长帧扩展(默认关, 用原版接收) */
#if FRAME_AA_EN
/* ===== variable-length frame core (0xAA) - channel links only ===== */
#define FRAME_HDR_AA           (0xAAu)
#define FRAME_HDR_LEG_GPIO     (0x55u)
#define FRAME_MAX_PAYLOAD      (251u)
#define FRAME_RX_GUARD_MS      (50u)
#define FRAME_TEST_PING        (0u)  /* 1=每秒向5口发0xAA回显自检ping(诊断用, 会叠加并发流量) */
#define FRAME_VAR_TOTAL_MAX    (257u)
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
        if (b == FRAME_HDR_LEG_GPIO) { rx->buf[0] = b; rx->idx = 1u; rx->state = 1u; }
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
    COM_PORT_E port;
    frame_rx_t rx;
    uint32_t lastRcvMs;
    uint8_t  radarVal;
    uint32_t varCnt;
    uint32_t rxByteCnt;
    uint32_t varCrcFail;
    uint32_t legCrcFail;
    uint8_t  varCmd;
    uint8_t  varAddr;
    uint8_t  varPlen;
} port_rx_t;
static port_rx_t s_ports[STM_PORT_CNT];
static uint32_t s_ping_next[STM_PORT_CNT];
static uint8_t  s_ping_len[STM_PORT_CNT];

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
    if ((uint16_t)(pr->rx.buf[30] | ((uint16_t)pr->rx.buf[31] << 8)) != c) { pr->legCrcFail++; return; }
    r0 = (uint16_t)(pr->rx.buf[10] | ((uint16_t)pr->rx.buf[11] << 8));
    pr->radarVal = (r0 == 1u) ? 1u : 0u;
    pr->lastRcvMs = now;
}
static void stm_handle_var(port_rx_t *pr)
{
    pr->varCmd  = pr->rx.buf[2];
    pr->varAddr = pr->rx.buf[3];
    pr->varPlen = (uint8_t)(pr->rx.len - 2u);
    pr->varCnt++;
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




/* ===== AA variable-length in-poll self-test (10ms cadence, one port per 1s window) ===== */
#define AA_PING_EN       (1u)
#define RADAR_POLL_MS    (30u)  /* one frame per beat (blocking TX on UART3/4/5 needs headroom) */
#define AA_PING_MS       (500u)
#define AA_MAXBUF        (260u)
static const COM_PORT_E aa_com[5] = { COM6, COM2, COM3, COM4, COM5 };
static uint32_t aa_next_ms[5];

static uint8_t  aa_win = 0xFFu;
static uint32_t aa_win_until = 0u;
static uint8_t  aa_state = 0u;
static uint8_t  aa_len = 0u;
static uint16_t aa_idx = 0u;
static uint8_t  aa_buf[AA_MAXBUF];
static uint32_t aa_last_ms = 0u;
static uint8_t  aa_cycle = 0u;

static uint16_t aa_crc16(const uint8_t *p, uint16_t n)
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
static void aa_open_window(uint8_t idx, uint32_t now)
{
    static const uint8_t plens[4] = { 0u, 8u, 32u, 80u };
    uint8_t out[AA_MAXBUF];
    uint8_t plen;
    uint8_t i;
    uint16_t total;
    uint16_t c;
    aa_state = 0u; aa_len = 0u; aa_idx = 0u;
    aa_win = idx;
    aa_win_until = now + 200u;
    aa_last_ms = now;
    plen = plens[aa_cycle & 3u];
    total = (uint16_t)plen + 6u;
    out[0] = 0xAAu;
    out[1] = (uint8_t)(plen + 2u);
    out[2] = 0x01u;
    out[3] = (uint8_t)(idx + 1u);
    for (i = 0u; i < plen; i++) { out[4u + i] = (uint8_t)(0xA0u + i); }
    c = aa_crc16(out, (uint16_t)(total - 2u));
    out[total - 2u] = (uint8_t)(c & 0xFFu);
    out[total - 1u] = (uint8_t)(c >> 8);
    comSendBuf(aa_com[idx], out, total);
    txcnt++;
    aa_cycle++;
    aa_next_ms[idx] = now + AA_PING_MS;
}
static void aa_feed(uint8_t b)
{
    uint16_t t;
    uint16_t c;
    if (aa_state == 0u)
    {
        if (b == 0xAAu) { aa_buf[0] = b; aa_idx = 1u; aa_state = 1u; }
        return;
    }
    if (aa_state == 1u)
    {
        aa_len = b;
        if ((aa_len < 2u) || (aa_len > 251u)) { aa_state = 0u; aa_idx = 0u; return; }
        aa_buf[aa_idx++] = b;
        aa_state = 2u;
        return;
    }
    aa_buf[aa_idx++] = b;
    t = (uint16_t)aa_len + 4u;
    if (aa_idx >= t)
    {
        c = aa_crc16(aa_buf, (uint16_t)(t - 2u));
        if (((uint8_t)(c & 0xFFu) == aa_buf[t - 2u]) && ((uint8_t)(c >> 8) == aa_buf[t - 1u]))
        {
            if (aa_buf[2] == 0x81u) { rxcnt++; }
            aa_state = 0u; aa_idx = 0u;
            aa_win = 0xFFu;
            return;
        }
        aa_state = 0u; aa_idx = 0u;
    }
}
static void send_legacy_one(uint8_t idx)
{
    alarm_pdu q;
    memset(&q, 0, sizeof(q));
    q.FrameHead = GPIOHEAD;
    q.Pdu_len = sizeof(q);
    q.Radarcfg[0] = 0xFF;
    q.crc = ipcCrc((uint8_t *)&q, sizeof(q) - 2u);
    comSendBuf(aa_com[idx], (uint8_t *)&q, sizeof(q));
    txcnt++;
}
static volatile uint32_t uart3_tx=0, uart3_rx=0, uart3_d=0, uart3_miss=0;
static volatile uint32_t uart4_tx=0, uart4_rx=0, uart4_d=0, uart4_miss=0;
static volatile uint32_t uart6_tx=0, uart6_rx=0;
static volatile uint32_t uart5_tx=0, uart5_rx=0, uart5_d=0, uart5_miss=0;
static uint16_t aa_seq[5];
static uint16_t exp_seq[5] = { 1u, 1u, 1u, 1u, 1u };
static uint8_t  aa5_state[5];
static uint8_t  aa5_len[5];
static uint16_t aa5_idx[5];
static uint8_t  aa5_buf[5][AA_MAXBUF];
static uint32_t aa5_last[5];

static void aa5_feed(uint8_t p, uint8_t b)
{
    uint16_t t;
    uint16_t c;
    if (aa5_state[p] == 0u)
    {
        if (b == 0xAAu) { aa5_buf[p][0] = b; aa5_idx[p] = 1u; aa5_state[p] = 1u; }
        return;
    }
    if (aa5_state[p] == 1u)
    {
        aa5_len[p] = b;
        if ((aa5_len[p] < 2u) || (aa5_len[p] > 251u)) { aa5_state[p] = 0u; aa5_idx[p] = 0u; return; }
        aa5_buf[p][aa5_idx[p]++] = b;
        aa5_state[p] = 2u;
        return;
    }
    aa5_buf[p][aa5_idx[p]++] = b;
    t = (uint16_t)aa5_len[p] + 4u;
    if (aa5_idx[p] >= t)
    {
        c = aa_crc16(aa5_buf[p], (uint16_t)(t - 2u));
        if (((uint8_t)(c & 0xFFu) == aa5_buf[p][t - 2u]) && ((uint8_t)(c >> 8) == aa5_buf[p][t - 1u]))
        {
            if (aa5_buf[p][2] == 0x81u)
            {
                uint16_t sq;
                rxcnt++;
                if (p == 0u) { uart6_rx++; }
                else if (p == 2u || p == 3u || p == 4u)
                {
                    sq = (uint16_t)(aa5_buf[p][4]) | ((uint16_t)aa5_buf[p][5] << 8u);
                    if (sq > exp_seq[p]) {
                        if (p == 2u) { uart3_miss += (uint32_t)(sq - exp_seq[p]); }
                        else if (p == 3u) { uart4_miss += (uint32_t)(sq - exp_seq[p]); }
                        else if (p == 4u) { uart5_miss += (uint32_t)(sq - exp_seq[p]); }
                    }
                    exp_seq[p] = (uint16_t)(sq + 1u);
                    if (p == 2u) { uart3_rx++; uart3_d = uart3_tx - uart3_rx; }
                    else if (p == 3u) { uart4_rx++; uart4_d = uart4_tx - uart4_rx; }
                    else if (p == 4u) { uart5_rx++; uart5_d = uart5_tx - uart5_rx; }
                }
            }
            aa5_state[p] = 0u; aa5_idx[p] = 0u;
            return;
        }
        aa5_state[p] = 0u; aa5_idx[p] = 0u;
    }
}

static void aa_broadcast_all(uint32_t now)
{
    static const uint8_t plens[4] = { 0u, 8u, 32u, 150u };
    uint8_t out[AA_MAXBUF];
    uint8_t plen;
    uint8_t p;
    uint8_t i;
    uint16_t total;
    uint16_t c;
    static const uint8_t tx_order[5] = { 2u, 0u, 1u, 3u, 4u };   /* UART3 first */
    uint8_t k;
    plen = plens[aa_cycle & 3u];
    total = (uint16_t)plen + 6u;
    for (k = 0u; k < 5u; k++)
    {
        p = tx_order[k];
        out[0] = 0xAAu;
        out[1] = (uint8_t)(plen + 2u);
        out[2] = 0x01u;
        out[3] = (uint8_t)(p + 1u);
        aa_seq[p]++;
        if (plen >= 2u)
        {
            out[4u] = (uint8_t)(aa_seq[p] & 0xFFu);
            out[5u] = (uint8_t)(aa_seq[p] >> 8u);
            for (i = 2u; i < plen; i++) { out[4u + i] = (uint8_t)(0xA0u + i); }
        }
        else
        {
            for (i = 0u; i < plen; i++) { out[4u + i] = (uint8_t)(0xA0u + i); }
        }
        c = aa_crc16(out, (uint16_t)(total - 2u));
        out[total - 2u] = (uint8_t)(c & 0xFFu);
        out[total - 1u] = (uint8_t)(c >> 8);
        comSendBuf(aa_com[p], out, total);
        if (p == 0u) { uart6_tx++; }
        else if (p == 2u) { uart3_tx++; uart3_d = uart3_tx - uart3_rx; }
        else if (p == 3u) { uart4_tx++; uart4_d = uart4_tx - uart4_rx; }
        else if (p == 4u) { uart5_tx++; uart5_d = uart5_tx - uart5_rx; }
        txcnt++;
        aa5_state[p] = 0u; aa5_idx[p] = 0u;
        aa5_last[p] = now;
    }
    aa_cycle++;
}
/* ===== end AA helpers ===== */
void Radar_thread(void)
{
    static uint32_t last_beat = 0u;
    uint32_t now = HAL_GetTick();
    uint8_t i;
    uint8_t b;

    if ((now - last_beat) < RADAR_POLL_MS) { return; }
    last_beat = now;

    /* collect AA replies of previous broadcast (legacy poll disabled) */
    for (i = 0u; i < 5u; i++)
    {
        while (comGetChar(aa_com[i], &b))
        {
            aa5_last[i] = now;
            aa5_feed(i, b);
        }
        if ((aa5_state[i] != 0u) && ((now - aa5_last[i]) > 30u))
        {
            aa5_state[i] = 0u; aa5_idx[i] = 0u;
        }
    }

    /* broadcast AA to all 5 every beat */
    aa_broadcast_all(now);
}
#endif
