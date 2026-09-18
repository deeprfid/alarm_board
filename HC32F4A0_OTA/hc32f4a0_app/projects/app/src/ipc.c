
#include <stdio.h>
#include <string.h>
#include "ipc.h"
#include "rdr_cfg_kv.h"
//#include "yyjson.h"
#include "hc32f46_driver.h"
#include "rfid_distance.h"
#include "reader_init.h"   /* v9.81ch: rdr_* access */

unsigned short ipcCrc(unsigned char *msgbuf, int msglen);
void set_eastag_to_flash(void);
mqttcfg  mymqttcfg;
osMutexId_t tb_SpiMux;
osRtxMutex_t gTb_SpiMux_cb;

osMutexId_t uploadMuxID;
osRtxMutex_t uploadMux_cb;
osMutexId_t httpReqMuxID;      /* v9.82j: HTTP 请求串行化(共享 pHttpReq 防并发重置跑飞) */
osRtxMutex_t httpReqMux_cb;

__align(64) rfidcfg mycfgdata;

osMessageQueueId_t gIdMsgQue_alarm_tag;
osMessageQueueId_t gIdMsgQue_HPM6340;
LTNode *taglist;
LTNode *staticlist;
LTNode *ADDlist;
LTNode *DELlist;
LTNode *g_Uploadtag;
/******************************************************************************/
/** 设置EAS默认参数
 ** 如果校验出错，恢复出厂设置
 **
 **
 **
 **
 **
 ******************************************************************************/
void set_eas_defualt(void)
{
    uint16_t crc;
    crc = ipcCrc((unsigned char *)&mycfgdata, sizeof(mycfgdata) - 4);

    if (crc == mycfgdata.crc)
    {
        mycfgdata.totaltagcnt = 0;
        mycfgdata.totalalarmcnt = 0;
        return;
    }
    else
    {
        memset(&mycfgdata, 0, sizeof(mycfgdata));
        mycfgdata.tagstoragedays[0] = 0x35;
        mycfgdata.system_time[0] = 0x30;
        mycfgdata.radar_range = 1;
        mycfgdata.tag_read_cnt = 5;
        mycfgdata.alarm_volume = 5;
        mycfgdata.alarm_duration = 5;
        set_eastag_to_flash();
    }
}

void HPM6340msg_init(void)
{
    osThreadAttr_t thAttr_t;
    osMessageQueueAttr_t mqAttr;
    int mqdsize;
    mqdsize = sizeof(MsgQueObj_HPM6340);
    mqdsize += 32 - mqdsize % 32;
    mqdsize = mqdsize * 32;
    TRACE("mqdsize:%d\n", mqdsize);

    mqAttr.name = NULL;
    mqAttr.attr_bits = 0;
    mqAttr.cb_mem = malloc_hexp(sizeof(osRtxMessageQueue_t));
    mqAttr.cb_size = sizeof(osRtxMessageQueue_t);
    mqAttr.mq_mem = malloc_hexp(mqdsize);
    mqAttr.mq_size = mqdsize;

    gIdMsgQue_HPM6340 = osMessageQueueNew(16, sizeof(MsgQueObj_HPM6340), &mqAttr);

    if (gIdMsgQue_HPM6340 != NULL)
        TRACE("HPM6340 osMessageQueueNew is ok\n");
    else
        TRACE("HPM6340 osMessageQueueNew is failed\n");

    taglist      = LTInit();
    staticlist   = LTInit();
    g_Uploadtag  = LTInit();
    ADDlist      = LTInit();
    DELlist      = LTInit();
    init_osThreadAttr_t(&thAttr_t, 1024 * 2, osPriorityNormal);
    osThreadNew(Tag_update_thread, NULL, &thAttr_t);
}

void httpbuf_init(void)
{
    osMessageQueueAttr_t mqAttr;
    int mqdsize;
    mqdsize = sizeof(MsgQueObj_alarm_tag);
    mqdsize += 32 - mqdsize % 32;
    mqdsize = mqdsize * 32;
    TRACE("mqdsize:%d\n", mqdsize);

    mqAttr.name = NULL;
    mqAttr.attr_bits = 0;
    mqAttr.cb_mem = malloc_hexp(sizeof(osRtxMessageQueue_t));
    mqAttr.cb_size = sizeof(osRtxMessageQueue_t);
    mqAttr.mq_mem = malloc_hexp(mqdsize);
    mqAttr.mq_size = mqdsize;

    gIdMsgQue_alarm_tag = osMessageQueueNew(16, sizeof(MsgQueObj_alarm_tag), &mqAttr);

    if (gIdMsgQue_alarm_tag != NULL)
        TRACE("alarm osMessageQueueNew is ok\n");
    else
        TRACE("alarm osMessageQueueNew is failed\n");

}

