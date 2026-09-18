/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief TSDB samples.
 *
 * Time series log (like TSDB) feature samples source file.
 *
 * TSL is time series log, the TSDB saved many TSLs.
 */
#include "ipc.h"
#include <flashdb.h>
#include <string.h>
#include "List.h"
#ifdef FDB_USING_TSDB

#define FDB_LOG_TAG "[RFID][TSDB]"

#ifdef FDB_USING_TIMESTAMP_64BIT
    #define __PRITS "lld"
#else
    #define __PRITS "d"
#endif

struct env_status
{
    int temp;
    int humi;
};

bool query_cb(fdb_tsl_t tsl, void *arg);
static bool query_by_time_cb(fdb_tsl_t tsl, void *arg);
bool set_status_cb(fdb_tsl_t tsl, void *arg);
extern  struct fdb_tsdb whitelistDB;
extern list_err_t tagtable_list_update_with_duplicate(LTNode* phead, LTDataType epcID, uint8_t method);
uint32_t Match_EPC_inTSDB(fdb_tsdb_t tsdb, uint8_t *input_tag, uint16_t tlen);   /* v9.82j: save_tag_TSDB 去重前置声明 */

extern osMutexId_t uploadMuxID;   /* v9.82j: ADDlist/DELlist 释放与 reader tagmatching 遍历互斥(防 use-after-free HardFault) */
void Init_list_clear(LTNode *phead)
{
    osMutexAcquire(uploadMuxID, osWaitForever);
    LTClear(phead);
    osMutexRelease(uploadMuxID);
}

void tsdb_sample(fdb_tsdb_t tsdb)
{
    struct fdb_blob blob;

    FDB_INFO("==================== tsdb_sample ====================\n");

    {
        /* APPEND new TSL (time series log) */
        struct env_status status;

        /* append new log to TSDB */
        status.temp = 36;
        status.humi = 85;
        fdb_tsl_append(tsdb, fdb_blob_make(&blob, &status, sizeof(status)));
        FDB_INFO("append the new status.temp (%d) and status.humi (%d)\n", status.temp, status.humi);

        status.temp = 38;
        status.humi = 90;
        fdb_tsl_append(tsdb, fdb_blob_make(&blob, &status, sizeof(status)));
        FDB_INFO("append the new status.temp (%d) and status.humi (%d)\n", status.temp, status.humi);
    }

    FDB_INFO("===========================================================\n");
}

uint32_t Query_all_TSL_in_TSDB_bytime(fdb_tsdb_t tsdb, fdb_tsl_status_t TSLstatus)
{
    /* QUERY the TSDB by time */
    /* prepare query time (from 1970-01-01 00:00:00 to 2020-05-05 00:00:00) */
    time_t  from_time = 0;
    time_t  to_time = tsdb->last_time;
    uint32_t count;
    /* query all TSL in TSDB by time */
    fdb_tsl_iter_by_time(tsdb, from_time, to_time, query_by_time_cb, tsdb);
    /* query all FDB_TSL_WRITE status TSL's count in TSDB by time */
    count = fdb_tsl_query_count(tsdb, from_time, to_time, TSLstatus);
    FDB_INFO("[query total count is: %zu]\n", count);
    return 	count;
}

uint32_t Get_Total_tsl_TSDB_bytime(fdb_tsdb_t tsdb, fdb_tsl_status_t TSLstatus)
{
    /* QUERY the TSDB by time */
    /* prepare query time (from 1970-01-01 00:00:00 to 2020-05-05 00:00:00) */
//        struct tm tm_from = { .tm_year = 1970 - 1900, .tm_mon = 0, .tm_mday =  1, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
//        struct tm tm_to   = { .tm_year = 2024 - 1900, .tm_mon = 9, .tm_mday = 25, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
//        time_t from_time  = mktime(&tm_from), to_time = mktime(&tm_to);
    uint32_t count;
    time_t  from_time = 0;
    time_t  to_time = tsdb->last_time;
    count = fdb_tsl_query_count(tsdb, from_time, to_time, TSLstatus);
    FDB_INFO("[Query total count is: %zu]\n", count);
    return 	count;
}

