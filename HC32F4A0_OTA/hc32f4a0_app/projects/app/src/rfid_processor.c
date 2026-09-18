
//#include "bsp.h"
#include "ipc.h"
#include "arm_math.h"
#include "rfid_distance.h"
#include "EPC_TagStorage.h"
static debug_data  filter_coeff;


/**
 * @brief 计算两个相位之间的差值（解缠绕），结果归一化到 [-π, π] 区间
 *
 * @param start_phase 起始相位（弧度）
 * @param end_phase 结束相位（弧度）
 * @return float 归一化的相位差（弧度）
 */
float phase_difference(float start_phase, float end_phase)
{
    // 计算原始相位差

    const float TWO_PI = 2.0f * PI;

    float delta = end_phase - start_phase;

    // 将相位差模规约到 [-2π, 2π]
    delta = fmodf(delta, TWO_PI);

    // 进一步规约到 [-π, π]
    if (delta > PI)
    {
        delta -= TWO_PI;
    }
    else if (delta < -PI)
    {
        delta += TWO_PI;
    }

    // 使用 atan2f(sin, cos) 将相位差归一化到 [-π, π]
    float normalized_phase = atan2f(sinf(delta), cosf(delta));

    return normalized_phase;
}

float phase2rad(unsigned char *ph)
{
    uint16_t phase = GetNumU16(ph);
    phase = phase & 0x0FFF;
    float    ph_f  = (float)phase;
    ph_f  = (ph_f / 4096) * 360;
    float      rad = ph_f * (M_PI / 180.0f);
    return     rad;
}

float degree2rad(unsigned char *ph)
{
    float  phase = ph[1];
    phase = phase * (M_PI / 180.0f);
    return  (phase);
}

float pow10f_cmsis(float exponent)
{
    float ln10 = 2.302585092994046f;
    float result;
    arm_vexp_f32(&exponent, &result, 1);
    arm_scale_f32(&result, ln10, &result, 1);
    return result;
}

/*******************************************************************************************************************/

// 改进的相位距离计算方法（使用RSSI辅助解模糊）
float Calculate_Distance_Phase(float phase, uint32_t freq, float rssi_distance)
{
    const float lambda = SPEED_OF_LIGHT / freq;

    // 1. 相位规范化
    phase = fmodf(phase, 2 * M_PI);

    if (phase < 0) phase += 2 * M_PI;

    // 2. 基础相位距离
    float phase_distance = (phase * lambda) / (4 * M_PI);

    // 3. 使用RSSI距离解决模糊
    if (rssi_distance > 0)
    {
        float max_range = lambda / 2;
        int wraps = (int)(rssi_distance / max_range);

        float candidate1 = wraps * max_range + phase_distance;
        float candidate2 = (wraps + 1) * max_range + phase_distance;

        // 选择最接近RSSI距离的候选
        return fabs(candidate1 - rssi_distance) < fabs(candidate2 - rssi_distance) ?
               candidate1 : candidate2;
    }

    return phase_distance;
}

// RSSI距离计算（使用环境校准参数）
float Calculate_Distance_RSSI(float rssi, uint32_t freq)
{
    const float tx_power = 20.0f;
    const float antenna_gain = 3.0f;
    const float n = 2.5f;
    const float lambda = 299792458.0f / freq;

    float distance = pow(10.0f, (tx_power + antenna_gain - rssi) / (10 * n));
    distance *= lambda / (4 * M_PI);
    distance *= 0.04f;
    return distance;
}

void reconfig_filter(void)
{
    kalman_filter_init();
}
// 場宎趙薦疏け饜离
void filter_config_data_init(void)
{
    filter_coeff.kfQ = 0.0005f;
    filter_coeff.kfR = 0.005f;
    filter_coeff.kfcoeff = 1.05f;
    filter_coeff.EkfQ = 0.005f;
    filter_coeff.EkfR = 0.01f;
    filter_coeff.Ekfcoeff = 0.95f;
    filter_coeff.kalrssiQ = 0.0005f;
    filter_coeff.kalrssiR = 0.005f;
	 reconfig_filter();
}



// LMS 薦疏け賦凳
typedef struct
{
    arm_lms_norm_instance_f32 instance;
    float32_t state[FILTER_TAPS + 3 - 1]; // 袨怓遣喳Е
    float32_t coeffs[FILTER_TAPS];         // 薦疏け炵杅
    bool  filter_is_init;
} LMS_Filter;

// 場宎趙LMS薦疏け
void LMS_Init(LMS_Filter* filter)
{
    // 場宎炵杅 (褫扢峈ⅸ歙薦疏け)
    for (int i = 0; i < FILTER_TAPS; i++)
    {
        filter->coeffs[i] = 1.0f / FILTER_TAPS;
    }

    // 場宎趙LMS妗瞰
    arm_lms_norm_init_f32(&filter->instance, FILTER_TAPS,
                          filter->coeffs, filter->state,
                          LEARNING_RATE, 1);
}



// 妏蚚LMS薦疏揭燴擒燭嘛數
float LMS_UpdateDistance(LMS_Filter* filter, float raw_distance, float reference_estimate)
{
    float32_t input = raw_distance;
    float32_t reference = reference_estimate;
    float32_t output, error;

    if (!filter->filter_is_init)
    {
        LMS_Init(filter);
        filter->filter_is_init = true;
    }

    // 硒俴LMS載陔
    arm_lms_norm_f32(&filter->instance, &input, &reference, &output, &error, 1);
    return output;
}



float Get_Filtered_Distance(const Tag_Data_f32 *tag_data,uint8_t uflag)
{
    static LMS_Filter filter_dist;
	  static LMS_Filter filter_ph;
	  static LMS_Filter filter_rssi;
    float filtered=0.00f ;
	   

	  
	 
	
	 switch(uflag)
	 {
		 case CH_DIST :{
		                  float rssi_distance  = Calculate_Distance_RSSI(tag_data->rssi, tag_data->freq);
                      float phase_distance = Calculate_Distance_Phase(tag_data->start_phase, tag_data->freq, rssi_distance);
                      phase_distance *= 0.02f;
                      filtered = LMS_UpdateDistance(&filter_dist, phase_distance, 1.0f);
		                  break;
		               }
		 
		 case CH_RSSI :{ 
			                 filtered = LMS_UpdateDistance(&filter_rssi, tag_data->rssi, 1.0f);
		                  break;
		               }
		 
		 case CH_PHASE:{
		                  filtered = LMS_UpdateDistance(&filter_ph, tag_data->start_phase, 1.0f);  
		                  break;
		               }
	    default: { break;}
	 } 
		
    return filtered;

}

float Process_Tag_Data(float start_phase, float end_phase, float freq)
{
    // 空函数，不再处理运动检测
    return 0.0f;
}



/*************************************************************************************************/


/* v9.81ch: filter coeff access (replaces cross-file extern) */
debug_data *rssi_filter_coeff(void) { return &filter_coeff; }