uint32_t Get_whitetags_total_count(LTNode *phead)
{
    uint32_t totaltags = 0;
    osMutexAcquire(uploadMuxID, osWaitForever);
    totaltags = LTTotal(phead);
    osMutexRelease(uploadMuxID);
    return totaltags;
}

LTDataType Getlist_tag(LTNode *phead, uint32_t index)
{
    osMutexAcquire(uploadMuxID, osWaitForever);
    LTNode *tagnode;
    tagnode = LTGet(phead, index);
    osMutexRelease(uploadMuxID);
    return tagnode->data;
}

void uploadlist_clear(void)
{

    LTClear(g_Uploadtag);
}

void tagtable_list_update(LTNode *phead, LTDataType epcID, uint8_t method)
{
    osMutexAcquire(uploadMuxID, osWaitForever);

    if (method == OPTION_INIT) // INIT
    {
        if (LTFind(phead, epcID) == NULL)
        {
            LTPushFront(phead, epcID);
        }
    }

    if (method == OPTION_DEL) // DEL
    {
        LTRemove(phead, epcID);
    }

    if (method == OPTION_ADD) // ADD
    {
        if (LTFind(phead, epcID) == NULL)
        {
            LTPushFront(phead, epcID);
        }
    }

    osMutexRelease(uploadMuxID);
}

void tagtable_update(LTNode *phead, LTDataType epcID)
{
    LTNode* pos = LTFind(phead, epcID);

    if (pos == NULL)
    {
        LTPushFront(phead, epcID);

    }
    else
    {

        LTModify(pos, epcID);
    }


}

list_err_t tagtable_list_update_with_duplicate(LTNode *phead, LTDataType epcID, uint8_t method)
{
    osMutexAcquire(uploadMuxID, osWaitForever);

    list_err_t result = list_NO_ERR;

    if (method == OPTION_INIT) // INIT
    {
        if (LTFind(phead, epcID) == NULL)
        {
            LTPushFront(phead, epcID);
            result = list_FIND_NONE;
        }
        else
        {
            result = list_FINDIN;
        }
    }
    else if (method == OPTION_DEL) // DEL
    {
        LTRemove(phead, epcID);
    }
    else if (method == OPTION_ADD) // ADD
    {
        if (LTFind(phead, epcID) == NULL)
        {
            LTPushFront(phead, epcID);
            result = list_FIND_NONE;
        }
        else
        {
            result = list_FINDIN;
        }
    }

    osMutexRelease(uploadMuxID);
    return result;
}

void Init_whitelist_to_flash(LTNode *phead)
{

    LTClear(phead);
}

int get_hpm6340_tag_que(MsgQueObj_HPM6340 *mqObj)
{
    osStatus_t gmqret;

    gmqret = osMessageQueueGet(gIdMsgQue_HPM6340, mqObj, NULL, NULL);

    if (gmqret == osOK)
    {
        return 0;
    }

    return -1;
}

/* v9.81ch: rdr_get_handle() 接口见 reader_init.h */



unsigned int swapEndian(unsigned int num)
{
    return ((num >> 24) & 0x000000FF) |
           ((num >> 8) & 0x0000FF00) |
           ((num << 8) & 0x00FF0000) |
           ((num << 24) & 0xFF000000);
}

void MessageQueueReset(osMessageQueueId_t mq_id)
{
    osMessageQueueReset(mq_id);
}

void Tag_update_thread(void *arg)
{

    Usart_RS485_init();
    AppTaskCreate();
    RED_LED_ON();
    osDelay(250);
    RED_LED_OFF();

    while (1)
    {
        if (osMessageQueueGetCount(gIdMsgQue_HPM6340))
        {
            MsgQueObj_HPM6340 hpmtag;
            memset(&hpmtag, 0, sizeof(hpmtag));

            if(osOK == osMessageQueueGet(gIdMsgQue_HPM6340, &hpmtag, NULL, NULL))
            {
                tag_package(&hpmtag);
            }
        }

        RGB_Lighting_update();
       //LED_Runing_Status(1);
        osThreadYield();
    }
}

void LED_Runing_Status(uint8_t startbit)
{
    static unsigned long long Ticknow = 0;
    Ticknow = getSysTick();

    if (Ticknow % 3200 == 0 && startbit)
    {
        BLUE_LED_ON();
        return;
    }

    if (Ticknow % 400 == 0 && startbit)
    {
        BLUE_LED_OFF();
        return;
    }
}

void UDP_SendAll(osMessageQueueId_t sendmsg, uint8_t optioncode)
{
    uint8_t que_cnt = 0;
    osStatus_t gmqret;
    MsgQueObj_HPM6340 mqObj;

    que_cnt = osMessageQueueGetCount(sendmsg);

    for (uint8_t i = 0; i < que_cnt; i++)
    {
        gmqret = osMessageQueueGet(sendmsg, &mqObj, NULL, NULL);

        if (gmqret == osOK)
        {
            UDP_upload(&mqObj, optioncode);
            osDelay(1);
        }
    }
}