bool query_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    struct env_status status;
    fdb_tsdb_t db = arg;

    fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &status, sizeof(status))));
    FDB_INFO("[query_cb][queried a TSL: time: %" __PRITS ", temp: %d, humi: %d]\n", tsl->time, status.temp, status.humi);

    return false;
}

static bool query_by_time_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    LTDataType  epcid;
    fdb_tsdb_t db = arg;
    char epcstr[64];

    if(tsl->status == FDB_TSL_WRITE)
    {
        fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &epcid, sizeof(epcid))));
        Hex2Str(epcid.epc, epcid.Epclen, epcstr);
        FDB_INFO("[query_by_time_cb][queried a TSL: time: %" __PRITS ", epcID: %s]\r\n", tsl->time, epcstr);
    }

    return false;
}

bool set_status_cb(fdb_tsl_t tsl, void *arg)
{
    fdb_tsdb_t db = arg;

    FDB_INFO("[set the TSL (time %" __PRITS ") status from %d to %d]\n", tsl->time, tsl->status, FDB_TSL_USER_STATUS1);
    fdb_tsl_set_status(db, tsl, FDB_TSL_USER_STATUS1);

    return false;
}

extern  uint32_t Get_whitetags_total_count(LTNode* phead);
LTDataType Getlist_tag(LTNode* phead, uint32_t index);
extern LTNode* taglist;
extern LTNode* staticlist;
extern LTNode *ADDlist;
extern LTNode *DELlist;

/* v9.81ce: 前向声明（哈希实现定义在 set_del_status_cb 之后） */
static int del_hash_find(LTDataType *e);

static bool set_del_status_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    fdb_tsdb_t db = arg;
    LTDataType epcidtsdb;

    fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &epcidtsdb, sizeof(epcidtsdb))));

    if(blob.saved.len > 0)
    {

    }

    if(del_hash_find(&epcidtsdb) != 0)
    {
        fdb_tsl_set_status(db, tsl, FDB_TSL_DELETED);
    }

    return false;
}

/* ============ v9.81ce: DELlist 精确哈希（删除提速，替代 LTFind 线性查找） ============ */
#define DEL_HASH_SIZE 512
typedef struct del_hash_node {
    LTDataType data;
    struct del_hash_node *next;
} del_hash_node_t;

static del_hash_node_t *g_del_hash[DEL_HASH_SIZE];

static uint32_t del_hash_key(LTDataType *e)
{
    uint32_t h = e->Epclen + 7;
    int i;
    for (i = 0; i < e->Epclen; i++)
        h = h * 31 + e->epc[i];
    return h % DEL_HASH_SIZE;
}

static void del_hash_build(LTNode *plist)
{
    LTNode *cur;
    osMutexAcquire(uploadMuxID, osWaitForever);   /* v9.82j: 遍历 DELlist 与并发增/清互斥 */
    memset(g_del_hash, 0, sizeof(g_del_hash));
    cur = plist->next;
    while (cur != plist)
    {
        del_hash_node_t *n = (del_hash_node_t *)malloc(sizeof(del_hash_node_t));
        if (n != NULL)
        {
            n->data = cur->data;
            n->next = g_del_hash[del_hash_key(&cur->data)];
            g_del_hash[del_hash_key(&cur->data)] = n;
        }
        cur = cur->next;
    }
    osMutexRelease(uploadMuxID);
}

static void del_hash_free(void)
{
    int i;
    for (i = 0; i < DEL_HASH_SIZE; i++)
    {
        del_hash_node_t *n = g_del_hash[i];
        while (n != NULL)
        {
            del_hash_node_t *t = n;
            n = n->next;
            free(t);
        }
        g_del_hash[i] = NULL;
    }
}

static int del_hash_find(LTDataType *e)
{
    del_hash_node_t *n = g_del_hash[del_hash_key(e)];
    while (n != NULL)
    {
        if (n->data.Epclen == e->Epclen &&
            memcmp(n->data.epc, e->epc, e->Epclen) == 0)
            return 1;
        n = n->next;
    }
    return 0;
}

