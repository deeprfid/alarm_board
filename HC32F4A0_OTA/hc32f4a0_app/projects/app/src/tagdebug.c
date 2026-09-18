//#include "bsp.h"
#include "ipc.h"
#include "tagdebug.h"
#include "rfid_distance.h"
#include "fdacoefs.h"
//  927250 902750
//#define NumberofSections 4

static float iirstage[NumberofSections][3];

static float32_t Phase_f32[FFT_LENGTH_SAMPLES * 2]; /* 相位*/

void coff_buff_init(void)
{

    memset(iirstage, 0, sizeof(iirstage));
}

 void arm_hamming_f32(
    float32_t * pDst,
    uint32_t blockSize)
{
    float32_t k = 2.0f / ((float32_t) blockSize);
    float32_t w;

    for(uint32_t i = 0; i < blockSize; i++)
    {
        w = 0.54f - 0.46f * cosf (PI * i * k);
        pDst[i] = w;
    }
}

 void arm_accumulate_f32(
    const float32_t * pSrc,
    uint32_t blockSize,
    float32_t * pResult)
{
    uint32_t blkCnt;                               /* Loop counter */
    float32_t sum = 0.0f;                          /* Temporary result storage */



    /* Initialize blkCnt with number of samples */
    blkCnt = blockSize;


    while (blkCnt > 0U)
    {
        /* C = (A[0] + A[1] + A[2] + ... + A[blockSize-1]) */
        sum += *pSrc++;

        /* Decrement loop counter */
        blkCnt--;
    }

    /* C = (A[0] + A[1] + A[2] + ... + A[blockSize-1])  */
    /* Store result to destination */
    *pResult = sum ;
}

void coetexM7_levinson_durbin_f32(
    const float32_t *phi,
    float32_t *a,
    float32_t *err,
    int nbCoefs)
{
    float32_t e = phi[0];
    float32_t k;

    // 初始条件
    a[0] = 1.0f;

    for(int n = 1; n <= nbCoefs; n++)
    {
        // 计算反射系数
        k = phi[n];

        for(int j = 1; j < n; j++)
        {
            k -= a[j] * phi[n - j];
        }

        k /= e;

        // 更新预测系数
        for(int j = 1; j <= (n - 1) / 2; j++)
        {
            float32_t aj = a[j];
            float32_t anmj = a[n - j];
            a[j] = aj - k * anmj;
            a[n - j] = anmj - k * aj;
        }

        if(n % 2 == 0)
        {
            a[n / 2] *= (1 - k);
        }

        a[n] = k;
        e *= (1 - k * k);
    }

    *err = e;
}


void iirfilter(float *xin, float *yout, float coff[][3], uint32_t length)
{
    float temp, x, y, z;

    for (uint32_t i = 0; i < length; i++)

    {
        temp = xin[i];

        for (uint32_t k = 0; k < NumberofSections; k++)

        {
            x = DEN[2 * k + 1][1] * coff[k][1];
            y = DEN[2 * k + 1][2] * coff[k][2];

            coff[k][0] = temp - x - y;

            x = NUM[2 * k + 1][0] * coff[k][0];
            y = NUM[2 * k + 1][1] * coff[k][1];
            z = NUM[2 * k + 1][2] * coff[k][2];

            temp = x + y + z;

            coff[k][2] = coff[k][1];

            coff[k][1] = coff[k][0];

            temp *= NUM[2 * k][0];
        }

        yout[i] = temp;
    }
}


void PowerPhaseRadians_f32(float32_t *_ptr, float32_t *_phase, uint16_t _usFFTPoints, float32_t _uiCmpValue)
{
    float32_t lX, lY;
    uint16_t i;
    float32_t phase;
    float32_t mag;


    for (i = 0; i < _usFFTPoints; i++)
    {
        lX = _ptr[2 * i];  	 /* 实部 */
        lY = _ptr[2 * i + 1]; /* 虚部 */

        phase = atan2f(lY, lX);    		  				 /* atan2求解的结果范围是(-pi, pi], 弧度制 */
        arm_sqrt_f32((float32_t)(lX * lX + lY * lY), &mag); /* 求模 */

        if(_uiCmpValue > mag)
        {
            Phase_f32[i] = 0;
        }
        else
        {
            Phase_f32[i] = phase * 180.0f / 3.1415926f; /* 将求解的结果由弧度转换为角度 */
        }
    }
}

