#ifndef _TASK_MONITOR_H
#define _TASK_MONITOR_H

#include "stdint.h"




#define MAX_SWDT_ID 10

#define FLASHDB_SWDT_ID             (0)
#define RFID_ACTIVE_SWDT_ID         (1)
#define RFID_PASSIVE_SWDT_ID        (2)
#define MQTT_SWDT_ID                (3)
#define HTTP_SWDT_ID                (4)
#define DHCP_SWDT_ID                (5)
#define USER_MAIN_SWDT_ID           (6)
#define SEND_TAGS_SWDT_ID           (7)
#define SEND_FUNC_SWDT_ID           (8)
#define SEND_THREAD_SWDT_ID         (9)
#define TAG_UPDATE_THREAD_SWDT_ID   (10)



typedef enum {
    SWDT_STAT_IDLE =1,   
    SWDT_STAT_SUSPEND=2,
    SWDT_STAT_RUN=3     
} SWDT_STAT;


typedef struct soft_watch_dog_timer {
    uint32_t  watchDogTimeOut;  
    uint32_t  watchDogTime;      
    SWDT_STAT watchDogState;    
} SOFT_WATCH_DOG_TIMER;



void SoftWdtFed(uint8_t SwdtId);
void OSTimeTickHook(void);
void SoftWDTInit(void);
void SoftWdtISR(void);
void Task_Monitor (void *argument);
uint8_t System_guard_reg(uint8_t SwdtId, uint32_t  watchDogTimeOut,SWDT_STAT iwdgState);
#endif


