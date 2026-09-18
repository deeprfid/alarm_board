
#include "ModuleReader.h"
#include "List.h"
#include "time.h"
#include <flashdb.h>
#include <stdio.h>
#include <string.h>
#include "alarm.h"
#include "task_monitor.h"
#include "kalman_filter.h"
#include "math.h"
#define MSG_CRC_INIT         (0xFFFF)
#define MSG_CCITT_CRC_POLY   (0x1021)
#define Eastag_SectorNum     (125)
#define EastagPage_Addr      (0xFA000)
#define QUELENTH             (32)
#define EPCIDMAXLEN          (16)

#define RED_LED_ON()      (gpo_set(3,1))
#define RED_LED_OFF()     (gpo_set(3,0))
#define GREEN_LED_ON()    (gpo_set(4,1))
#define GREEN_LED_OFF()   (gpo_set(4,0))
#define BLUE_LED_ON()     (gpo_set(6,1))
#define BLUE_LED_OFF()    (gpo_set(6,0))

#define OPTION_INIT        (1)
#define OPTION_ADD         (2)
#define OPTION_DEL         (3)

#define MATCHHEAD (1)
#define MATCHTAIL (0)

#define PRINT(window, fmt, args...) printf("{"#window"}"fmt"\n", ##args)

typedef struct
{
    uint8_t ant;
    uint8_t epclen;
    uint16_t crc;
    uint8_t epcid[EPCIDMAXLEN];
    unsigned int TimeStamp;
} MsgQueObj_alarm_tag;

typedef struct
{
    uint8_t frameHead;
    uint8_t ant;
    uint8_t epclen;
    uint16_t crc;
    uint8_t EmbededData[EPCIDMAXLEN];
    uint8_t epcid[EPCIDMAXLEN];
    unsigned int TimeStamp;
    unsigned int EmbededDatalen;
} MsgQueObj_HPM6340;

typedef struct
{
    uint8_t  frameHead;
    uint8_t  maclen;
    uint16_t datalen;
    uint8_t  cmdcode;
    uint8_t  cmdflag;
    uint8_t  macaddr[6];
    uint8_t  epclen;
    uint8_t  epc[EPCIDMAXLEN];
    uint16_t crc;
} udp_package;


typedef struct {
	float kfQ;
	float kfR;
	float kfcoeff;
	float EkfQ;
	float EkfR;
	float Ekfcoeff;
	float kalrssiQ;
	float kalrssiR;
}debug_data;


#ifndef M_PI
    #define M_PI 3.14159265358979323846f 
#endif


#define MAX_RSSI_META    (5)
#define MAX_RSSI_LEN     (5)
#define NumberofSections 2


typedef enum {
    list_NO_ERR,
    list_ADD_ERR,
	  list_ADD_NO_ERR,
    list_DEL_ERR,
	  list_DEL_NO_ERR,
    list_FINDIN,
	  list_FIND_NONE,
} list_err_t;

void tagfiltbuff_init(void);
void get_eastag_to_flash(void);
void set_eas_defualt(void);
void rd_idkey_fun(void);
int  put_tag_que(TAGINFO *tag);
void HPM6340msg_init(void);
void UDP_upload(MsgQueObj_HPM6340 *udpepc, uint8_t optioncode);
void RollBack(MsgQueObj_HPM6340 *RollBackTag);
void LED_Runing_Status(uint8_t startbit);
void UDP_SendAll(osMessageQueueId_t sendmsg, uint8_t optioncode);
void Killepctag(MsgQueObj_HPM6340 *RollBackTag);
list_err_t tagtable_list_update_with_duplicate(LTNode* phead,LTDataType epcID,uint8_t method);
void Init_whitelist_to_flash(LTNode* phead);
void tagtable_update(LTNode *phead, LTDataType epcID);
void tag_package(MsgQueObj_HPM6340 *tagobj);	
void Tag_update_thread(void *arg);
void Yjson_test_read(void);
void Yjson_test_write(void);
void SetEasBit(uint8_t *epcid, uint16_t tlen, const uint32_t easflag, bool match_head_tail);
uint8_t EASbitmatch(uint8_t *epcid, uint16_t tlen, uint32_t easflag, bool match_head_tail);
