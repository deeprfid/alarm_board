
#include "kalman_filter.h"
#include "math.h"
#include "stdint.h"
#include "ipc.h"

extern debug_data *rssi_filter_coeff(void);   /* v9.81ch: rfid_processor.c 定义 */

static kalman_struct ph_rssi[MAX_RSSI_META];
static kalman_struct kalrssi[MAX_RSSI_META];
/**
 *kalman_init - 卡尔曼滤波器初始化
 *@kalman_lcw：卡尔曼滤波器结构体
 *@init_x：待测量的初始值
 *@init_p：后验状态估计值误差的方差的初始值

 1:调试时可以先将Q从小往大调整，将R从大往小调整；先固定一个值去调整另外一个值，看收敛速度与波形输出
 2:Q值为过程噪声，越小系统越容易收敛，表示对模型预测的值信任度越高；但是太小则容易发散，如果Q为零，那么我们只相信预测值；
   Q值越大表示对于预测的信任度就越低，而对测量值的信任度就变高；如果Q值无穷大，那么则表示信任测量值。
 3:R值为测量噪声。R太大，滤波的响应（此处响应特指对量测值的响应）会变慢，因为它对新测量的值的信任度降低；越小系统收敛越快，但过小则容易出现震荡

 */
void kalman_init(kalman_struct *kalman_lcw, float init_x, float init_p, float Q, float R)
{
    kalman_lcw->x = init_x;//待测量的初始值，如有中值一般设成中值（如陀螺仪）
    kalman_lcw->p = init_p;//后验状态估计值误差的方差的初始值
    kalman_lcw->A = 1;
    kalman_lcw->H = 1;
    kalman_lcw->q = Q;//10e-6;//2e2;predict noise convariance 预测（过程）噪声方差 实验发现修改这个值会影响收敛速率
    kalman_lcw->r = R;//10e-5;//测量（观测）噪声方差。以陀螺仪为例，测试方法是：
    kalman_lcw->residual_var = R;
    //保持陀螺仪不动，统计一段时间内的陀螺仪输出数据。数据会近似正态分布，
    //按3σ原则，取正态分布的(3σ)^2作为r的初始化值
}


/**
 *kalman_filter - 卡尔曼滤波器
 *@kalman_lcw:卡尔曼结构体
 *@measure；测量值
 *返回滤波后的值
 */
float kalman_filter(kalman_struct *kalman_lcw, float measure)
{

    /* Predict */
    kalman_lcw->x = kalman_lcw->x;
    kalman_lcw->p = kalman_lcw->p + kalman_lcw->q;  /* p(n|n-1)=A^2*p(n-1|n-1)+q */

    /* Measurement */
    kalman_lcw->gain = kalman_lcw->p  / (kalman_lcw->p  + kalman_lcw->r);
    kalman_lcw->x = kalman_lcw->x + kalman_lcw->gain * (measure - kalman_lcw->x);
    kalman_lcw->p = (1 - kalman_lcw->gain) * kalman_lcw->p;


    return kalman_lcw->x;
}

float kalman_filter_optimized(kalman_struct *k, float measure)
{
    // 1. 预测步骤（简化模型）
    k->p += k->q;

    // 2. 更新步骤
    float residual = measure - k->x;
    float gain = k->p / (k->p + k->r);
    k->x += gain * residual;
    k->p *= (1 - gain);

    // 3. 智能自适应调节（关键改进点）
    k->residual_var = 0.6f * k->residual_var + 0.4f * residual * residual;

    /* Q调节策略 */
    if(k->residual_var > 3.0f * k->r)         // 残差过大
    {
        k->q *= rssi_filter_coeff()->kfcoeff + 0.25f * (k->residual_var / k->r); // 动态调整幅度
    }
    else if(k->residual_var < 0.5f * k->r)    // 残差过小
    {
        k->q *= 0.950f*(2-rssi_filter_coeff()->kfcoeff);
    }

    /* R调节策略（更保守）*/
    static uint8_t r_adjust_cnt = 0;

    if(++r_adjust_cnt)   // 每10次采样调整一次R
    {
        r_adjust_cnt = 0;

        if(k->residual_var > 1.0f * k->r)
        {
            k->r *= 1.005f;
        }
        else if(k->residual_var < 0.2f * k->r)
        {
            k->r *= 0.995f;
        }
    }

    // 4. 动态限幅（根据应用场景调整）
    k->q = fmaxf(1e-10f,   fminf(k->q, 10.0f));
    k->r = fmaxf(1e-8f,   fminf(k->r, 5.0f));
  //  PRINT(Kalman, "%.10f,%.10f", k->q,k->r);
    return k->x;
}



