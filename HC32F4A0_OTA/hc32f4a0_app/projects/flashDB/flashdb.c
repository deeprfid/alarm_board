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
#include <stdio.h>

#include <flashdb.h>
#include "fdb_low_lvl.h"
//#include "bsp.h"
//#include "sfud.h"
#define FDB_LOG_TAG "[main]"

static uint32_t boot_count  = 0;
static time_t boot_timer[10] = {0, 1, 2, 3};
/* default KV nodes */
static struct fdb_default_kv_node default_kv_table[] = {
        {"username", "bma_rfid", 0}, /* string KV */
        {"password", "12345678", 0}, /* string KV */
        {"boot_count", &boot_count, sizeof(boot_count)}, /* int type KV */
        {"boot_timer" , &boot_timer , sizeof(boot_timer)},    /* int array type KV */


};
/* KVDB object */
 struct fdb_kvdb AlarmDB = { 0 };
/* TSDB object */
 struct fdb_tsdb whitelistDB = { 0 };
 struct fdb_tsdb LogDB = { 0 };
/* counts for simulated timestamp */
static fdb_time_t counts = 0;
extern void kvdb_type_string_sample(fdb_kvdb_t kvdb);
extern void kvdb_type_blob_sample(fdb_kvdb_t kvdb);
extern void tsdb_sample(fdb_tsdb_t tsdb);
extern void whitelist_index_rebuild(fdb_tsdb_t tsdb);   /* v9.82i: 白名单哈希索引重建 */
extern unsigned long long getSysTick(void);
static void lock(fdb_db_t db)
{
   // __disable_irq();
}

static void unlock(fdb_db_t db)
{
   // __enable_irq();
}

static fdb_time_t get_time(void)
{
    return ++counts;
}

int flashdb(void)
{
    fdb_err_t result;
 
#ifdef FDB_USING_KVDB
    { 
        struct fdb_default_kv default_kv;
        fdb_kvdb_deinit(&AlarmDB);
        default_kv.kvs = default_kv_table;
        default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

        fdb_kvdb_control(&AlarmDB, FDB_KVDB_CTRL_SET_LOCK, (void *)lock);
        fdb_kvdb_control(&AlarmDB, FDB_KVDB_CTRL_SET_UNLOCK, (void *)unlock);

        result = fdb_kvdb_init(&AlarmDB, "para", "fdb_kvdb1", &default_kv, NULL);

        if (result != FDB_NO_ERR) {
					
					
            return -1;
        }

        /* FIX(2026-08-14): 移除 FlashDB 官方示例 kvdb_basic_sample() 调用——
           原逻辑每次启动向生产 KVDB 写入示例键 boot_count/boot_time(local_Rtc+=100)，
           污染生产数据并损耗 Flash；如需重启计数功能请单独实现 */

    }
#endif /* FDB_USING_KVDB */

#ifdef FDB_USING_TSDB
    { 
			  fdb_time_t startT,endtime;
       // fdb_tsdb_deinit(&whitelistDB);
			 // fdb_tsdb_deinit(&LogDB);
        TRACE("[FlashDB][RFID][TSDB][whitelistDB & LogDB Init Start.......\r\n");
	      startT=getSysTick();
        fdb_tsdb_control(&whitelistDB, FDB_TSDB_CTRL_SET_LOCK  , (void *)lock);
        fdb_tsdb_control(&whitelistDB, FDB_TSDB_CTRL_SET_UNLOCK, (void *)unlock);

        result = fdb_tsdb_init(&whitelistDB, "alarm", "fdb_tsdb1", get_time, 64, NULL);
			  endtime=getSysTick();
			 
			 if (result != FDB_NO_ERR) {
            return -1;
        }
 
         fdb_tsdb_control(&whitelistDB, FDB_TSDB_CTRL_GET_LAST_TIME, &counts);

#ifdef WL_INIT_WIPE   /* v9.82j: 一次性物理清空白名单(交付初始化用)——加 #define WL_INIT_WIPE 1 后烧录重启一次即空库, 之后删掉本宏恢复正常启动 */
         TRACE("[wl] WIPE-ONCE: erasing whitelist TSDB partition...\r\n");
         {
             extern bool Double_EPC_Rmove(fdb_tsdb_t tsdb);
             /* 直接整库擦除：不保留任何旧数据(交付前从空库导 2 万) */
             fdb_tsl_clean(&whitelistDB);
             TRACE("[wl] WIPE-ONCE: done, partition empty\r\n");
         }
#endif
         whitelist_index_rebuild(&whitelistDB);   /* v9.82i: 启动时显式构建白名单哈希索引 */
         TRACE("[FlashDB][RFID][TSDB][whitelistDB Init take %f s]\r\n",(double)(endtime-startT)*0.001);

				startT=getSysTick();
				fdb_tsdb_control(&LogDB, FDB_TSDB_CTRL_SET_LOCK  , (void *)lock);
        fdb_tsdb_control(&LogDB, FDB_TSDB_CTRL_SET_UNLOCK, (void *)unlock);
        result = fdb_tsdb_init(&LogDB, "log", "fdb_tsdb2", get_time, 64, NULL);
				LogDB.last_time=counts;

        if (result != FDB_NO_ERR) {
            return -1;
        }
				endtime=getSysTick();
			 TRACE("[FlashDB][RFID][TSDB][whitelistDB Init take %f s]\r\n",(double)(endtime-startT)*0.001);
		   TRACE("[FlashDB][RFID][TSDB][whitelistDB & LogDB Init Success.......\r\n");
   }
		
	 
#endif /* FDB_USING_TSDB */

    return 0;
}


 void Check_format_flashDB(fdb_tsdb_t db)
{
  if(db->cur_sec.addr==FDB_FAILED_ADDR || db->parent.oldest_addr==FDB_FAILED_ADDR || db->cur_sec.addr>0x1000000 || db->parent.oldest_addr> 0x1000000)  // 16*1024*1024
  {
  
   fdb_tsl_clean(db); 
   //sleep(100);
  }
}
