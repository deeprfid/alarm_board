#ifndef _READER_MSG_H_
#define _READER_MSG_H_
#include <time.h>
#include "hc32f46_driver.h"
#include "ModuleReader.h"
#include "app_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* v9.81ch: HeartBeatData_ST 定义在此（C 头自包含；Utility.h 同定义受 _HB_DATA_DEFINED 保护） */
#ifndef _HB_DATA_DEFINED
#define _HB_DATA_DEFINED
typedef struct
{
    unsigned char main_board;
    unsigned char rfid_mod;
    unsigned char software_version[4];
    unsigned char antcount;
    unsigned char connected_antennas[16];
    unsigned int hb_count;
} HeartBeatData_ST;
#endif

void RemoteCmd(void);
//void send_error(void);
//void send_pulse(int justpoweron);
int AddTag2SockBuffer(unsigned char *SBuffer, TAGINFO *tag, int pos);
int AddTag2SockBuffer_j(char *Jbuf, TAGINFO *tag);
void AddTagCnt2SockBuffer(unsigned char *SBuffer, int tagcnt, int pos);
void SetMsgDatalen(unsigned char *SBuffer, int totallen);
int AddMsgHeader2SockBuffer(unsigned char *SBuffer, MidMsgType mtype, int ecode);
int AddMsgHeader2SockBuffer_j(char *Jbuf, MidMsgType mtype);
void up_send(unsigned char *SBuffer, int dlen);
int write_n(int fd, void *buf, int len);
void CheckServerConnection(void);
int GpiChange(uint8 *gsts);
void send_evt_gpichan(uint8 state);
void send_evt_heartbeat(void);
void send_evt_tagcoming(TAGINFO *tag);
void send_evt_reader_err(void);
void send_evt_emptydata(void);
void send_evt_synctimereq(void);
void send_evt_tagbatch(void);


/* v9.81ch: network/heartbeat access interface (replaces cross-file extern) */
int  net_is_connected(void);
void net_set_connected(int c);
uint8 *net_get_ser_ip(void);
uint16 net_get_ser_port(void);
uint16 *net_get_ser_port_ptr(void);
void net_set_ser_port(uint16 p);
char *net_get_msg_ipstr(void);
char *net_get_msg_macstr(void);
HeartBeatData_ST *hb_data(void);

extern unsigned char gGpiMap;
extern volatile uint32 gUtcSecBase;
extern volatile uint32 gSysSecBase;

void seconds_to_date(time_t secs, char *date);
int date_to_seconds(char *date, uint32 *secs);
void reset_uart1_ex_dev(uint16 *failcnt);

#if Custom_By_Caipan
#endif

#ifdef __cplusplus
}
#endif
#endif


