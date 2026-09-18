
#include "ipc.h"
#include "rfid_distance.h"
#include "EPC_TagStorage.h"
#include <stdio.h>
#include <math.h>
#include "cmsis_os2.h"
#include "tagdebug.h"
// 全局存储实例
static TagStorage tag_storage = {0};
/* v9.81ch: ph_rssi 已 static；滤波状态用 kalman_rssi_state() */

// 初始化存储系统
void TAG_Init(PhaseAvailability phase_cap)
{
    memset(&tag_storage, 0, sizeof(TagStorage));
    tag_storage.initialized = true;
    tag_storage.system_phase_capability = phase_cap;
}

// 获取频点索引
static int get_freq_index(uint32_t freq)
{
    if (freq < START_FREQ || freq > END_FREQ)
        return -1;

    uint32_t offset = freq - START_FREQ;

    if (offset % FREQ_STEP != 0)
        return -1;

    return offset / FREQ_STEP;
}

// 查找标签索引
int find_tag_index(const uint8_t* epc_bin, uint8_t epc_len)
{
    for (uint32_t i = 0; i < tag_storage.count; i++)
    {
        if (tag_storage.tags[i].epc_len == epc_len &&
                memcmp(tag_storage.tags[i].epc, epc_bin, epc_len) == 0)
        {
            return i;
        }
    }

    return -1;
}

// 更新标签访问时间
void update_access_time(uint32_t tag_index)
{
    if (tag_index < MAX_TAGS)
    {
        uint32_t current_time = osKernelGetTickCount();
        tag_storage.tags[tag_index].last_access = current_time;

        if (tag_storage.tags[tag_index].first_seen == 0)
        {
            tag_storage.tags[tag_index].first_seen = current_time;
        }
    }
}

// 查找LRU标签索引
static uint32_t find_lru_tag(void)
{
    if (tag_storage.count == 0) return 0;

    uint32_t lru_index = 0;
    uint32_t oldest_time = tag_storage.tags[0].last_access;

    for (uint32_t i = 1; i < tag_storage.count; i++)
    {
        if (tag_storage.tags[i].last_access < oldest_time)
        {
            oldest_time = tag_storage.tags[i].last_access;
            lru_index = i;
        }
    }

    return lru_index;
}


float sort_dist(float *inbuf, float *outbuf, uint8_t len )
{

    arm_sort_instance_f32 S;
    arm_sort_init_f32(&S, ARM_SORT_QUICK, ARM_SORT_DESCENDING);
    arm_sort_f32(&S, inbuf, outbuf, len);
    return outbuf[3];
}

// 实时距离计算函数（RSSI为主，相位辅助）
static float calculate_realtime_distance(const Tag_Data_f32 *tag_data)
{
    // 1. 计算RSSI距离
    float rssi_dist =  Calculate_Distance_RSSI(tag_data->rssi, tag_data->freq);

    float final_dist = Calculate_Distance_Phase(tag_data->start_phase, tag_data->freq, rssi_dist);

    return final_dist;
}

// 初始化卡尔曼滤波器
static void kalman_init_multpoint(KalmanState* state, float init_value)
{
    state->x = init_value;
    state->P = 1.0e-6f; // 初始协方差
}

// 卡尔曼滤波更新（带自适应响应）
 float kalman_update(KalmanState* state, float measurement, float Q, float R)
{
    state->P = state->P + Q;


    float K = state->P / (state->P + R);
    state->x = state->x + K * (measurement - state->x);
    state->P = (1 - K) * state->P;



    return state->x;
}