/* v9.81ce: 软删除全标记——cleartag 用（不擦 FlashDB 分区，保留结构/磨损均衡） */
bool del_all_cb(fdb_tsl_t tsl, void *arg)
{
    fdb_tsdb_t db = arg;

    if (tsl->status == FDB_TSL_WRITE)
    {
        fdb_tsl_set_status(db, tsl, FDB_TSL_DELETED);
    }

    return false;
}


static bool createlist_query_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    LTDataType epcID;
    fdb_tsdb_t db = arg;

    /* v9.81ce: 先查状态——跳过非 WRITE（旧 DELETED 记录不读 blob，避免垃圾数据/拖慢遍历） */
    if(tsl->status != FDB_TSL_WRITE)
    {
        return false;
    }

    fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &epcID, sizeof(epcID))));

    if(blob.saved.len > 0)
    {
        /* v9.81ce: Epclen 越界防护（TSDB 混有旧版本 blob 时 Epclen 可能异常 → Hex2Str 越界读乱码） */
        if(epcID.Epclen > 0 && epcID.Epclen <= 16)
        {
            if(list_FINDIN == tagtable_list_update_with_duplicate(staticlist, epcID, OPTION_ADD))
            {
                fdb_tsl_set_status(db, tsl, FDB_TSL_DELETED);
            }
        }
    }

    return false;
}


void save_tag_TSDB(fdb_tsdb_t tsdb, LTNode* plist, uint8_t method)
{
    struct fdb_blob blob;
    uint32_t    tagcnt;
    LTDataType  epcid;
    uint8_t     uflag = method;

    tagcnt = Get_whitetags_total_count(plist);

//	tsdb_tarversal_total(tsdb);
    if(uflag == OPTION_ADD)
    {
        for(uint32_t i = 0; i < tagcnt; i++)
        {
            epcid = Getlist_tag(plist, i + 1);
            /* v9.82j: 跨推送去重——标签已在库(上次重建的索引命中)则跳过，重复推送不再累积 WRITE 记录 */
            if (Match_EPC_inTSDB(tsdb, epcid.epc, epcid.Epclen) != 0)
                continue;
            fdb_tsl_append(tsdb, fdb_blob_make(&blob, &epcid, sizeof(epcid)));
        }

        Init_list_clear(plist);
        TRACE("[FlashDB][RFID][TSDB][insert: %d tags\r\n", tagcnt);
    }

}

void Del_Tag_TSDB(fdb_tsdb_t tsdb,LTNode* plist,uint8_t method)
{

    uint32_t tagcnt = Get_whitetags_total_count(plist);
	
	  if(tagcnt<=0)  return;
			

    if(method == OPTION_DEL)
    {
        /* v9.81ce: 精确删除提速——DELlist 构建哈希表，set_del_status_cb O(1) 查找
         * (原 LTFind 线性 4001x4001=1600 万次 memcmp -> 13s; 按 EPC 内容精确匹配，不误删) */
        del_hash_build(plist);
        fdb_tsl_iter(tsdb, set_del_status_cb, tsdb);
        del_hash_free();
    }

    Init_list_clear(plist);
    TRACE("[FlashDB][RFID][TSDB][delete: %d tags\r\n", tagcnt);
}

/* ==== v9.82i: 白名单 64 位哈希索引（解决 Match_EPC_inTSDB 全遍历 O(n) 慢） ====
 * key = (fdb_calc_crc32(0,epc,len) << 32) | fdb_calc_crc32(0xFFFFFFFF,epc,len)
 * 64 位碰撞率≈0（2万条期望 1e-11 对）；查找二分 O(log n)，不碰 flash。
 * 构建：whitelist_index_rebuild() 遍历 TSDB 一次性建索引（启动/变更后调用）。 */
#define WL_INDEX_MAX   (20000)

static uint64_t *s_wl_key = NULL;   /* 64 位 key 排序数组（按实际白名单条数动态扩容，最高 WL_INDEX_MAX） */
static uint32_t  s_wl_cnt = 0;
static uint32_t  s_wl_cap = 0;      /* v9.82j: 当前索引容量（条数），随白名单增长按需扩容 */