float kalman_filter_phase(kalman_struct *k, float measure)
{
    // 1. 预测步骤（简化模型）
    k->p += k->q;

    // 2. 更新步骤
    float residual = measure - k->x;
    float gain = k->p / (k->p + k->r);
    k->x += gain * residual;
    k->p *= (1 - gain);

    // 3. 智能自适应调节（关键改进点）
    k->residual_var = 0.33f * k->residual_var + 0.67f * residual * residual;

    /* Q调节策略 */
    if(k->residual_var > 3.0f * k->r)         // 残差过大
    {
        k->q *= rssi_filter_coeff()->kfcoeff + 0.15f * (k->residual_var / k->r); // 动态调整幅度
    }
    else if(k->residual_var < 0.5f * k->r)    // 残差过小
    {
        k->q *= 0.995f*(2-rssi_filter_coeff()->kfcoeff);
    }

    /* R调节策略（更保守）*/
    static uint8_t r_adjust_cnt = 0;

    if(++r_adjust_cnt)   // 每10次采样调整一次R
    {
        r_adjust_cnt = 0;

        if(k->residual_var > 1.0f * k->r)
        {
            k->r *= 1.005f;
        }
        else if(k->residual_var < 0.2f * k->r)
        {
            k->r *= 0.995f;
        }
    }

    // 4. 动态限幅（根据应用场景调整）
    k->q = fmaxf(1e-8f ,   fminf(k->q, 1.0f));
    k->r = fmaxf(1e-3f ,   fminf(k->r, 0.01f));

	//  PRINT(Kalman, "%.10f,%.10f", k->q,k->r);
    return k->x;
}

/**
 *kalman_init - 卡尔曼滤波器参数初始化

 */
void kalman_filter_init(void)
{
    kalman_init(&kalrssi[0], 1, 1,  rssi_filter_coeff()->kalrssiQ, rssi_filter_coeff()->kalrssiR);
  //kalman_init(&ph_rssi[0]   , 1, 1,  rssi_filter_coeff()->kfQ, rssi_filter_coeff()->kfR);       //   Q/R=0.01f
	  kalman_init(&ph_rssi[0]   ,1, 1,  0.00001f ,  0.0001f);
    kalman_init(&ph_rssi[1]   ,1, 1,  0.00001f ,  0.0001f);
    kalman_init(&ph_rssi[2]   ,1, 1,  0.00001f ,  0.0001f);
    kalman_init(&ph_rssi[3]   ,1, 1,  0.00001f ,  0.0001f);
    kalman_init(&ph_rssi[4]   ,1, 1,  0.00001f ,  0.0001f);
	
	  kalman_init(&kalrssi[1], 1, 1,  0.00001f ,  0.0001f);
	  kalman_init(&kalrssi[2], 1, 1,  0.00001f ,  0.0001f);
	  kalman_init(&kalrssi[3], 1, 1,  0.00001f ,  0.0001f);
//    kalman_init(&ph_rssi[5], 1, 1,  0.0006f,  0.06f);
//    kalman_init(&ph_rssi[6], 1, 1,  0.0007f,  0.07f);
//    kalman_init(&ph_rssi[7], 1, 1,  0.0008f,  0.08f);
//    kalman_init(&ph_rssi[8], 1, 1,  0.0009f,  0.09f);
//    kalman_init(&ph_rssi[9], 1, 1,  0.001f ,  0.1f);

}


/* v9.81ch: kalman state access (replaces cross-file extern) */
kalman_struct *kalman_rssi_state(void) { return kalrssi; }
