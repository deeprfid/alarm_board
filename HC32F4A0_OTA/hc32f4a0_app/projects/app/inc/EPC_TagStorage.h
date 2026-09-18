#ifndef EPC_TAG_STORAGE_H
#define EPC_TAG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "rfid_distance.h"
// 配置参数
#define MAX_EPC_LEN     24      // 最大EPC长度(字节)
#define HISTORY_SIZE    5      // 每个频点的历史数据量
#define MAX_TAGS        20      // 最大标签数量
#define RECENT_RECORDS  10      // 最近记录数量
#define SPEED_OF_LIGHT 299792458.0f
// 频点范围: 902MHz(902000000) 到 928MHz(928000000), 500kHz步进
#define START_FREQ      902750000
#define END_FREQ        928000000
#define FREQ_STEP       500000
#define FREQ_COUNT      50      // 固定频点数


typedef struct {
    float x;  // 状态估计值
    float P;  // 估计误差协方差
} KalmanState;
// 相位数据可用标志
typedef enum {
    PHASE_START_ONLY = 0,   // 仅起始相位可用
    PHASE_BOTH = 1          // 起始和结束相位都可用
} PhaseAvailability;

// 单次读取的标签数据
typedef struct {
    uint8_t Epclen;            // EPC长度
    uint8_t EpcId[MAX_EPC_LEN];// EPC数据
    uint32_t freq;             // 频率（单位：Hz）
    float rssi;                // RSSI值
    float start_phase;         // 起始相位 (始终可用)
    float end_phase;           // 结束相位 (未来版本可用)
    float timestamp;           // 时间戳
	  float deltaPH;
    PhaseAvailability phase_avail; // 相位数据可用标志
} Tag_Data_f32;

// 单条数据记录
typedef struct {
    float start_phase;  // 起始相位值
    float end_phase;    // 结束相位值
    float rssi;         // RSSI值
    float timestamp;    // 时间戳
    uint32_t freq;      // 频率值（新增）
    bool end_phase_valid; // 结束相位是否有效
} DataRecord;

// 频点历史数据
typedef struct {
    DataRecord records[HISTORY_SIZE];
    uint8_t head;
    uint8_t count;
    float last_distance;
    PhaseAvailability phase_capability;
    
    // 替换原有的滤波值，改为卡尔曼状态
    KalmanState kalman_phase;  // 相位滤波状态
    KalmanState kalman_rssi;   // RSSI滤波状态
    
    // 保留滤波值用于快速访问（由卡尔曼状态计算得出）
    float filtered_phase;    
    float filtered_rssi;  
    uint32_t last_update;             // 上次更新时间 (新增)	
} FreqHistory;

// EPC标签数据结构
typedef struct {
    uint8_t epc[MAX_EPC_LEN];         // EPC二进制数据
    uint8_t epc_len;                  // EPC实际长度
    uint32_t last_access;             // 最后访问时间
    uint32_t first_seen;              // 首次发现时间
    FreqHistory freq_data[FREQ_COUNT]; // 所有频点的历史数据
    
    // 新增：全局最近扫描记录（跨所有频点）
    DataRecord recent_records[RECENT_RECORDS];    // 最近10条扫描记录
    uint8_t recent_head;              // 环形缓冲区头指针
    uint8_t recent_count;             // 当前记录数量
    // 新增：全局距离卡尔曼滤波器
    KalmanState distance_kalman;      // 距离卡尔曼滤波状态
} TagData;

// 存储系统状态
typedef struct {
    TagData tags[10];  // 标签数组
    uint32_t count;          // 当前标签数量
    bool initialized;        // 初始化标志
    PhaseAvailability system_phase_capability; // 系统相位采集能力
} TagStorage;

// 距离计算结果
typedef struct {
    float distance;        // 计算距离 (米)
    float confidence;      // 置信度 (0.0-1.0)
    uint8_t valid_freqs;   // 使用的有效频点数
    float pdoa_distance;   // PDOA计算距离 (新增)
    float rssi_distance;   // RSSI计算距离 (新增)
} DistResult;


// 初始化存储系统
void TAG_Init(PhaseAvailability phase_cap);

// 添加/更新数据
int TAG_AddData(const Tag_Data_f32 *tag_data);

// 删除标签
int TAG_Delete(const uint8_t* epc_bin, uint8_t epc_len);

// 查询数据
const FreqHistory* TAG_Query(const uint8_t* epc_bin, uint8_t epc_len, uint32_t freq);

// 修改指定记录
int TAG_ModifyRecord(const uint8_t* epc_bin, uint8_t epc_len, uint32_t freq, uint8_t record_idx,
                     float new_start_phase, float new_rssi);

// 获取存储系统状态
TagStorage* TAG_GetStorage(void);

// 安全获取记录
int TAG_GetRecord(const FreqHistory* history, uint8_t index, DataRecord** record);

int find_tag_index(const uint8_t* epc_bin, uint8_t epc_len);
void update_access_time(uint32_t tag_index);

float Get_Filtered_Distance(const Tag_Data_f32 *tag_data,uint8_t uflag);

float TAG_GetAverageDistance(const uint8_t* epc_bin, uint8_t epc_len) ;

float TAG_GetRecentDistance(const uint8_t* epc_bin, uint8_t epc_len);

DistanceResult TAG_GetRecentDistance_Kalman(const uint8_t* epc_bin, uint8_t epc_len);
// 在EPC_TagStorage.h中声明
float TAG_GetFilteredValues(const uint8_t* epc_bin, uint8_t epc_len, uint32_t freq, 
                        float* filtered_phase, float* filtered_rssi);
#endif // EPC_TAG_STORAGE_H

