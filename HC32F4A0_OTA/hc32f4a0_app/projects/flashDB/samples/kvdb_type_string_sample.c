/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief string KV samples.
 *
 * Key-Value Database string type KV feature samples source file.
 */

#include <flashdb.h>
#include <string.h>
#include <stdio.h>

#ifdef FDB_USING_KVDB

#define FDB_LOG_TAG "[RFID][kvdb][string]"
extern time_t rtc_time_update(char *systime);

void kvdb_type_string_sample(fdb_kvdb_t kvdb)
{ 
			char realtime[32]={0};
	     uint32_t utc_time_stamp=rtc_time_update(realtime);
		   time_t endtime=0;
    FDB_INFO("==================== kvdb_type_string_sample ====================\n");

    { /* CREATE new Key-Value */
        //char temp_data[10] = "36C";
        char asc[8]={0};

        /* It will create new KV node when "temp" KV not in database. */
				
			 for(uint32_t i=0;i<10;i++)
			{
				sprintf(asc,"%d",i);
        fdb_kv_set(kvdb, asc, asc);
       // FDB_INFO("create the 'temp' string KV, value is: %s\n", temp_data);
			}	
    }
     endtime= rtc_time_update(realtime);   
		 printf("set time=%d\n",endtime-utc_time_stamp);
    { /* GET the KV value */
        char *return_value, temp_data[10] = { 0 };

        /* Get the "temp" KV value.
         * NOTE: The return value saved in fdb_kv_get's buffer. Please copy away as soon as possible.
         */
				 char asc[8]={0};
        /* It will create new KV node when "temp" KV not in database. */
				 utc_time_stamp=rtc_time_update(realtime);
			 for(uint32_t i=0;i<10;i++)
			{
				sprintf(asc,"%d",i);
        return_value=fdb_kv_get(kvdb, asc);
        //FDB_INFO("create the 'temp' string KV, value is: %s\n", temp_data);
			}	
			
         endtime= rtc_time_update(realtime);   
		 printf("end time=%d\n",endtime-utc_time_stamp);
        /* the return value is NULL when get the value failed */
        if (return_value != NULL) {
            strncpy(temp_data, return_value, sizeof(temp_data));
            FDB_INFO("get the 'temp' value is: %s\n", temp_data);
        }
    }

    { /* CHANGE the KV value */
        char temp_data[10] = "38C";

        /* change the "temp" KV's value to "38.1" */
        fdb_kv_set(kvdb, "temp", temp_data);
        FDB_INFO("set 'temp' value to %s\n", temp_data);
    }

    { /* DELETE the KV by name */
        fdb_kv_del(kvdb, "temp");
        FDB_INFO("delete the 'temp' finish\n");
    }

    FDB_INFO("===========================================================\n");
}

#endif /* FDB_USING_KVDB */
