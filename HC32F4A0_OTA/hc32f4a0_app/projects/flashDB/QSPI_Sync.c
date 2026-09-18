/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-05-31     armink       first version
 */
#include "ipc.h"




#define  FlashDB_LEN_MAX    (16 * 1024 * 1024)


uint32_t  FlashDB_Sync_flag=0;

extern  void save_tag_flashDB(fdb_kvdb_t kvdb, LTDataType  epcid,uint8_t method);
extern  void save_tag_TSDB(fdb_tsdb_t tsdb, LTNode* plist,uint8_t method);
extern  uint32_t Get_whitetags_total_count(LTNode* phead);
LTDataType Getlist_tag(LTNode* phead,uint32_t index);
extern  struct fdb_tsdb whitelistDB;
extern uint8_t tag_method;
extern void Del_Tag_TSDB(fdb_tsdb_t tsdb,LTNode* plist,uint8_t method);
extern LTNode* taglist;
extern LTNode *ADDlist;
extern LTNode *DELlist;
extern bool Double_EPC_Rmove(fdb_tsdb_t tsdb);
extern void whitelist_index_rebuild(fdb_tsdb_t tsdb);   /* v9.82i: 白名单 64 位哈希索引重建 */

void Transfer_EPC_from_list_to_kvDB(fdb_kvdb_t kvdb,uint8_t method)
{
	uint8_t uflag=method;
	LTDataType  epcid;
	uint32_t tagcnt;

	tagcnt=Get_whitetags_total_count(taglist);
	TRACE("[FlashDB][RFID][TSDB][before insert FlashDB total tags=%d]\r\n",tagcnt);
	for(uint32_t i=0;i<tagcnt;i++)
	{
	epcid= Getlist_tag(taglist,i+1);	
  save_tag_flashDB(kvdb,epcid,uflag);

  }

	tagcnt=Get_whitetags_total_count(taglist);
	TRACE("[FlashDB][RFID][TSDB][after insert FlashDB total tags=%d]\r\n",tagcnt);
}	

void Transfer_EPC_from_list_to_TSDB(fdb_tsdb_t tsDB,LTNode* plist,uint8_t method)
{
	uint8_t  uflag=method;
  uint32_t startT,endtime;


	startT=getSysTick();
	
	if(uflag==OPTION_ADD)
	{	
    save_tag_TSDB(tsDB,plist,uflag);
		endtime=getSysTick();
	  TRACE("[FlashDB][RFID][TSDB][Its take %f s][OPTION_ADD]\r\n",(double)(endtime-startT)*0.001);
		return;
	}	
	
	if(uflag==OPTION_DEL)
	{
		Del_Tag_TSDB(tsDB,plist,uflag);
		endtime=getSysTick();
	  TRACE("[FlashDB][RFID][TSDB][Its take %f s][OPTION_DEL]\r\n",(double)(endtime-startT)*0.001);
		return;
	}	
  

	
}	

void Transfer_EPC_from_list_to_RL_Flash(LTNode* plist,uint8_t method)
{
	uint8_t  uflag=method;
  uint32_t startT,endtime;


	startT=getSysTick();
	
	if(uflag==OPTION_ADD)
	{	
    //save_tag_RL_Flash(plist,uflag);
	}	
	
	if(uflag==OPTION_DEL)
	{
		//Del_Tag_TSDB(tsDB,uflag);
	}	
  endtime=getSysTick();
	TRACE("[FlashDB][RFID][TSDB][Its take %f s]\r\n",(double)(endtime-startT)*0.001);

	
}	
void Check_Buffer_Diff(uint8_t flag)
{
	 extern rfidcfg  mycfgdata;
	 static uint32_t timestamp=0;

   if(FlashDB_Sync_flag>10)
	 {
		 FlashDB_Sync_flag=0;
		 Transfer_EPC_from_list_to_TSDB(&whitelistDB,ADDlist,OPTION_ADD);
     Transfer_EPC_from_list_to_TSDB(&whitelistDB,DELlist,OPTION_DEL);
		 whitelist_index_rebuild(&whitelistDB);   /* v9.82i: ADD/DEL 落盘后重建索引 */
	   TRACE("[FlashDB][RFID][TSDB][SDRAM<-->QSPI FLASH SYNC SUCCESS]\r\n");
 
	 }
   if(timestamp++>1080000)//30X60X6 hours  cycle=2s
   {
     timestamp=0;	   
		// Double_EPC_Rmove(&whitelistDB);
	 }		 

}	