// 定义过程噪声和测量噪声
#define Q_PHASE 0.0000001f    // 相位过程噪声
#define R_PHASE 0.0001f      // 相位测量噪声
#define Q_RSSI  0.000001f    // RSSI过程噪声
#define R_RSSI  0.0001f      // RSSI测量噪声
// 添加/更新数据 (兼容当前和未来版本)
int TAG_AddData(const Tag_Data_f32 *tag_data)
{
    if (!tag_storage.initialized) return -1;

    if (!tag_data) return -5;

    uint32_t current_time = osKernelGetTickCount();
    // 从结构体获取数据
    uint8_t epc_len = tag_data->Epclen;
    const uint8_t* epc_bin = tag_data->EpcId;
    uint32_t freq = tag_data->freq;
    float start_phase = tag_data->start_phase;
    float end_phase = tag_data->end_phase;
    float rssi = tag_data->rssi;
    float timestamp = current_time;

    PhaseAvailability phase_avail = tag_data->phase_avail;

    // 验证EPC长度
    if (epc_len == 0 || epc_len > MAX_EPC_LEN)
        return -4;

    int freq_idx = get_freq_index(freq);

    if (freq_idx < 0 || freq_idx >= FREQ_COUNT)
        return -2;

    // 查找标签
    int tag_index = find_tag_index(epc_bin, epc_len);
    TagData* tag = NULL;

    if (tag_index >= 0)
    {
        tag = &tag_storage.tags[tag_index];
        //  update_access_time(tag_index);
    }
    else
    {
        // 新标签处理
        if (tag_storage.count < MAX_TAGS)
        {
            tag_index = tag_storage.count;
            tag = &tag_storage.tags[tag_index];
            memset(tag, 0, sizeof(TagData)); // 初始化新标签

            // 显式初始化最近记录字段
            tag->recent_head = 0;
            tag->recent_count = 0;

            // 初始化距离卡尔曼滤波器
            kalman_init_multpoint(&tag->distance_kalman, rssi);
            tag_storage.count++;
        }
        else
        {
            tag_index = find_lru_tag();
            tag = &tag_storage.tags[tag_index];
        }

        // 初始化新标签
        memcpy(tag->epc, epc_bin, epc_len);
        tag->epc_len = epc_len;
        tag->last_access = 0;

        /* v9.81cn: HID键盘输出已迁移至 tagInsert_wp（user_main.c 主路径）。
         * 本文件(EPC_TagStorage.c)在工程中排除编译(IncludeInBuild=0)，此处不再挂接。 */

        // 初始化所有频点的相位能力
        for (int i = 0; i < FREQ_COUNT; i++)
        {
            tag->freq_data[i].phase_capability = tag_storage.system_phase_capability;
        }

        //  update_access_time(tag_index);
    }

    // 获取频点历史
    FreqHistory* history = &tag->freq_data[freq_idx];

    // 更新相位能力（如果新数据能力更强）
    if (phase_avail > history->phase_capability)
    {
        history->phase_capability = phase_avail;
    }

    // 创建新记录
    DataRecord new_record =
    {
        .start_phase = start_phase,
        .rssi = rssi,
        .timestamp = timestamp,
        .freq = freq,
        .end_phase_valid = (phase_avail == PHASE_BOTH)
    };

    // 如果有结束相位则存储
    if (phase_avail == PHASE_BOTH)
    {
        new_record.end_phase = end_phase;
    }


    // ===== 新增：更新全局最近记录列表 =====
    if (tag->recent_count < RECENT_RECORDS)
    {
        // 列表未满，直接添加
        tag->recent_records[tag->recent_count] = new_record;
        tag->recent_count++;
    }
    else
    {
        // 列表已满，使用环形缓冲区
        tag->recent_head = (tag->recent_head + 1) % RECENT_RECORDS;
        tag->recent_records[tag->recent_head] = new_record;
    }

    if (history->count == 0)
    {
        // 初始化新频点的卡尔曼滤波器
        history->records[0] = new_record;
        history->head = 0;
        history->count = 1;
        history->filtered_phase = start_phase;
        history->filtered_rssi = rssi;
			  float deltph=phase_difference(start_phase,end_phase);
        kalman_init_multpoint(&history->kalman_phase, deltph);
        kalman_init_multpoint(&history->kalman_rssi, rssi);

    }
    else
    {


        history->head = (history->head + 1) % HISTORY_SIZE;
        history->records[history->head] = new_record;

        if (history->count < HISTORY_SIZE) history->count++;

        float deltph=phase_difference(start_phase,end_phase);
        // 更新卡尔曼滤波器
//        history->filtered_phase = kalman_update(&history->kalman_phase,
//                                                deltph,
//                                                Q_PHASE,
//                                                R_PHASE);

//        history->filtered_rssi = kalman_update(&history->kalman_rssi,
//                                               rssi,
//                                               Q_RSSI,
//                                               R_RSSI);
			

        history->last_update = current_time;

    Tag_Data_f32  newdata =
    {
        .start_phase = deltph,
        .rssi        =rssi,
        .timestamp   = timestamp,
        .freq        = freq
    };
    // 实时距离计算与更新

   // float    current_dist = calculate_realtime_distance(&newdata);
        float current_dist  = Get_Filtered_Distance(&newdata,CH_DIST);
     history->filtered_rssi = Get_Filtered_Distance(&newdata,CH_RSSI);
		history->filtered_phase = Get_Filtered_Distance(&newdata,CH_PHASE);
    if (tag->last_access == 0)
    {
        // 第一次计算，直接赋值
        history->last_distance = current_dist;
    }
    else
    {
        //   固定滤波系数（0.5）确保实时性
        float alpha = 0.05f;
        // history->last_distance = kalman_filter(&kalman_rssi_state()[1], current_dist);
        history->last_distance = history->last_distance * alpha + (1 - alpha) * current_dist;
    }
	}
    update_access_time(tag_index);
    return 0;
}

