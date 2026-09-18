/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief basic KV samples.
 *
 * basic Key-Value Database KV feature samples
 * get and show currnet boot counts
 */

#include <flashdb.h>

#ifdef FDB_USING_KVDB

#define FDB_LOG_TAG "[RFID][kvdb][basic]"
extern time_t rtc_time_update(char *systime);
extern bool UTCTime_Show(time_t utc, char*localshow);
void kvdb_basic_sample(fdb_kvdb_t kvdb)
{
    struct fdb_blob blob;
    int boot_count = 0;
	  fdb_err_t result = FDB_NO_ERR;
//	  char realtime[32]={0};
	  static uint32_t local_Rtc=0;
    FDB_INFO("==================== kvdb_basic_sample ====================\n");

    { /* GET the KV value */
        /* get the "boot_count" KV value */
        fdb_kv_get_blob(kvdb, "boot_count", fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));
        /* the blob.saved.len is more than 0 when get the value successful */
        if (blob.saved.len > 0) {
					FDB_INFO("boot_count :%d\n", boot_count);
        } else {
            FDB_INFO("get the 'boot_count' failed\n");
        }
    }

    { /* CHANGE the KV value */
        /* increase the boot count */
        boot_count ++;
        /* change the "boot_count" KV's value */
       result= fdb_kv_set_blob(kvdb, "boot_count", fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));
			 if(result==FDB_NO_ERR)
			 {
       // FDB_INFO("set the 'boot_count' value to %d\n", boot_count);
			 }	 
			 else
			 {
				 FDB_INFO("set the 'boot_count' failed\n");	
			 }	 
    }
		
		
		{
		
			 fdb_kv_get_blob(kvdb, "boot_time", fdb_blob_make(&blob, &local_Rtc, sizeof(local_Rtc)));
			 if (blob.saved.len > 0)
			 {
				// UTCTime_Show(local_Rtc+3600*8,realtime);
				 FDB_INFO("boot_time:%d\n",local_Rtc);
			 }
        else
 			 {
         FDB_INFO("get the 'boot_time' failed\n");
       }					
			 //local_Rtc=rtc_time_update(realtime);
			 local_Rtc+=100;
		   result= fdb_kv_set_blob(kvdb, "boot_time", fdb_blob_make(&blob, &local_Rtc, sizeof(local_Rtc)));
		    if(result==FDB_NO_ERR)
			 {
        FDB_INFO("set the 'boot_time' value to %d\n", local_Rtc);
			 }	 
			 else
			 {
				 FDB_INFO("set the 'boot_time' failed\n");	
			 }	
			 
		} 
   // FlashDB_Sync_flag=11;
    FDB_INFO("===========================================================\n");
}

#endif /* FDB_USING_KVDB */