extern float Hammingwin [FFT_LENGTH_SAMPLES];
void arm_rfft_f32_app(float *inbuf, float *outbuf, float *mag)
{

    arm_rfft_fast_instance_f32 S;
	//  uint32_t  pIndex=0;
	//  float pResult;
    float fftinbuf[FFT_LENGTH_SAMPLES];
    memset(fftinbuf, 0, sizeof(fftinbuf));
    memcpy(fftinbuf, inbuf, (sizeof(fftinbuf)) / 2);
    float mean;
    arm_mean_f32(inbuf, MAX_RSSI_LEN, &mean);
   // arm_absmax_f32(inbuf,MAX_RSSI_LEN,&pResult,&pIndex);
		//inbuf[pIndex]=mean;
	//	pResult=fabs(pResult);
//		if(pResult!=0.0f)
//		{	
//		arm_scale_f32(inbuf,1/pResult,fftinbuf,MAX_RSSI_LEN);
//		}	
		
	//	 arm_mean_f32(fftinbuf, MAX_RSSI_LEN, &mean);
    /* 正变换 */
    for(uint8_t i = 0; i < FFT_LENGTH_SAMPLES / 2; i++)
    {

        fftinbuf[i] -= mean;
        fftinbuf[i + FFT_LENGTH_SAMPLES / 2] = fftinbuf[i];
    }

    arm_mult_f32(fftinbuf, Hammingwin, fftinbuf, FFT_LENGTH_SAMPLES);
    /* 初始化结构体S中的参数 */
    arm_rfft_fast_init_f32(&S, FFT_LENGTH_SAMPLES);

    /* 1024点实序列快速FFT */
    arm_rfft_fast_f32(&S, fftinbuf, outbuf, 0);

    /* 为了方便跟函数arm_cfft_f32计算的结果做对比，这里求解了1024组模值，实际函数arm_rfft_fast_f32
       只求解出了512组
    */
    arm_cmplx_mag_f32(outbuf, mag, FFT_LENGTH_SAMPLES);


    /* 求相频 */
    PowerPhaseRadians_f32(outbuf, Phase_f32, FFT_LENGTH_SAMPLES, 0.5f);



}


// 定义常量
//#define PI 3.14159265358979323846f


// 初始化定位系统
void Positioning_Init(PositioningSystem *sys) {
    memset(sys, 0, sizeof(PositioningSystem));
    Kalman_Init(&sys->distance_filter, KALMAN_Q, KALMAN_R, 1.0f);
    sys->path_loss_exponent = RSSI_PATH_LOSS_EXPONENT;
    sys->history_index = 0;
}

// 相位解缠
float Phase_Unwrap(float current_phase, float *last_phase) {
    static float prev_phase = 0.0f;
    float unwrapped = current_phase;
    
    if (last_phase != NULL) {
        prev_phase = *last_phase;
    }
    
    float diff = current_phase - prev_phase;
    if (diff > PI) {
        unwrapped -= 2 * PI;
    } else if (diff < -PI) {
        unwrapped += 2 * PI;
    }
    
    prev_phase = unwrapped;
    if (last_phase != NULL) {
        *last_phase = unwrapped;
    }
    
    return unwrapped;
}

// PDoA距离计算
float PDOA_Calculate_Distance(RFID_Measurement *meas, uint8_t count) {
    // 检查数据有效性
    if (count < 2) return -1.0f;
    
    // 相位解缠
    float last_phase = meas[0].phase;
    for (uint8_t i = 0; i < count; i++) {
        meas[i].phase = Phase_Unwrap(meas[i].phase, &last_phase);
    }
    
    // 线性拟合: φ = (2ωd/c) + φ0
    float sum_omega = 0.0f, sum_phase = 0.0f;
    float sum_omega2 = 0.0f, sum_omega_phase = 0.0f;
    
    for (uint8_t i = 0; i < count; i++) {
        float omega = 2 * PI * meas[i].frequency * 1e+6f; // 角频率 (rad/s)
        float phase = meas[i].phase;
        
        sum_omega += omega;
        sum_phase += phase;
        sum_omega2 += omega * omega;
        sum_omega_phase += omega * phase;
    }
    
    float delta = count * sum_omega2 - sum_omega * sum_omega;
    if (fabsf(delta) < 1e-6f) return -1.0f; // 避免除以零
    
    // 计算距离 d
    float d = (count * sum_omega_phase - sum_omega * sum_phase) / delta;
    d *= C_LIGHT / 2.0f; // 转换为距离
    
    // 确保距离为正
    return fabsf(d);
}

// RSSI转距离
float RSSI_To_Distance(float rssi, float path_loss_exp) {
    // 路径损耗模型: PL = PL0 + 10n*log10(d/d0)
    // 转换为: d = d0 * 10^((PL0 - RSSI)/(10n))
    return powf(10.0f, (RSSI_TX_POWER - rssi) / (10.0f * path_loss_exp));
}

// 初始化卡尔曼滤波器
void Kalman_Init(KalmanFilter *kf, float q, float r, float initial_value) {
    kf->q = q;
    kf->r = r;
    kf->x = initial_value;
    kf->p = 1.0f; // 初始估计误差
}

// 卡尔曼滤波更新
float Kalman_Update(KalmanFilter *kf, float measurement) {
    // 预测
    kf->p = kf->p + kf->q;
    
    // 更新
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1.0f - kf->k) * kf->p;
    
    return kf->x;
}

