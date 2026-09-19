#include "qspi_flash.h"   /* ??????hc32f46_driver.h ??(hc32f4a0.h:3630) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ModuleReader.h"
#include "hc32f46_driver.h"
#include "mp_pool.h"
#include "app_conf.h"
#include "http_callback.h"
#include "mqtt_interface.h"
#include "MQTTClient.h"
#include "reader_msg.h"
#include "reader_init.h"
#include "ErrChecker.h"
#include "event_mq.h"
#include "ota_agent.h"
#include "radar_link.h"    /* F4A0 <-> 报警板链路轮询 */
#include "ota_storage.h"      /* OTA_QSPI_STAGE_BASE */
#include "ota_state.h"        /* ota_set_progress */
#include "ota_usb.h"
#include "ota_http.h"       /* v9.81j: HTTP OTA ??????? */
#include "ota_transport_uart.h"   /* v9.81cl: ?????????? OTA ???? */
#include "ota_transport_uart.h"   /* v9.81cl: ?????????? OTA ???? */
#include "usb_msc_ota.h"   /* v9.81ci: USB ???????????????*/
#include "ota_integration.h"   /* v1.0: OTA ????? */
#include "ipc.h"
#include "hw_api.h"   /* v9.81ci: driver config access */
/*****************************************************************************
*??????
1??driverlib: ???GPIO_Configuration()????Ipio.c???GPIO??????????wiegand_init();?????
               #define IS_RTOS2_SUPPORT 1
               #define ENABLE_ICG_TABLE 0
2??firmware:  ???init_usb(1);init_usb(gPRdrStaSet->app_init.usb_type);

3:  #define Custom_By_Caipan    0
    #define Custom_By_ZHXX_ZSYH 0
    #define Custom_By_CDZNWL    0
    #define Custom_By_GZTD      0
    #define Custom_By_HZWXZN    0
    #define Custom_By_SZBMA     1
4: ???boot???????
*
*
*
*******************************************************************************/

int user_main_thstk_size = 1024 * 4;
osPriority_t user_main_priority = osPriorityNormal;
int is_enable_fwupdate = 2;

int reason2 = 0;
int reason1 = 0;

static READER_ERR gRdrErr = MT_OK_ERR;
volatile int gErrSend = 0;
volatile int gRdrStateFlag = 0;

static WorkMode_Code gCurWorkMode = WorkMode_None;
volatile int gIsFinInit = 0;

ReaderRunTimeSettings_ST *gRtSetting  = NULL;
ReaderStaticSettings_ST  *gPRdrStaSet = NULL;
static int gIsStartAsyncInv = 0;
void wait_init_ok(void)
{
    while (1)
    {
        if (gIsFinInit == 1)
            break;

        sleep_ms(50);
    }
}

typedef struct
{
    volatile int finflag[4];
    unsigned long long starttime;
    volatile int isFire;
} LocGpoSet_ST;

static LocGpoSet_ST gTRLocGpoActs;

void resetLocGpoSet(LocGpoSet_ST *pLGS)
{
    int i;
    pLGS->isFire = 0;

    for (i = 0; i < 4; ++i)
        pLGS->finflag[i] = 0;
}

void checkLocGPOAct(void *data)
{
    LocGpoSet_ST *pLGS = (LocGpoSet_ST *)data;

    if (gRtSetting->gpo_act.count > 0 && pLGS->isFire == 1)
    {
        int i;
        int isallfin = 1;
        int dur = getSysTick() - pLGS->starttime;

        for (i = 0; i < gRtSetting->gpo_act.count; ++i)
        {
            if (pLGS->finflag[i] == 0)
            {
                if (dur >= gRtSetting->gpo_act.durs[i])
                {
                    gpo_set(gRtSetting->gpo_act.ids[i], 1 - gRtSetting->gpo_act.states[i]);
                    pLGS->finflag[i] = 1;
                }

                isallfin = 0;
            }
        }

        if (isallfin == 1)
            resetLocGpoSet(pLGS);
    }
}

