

#include "hc32f46_driver.h"
#include "ModuleReader.h"
#include "List.h"

#define PDUHEAD              (0xFF)




typedef struct  
     {
         char warehouseCode[32];
         char warehouseType[32];
         char deviceCode   [32];
         char deviceType   [32];
		     char deviceModel  [32];
		     char deviceBrand  [32];
		     char deviceSn     [32];
		     char machineCode  [32];
				 char imei         [32];
				 char deviceIp     [32];
				 char softName     [32];
				 char softVersion  [32];
				 char empCode      [32];
			   char macAddress   [32];
			   char remark       [32];
			   char time         [32]; 
			 	 char deviceName   [32];
			   char heartbeatReportUrl[64];
			   char heartbeatIntervalTime[4];
			   int   crc;
      } HeartBeat;
		 
typedef struct  
	{
		char host[32];
		char port[32];
		char user_name[32];
		char user_pwd[32];
		char whitelist_topic[64];
    char showtag_topic[64];
    HeartBeat system_mqtt_heartbeat;
	}mqttcfg;	

typedef struct  
	{
         uint8_t pdulen[2];
         uint8_t frameHead[4];
         uint8_t datafieldlen;
         uint8_t appcommand;
         uint8_t registered[64];
         uint8_t crc[2];
         uint8_t random_forest[4];
    } lkt_pdu;  

 typedef struct  
	{
         unsigned char FrameHead;
         unsigned char Pdu_len;
         unsigned char DeviceID;
         unsigned char AntID;
         unsigned char Alarm_Duration[6];
         uint16_t  Radarcfg[5];
         uint32_t  time_stamp;
         uint32_t  random_forest;
         uint16_t  reserved;
         uint16_t  crc;
        } alarm_pdu;
	
void ipc_lkt_message(uint8_t *SBuffer, uint8_t slen, uint8_t *RBuffer);
void ipc_hpm_message(uint8_t *upload, uint8_t dlen,uint8_t antid);	
void AppTaskCreate (void);		
