

#include "ipc.h"
#include "ota_dist.h"

extern rfidcfg mycfgdata;
extern LTNode *g_Uploadtag;
extern LTNode *ADDlist;
extern LTNode *DELlist;
extern uint32_t FlashDB_Sync_flag;
unsigned short ipcCrc(unsigned char *msgbuf, int msglen);

uint32_t Match_EPC_inTSDB(fdb_tsdb_t tsdb, uint8_t *input_tag, uint16_t tlen);
void tagtable_list_update(LTNode *phead, LTDataType epcID, uint8_t method);
extern struct fdb_tsdb whitelistDB;



// 标签白名单过滤
uint8_t tagmatching(LTNode* match_list, uint8_t *input_tag, uint16_t tlen)
{
    LTDataType epcid;

    if (tlen > 32)
        return 0;

    memcpy(epcid.epc, input_tag, tlen);
    epcid.Epclen = tlen;
    epcid.crc = ipcCrc(epcid.epc, tlen);

    LTNode *searchNode = LTFind(match_list, epcid);


    return searchNode ? 1 : 0;
}



void tag_package(MsgQueObj_HPM6340 *tagobj)
{
    char realtime[32];
    uint8_t maskcode = 0, matchcode = 0;
    uint16_t crcdata = 0;
    alarm_pdu alarmboard;
    LTDataType ltepcid;
    time_t utc_time_stamp = rtc_time_update(realtime);

    // 陈列白名单过滤
    //	matchcode=tagmatching(showtaglist,tagobj->epcid,tagobj->epclen);
    if (matchcode)
    {
        memcpy(ltepcid.epc, tagobj->epcid, tagobj->epclen);
        ltepcid.Epclen = tagobj->epclen;
        ltepcid.TimeStamp = utc_time_stamp;
        ltepcid.AntennaID = 0xFF & (tagobj->ant);
        ltepcid.crc = 0;
        tagtable_list_update(g_Uploadtag, ltepcid, 2);
        return;
    }

   if(mycfgdata.easflag == 0)
    {
			 
			uint8_t	mask_add=tagmatching(ADDlist,tagobj->epcid,tagobj->epclen); 
			uint8_t	mask_del=tagmatching(DELlist,tagobj->epcid,tagobj->epclen);
      uint8_t	mask_tsdb = Match_EPC_inTSDB(&whitelistDB, tagobj->epcid, tagobj->epclen);
			if(mask_add || mask_tsdb)
			{
			   maskcode=1;
			}	
			if(mask_del)
			{
			  maskcode=0;
			}	
    }
    else
    {
        maskcode = EASbitmatch(tagobj->epcid, tagobj->epclen,mycfgdata.easflag,MATCHTAIL);
    }

   
    memset(&alarmboard, 0, sizeof(alarmboard));
    alarmboard.FrameHead = PDUHEAD;
    alarmboard.Pdu_len = sizeof(alarmboard);
    alarmboard.AntID = tagobj->ant;
    alarmboard.Alarm_Duration[0] = mycfgdata.alarm_volume;                   // LED_R
    alarmboard.Alarm_Duration[1] = mycfgdata.radar_range;                    // LED_B
    alarmboard.Alarm_Duration[2] = (maskcode ? ALARM_G_CODE : ALARM_R_CODE); // LED_G  p->cmd
    alarmboard.Alarm_Duration[3] = mycfgdata.alarm_duration;
   	alarmboard.Alarm_Duration[5] = 0;
    alarmboard.random_forest = 0;
    memcpy(ltepcid.epc, tagobj->epcid, tagobj->epclen);
    ltepcid.Epclen = tagobj->epclen;
    ltepcid.TimeStamp = utc_time_stamp;
    ltepcid.AntennaID = 0xFF & (tagobj->ant);
    ltepcid.crc = maskcode ? 0 : 1;

    tagtable_update(g_Uploadtag, ltepcid);

    crcdata = ipcCrc((uint8_t *)&alarmboard, sizeof(alarmboard) - 2);
    alarmboard.crc = crcdata;
    mycfgdata.peoplecount++;
    ipc_hpm_message((uint8_t *)&alarmboard, sizeof(alarmboard), tagobj->ant);
    Alarm_Output (BOARD_LED3,2,1,1);
    Alarm_On(maskcode ? ALARM_G_CODE : ALARM_R_CODE,mycfgdata.radar_range,mycfgdata.alarm_volume);
}

void ipc_lkt_message(uint8_t *SBuffer, uint8_t slen, uint8_t *RBuffer)
{
}