void send_tags(void *arg)
{
    TAGINFO tmpTag;
    uint64 now_time;

    wait_init_ok();

    if (evt_is_enabled(EVT_FLAG_SENDTAGONLY) == 0)
    {
        if (evt_is_enabled(EVT_FLAG_HEARTBEAT) == 1)
        {
            send_evt_heartbeat();
            evt_set_hb_last_time(getSysTick());
        }

        if (evt_is_enabled(EVT_FLAG_SYNCTIMEREQ) == 1 &&
            (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Http ||
             gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt))
        {
            while (1)
            {
                TRACE("send synctimereq\n");
                send_evt_synctimereq();

                if (gUtcSecBase > 0)
                    break;

                sleep_ms(2000);
            }
        }
    }

    evt_set_batch_last_time(getSysTick());

    while (1)
    {
        if (gRtSetting->upload.sw_potl == Upload_Trans_Potl_Mqtt)
            mqtt_task(NULL, 0);

        RemoteCmd();
        sleep_ms(5);

        now_time = getSysTick();

        if (evt_is_enabled(EVT_FLAG_SENDTAGONLY) == 0)
        {
            if (evt_is_enabled(EVT_FLAG_HEARTBEAT) == 1)
            {
                if (now_time - evt_hb_last_time() >= gRtSetting->glob_params.hb_cylce * 1000)
                {
                    send_evt_heartbeat();
                    evt_set_hb_last_time(getSysTick());
                }
            }

            if (evt_is_enabled(EVT_FLAG_GPICHAN) == 1)
            {
                uint8 gpistates;

                if (GpiChange(&gpistates) == 1)
                    send_evt_gpichan(gpistates);
            }

            if (evt_is_enabled(EVT_FLAG_SENDRDRERR) == 1)
            {
                if (gErrSend != 0)
                {
                    gRdrStateFlag = gErrSend;
                    send_evt_reader_err();
                    gErrSend = 0;
                }
            }
        }

        if (gRtSetting->gpi_trigger.is_gpi_trigger != 1 &&
            gRtSetting->upload.data_aggr.mode == 1 && gRdrStateFlag == 0)
        {
            if (now_time - evt_batch_last_time() >= gRtSetting->upload.data_aggr.timeval)
            {
                send_evt_tagbatch();
                evt_set_batch_last_time(now_time);
            }
        }

        if (get_evt_que(&tmpTag) == 0)
        {
            switch (tmpTag.PC[0])
            {
            case App_Evt_TagComing:
                if (evt_is_enabled(EVT_FLAG_SENDTAGONLY) == 0)
                    send_evt_tagcoming(&tmpTag);

                break;

            case App_Evt_BatchMoment:
            {
                send_evt_tagbatch();
                break;
            }

            default:
                break;
            }
        }
    }
}

typedef enum
{
    BackReadGpi_WaitStart = 0,
    BackReadGpi_WaitStop = 1,
    BackReadGpi_WaitTimeout = 2,
} BackReadGpiTriState;
void getAllGpi(rdr_rt_set_gpi_cond *pGinfo)
{
    pGinfo->count = 4;
    pGinfo->ids[0] = 1;
    pGinfo->states[0] = gpi_get(1);
    pGinfo->ids[1] = 2;
    pGinfo->states[1] = gpi_get(2);
    pGinfo->ids[2] = 3;
    pGinfo->states[2] = gpi_get(3);
    pGinfo->ids[3] = 4;
    pGinfo->states[3] = gpi_get(4);
}

int GpiTriContains(rdr_rt_set_gpi_cond *littleGInfo, rdr_rt_set_gpi_cond *bigGInfo)
{
    int i;

    for (i = 0; i < littleGInfo->count; ++i)
    {
        if (bigGInfo->states[littleGInfo->ids[i] - 1] != littleGInfo->states[i])
            return 0;
    }

    return 1;
}

void fireLocGPOAct(LocGpoSet_ST *pLGS)
{
#if Custom_By_Caipan || Custom_By_ZHXX_ZSYH
#else

    if (gRtSetting->gpo_act.count > 0 && pLGS->isFire == 0)
    {
        int i;

        for (i = 0; i < gRtSetting->gpo_act.count; ++i)
            gpo_set(gRtSetting->gpo_act.ids[i], gRtSetting->gpo_act.states[i]);

        pLGS->starttime = getSysTick();
        pLGS->isFire = 1;
    }

#endif
}

