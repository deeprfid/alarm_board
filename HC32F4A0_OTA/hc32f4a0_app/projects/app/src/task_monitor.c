
#include "ipc.h"




static osMutexId_t  Task_monitor_MuxID;
static osRtxMutex_t Task_monitorMux_cb;


static SOFT_WATCH_DOG_TIMER SoftWatchDogTimerList[MAX_SWDT_ID];


static bool StopWDTFedMake = false;



#define MAX_THREADS 16

void thread_check(void)
{
    osThreadId_t threadIds[MAX_THREADS];
    // osThreadGetCount
    uint32_t threadCount = osThreadEnumerate(threadIds, MAX_THREADS);

    // osThreadTerminate(ThreadIdlinkcheck);
    for (uint32_t i = 0; i < threadCount; i++)
    {
        osThreadState_t state = osThreadGetState(threadIds[i]);

        // TRACE("Thread %d state: %s\n", i, GetThreadStateString(state));
        if ((state == osThreadTerminated) || (state == osThreadError))
        {
            TRACE("%d has terminated. Initiating system reboot...\n", state);
            TRACE("system reset >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
            osDelay(1000);
            system_reset();
        }
    }




}


uint16_t MsToOSTicks(uint16_t ms)
{

    return ms;
}


void SoftWDTInit(void)
{

    osMutexAttr_t taskmonitor_attr =
    {
        NULL,
        osMutexRecursive | osMutexPrioInherit,
        &Task_monitorMux_cb,
        sizeof(Task_monitorMux_cb)
    };
    Task_monitor_MuxID = osMutexNew(&taskmonitor_attr);
    memset(SoftWatchDogTimerList, 0x0, sizeof(SOFT_WATCH_DOG_TIMER) * MAX_SWDT_ID);
    StopWDTFedMake = false;
    // iwdg_init(IWDG_PRESCALER_64, 500);
}


void SoftWdtFed(uint8_t SwdtId)
{
    SOFT_WATCH_DOG_TIMER *SoftWatchDogTimerPtr = SoftWatchDogTimerList;

    if (SwdtId >= MAX_SWDT_ID)
    {
        return;
    }

    SoftWatchDogTimerPtr += SwdtId;


    // OS_ENTER_CRITICAL();
    osMutexAcquire(Task_monitor_MuxID, osWaitForever);

    SoftWatchDogTimerPtr->watchDogTime = SoftWatchDogTimerPtr->watchDogTimeOut;

    osMutexRelease(Task_monitor_MuxID);
    // OS_EXIT_CRITICAL();
}


void SoftWdtISR(void)
{

    SOFT_WATCH_DOG_TIMER *SoftWatchDogTimerPtr = SoftWatchDogTimerList;
    uint8_t i;

    if (StopWDTFedMake)
    {
        return;
    }

    for (i = 0; i < MAX_SWDT_ID; i++)
    {

        if (SoftWatchDogTimerPtr->watchDogState == SWDT_STAT_RUN)
        {
            if (SoftWatchDogTimerPtr->watchDogTime > 0)
            {


                osMutexAcquire(Task_monitor_MuxID, osWaitForever);
                SoftWatchDogTimerPtr->watchDogTime--;
                osMutexRelease(Task_monitor_MuxID);


            }
            else
            {
                printf("System Guard Time out, threadID=%d\r\n", i);
                StopWDTFedMake = true;
                return;
            }
        }

        SoftWatchDogTimerPtr++;
    }


    // iwdg_feed();
}


void OSTimeTickHook(void)
{
    SoftWdtISR();
}


void Task_Monitor (void *argument)
{

    SoftWDTInit();
    const uint16_t usFrequency = 200; /* 延迟周期 */
    uint32_t tick = 1;

    while(1)
    {
        thread_check();
        tick += usFrequency;
        osDelayUntil(tick);
        osThreadYield();
			// TRACE("TestingTickCount is:%d\r\n", tick++);
			// TRACE("getSysTick()      =:%lld\r\n",getSysTick());
		 	 sleep_ms(1000);
    }


}

uint8_t System_guard_reg(uint8_t SwdtId, uint32_t  watchDogTimeOut, SWDT_STAT iwdgState)
{
    if(SwdtId > MAX_SWDT_ID)
    {
        return false;
    }

    SoftWatchDogTimerList[SwdtId].watchDogTimeOut = MsToOSTicks(watchDogTimeOut);
		SoftWatchDogTimerList[SwdtId].watchDogTime    = MsToOSTicks(watchDogTimeOut);
    SoftWatchDogTimerList[SwdtId].watchDogState   = iwdgState;
    StopWDTFedMake = false;
		//SoftWdtFed(SwdtId);
    return true;
}


