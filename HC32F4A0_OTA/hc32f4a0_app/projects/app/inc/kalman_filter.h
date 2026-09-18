#ifndef  _KALMAN_FILTER_H
#define  _KALMAN_FILTER_H

//1d卡尔曼滤波
typedef struct {
    float x;  // state 
    float A;  // x(n)=A*x(n-1)+u(n),u(n)~N(0,q) 
    float H;  // z(n)=H*x(n)+w(n),w(n)~N(0,r)   
    float q;  // process(predict) noise convariance 协方差
    float r;  // measure noise convariance 
    float p;  // estimated error convariance 估计误差协方差
    float gain;
	  float residual_var;
}kalman_struct;



void kalman_init(kalman_struct *kalman_lcw, float init_x, float init_p,float Q,float R);
float kalman_filter(kalman_struct *kalman_lcw, float z_measure);
void kalman_filter_init(void);
float kalman_filter_optimized(kalman_struct *k, float measure);
float kalman_filter_phase(kalman_struct *k, float measure);

/* v9.81ch: kalman state access (replaces cross-file extern) */
kalman_struct *kalman_rssi_state(void);
#endif  /*_KALMAN_FILTER_H*/