void tagInsert_wp(READER_ERR gErr, TAGINFO *tag, int triid, LocGpoSet_ST *pLGS)
{
    int Repet;

    if (gErr == MT_OK_ERR)
    {
        if (tag->Epclen + tag->EmbededDatalen <= gPRdrStaSet->app_init.max_tb_rec_len)
        {
            if (gRtSetting->gpi_trigger.is_gpi_trigger == 1)
            {
                if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP ||
                    gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TIMEOUTSTOP)
                {
                    if (gRtSetting->gpi_trigger.cond_order == 1)
                        tag->protocol = (SL_TagProtocol)triid;
                    else
                        tag->protocol = (SL_TagProtocol)(3 - triid);
                }
            }

            tag->TimeStamp = getSysTick() / 1000;
            Repet = tagInsert(tag);

            if (Repet == -1)
            {
                TAGINFO tmpTag;
                tagGetNext(&tmpTag);
                Repet = tagInsert(tag);
            }

            put_tag_que(tag);


            if (evt_is_enabled(EVT_FLAG_TAGCOMING) == 1 && Repet == 0)
            {
                tag->Phase = tag->TimeStamp;
                tag->PC[0] = App_Evt_TagComing;
                put_evt_que(tag);
            }

            fireLocGPOAct(pLGS);
        }
    }
}

const int DefNoSndGPOVal = 0;

char *gRdrErrStrBuf;
const uint8 GPITRIGGER_TRI1ANDTRI2START_TIMEOUTSTOP = 5;
int wait_tri_cond(rdr_rt_set_gpi_cond *cond, int stat, int timeout)
{
    rdr_rt_set_gpi_cond states;
    int nowstat;
    uint64 start = getSysTick();

    while (1)
    {
        sleep_ms(15);
        getAllGpi(&states);
        nowstat = GpiTriContains(cond, &states);

        if (nowstat == stat)
            break;
        else if (timeout > 0)
        {
            if (getSysTick() - start >= timeout)
                break;
        }
    }

    return nowstat;
}