static uint64_t wl_calc_key(const uint8_t *epc, uint16_t len)
{
    uint32_t crc1 = fdb_calc_crc32(0x00000000UL, epc, (size_t)len);
    uint32_t crc2 = fdb_calc_crc32(0xFFFFFFFFUL, epc, (size_t)len);
    return ((uint64_t)crc1 << 32) | (uint64_t)crc2;
}

static void wl_key_qsort(uint64_t *a, int lo, int hi)
{
    if (lo >= hi) return;
    uint64_t pivot = a[(lo + hi) >> 1];
    int i = lo, j = hi;
    while (i <= j) {
        while (a[i] < pivot) i++;
        while (a[j] > pivot) j--;
        if (i <= j) { uint64_t t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
    }
    if (lo < j) wl_key_qsort(a, lo, j);
    if (i < hi) wl_key_qsort(a, i, hi);
}

/* v9.82j: 索引容量不足时扩容（翻倍），避免开机/导入期预留 160KB 挤占堆 */
static void wl_index_grow(uint32_t need)
{
    uint32_t ncap;
    uint64_t *np;

    if (need <= s_wl_cap)
        return;
    ncap = (s_wl_cap == 0) ? 512 : s_wl_cap;
    while (ncap < need && ncap < WL_INDEX_MAX)
        ncap <<= 1;
    if (ncap > WL_INDEX_MAX)
        ncap = WL_INDEX_MAX;
    if (ncap <= s_wl_cap)
        return;   /* 已达硬上限，只能截断 */
    np = (uint64_t *)malloc((size_t)ncap * sizeof(uint64_t));
    if (np == NULL)
    {
        TRACE("[wl] index grow FAIL %luB\r\n", (unsigned long)((size_t)ncap * sizeof(uint64_t)));
        return;
    }
    if (s_wl_key != NULL)
        memcpy(np, s_wl_key, (size_t)s_wl_cnt * sizeof(uint64_t));
    __disable_irq();          /* v9.82j: 与 Match_EPC_inTSDB 二分查找互斥——换指针+释放旧缓冲期间 reader 不可能在读它 */
    {
        uint64_t *old = s_wl_key;
        s_wl_key = np;
        s_wl_cap = ncap;
        if (old != NULL)
            free(old);
    }
    __enable_irq();
}

static uint32_t s_iter_cnt = 0;    /* v9.82i 诊断: 遍历到的 TSL 总数 */
static uint32_t s_iter_write = 0;   /* v9.82i 诊断: WRITE 状态数 */

static bool wl_index_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    LTDataType epc;
    (void)arg;
    s_iter_cnt++;
    if ((s_iter_cnt & 0x1FF) == 0)   /* v9.82j: 每 512 条喂狗——全库含垃圾可达 8 万+条，遍历 ~1.5s，不喂会触发 FlashDB_Task 的 2s 看门狗复位 */
        SoftWdtFed(FLASHDB_SWDT_ID);
    if (tsl->status == FDB_TSL_WRITE) s_iter_write++;
    if (tsl->status != FDB_TSL_WRITE) return false;
    fdb_blob_read((fdb_db_t)&whitelistDB, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &epc, sizeof(epc))));
    if (blob.saved.len > 0 && epc.Epclen > 0 && epc.Epclen <= 16) {
        wl_index_grow(s_wl_cnt + 1);
        if (s_wl_cnt < s_wl_cap)
            s_wl_key[s_wl_cnt++] = wl_calc_key(epc.epc, epc.Epclen);
    }
    return false;
}