// 删除标签
int TAG_Delete(const uint8_t* epc_bin, uint8_t epc_len)
{
    if (!tag_storage.initialized) return -1;

    int tag_index = find_tag_index(epc_bin, epc_len);

    if (tag_index < 0) return -2;

    if (tag_index == (int)(tag_storage.count - 1))
    {
        tag_storage.count--;
    }
    else
    {
        memcpy(&tag_storage.tags[tag_index],
               &tag_storage.tags[tag_storage.count - 1],
               sizeof(TagData));
        tag_storage.count--;
    }

    return 0;
}

// 查询数据
const FreqHistory* TAG_Query(const uint8_t* epc_bin, uint8_t epc_len, uint32_t freq)
{
    if (!tag_storage.initialized) return NULL;

    int freq_idx = get_freq_index(freq);

    if (freq_idx < 0 || freq_idx >= FREQ_COUNT) return NULL;

    int tag_index = find_tag_index(epc_bin, epc_len);

    if (tag_index < 0) return NULL;

//    update_access_time(tag_index);
    return &tag_storage.tags[tag_index].freq_data[freq_idx];
}

// 修改指定记录
int TAG_ModifyRecord(const uint8_t* epc_bin, uint8_t epc_len, uint32_t freq, uint8_t record_idx,
                     float new_start_phase, float new_rssi)
{
    if (!tag_storage.initialized) return -1;

    int freq_idx = get_freq_index(freq);

    if (freq_idx < 0 || freq_idx >= FREQ_COUNT) return -2;

    int tag_index = find_tag_index(epc_bin, epc_len);

    if (tag_index < 0) return -4;

//    update_access_time(tag_index);
    FreqHistory* history = &tag_storage.tags[tag_index].freq_data[freq_idx];

    if (record_idx >= history->count) return -3;

    uint8_t real_idx = (history->head + HISTORY_SIZE - record_idx) % HISTORY_SIZE;
    history->records[real_idx].start_phase = new_start_phase;
    history->records[real_idx].rssi = new_rssi;

    return 0;
}

// 获取存储系统状态
TagStorage* TAG_GetStorage(void)
{
    return &tag_storage;
}

// 安全获取记录
int TAG_GetRecord(const FreqHistory* history, uint8_t index, DataRecord** record)
{
    if (!history || index >= history->count) return -1;

    uint8_t real_idx = (history->head + HISTORY_SIZE - index) % HISTORY_SIZE;
    *record = (DataRecord*)&history->records[real_idx];
    return 0;
}


// 获取标签的平均距离（跨所有频点）
float TAG_GetAverageDistance(const uint8_t* epc_bin, uint8_t epc_len)
{
    if (!tag_storage.initialized) return -1.0f;

    // 1. 查找标签索引
    int tag_index = find_tag_index(epc_bin, epc_len);

    if (tag_index < 0) return -1.0f; // 标签不存在

    TagData* tag = &tag_storage.tags[tag_index];
    // update_access_time(tag_index); // 更新访问时间

    float total_distance = 0.0f;
    int valid_freq_count = 0;
    uint32_t current_time = osKernelGetTickCount();

    // 2. 遍历所有50个频点
    for (int freq_idx = 0; freq_idx < FREQ_COUNT; freq_idx++)
    {
        FreqHistory* history = &tag->freq_data[freq_idx];

        if((current_time - history->last_update) > 500)
        {
            continue;
        }

        // 3. 检查频点数据是否有效
        if (history->count > 0 && history->last_distance > 0.0f)
        {
            total_distance += history->last_distance;
            valid_freq_count++;
        }
    }

    // 4. 计算平均值
    if (valid_freq_count > 0)
    {
        return total_distance / valid_freq_count;
    }

    return -1.0f; // 无有效距离数据
}