extern osMutexId_t tb_SpiMux;
void user_main_active(void)
{
    //	baseParaConfig config;
    int tagcnt;
    int i;
    TAGINFO tag;
    osThreadAttr_t thAttr_t;

    BackReadGpiTriState BRGstate = BackReadGpi_WaitStart;
    rdr_rt_set_gpi_cond gstates;
    int laststopgpitriid = -1;
    int CanStartTRiStopCase = 1;
    uint64 CanStartTm1;
    uint64 CanStartTm2;
    uint64 dtwstart;
    uint64 timenow;
    int startgpitriid = 0;
    int isGpiTriStop = 0;

    int isTri1equTri2 = 0;
    init_evt_sys();

    echr_init(90);
    gRtSetting->cus_param.param[gRtSetting->cus_param.len] = 0;
    TRACE("gRtSetting->cus_param.param:%s\n", gRtSetting->cus_param.param);

    resetLocGpoSet(&gTRLocGpoActs);
    add_cycle_task(checkLocGPOAct, &gTRLocGpoActs);
    dump_runtime_settings(gRtSetting);

    init_osThreadAttr_t(&thAttr_t, 1024 * 4, osPriorityNormal);
    osThreadNew(send_tags, NULL, &thAttr_t);

    /* 报警板链路轮询线程：三条 RS485 的收发、新鲜度、指示灯都在里面（10ms 一轮） */
    init_osThreadAttr_t(&thAttr_t, 1024 * 4, osPriorityNormal);
    osThreadNew(radar_link_task, NULL, &thAttr_t);

//     init_usb(1);
    sleep_ms(100);

    if (init_upload() != 0)
        goto FIN;

    for (i = 0; i < 3; ++i)
    {

        gRdrErr = OpenReader();
        TRACE("------ gRdrErr:%d\n", gRdrErr);

        if (gRdrErr == MT_OK_ERR)
            break;
    }

    get_left_heap_size("after OpenReader");
    gIsFinInit = 1;
    ota_confirm_after_init();   /* v1.0: OTA integration - new fw self-check (anti-rollback) */

    if (gRdrErr != MT_OK_ERR)
    {
        reason1 = 1;
        gErrSend = (int)gRdrErr;

        if (gRdrErr == MT_HARDWARE_ALERT_ERR_BY_NO_ANTENNAS)
            led_toggle(-1, 1000, NULL);
        else
            led_toggle(-1, 200, NULL);
    }

    TRACE("InitReader ok\n");
    //	heaptop = malloc(4);
    //	printf("4444444444444444444 heaptop:%p\n", heaptop);

    TRACE("cycle:%d, rdr_can_async_inv():%d\n", gPRdrStaSet->tagops_param.inventory.cycle, rdr_can_async_inv());

    TRACE("Reader initialization is finished\n");

    /* v9.81cj-g: ????????????????????????????? */
    get_left_heap_size("after init_finish_active");

   // led_on();
		Alarm_Disable(BOARD_LED1);
    Alarm_Output (BOARD_LED1, 50, 50, 0);
    //////gpi trigger
    if (gRtSetting->gpi_trigger.is_gpi_trigger == 1)
    {
        if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP ||
            gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP)
            isGpiTriStop = 1;

        if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP)
        {
            if (memcmp(&gRtSetting->gpi_trigger.cond_1, &gRtSetting->gpi_trigger.cond_1, sizeof(rdr_rt_set_gpi_cond)) == 0)
                isTri1equTri2 = 1;
        }
    }
    System_guard_reg(USER_MAIN_SWDT_ID,1000,SWDT_STAT_RUN);

    //////
    while (1)
    {
        //		int gettagcnt = 1;
        if (gRtSetting->gpi_trigger.is_gpi_trigger == 1)
        {
            int isStart = 1;
            int isStop = 1;

            if (BRGstate == BackReadGpi_WaitStart)
            {
                getAllGpi(&gstates);

                if (laststopgpitriid != -1 &&
                    (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP ||
                     gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP))
                {
                    if (CanStartTRiStopCase == 0 && isTri1equTri2 == 0)
                    {
                        int isLastCondOn;

                        if (laststopgpitriid == 1)
                            isLastCondOn = GpiTriContains(&gRtSetting->gpi_trigger.cond_1, &gstates);
                        else
                            isLastCondOn = GpiTriContains(&gRtSetting->gpi_trigger.cond_2, &gstates);

                        if (isLastCondOn == 1)
                            CanStartTm1 = getSysTick();
                        else
                        {
                            CanStartTm2 = getSysTick();

                            if ((CanStartTm2 - CanStartTm1) >= gRtSetting->gpi_trigger.timeval)
                                CanStartTRiStopCase = 1;
                        }

                        if (CanStartTRiStopCase == 0)
                        {
                            sleep_ms(20);
                            continue;
                        }
                    }
                }

                isStart = GpiTriContains(&gRtSetting->gpi_trigger.cond_1, &gstates);

                if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ANDTRI2START_TIMEOUTSTOP)
                {
                    if (isStart == 1)
                    {
                        isStart = wait_tri_cond(&gRtSetting->gpi_trigger.cond_2, 1, gRtSetting->gpi_trigger.timeval2);
                    }
                }
                else if (isTri1equTri2 == 1 && gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP)
                {
                    if (isStart == 1)
                        wait_tri_cond(&gRtSetting->gpi_trigger.cond_1, 0, -1);
                }
                else
                {
                    if (isStart == 1)
                        startgpitriid = 1;
                    else if (gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TIMEOUTSTOP ||
                             gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1ORTRI2START_TRI1ORTRI2STOP)
                    {
                        isStart = GpiTriContains(&gRtSetting->gpi_trigger.cond_2, &gstates);

                        if (isStart == 1)
                            startgpitriid = 2;
                    }
                }

                if (isStart == 1)
                {
                    if (isGpiTriStop == 1)
                        BRGstate = BackReadGpi_WaitStop;
                    else
                    {
                        dtwstart = getSysTick();
                        BRGstate = BackReadGpi_WaitTimeout;
                    }

                    if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && rdr_can_async_inv() == 1)
                        gIsStartAsyncInv = 0;

                    TRACE("Trigger start .........\n");
                }
                else
                {
                    sleep_ms(10);
                    continue;
                }
            }
            else
            {
                if (BRGstate == BackReadGpi_WaitStop)
                {
                    getAllGpi(&gstates);

                    if (startgpitriid == 1)
                        isStop = GpiTriContains(&gRtSetting->gpi_trigger.cond_2, &gstates);
                    else
                        isStop = GpiTriContains(&gRtSetting->gpi_trigger.cond_1, &gstates);

                    if (isStop == 1)
                    {
                        BRGstate = BackReadGpi_WaitStart;

                        if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && rdr_can_async_inv() == 1)
                        {
                            gRdrErr = SyncStopFastReading(rdr_get_handle());

                            if (gRdrErr != MT_OK_ERR)
                            {
                                if (HandleModErr() != 0)
                                {
                                    reason1 = 3;
                                    goto FIN;
                                }
                            }
                        }

                        ////insert event
                        tag.PC[0] = App_Evt_BatchMoment;
                        put_evt_que(&tag);
                        /////
                        laststopgpitriid = 3 - startgpitriid;
                        CanStartTRiStopCase = 0;
                        CanStartTm1 = getSysTick();
                        TRACE("Trigger BackReadGpi stop .........\n");

                        if (isTri1equTri2 == 1 && gRtSetting->gpi_trigger.mode == GPITRIGGER_TRI1START_TRI2STOP)
                            wait_tri_cond(&gRtSetting->gpi_trigger.cond_1, 0, -1);

                        continue;
                    }
                }
                else if (BRGstate == BackReadGpi_WaitTimeout)
                {
                    timenow = getSysTick();

                    if ((timenow - dtwstart) > (gRtSetting->gpi_trigger.timeval))
                    {
                        BRGstate = BackReadGpi_WaitStart;

                        if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && rdr_can_async_inv() == 1)
                        {
                            gRdrErr = SyncStopFastReading(rdr_get_handle());

                            if (gRdrErr != MT_OK_ERR)
                            {
                                if (HandleModErr() != 0)
                                {
                                    reason1 = 3;
                                    goto FIN;
                                }
                            }
                        }

                        ////insert event
                        tag.PC[0] = App_Evt_BatchMoment;
                        put_evt_que(&tag);
                        /////
                        TRACE("Trigger WaitTimeout stop .........\n");
                        continue;
                    }
                }
            }
        }

        if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && rdr_can_async_inv() == 1)
        {
            if (gIsStartAsyncInv == 0)
            {
                TRACE("SyncStartFastReading\n");
                gRdrErr = SyncStartFastReading(rdr_get_handle(), rdr_get_connants()->connectedants, rdr_get_connants()->antcnt,
                                               (((0x1 << 1) | (0x1 << 2) | (0x1 << 3) | (0x1 << 7)) << 8) | 0x80 | (0x01 << 24));

                if (gRdrErr == MT_OK_ERR)
                    gIsStartAsyncInv = 1;
                else
                    gIsStartAsyncInv = 0;
            }
        }
        else
        {
            if (gRdrErr == MT_OK_ERR)
            {
                if (gPRdrStaSet->tagops_param.inventory.cycle == 0)
                    gPRdrStaSet->tagops_param.inventory.cycle = 150;

                //				TRACE("44444444444444444444\n");
                osMutexAcquire(tb_SpiMux, osWaitForever);
                gRdrErr = TagInventory_Raw(rdr_get_handle(), rdr_get_connants()->connectedants, rdr_get_connants()->antcnt,
                                           gPRdrStaSet->tagops_param.inventory.cycle * rdr_get_connants()->antcnt, &tagcnt);

                osMutexRelease(tb_SpiMux);
            }
        }

        if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
        {
            if (gPRdrStaSet->tagops_param.inventory.cycle == 0 && rdr_can_async_inv() == 1)
            {
                if (gRdrErr == MT_OK_ERR || gRdrErr == MT_CMD_NO_TAG_ERR)
                {
                    //					printf("222222222222222222\n");
                    osMutexAcquire(tb_SpiMux, osWaitForever);
                    gRdrErr = SyncGetNextTag(rdr_get_handle(), &tag);
                    osMutexRelease(tb_SpiMux);
                    tagInsert_wp(gRdrErr, &tag, startgpitriid, &gTRLocGpoActs);
                }
            }
            else
            {
                for (i = 0; i < tagcnt; ++i)
                {
                    osMutexAcquire(tb_SpiMux, osWaitForever);
                    gRdrErr = GetNextTag(rdr_get_handle(), &tag);
                    osMutexRelease(tb_SpiMux);
                    tagInsert_wp(gRdrErr, &tag, startgpitriid, &gTRLocGpoActs);

                    if (gRdrErr != MT_OK_ERR)
                        break;
                }
            }

            //			printf("GetNextTag err:%d\n", gRdrErr);
            if (!(gRdrErr == MT_CMD_NO_TAG_ERR || gRdrErr == MT_OK_ERR))
            {
                TRACE("GetNextTag err:%d\n", gRdrErr);

                if (HandleModErr() != 0)
                {
                    reason1 = 3;
                    goto FIN;
                }
            }

            if (gPRdrStaSet->tagops_param.inventory.cycle != 0)
                sleep_ms(gPRdrStaSet->tagops_param.inventory.interval);
        }
        else
        {
            TRACE("TagInventory err:%d\n", gRdrErr);

            if (HandleModErr() != 0)
            {
                reason1 = 2;
                goto FIN;
            }
        }
				
			SoftWdtFed(USER_MAIN_SWDT_ID); 	
    }