void ipc_hpm_message(uint8_t *upload, uint8_t dlen, uint8_t antid)
{
    /* OTA 分发期间：全系统停发业务帧（链路独占，见 docs/ota_boot_design.md v0.2 §0） */
    if (ota_dist_busy() != 0) { return; }

    switch (antid)
    {
        case 0x1:
        {
            Uart_RS485_send(COMMON_INTERFACE_RS485_3, upload, dlen);
            break;
        }
				case 0x2:	       
        case 0x3:
        {
            Uart_RS485_send(COMMON_INTERFACE_RS485_2, upload, dlen);
            break;
        }
        case 0x4:
        {
            Uart_RS485_send(COMMON_INTERFACE_RS485_1, upload, dlen);
            break;
        }
				
				 case 0x0:
        {
            Uart_RS485_send(COMMON_INTERFACE_RS485_1, upload, dlen);
					  Uart_RS485_send(COMMON_INTERFACE_RS485_2, upload, dlen);
					  Uart_RS485_send(COMMON_INTERFACE_RS485_3, upload, dlen);
            break;
        }
        default:
        {
            break;
        }
    }
    mycfgdata.totalalarmcnt++;
}


void LED_test(void)
{
	  uint16_t crcdata = 0;
	  uint8_t maskcode = 0;
	  alarm_pdu alarmboard;
	  memset(&alarmboard, 0, sizeof(alarmboard));
    alarmboard.FrameHead = PDUHEAD;
    alarmboard.Pdu_len = sizeof(alarmboard);
    alarmboard.AntID = 0;
    alarmboard.Alarm_Duration[0] = 0;                    // LED_R
    alarmboard.Alarm_Duration[1] = 0;                    // LED_B
    alarmboard.Alarm_Duration[2] = (maskcode ? ALARM_G_CODE : ALARM_R_CODE);// LED_G  p->cmd
    alarmboard.Alarm_Duration[3] = 0;
    alarmboard.Alarm_Duration[5] = LED_BUZZ_TEST_CODE;
	  alarmboard.random_forest = 0;
	  crcdata = ipcCrc((uint8_t *)&alarmboard, sizeof(alarmboard) - 2);
	  alarmboard.crc = crcdata;
	  ipc_hpm_message((uint8_t *)&alarmboard, sizeof(alarmboard), alarmboard.AntID);
	
    BLUE_LED_OFF();
    RED_LED_OFF();
    GREEN_LED_OFF();
    Alarm_Output(LED_GLED, 15, 15, 2);
    osDelay(300);
    Alarm_Output(LED_BLED, 15, 15, 2);
    osDelay(300);
    Alarm_Output(LED_RLED, 15, 15, 2);
}

const osThreadAttr_t ThreadDB_Attr =
{
    .name = "osRtxflashdbThread",
    .cb_mem = NULL,
    .cb_size = 0,
    .attr_bits = osThreadDetached,
    .priority = osPriorityNormal,
    .stack_size = 2048,
};

const osThreadAttr_t Task_Monitor_Attr =
{
    .name = "Task_Monitor",
    .attr_bits = osThreadDetached,
    .priority = osPriorityNormal,
    .stack_size = 1024,
};

osThreadId_t ThreadIdTaskFlashDB  = NULL;
osThreadId_t ThreadIdTask_Monitor = NULL;
void timer_One_Shot(void *argument)
{

    uint64_t tick = osKernelGetTickCount();
    FlashDB_Sync_flag = 0xAA;
    TRACE("FLASH SYNC ticks is:%lld\r\n", tick);
}

void Check_Buffer_Diff(uint8_t flag);
extern osTimerId_t timerID_One_Shot;
void FlashDB_Task(void *argument)
{
    const uint16_t usFrequency = 200; /* 延迟周期 */
    uint32_t tick;
    static time_t utc_time_stamp = 0;

   // sleep_ms(500);
    /* 获取当前时间 */

    tick = osKernelGetTickFreq();
    TRACE("osKernelGetTickFreq      is:%d\r\n", tick);

    tick = osKernelGetSysTimerCount();
    TRACE("osKernelGetSysTimerCount is:%d\r\n", tick);

    tick = osKernelGetSysTimerFreq();
    TRACE("osKernelGetSysTimerFreq  is:%d\r\n", tick);

    tick = osKernelGetTickCount();
    TRACE("osKernelGetTickCount     is:%d\r\n", tick);
    System_guard_reg(FLASHDB_SWDT_ID, 2000, SWDT_STAT_RUN);   
    tick=0;
    while (1)
    {
        if (utc_time_stamp++ % 50 == 0 || FlashDB_Sync_flag > 10)
        {
           // while(utc_time_stamp);
            Check_Buffer_Diff(FlashDB_Sync_flag);
        }
        else
        {
           // bsp_gpio_toggle(3);
        }
       // uint64_t timenow = getSysTick();
       // TRACE("Current TickCount is:%lld\r\n", timenow);
        tick += usFrequency;
        SoftWdtFed(FLASHDB_SWDT_ID);
        osDelayUntil(tick);
    }
}

void AppTaskCreate(void)
{

    ThreadIdTask_Monitor = osThreadNew(Task_Monitor, NULL, &Task_Monitor_Attr);
    ThreadIdTaskFlashDB  = osThreadNew(FlashDB_Task, NULL, &ThreadDB_Attr);
}

extern LTNode *taglist;