float TAG_GetFilteredValues(const uint8_t* epc_bin, uint8_t epc_len, uint32_t freq,
                            float* filtered_phase, float* filtered_rssi)
{
    const FreqHistory* hist = TAG_Query(epc_bin, epc_len, freq);

    if (!hist || hist->count == 0) return -1;

    *filtered_phase = hist->filtered_phase;
    *filtered_rssi  = hist->filtered_rssi;
    float dist      =	hist->last_distance;
    return dist;
}

// 获取最近10条扫描记录的平均距离
float TAG_GetRecentDistance(const uint8_t* epc_bin, uint8_t epc_len)
{
    if (!tag_storage.initialized) return -1.0f;

    // 查找标签索引
    int tag_index = find_tag_index(epc_bin, epc_len);

    if (tag_index < 0) return -1.0f; // 标签不存在

    TagData* tag = &tag_storage.tags[tag_index];

    // 没有足够的记录
    if (tag->recent_count == 0) return -1.0f;

    float total_distance = 0.0f;
    int valid_count = 0;

    // 计算最近记录的实际起始位置
    int start_index = 0;

    if (tag->recent_count == RECENT_RECORDS)
    {
        // 环形缓冲区已满，从head+1开始（最旧记录）
        start_index = (tag->recent_head + 1) % RECENT_RECORDS;
    }

    // 遍历所有最近记录
    for (int i = 0; i < tag->recent_count; i++)
    {
        int idx = (start_index + i) % RECENT_RECORDS;
        DataRecord* record = &tag->recent_records[idx];

        // 创建临时标签数据结构
        Tag_Data_f32 tag_data =
        {
            .rssi = record->rssi,
            .start_phase = record->start_phase,
            .freq = record->freq, // 使用记录中的频率
            .timestamp = record->timestamp
        };

        // 计算距离
        float distance = calculate_realtime_distance(&tag_data);

        if (distance > 0)
        {
            total_distance += distance;
            valid_count++;
        }
    }

    // 检查是否有有效距离
    if (valid_count == 0) return -1.0f;

    return total_distance / valid_count;
}

DistanceResult TAG_GetRecentDistance_Kalman(const uint8_t* epc_bin, uint8_t epc_len)
{
	  DistanceResult Result={0};
    if (!tag_storage.initialized) return Result;

    // 查找标签索引
    int tag_index = find_tag_index(epc_bin, epc_len);

    if (tag_index < 0) return Result; // 标签不存在

    TagData* tag = &tag_storage.tags[tag_index];

    // 没有足够的记录
    if (tag->recent_count == 0) return Result;

    // 定义卡尔曼滤波参数
#define DIST_Q 0.0000001f    // 距离过程噪声
#define DIST_R 0.0001f     // 距离测量噪声

    // 遍历所有最近记录并应用卡尔曼滤波
 	  //float kalman_rssi = -1.0f;
    int valid_count = 0;

    // 计算最近记录的实际起始位置
    int start_index = 0;

    if (tag->recent_count == RECENT_RECORDS)
    {
        // 环形缓冲区已满，从head+1开始（最旧记录）
        start_index = (tag->recent_head + 1) % RECENT_RECORDS;
    }

    // 遍历所有最近记录
    for (int i = 0; i < tag->recent_count; i++)
    {
        int idx = (start_index + i) % RECENT_RECORDS;
        DataRecord* record = &tag->recent_records[idx];

        // 创建临时标签数据结构
        Tag_Data_f32 tag_data =
        {
            .rssi = record->rssi,
            .start_phase = record->start_phase,
					  .end_phase   = record->end_phase,
            .freq = record->freq,
            .timestamp = record->timestamp
        };

        // 计算距离
        float distance = calculate_realtime_distance(&tag_data);
        float deltph=phase_difference(record->start_phase,record->end_phase);
       // if (Result.distance > 0)
        {
            // 应用卡尔曼滤波
					  Result.distance  += kalman_filter(&kalman_rssi_state()[1], distance);
            Result.phase     += kalman_filter(&kalman_rssi_state()[2] , deltph);
					  Result.rssi      += kalman_filter(&kalman_rssi_state()[3], record->rssi);
            valid_count++;
        }
    }

	
    // 检查是否有有效距离
    if (valid_count == 0) return Result;
		Result.distance=Result.distance/valid_count;
		Result.phase=Result.phase/valid_count;
		Result.rssi=Result.rssi/valid_count;

    return Result;
}





