#ifndef _EVENT_MQ_H
#define _EVENT_MQ_H
#include "ModuleReader.h"
#include "hc32f46_driver.h"

typedef enum
{
	App_Evt_None = 0,
	App_Evt_Heartbeat = 1,
	App_Evt_BatchMoment = 2,
	App_Evt_GpiChange = 3,
	App_Evt_TagComing = 4,
	App_Evt_RdrError = 5,
} App_Evt_Code;

void init_evt_sys(void);
int put_evt_que(TAGINFO *evt);
int get_evt_que(TAGINFO *evt);
void event_generator(void *arg);

/* v9.81ch: event flag access interface (replaces cross-file extern) */
#define EVT_FLAG_TAGREAD     1
#define EVT_FLAG_TAGCOMING   2
#define EVT_FLAG_HEARTBEAT   3
#define EVT_FLAG_SENDTAGONLY 4
#define EVT_FLAG_EMPTYDATA   5
#define EVT_FLAG_SYNCTIMEREQ 6
#define EVT_FLAG_GPICHAN     7
#define EVT_FLAG_SENDRDRERR  8
uint8_t evt_is_enabled(int evt_flag);
uint64_t evt_hb_last_time(void);
void evt_set_hb_last_time(uint64_t t);
uint64_t evt_batch_last_time(void);
void evt_set_batch_last_time(uint64_t t);


#endif


