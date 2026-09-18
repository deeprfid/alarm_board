
#ifndef __RFID_DISTANCE_H__
#define __RFID_DISTANCE_H__

#include "arm_math.h"
#include "math.h"

// け僇饜离
#define HOPTABLE    (50)
#define FILTER_TAPS 16       // 薦疏ん論杅
#define LEARNING_RATE 0.15f  // 悝炾薹
#define FFT_LENGTH_SAMPLES 32 

#define  CH_DIST    (1)
#define  CH_RSSI    (2)
#define  CH_PHASE   (3)


typedef struct
{
    float distance;      // 郔笝擒燭嘛數 (m)
    float rssi;          // 絞ヶRSSI (dBm)
    float phase;         // 絞ヶ眈弇 (rad)
} DistanceResult;


typedef struct {
    float variance;          // 眈弇船源船
    float autocorr_peak;     // 赻眈壽瑕硉
    float lpc_error;         // 啎聆昫船
} MotionFeatures;

/************************************************************************************************/

float Calculate_Distance_RSSI(float rssi, uint32_t freq);
float Calculate_Distance_Phase(float phase, uint32_t freq, float rssi_distance);

/************************************************************************************************/

void RFID_Distance_Init(void);
float phase2rad(unsigned char *ph);
float degree2rad(unsigned char *ph);
float pow10f_cmsis(float exponent);

/***********************************************************************************************/

// 滲杅汒隴
void filter_config_data_init(void);
float Process_Tag_Data(float start_phase, float end_phase,float freq);
void arm_rfft_f32_app(float *inbuf,float *outbuf,float *mag);
float Get_Filtered_Rssi(float phase, float rssi_in, float ref, uint32_t freq);
float Get_Filtered_Phase(float phase, float rssi, float ref, uint32_t freq);
float phase_difference(float start_phase, float end_phase);
void Freq_rssi_init(void);
DistanceResult Mobile_Net(TAGINFO *mobiletag);

/***********************************************************************************************/

#endif /* __RFID_DISTANCE_H__ */