void UDP_upload(MsgQueObj_HPM6340 *udpepc, uint8_t optioncode)
{

    udp_package brdcstudp;
    uint8_t macaddr[6] = {0};
    brdcstudp.frameHead = 0xEE;
    brdcstudp.maclen = 0x06;
    brdcstudp.cmdflag = optioncode;
    brdcstudp.cmdcode = 0x54;
    SetNumU16((uint8_t *)&brdcstudp.datalen, udpepc->epclen);
    brdcstudp.epclen = udpepc->epclen;
    memcpy(brdcstudp.macaddr, macaddr, 0x06);
    memcpy(brdcstudp.epc, udpepc->epcid, udpepc->epclen);
    uint16_t crc = ipcCrc((unsigned char *)&brdcstudp, sizeof(brdcstudp) - 2);
    brdcstudp.crc = crc;
    UDP_send((uint8_t *)&brdcstudp, sizeof(brdcstudp));
}


void RollBack(MsgQueObj_HPM6340 *RollBackTag)
{
    unsigned char acpwd[9] = {0};
    MsgQueObj_HPM6340 tagTID;
    memset(&tagTID, 0, sizeof(tagTID));

    if (RollBackTag->EmbededDatalen > 0 && RollBackTag->EmbededDatalen <= 32)
    {
        tagTID.epclen = RollBackTag->EmbededDatalen;
        memcpy(tagTID.epcid, RollBackTag->EmbededData, RollBackTag->EmbededDatalen);
    }

    if (memcmp(RollBackTag->epcid, RollBackTag->EmbededData, RollBackTag->EmbededDatalen) == 0)
    {

        goto FINEXIT;
    }

    osMutexAcquire(tb_SpiMux, osWaitForever);

    while (MT_OK_ERR != WriteTagEpcEx(rdr_get_handle(), 1, tagTID.epcid, tagTID.epclen, acpwd, 100))
    {
    }

FINEXIT: // 2025-02-14

    osMutexRelease(tb_SpiMux);
    MessageQueueReset(gIdMsgQue_HPM6340);
}

void Killepctag(MsgQueObj_HPM6340 *RollBackTag)
{
}





int8_t filte_rule_process(uint8_t *epcid, uint8_t epclen)
{
    uint8_t mask[20] = {0};
    uint8_t result = 0xff, flag_mask = 0, match_mask = 0;

    for (uint8_t i = 0; i < 10; i++)
    {
        if (mycfgdata.tagfilter_rule[i].chstate == 0)
        {
            flag_mask++;
            continue;
        }

        if (mycfgdata.tagfilter_rule[i].chstate == 1)
        {
            strTohex(mycfgdata.tagfilter_rule[i].maskcode, mycfgdata.tagfilter_rule[i].match_len, mask);
            result = memcmp(epcid + mycfgdata.tagfilter_rule[i].start_addr, mask, (mycfgdata.tagfilter_rule[i].match_len) / 2);

            if (result == 0)
            {
                match_mask++;
            }
        }
    }

    if (flag_mask == 10)
    {
        return 0x1;
    }
    else if (flag_mask < 10 && match_mask)
    {
        return 0x1;
    }
    else if (flag_mask < 10 && match_mask == 0)
    {
        return 0;
    }

    return -2;
}

int put_tag_que(TAGINFO *tag)
{
    static uint8_t EPCID[EPCIDMAXLEN] = {0};
    osStatus_t osSta;
    MsgQueObj_alarm_tag mytagobj;
    MsgQueObj_HPM6340 hpmtag;
    if (0x00 == filte_rule_process(tag->EpcId, tag->Epclen))
    {
        return -2;
    }

    hpmtag.ant = tag->AntennaID;
    hpmtag.frameHead = 0xFF;
    hpmtag.epclen = tag->Epclen;
    memcpy(&hpmtag.epcid, tag->EpcId, tag->Epclen);
    memcpy(&hpmtag.crc, tag->CRC, 2);
    memcpy(&hpmtag.EmbededData, tag->EmbededData, tag->EmbededDatalen);
    hpmtag.EmbededDatalen = tag->EmbededDatalen;
    osSta = osMessageQueuePut(gIdMsgQue_HPM6340, &hpmtag, NULL, NULL);


   // Mobile_Net(tag);
    // que_cnt=osMessageQueueGetCount(gIdMsgQue_alarm_tag);

    // if(memcmp(tag->EpcId,EPCID,tag->Epclen) || que_cnt==0)
    {
        memset(&mytagobj, 0, sizeof(mytagobj));
        mytagobj.ant = tag->AntennaID;
        memcpy(EPCID, tag->EpcId, tag->Epclen);
        mytagobj.epclen = tag->Epclen;
        memcpy(&mytagobj.epcid, tag->EpcId, tag->Epclen);
        memcpy(&mytagobj.crc, tag->CRC, 2);
        osSta = osMessageQueuePut(gIdMsgQue_alarm_tag, &mytagobj, NULL, NULL);

        // osSta = osMessageQueuePut(gIdMsgQue_HPM6340  , &hpmtag  , NULL, NULL);
        if (osSta != osOK)
            return -2;
    }

    return 0;
}