// 自适应路径损耗指数计算
float Adaptive_Path_Loss_Exponent(float *rssi_history, uint8_t count, float known_distance) {
    if (count == 0 || known_distance < 0.1f) return RSSI_PATH_LOSS_EXPONENT;
    
    float avg_rssi = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        avg_rssi += rssi_history[i];
    }
    avg_rssi /= count;
    
    // 路径损耗模型: PL = PL0 + 10n*log10(d/d0)
    // 转换为: n = (PL - PL0) / (10 * log10(d/d0))
    return (avg_rssi - RSSI_TX_POWER) / (10.0f * log10f(known_distance));
}

// 多径检测
uint8_t Multipath_Detection(RFID_Measurement *meas, uint8_t count) {
    if (count < 3) return 0;
    
    // 计算相位均值
    float mean_phase = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        mean_phase += meas[i].phase;
    }
    mean_phase /= count;
    
    // 计算相位标准差
    float std_dev = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float diff = meas[i].phase - mean_phase;
        std_dev += diff * diff;
    }
    std_dev = sqrtf(std_dev / count);
    
    // 计算相位-频率线性度
    float sum_x = 0.0f, sum_y = 0.0f;
    float sum_xy = 0.0f, sum_x2 = 0.0f;
    
    for (uint8_t i = 0; i < count; i++) {
        float x = meas[i].frequency;
        float y = meas[i].phase;
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    float slope = (count * sum_xy - sum_x * sum_y) / (count * sum_x2 - sum_x * sum_x);
    float intercept = (sum_y - slope * sum_x) / count;
    
    // 计算R?值
    float ss_res = 0.0f, ss_tot = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        float y_pred = slope * meas[i].frequency + intercept;
        ss_res += (meas[i].phase - y_pred) * (meas[i].phase - y_pred);
        ss_tot += (meas[i].phase - mean_phase) * (meas[i].phase - mean_phase);
    }
    
    float r_squared = 1.0f - (ss_res / ss_tot);
    
    // 多径检测标准
    if (std_dev > 0.5f || r_squared < 0.7f) {
        return 1; // 检测到多径
    }
    return 0; // 未检测到多径
}

// 处理测量数据
void Process_Measurements(PositioningSystem *sys) {
    // 1. 多径检测
  //  uint8_t multipath_detected = Multipath_Detection(sys->measurements, NUM_FREQ_POINTS);
    
    // 2. 计算PDoA距离
    float pdoa_dist = PDOA_Calculate_Distance(sys->measurements, NUM_FREQ_POINTS);
    
    // 3. 计算平均RSSI
////    float avg_rssi = 0.0f;
////    for (uint8_t i = 0; i < NUM_FREQ_POINTS; i++) {
////        avg_rssi += sys->measurements[i].rssi;
////    }
////    avg_rssi /= NUM_FREQ_POINTS;
//    
//    // 4. 自适应路径损耗指数
//    if (sys->history_index >= 10) { // 有足够历史数据
//        sys->path_loss_exponent = Adaptive_Path_Loss_Exponent(
//            sys->distance_history, 
//            sys->history_index, 
//            sys->last_distance
//        );
//        // 限制在合理范围内
//        if (sys->path_loss_exponent < 1.8f) sys->path_loss_exponent = 1.8f;
//        if (sys->path_loss_exponent > 4.5f) sys->path_loss_exponent = 4.5f;
//    }
//    
//    // 5. 计算RSSI距离
//    float rssi_dist = RSSI_To_Distance(avg_rssi, sys->path_loss_exponent);
//    
//    // 6. 融合距离估计
//    float fused_distance;
//    if (multipath_detected) {
//        // 多径环境下更依赖RSSI
//        fused_distance = 0.3f * pdoa_dist + 0.7f * rssi_dist;
//    } else {
//        // 正常环境下更依赖PDoA
//        fused_distance = 0.7f * pdoa_dist + 0.3f * rssi_dist;
//    }
//    
//    // 7. 应用卡尔曼滤波
//    float filtered_distance = Kalman_Update(&sys->distance_filter, fused_distance);
//    sys->last_distance = filtered_distance;
//    
//    // 8. 存储历史数据
//    if (sys->history_index < MAX_MEASUREMENTS) {
//        sys->distance_history[sys->history_index++] = filtered_distance;
//    } else {
//        // 循环缓冲区
//        memmove(sys->distance_history, &sys->distance_history[1], 
//                (MAX_MEASUREMENTS - 1) * sizeof(float));
//        sys->distance_history[MAX_MEASUREMENTS - 1] = filtered_distance;
//    }
    
    // 9. 输出结果 (实际应用中可通过UART、USB等输出)
    TRACE("PDoA: %.3f\n",pdoa_dist);
}



