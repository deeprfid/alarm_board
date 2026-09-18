/**
 ****************************************************************************************************
 * @file        rng.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-06-12
 * @brief       随机数发生器驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 北极星 H750开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __TAGDEBUG_H__
#define __TAGDEBUG_H__


#include "arm_math.h"
#include "math.h"
// 频段配置
// 物理常量定义

#define NUM_FREQ 50  // 902-928MHz频点数
#define LPC_ORDER 4  // 线性预测阶数
#define AUTOCORR_LAGS 10  // 自相关滞后点数



void iirfilter(float *xin, float *yout, float coff[][3], uint32_t length);
void coff_buff_init(void);

#define C_LIGHT 299792458.0f         // 光速 (m/s)
#define NUM_FREQ_POINTS 5            // 每次测量使用的频点数
#define MAX_MEASUREMENTS 10         // 最大存储的测量值
#define KALMAN_Q 0.01f               // 过程噪声协方差
#define KALMAN_R 0.1f                // 测量噪声协方差
#define RSSI_TX_POWER -18.0f         // 1米处的参考RSSI (dBm)
#define RSSI_PATH_LOSS_EXPONENT 2.5f // 路径损耗指数

// RFID测量数据结构
typedef struct {
    float frequency;  // 频率 (MHz)
    float phase;      // 相位 (rad)
    float rssi;       // RSSI (dBm)
    uint32_t timestamp; // 时间戳
} RFID_Measurement;

// 卡尔曼滤波器结构
typedef struct {
    float q;      // 过程噪声协方差
    float r;      // 测量噪声协方差
    float x;      // 估计值
    float p;      // 估计误差协方差
    float k;      // 卡尔曼增益
} KalmanFilter;

// 定位系统结构
typedef struct {
    RFID_Measurement measurements[NUM_FREQ_POINTS];
    KalmanFilter distance_filter;
    float last_distance;
    float distance_history[MAX_MEASUREMENTS];
    uint16_t history_index;
    float path_loss_exponent; // 自适应路径损耗指数
} PositioningSystem;

// 函数声明
void Positioning_Init(PositioningSystem *sys);
float Phase_Unwrap(float current_phase, float *last_phase);
float PDOA_Calculate_Distance(RFID_Measurement *meas, uint8_t count);
float RSSI_To_Distance(float rssi, float path_loss_exp);
void Kalman_Init(KalmanFilter *kf, float q, float r, float initial_value);
float Kalman_Update(KalmanFilter *kf, float measurement);
float Adaptive_Path_Loss_Exponent(float *rssi_history, uint8_t count, float known_distance);
uint8_t Multipath_Detection(RFID_Measurement *meas, uint8_t count);
void Process_Measurements(PositioningSystem *sys);
	
#endif /* __TAGDEBUG_H__ */