FIN:
    Alarm_Disable(BOARD_LED1);
    sleep_ms(1000);
    system_reset();
    // while(1);
}

void user_main_passive(void);
extern volatile int uart0fd;
int user_main_httpapi(void);
int ispassive = 1;   /* v1.0: promoted to global (ota_integration dispatch reads it) */

void user_main(void)
{
    int i;

    wait_fin_init();
    TRACE("FW version: 0x%08X\n", (unsigned)OTA_FW_VERSION);
    brdcst_conf_init(COMMON_INTERFACE_SOCKET3);   /* v9.81t-b(????B): ????? SOCKET3???????????? */
    gCurWorkMode = TestFwType_ex();
    /* v9.81q: mp_init ???????json-parser ????? malloc/free??mp_pool ??????????8KB ???????*/

#ifdef _DEBUG
#if (AppDubugPrintf == 1)
    InitDegutPrintf(1, COMMON_INTERFACE_SOCKET2, "192.168.1.44", 9999);
#elif (AppDubugPrintf == 2)
    InitDegutPrintf(2, getMaxSocketId(), "192.168.1.44", 9999);
#endif
#endif

    gPRdrStaSet = malloc_hexp(sizeof(ReaderStaticSettings_ST));
    get_rdr_static_settings(gPRdrStaSet);
    gRdrErrStrBuf = malloc_hexp(100);
    TRACE("user_main gCurWorkMode:%d\n", gCurWorkMode);

    if (gCurWorkMode == WorkMode_ActVer_1 ||
        gCurWorkMode == WorkMode_ActVer_2)
    {
        gRtSetting = malloc_hexp(sizeof(ReaderRunTimeSettings_ST));
        setdef_rdr_runtime_settings(gRtSetting);

        if (gCurWorkMode == WorkMode_ActVer_1)
        {
            AppCustomParams *pAppCusParam = malloc_hexp(sizeof(AppCustomParams));

            if (get_active_mode_config(pAppCusParam) == 0)
            {
                AppCustomParams_To_static_settings(pAppCusParam, gPRdrStaSet);
                AppCustomParams_To_runtime_settings(pAppCusParam, gRtSetting);
                ispassive = 0;
            }
            else
                TRACE("get_active_mode_config error\n");

            free_hexp(pAppCusParam);
        }
        else if (gCurWorkMode == WorkMode_ActVer_2)
        {
            if (get_rdr_runtime_settings(gRtSetting) != 0)
            {
                TRACE("get_rdr_runtime_settings error\n");
                free_hexp(gRtSetting);
                gRtSetting = NULL;   /* v9.81cm: ?????????ota_usb ?????????? */
            }
            else
            {
                if (gRtSetting->upload.hw_inf != 0)
                    ispassive = 0;
            }
        }
    }

    for (i = 0; i < 4; ++i)
    {
        if (gPRdrStaSet->gpos[i] != 0)
            gpo_set(i + 1, gPRdrStaSet->gpos[i] - 1);
        else
            gpo_set(i + 1, DefNoSndGPOVal);
    }

#if Custom_By_SZBMA
		rd_idkey_fun();
    tagfiltbuff_init();

#endif
    ota_init_all();   /* v1.0: OTA boot chain (integration): ver-check + bank-confirm + USB/HTTP + dispatch */


    if (ispassive == 1)
    {
        TRACE("run user_main_passive\n");
        hw_brdcst_info()->workmode = 1;
        user_main_passive();
    }
    else
    {
        TRACE("run user_main_active\n");
        hw_brdcst_info()->workmode = 2;

        user_main_active();
    }
}

/* v9.81ch: app runtime state access interface (replaces cross-file extern) */
READER_ERR rdr_err(void) { return gRdrErr; }
void rdr_set_err(READER_ERR e) { gRdrErr = e; }
WorkMode_Code rdr_workmode(void) { return gCurWorkMode; }
int rdr_async_inv_started(void) { return gIsStartAsyncInv; }
void rdr_set_async_inv(int v) { gIsStartAsyncInv = v; }