uint8_t get_tag_counter(void)
{
    uint8_t que_cnt = 0;

    que_cnt = osMessageQueueGetCount(gIdMsgQue_alarm_tag);

    return que_cnt;
}

int get_tag_que(MsgQueObj_alarm_tag *mqObj)
{
    osStatus_t gmqret;
    gmqret = osMessageQueueGet(gIdMsgQue_alarm_tag, mqObj, NULL, NULL);

    if (gmqret == osOK)
    {
        return 0;
    }

    return -1;
}

uint8_t removeDuplicates(uint8_t *arr, uint8_t *epcid, uint8_t len)
{

    uint8_t j = 0;

    for (int i = 0; i < QUELENTH * 5; i++)
    {
        if (memcmp(arr + i * EPCIDMAXLEN, epcid, len) == 0)
        {

            j++;
        }
    }

    return j ? 0 : -1;
}

int flashdb(void);
void tagfiltbuff_init(void)
{

    get_eastag_to_flash();
    set_eas_defualt();
    httpbuf_init();
    HPM6340msg_init();
    /* v9.81s: flashdb()+migrate 已提前到 driver init_thread（rdr_cfg_kv_init） */
  //  Freq_rssi_init();
    osMutexAttr_t mux_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &gTb_SpiMux_cb,
        sizeof(gTb_SpiMux_cb)
    };
    tb_SpiMux = osMutexNew(&mux_attr);

    osMutexAttr_t mux_upload_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &uploadMux_cb,
        sizeof(uploadMux_cb)
    };
    uploadMuxID = osMutexNew(&mux_upload_attr);

    osMutexAttr_t mux_http_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &httpReqMux_cb,
        sizeof(httpReqMux_cb)
    };
    httpReqMuxID = osMutexNew(&mux_http_attr);

    //  Yjson_test_read();
    //  Yjson_test_write();
}

/* v9.82j: HTTP 入口互斥——同一时刻只允许一个请求解析/响应(两个监听任务共用全局 pHttpReq/pHttpMAPI) */
void http_req_enter(void)
{
    osMutexAcquire(httpReqMuxID, osWaitForever);
}

void http_req_leave(void)
{
    osMutexRelease(httpReqMuxID);
}

void deviceID_update(ReaderRunTimeSettings_ST *prtset)
{
    int namelen = strlen(prtset->glob_params.name);

    if (namelen > 0 && namelen < 20)
    {
        memset(&mycfgdata.deviceID, 0, sizeof(mycfgdata.deviceID));
        memcpy(&mycfgdata.deviceID, prtset->glob_params.name, namelen);
    }
}

void rd_idkey_fun(void)
{

    static uint32_t uuid;
    uuid = Ucode_read((uint8_t *)&uuid, sizeof(uuid));

    if (uuid != 0x423B6C6E)
    {
        Alarm_Output (BEEP_CTRL , 15, 15, 3);
        Alarm_Output (BOARD_LED2, 15, 15, 0);
        while (uuid);

    }
    else
    {
        Alarm_Output (BEEP_CTRL , 15, 15, 2);
        Alarm_Output (BOARD_LED2, 15, 15, 2);

    }
}

void set_eastag_to_flash(void)
{

    // 20250315
    uint8_t *buf_ = (uint8_t *)&mycfgdata;
    uint16_t crc;
    crc = ipcCrc((unsigned char *)&mycfgdata, sizeof(mycfgdata) - 4);
    mycfgdata.crc = crc;
    flash_sector_erase(EastagPage_Addr);
    flash_bytes_write(EastagPage_Addr, buf_, sizeof(mycfgdata));
}

void Erase_eastag_to_flash(void)
{

    // 20250315
    uint8_t *buf_ = (uint8_t *)&mycfgdata;
    memset(&mycfgdata, 0, sizeof(mycfgdata));
    flash_sector_erase(EastagPage_Addr);
    flash_bytes_write(EastagPage_Addr, buf_, sizeof(mycfgdata));
}

void get_eastag_to_flash(void)
{

    flash_bytes_read(EastagPage_Addr, &mycfgdata, sizeof(mycfgdata));
}