void UDP_Trunk(uint8_t *udp_in)
{
    udp_package *decoder_udp = (udp_package *)udp_in;
    char epcstr[64];

    if (decoder_udp->frameHead == 0xEE && decoder_udp->cmdcode == 0x54)
    {
        LTDataType epcid;
        memcpy(epcid.epc, decoder_udp->epc, decoder_udp->epclen);
        epcid.Epclen = decoder_udp->epclen;

        uint64_t timestamp = osKernelGetTickCount();
        Hex2Str(decoder_udp->epc, decoder_udp->epclen, epcstr);
        TRACE("whitelist tag timestamp:%llu,Receive EPCID is '%s'\r\n", timestamp, epcstr);

        if (decoder_udp->cmdflag == OPTION_ADD)
        {

            tagtable_list_update_with_duplicate(ADDlist, epcid, OPTION_ADD);
            osTimerStart(timerID_One_Shot, 1000U);
        }

        if (decoder_udp->cmdflag == OPTION_DEL)
        {
            tagtable_list_update_with_duplicate(DELlist, epcid, OPTION_ADD);
            osTimerStart(timerID_One_Shot, 1000U);
        }
    }
}



uint8_t EASbitmatch(uint8_t *epcid, uint16_t tlen, uint32_t easflag, bool match_head_tail)
{
    if (easflag == 0 || epcid == NULL || tlen < 4)
        return 0;

    uint32_t tagbits;

    if (match_head_tail)
    {
        tagbits = (epcid[0] << 24) | (epcid[1] << 16) | (epcid[2] << 8) | epcid[3];
    }
    else
    {
        tagbits = (epcid[tlen - 4] << 24) |
                  (epcid[tlen - 3] << 16) |
                  (epcid[tlen - 2] << 8)  |
                  (epcid[tlen - 1]);
    }


    int highest = -1;

    for (int i = 7; i >= 0; --i)
    {
        if (((easflag >> (i * 4)) & 0xF) != 0)
        {
            highest = i;
            break;
        }
    }

    if (highest < 0) return 0;

    int k = highest + 1;

    if (match_head_tail)
    {

        for (int i = 0; i < k; ++i)
        {
            uint8_t tag_nib = (tagbits >> ((7 - i) * 4)) & 0xF;
            uint8_t eas_nib = (easflag >> ((k - 1 - i) * 4)) & 0xF;

            if (tag_nib != eas_nib) return 0;
        }
    }
    else
    {

        for (int i = 0; i < k; ++i)
        {
            uint8_t tag_nib = (tagbits >> (i * 4)) & 0xF;
            uint8_t eas_nib = (easflag >> (i * 4)) & 0xF;

            if (tag_nib != eas_nib) return 0;
        }
    }

    return 1;
}





void SetEasBit(uint8_t *epcid, uint16_t tlen, const uint32_t easflag, bool match_head_tail)
{
    if (epcid == NULL || tlen < 4)
        return;

    if (easflag == 0)
        return;

    if (match_head_tail)
    {

        uint8_t n[8];
        n[0] = (epcid[0] >> 4) & 0xF;
        n[1] = epcid[0] & 0xF;
        n[2] = (epcid[1] >> 4) & 0xF;
        n[3] = epcid[1] & 0xF;
        n[4] = (epcid[2] >> 4) & 0xF;
        n[5] = epcid[2] & 0xF;
        n[6] = (epcid[3] >> 4) & 0xF;
        n[7] = epcid[3] & 0xF;


        int k = 0;

        int highest_nibble = -1;

        for (int i = 7; i >= 0; --i)
        {
            if (((easflag >> (i * 4)) & 0xF) != 0)
            {
                highest_nibble = i;
                break;
            }
        }

        if (highest_nibble < 0) return;


        k = highest_nibble + 1;


        uint8_t eas_nibbles[8];

        for (int i = 0; i < k; ++i)
        {

            int shift = (k - 1 - i) * 4;
            eas_nibbles[i] = (easflag >> shift) & 0xF;
        }


        for (int i = 0; i < k; ++i)
            n[i] = eas_nibbles[i];


        epcid[0] = (uint8_t)((n[0] << 4) | (n[1] & 0xF));
        epcid[1] = (uint8_t)((n[2] << 4) | (n[3] & 0xF));
        epcid[2] = (uint8_t)((n[4] << 4) | (n[5] & 0xF));
        epcid[3] = (uint8_t)((n[6] << 4) | (n[7] & 0xF));
    }
    else
    {

        uint32_t tag = (epcid[tlen - 4] << 24) | (epcid[tlen - 3] << 16) |
                       (epcid[tlen - 2] << 8)  | epcid[tlen - 1];


        int valid_nibbles = 8;

        while (valid_nibbles > 0 &&
                ((easflag >> ((valid_nibbles - 1) * 4)) & 0xF) == 0)
            valid_nibbles--;

        if (valid_nibbles == 0)
            return;

        uint32_t mask = 0;

        for (int i = 0; i < valid_nibbles; i++)
            mask |= (0xFu << (i * 4));

        tag &= ~mask;
        tag |= (easflag & mask);

        epcid[tlen - 4] = (tag >> 24) & 0xFF;
        epcid[tlen - 3] = (tag >> 16) & 0xFF;
        epcid[tlen - 2] = (tag >> 8)  & 0xFF;
        epcid[tlen - 1] = tag & 0xFF;
    }
}