void whitelist_index_rebuild(fdb_tsdb_t tsdb)
{
    if (s_wl_key == NULL)
        wl_index_grow(256);   /* 空表也建最小缓冲，避免 Match_EPC 每次都触发懒重建全遍历 */
    s_wl_cnt = 0;
    s_iter_cnt = 0; s_iter_write = 0;
    fdb_tsl_iter(tsdb, wl_index_cb, NULL);
    if (s_wl_cnt > 1) wl_key_qsort(s_wl_key, 0, (int)s_wl_cnt - 1);
    /* v9.82j: 去重——同一 EPC 被重复推送时 TSDB 有多条 WRITE，排序后相邻相同 key 压缩，索引只留唯一标签 */
    if (s_wl_cnt > 1)
    {
        uint32_t u = 1;
        for (uint32_t r = 1; r < s_wl_cnt; r++)
        {
            if (s_wl_key[r] != s_wl_key[u - 1])
                s_wl_key[u++] = s_wl_key[r];
        }
        s_wl_cnt = u;
    }
    TRACE("[wl] index built: %u unique tags (iter=%u write=%u)\r\n", (unsigned)s_wl_cnt, (unsigned)s_iter_cnt, (unsigned)s_iter_write);
}

uint32_t Match_EPC_inTSDB(fdb_tsdb_t tsdb, uint8_t *input_tag, uint16_t tlen)
{
    uint64_t key;
    int lo, hi, mid;
    uint32_t cnt;
    int result = 0;

    /* v9.82i: 懒构建——启动后无显式加载路径，首次查找时自动建索引（一次性全遍历，之后走二分） */
    if (s_wl_key == NULL)
        whitelist_index_rebuild(tsdb);

    key = wl_calc_key(input_tag, tlen);
    __disable_irq();          /* v9.82j: 关中断做二分——wl_index_grow/rebuild 释放旧缓冲时本任务不可能正在读它 */
    cnt = s_wl_cnt;
    if (s_wl_key != NULL && cnt > 0)
    {
        lo = 0;
        hi = (int)cnt - 1;
        while (lo <= hi)
        {
            mid = (lo + hi) >> 1;
            if (s_wl_key[mid] == key) { result = 1; break; }
            if (s_wl_key[mid] < key) lo = mid + 1;
            else hi = mid - 1;
        }
    }
    __enable_irq();
    return (uint32_t)result;
}







extern    rfidcfg  mycfgdata;
uint32_t tsdb_tarversal_total(fdb_tsdb_t tsdb)
{

    uint32_t  counttag = 0;
    LTClear(staticlist);
    fdb_tsl_iter(tsdb, createlist_query_cb, tsdb);
    counttag = Get_whitetags_total_count(staticlist);
    mycfgdata.totaltagcnt = counttag;
    /* v9.82i: 不在此重建索引——readtag(获取)也调本函数，重建会每次获取都全遍历；
     * 索引只在启动(flashdb.c) + 增删落盘后(Check_Buffer_Diff)重建 */
    return counttag;
}





extern void Transfer_EPC_from_list_to_TSDB(fdb_tsdb_t tsDB, LTNode* plist, uint8_t method);
extern void Init_whitelist_to_flash(LTNode* phead);
bool Double_EPC_Rmove(fdb_tsdb_t tsdb)
{
    uint32_t unique_epc_count = 0;
    uint32_t overlap_epc_count = 0, deletedTags = 0;
    extern uint32_t FlashDB_Sync_flag;
    unique_epc_count  = tsdb_tarversal_total(tsdb);
    mycfgdata.totaltagcnt = unique_epc_count;
    overlap_epc_count = Get_Total_tsl_TSDB_bytime(tsdb, FDB_TSL_WRITE);
    deletedTags       = Get_Total_tsl_TSDB_bytime(tsdb, FDB_TSL_DELETED);
    FDB_INFO("[FlashDB  Duplicated      : %d] \r\n", overlap_epc_count);
    FDB_INFO("[FlashDB staticlist rawtag: %d] \r\n", unique_epc_count);
    FDB_INFO("[FlashDB       deletedTags: %d] \r\n", deletedTags);

    if((overlap_epc_count + deletedTags) - unique_epc_count > 2000)
    {
        fdb_tsl_clean(tsdb);
        Transfer_EPC_from_list_to_TSDB(tsdb, staticlist, OPTION_ADD);
        Init_whitelist_to_flash(taglist);
        FlashDB_Sync_flag = 11;
        TRACE("[FlashDB][RFID][TSDB][Duplicated tag removed]\r\n");
        return true;
    }
    else
    {
        return false;
    }
}

#endif /* FDB_USING_TSDB */
