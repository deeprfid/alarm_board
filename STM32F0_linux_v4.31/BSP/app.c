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


#define ipcReportOnQuery  (1u)   /* 1 = Linux 0xFF PDUHEAD(AntID!=0) 查询到达时立即应答一帧 32B gpio_pdu */

#if (GET_RADAR_ENABLE && frameAaEn && ipcReportOnQuery)
static void ipcReportForce(void);   /* 立即应答一帧 32B gpio_pdu */
#endif

void ipc_hpm_message(uint8_t *upload, uint8_t dlen, uint8_t antid)
{

    /*       Port1  Port2  Port3 Port4  Port5
    *Type1:    1-----23-----45-----67-----8
     Type2:    12----34
     Type3:                        12-----34
    */
#if (GET_RADAR_ENABLE && frameAaEn && ipcReportOnQuery)
    /* Linux 下发 0xFF PDUHEAD(AntID!=0) 时立即应答一帧 32B gpio_pdu */
    if(antid)
		{
		   ipcReportForce();
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





#if GET_RADAR_ENABLE

#if frameAaEn
/* ===== frame core: 0xAA variable-length (both links) + 0xFF fixed 32B (COM1 downlink) ===== */
#define frameHdrAa          (0xAAu)
#define frameHdrLegPdu      (0xFFu)
#define frameMaxPayload     (251u)
#define frameRxGuardMs      (20u)
#define frameVarTotalMax    (255u)
#define frameEvNone         (0)
#define frameEvLegacy       (1)
#define frameEvVar          (2)
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
        if (b == frameHdrLegPdu)                      /* Linux 下行定长 32B: 仅 PDUHEAD */
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
/* ===== end frame core part2 ===== */
#endif /* frameAaEn */





#if frameAaEn
/* ===== 0xAA Cmd 0x10 radar status polling (5-port) ===== */
#define radarPollMs    (20u)

static uint8_t  sPumpInit = 0u;

static void ipcReportStatus(uint32_t now);   /* defined below Radar_thread */


/* broadcast Cmd 0x10 empty query to all 5 ports */
static void radarQueryAll(void)
{
    uint8_t i;
    for (i = 0u; i < stmPortCnt; i++)
    {
        (void)stmVarSend(sPortCom[i], 0x10u, (uint8_t)(i + 1u), 0, 0u);
    }
}

/* pump one port's FIFO through the var-frame state machine */
/* ===== 雷达触发输出: 轮询到某口有人(radarVal=1) -> Host_IRQ 输出 1 + 对应口 LED, 保持 radarTrigHoldMs ===== */
#define radarTrigHoldMs     (1000u)   /* 触发信号与点灯的保持时间(ms) */
#define radarTrigLedOn      (10u)     /* LED_Start 参数: 亮 10*10ms */
#define radarTrigLedOff     (10u)     /* LED_Start 参数: 灭 10*10ms */

static uint8_t  sTrigOn[stmPortCnt];      /* 1 = 该口处于触发保持窗口内 */
static uint32_t sTrigMs[stmPortCnt];      /* 该口最近一次收到"有人"的时刻 */
static LED_T *const sTrigLed[stmPortCnt] = { &Port_1_LED, &Port_2_LED, &Port_3_LED, &Port_4_LED, &Port_5_LED };
static const uint8_t sTrigLedNo[stmPortCnt] = { PORTLED_1, PORTLED_2, PORTLED_3, PORTLED_4, PORTLED_5 };

static void radarTriggerOut(uint32_t now)
{
    uint8_t i;
    uint8_t active = 0u;

    for (i = 0u; i < stmPortCnt; i++)
    {
        if (sPorts[i].radarVal != 0u)
        {
            sTrigOn[i] = 1u;                    /* 有人: 打开/刷新保持窗口 */
            sTrigMs[i] = now;
        }
        else if ((sTrigOn[i] != 0u) && ((now - sTrigMs[i]) >= radarTrigHoldMs))
        {
            sTrigOn[i] = 0u;                    /* 保持时间内再没收到有人: 窗口结束 */
        }

        if (sTrigOn[i] != 0u)
        {
            /* 窗口内每拍刷新一次, LED 保持点亮; 窗口结束后由 LED_Pro 收尾熄灭 */
            LED_Start(sTrigLed[i], sTrigLedNo[i], radarTrigLedOn, radarTrigLedOff, 1u);
            active = 1u;
        }
    }

    /* 任一口在窗口内 -> 触发信号输出 1; 全部结束 -> 输出 0 */
    HAL_GPIO_WritePin(Host_IRQ_GPIO_Port, Host_IRQ_Pin,
                      (active != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void radarPumpPort(uint8_t i, uint32_t now)
{
    uint8_t b;
    int ev;
    frameRxGuard(&sPorts[i].rx, now);
    while (comGetChar(sPortCom[i], &b))
    {
        sPorts[i].rx.lastByteMs = now;   /* refresh liveness so guard cannot clear mid-frame */
        ev = frameRxFeed(&sPorts[i].rx, b);
        if (ev == frameEvVar)
        {
            stmHandleVar(&sPorts[i]);
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

    /* 2) broadcast Cmd 0x10 query */
    radarQueryAll();

    /* 3) 有人触发输出: Host_IRQ=1 + 对应口 LED 点亮, 保持 radarTrigHoldMs */
    radarTriggerOut(now);

    /* 2b) report port/channel status to Linux (PDUHEAD gpio_pdu 32B, on change) */
    ipcReportStatus(now);
}

/* ===== Linux IPC (COM1) 唯一上行帧: PDUHEAD(0xFF) gpio_pdu 定长 32B =====
 * 帧头与 Linux 下行一致, 均为 PDUHEAD(0xFF)
 * 触发: 变化即报 + ipcReportIdleMs 无变化心跳; Linux 下发 PDUHEAD 查询时立即应答一帧
 * 0xAA 变长上行后续要用, 以 ipcReportVar20En=0 保留代码
 */
#define ipcReportIdleMs      (1000u)  /* 无变化时的心跳周期(ms) */
#define ipcReportDeviceId    (0x00u)  /* TODO: Linux 侧 DeviceID 语义待确认 */
#define ipcReportAntId       (0x00u)  /* 0 = 整机聚合上报(不分通道) */
#define ipcReportVar20En     (0u)     /* 1 = 启用旧的 0xAA Cmd 0x20 (5口x3B) 变长聚合帧 */

/* 编译期校验: gpio_pdu 必须正好 32B(无结构体填充), 与 APP_FRAME_LEN_MAX 一致 */
typedef char ipcGpioPduSizeChk[(sizeof(gpio_pdu) == APP_FRAME_LEN_MAX) ? 1 : -1];

static gpio_pdu sIpcGpioPdu;                     /* 上行帧(Linux 给定格式) */
static uint8_t  sIpcReportLast[stmPortCnt][3];   /* last reported per-port 3B */
static uint32_t sIpcReportMs = 0u;

/* 组帧: 刷新 sIpcGpioPdu, 返回 1 = 相对上次已发内容有变化 */
static uint8_t ipcReportBuild(void)
{
    uint8_t changed = 0u;
    uint8_t i, ch;

    memset(&sIpcGpioPdu, 0, sizeof(sIpcGpioPdu));
    sIpcGpioPdu.FrameHead = PDUHEAD;   /* 上行响应帧头 = 下行帧头 = PDUHEAD(0xFF) */
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
    return changed;
}

/* 发送当前 sIpcGpioPdu 并记录"已发快照" */
static void ipcReportSend(uint32_t now)
{
    uint8_t i;

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

/* 周期调用: 变化即报 + ipcReportIdleMs 无变化心跳 */
static void ipcReportStatus(uint32_t now)
{
    uint8_t changed = ipcReportBuild();

    if ((changed != 0u) || ((now - sIpcReportMs) >= ipcReportIdleMs))
    {
        ipcReportSend(now);
    }
}

/* Linux 下发 PDUHEAD 查询时立即应答一帧 32B gpio_pdu(不走变化/心跳判定) */
static void ipcReportForce(void)
{
    (void)ipcReportBuild();
    ipcReportSend(HAL_GetTick());
}

#if ipcReportVar20En
/* ===== 旧的 0xAA Cmd 0x20 (5口 x 3B = 15B) 聚合上报, 已被 gpio_pdu 取代, 代码保留 =====
 * 需要改回旧格式时: 置 ipcReportVar20En=1 并把 Radar_thread 里的 ipcReportStatus()
 * 换成 ipcReportStatusVar20()
 */
#define ipcReportCmd       (0x20u)
#define ipcReportAddr      (0x00u)
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

/* ===== Linux IPC (COM1) frame pump: PDUHEAD fixed 32B downlink + 0xAA variable (reserved) ===== */
static frameRx_t sIpcRx;
static uint8_t   sIpcInited = 0u;

/* Linux 下行定长 32B 帧: 只有 PDUHEAD(0xFF) 一种, CRC 通过后按 AntID 分发 */
static void ipcHandleLegacy(void)
{
    uint16_t c;
    uint16_t crcRcv;

    if (sIpcRx.buf[0] != PDUHEAD) { return; }
    c = ipcCrc(sIpcRx.buf, APP_FRAME_LEN_MAX - 2u);
    crcRcv = (uint16_t)(sIpcRx.buf[30] | ((uint16_t)sIpcRx.buf[31] << 8));
    if (crcRcv != c) { return; }   /* drop bad frame; state machine resyncs on next header */

    memcpy(&alarmboard, sIpcRx.buf, sizeof(alarm_pdu));

    ipc_hpm_message((uint8_t *)&alarmboard, sizeof(alarmboard), alarmboard.AntID);
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

//    if (Port_5_LED.ucEnalbe == 0)
//    {
//        LED_Start(&Port_5_LED, PORTLED_5, 5, 50, 1);
//    }
}

#endif /* frameAaEn */
#endif /* GET_RADAR_ENABLE */
