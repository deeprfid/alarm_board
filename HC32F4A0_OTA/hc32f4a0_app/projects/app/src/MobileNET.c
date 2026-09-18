//#include "bsp.h"
#include "ipc.h"
#include "arm_math.h"
//#include "ring_buf.h"
#include "EPC_TagStorage.h"
#include "rfid_distance.h"
#include "tagdebug.h"

void arm_hamming_f32(float32_t * pDst,uint32_t blockSize);
/* v9.81ch: kalrssi 已 static 化，如需滤波状态用 kalman_rssi_state() (kalman_filter.h) */

__attribute__((section (".RAM_D1"))) float rssi_track [MAX_RSSI_LEN];
__attribute__((section (".RAM_D1"))) float dist_track [MAX_RSSI_LEN];
__attribute__((section (".RAM_D1"))) float phase_track[MAX_RSSI_LEN];
__attribute__((section (".RAM_D3"))) float ringbuf    [MAX_RSSI_LEN];
__attribute__((section (".RAM_D3"))) float fftbuf     [FFT_LENGTH_SAMPLES * 2];
__attribute__((section (".RAM_D3"))) float magbuf     [FFT_LENGTH_SAMPLES * 2];
__attribute__((section (".RAM_D3"))) float Hammingwin [FFT_LENGTH_SAMPLES];


 void arm_absmax_f32(
    const float32_t * pSrc,
    uint32_t blockSize,
    float32_t * pResult,
    uint32_t * pIndex)
{
    float32_t maxVal, out;                         /* Temporary variables to store the output value. */
    uint32_t blkCnt, outIndex;                     /* Loop counter */



    /* Initialise index value to zero. */
    outIndex = 0U;

    /* Load first input value that act as reference value for comparision */
    out = fabsf(*pSrc++);

    /* Initialize blkCnt with number of samples */
    blkCnt = (blockSize - 1U);


    while (blkCnt > 0U)
    {
        /* Initialize maxVal to the next consecutive values one by one */
        maxVal = fabsf(*pSrc++);

        /* compare for the maximum value */
        if (out < maxVal)
        {
            /* Update the maximum value and it's index */
            out = maxVal;
            outIndex = blockSize - blkCnt;
        }

        /* Decrement loop counter */
        blkCnt--;
    }

    /* Store the maximum value and it's index into destination pointers */
    *pResult = out;
    *pIndex = outIndex;
}

void Freq_rssi_init(void)
{
    arm_fill_f32(1.0f, phase_track, MAX_RSSI_LEN);
    arm_fill_f32(1.0f, rssi_track, MAX_RSSI_LEN);
    arm_fill_f32(1.0f, dist_track, MAX_RSSI_LEN);
    arm_fill_f32(1.0f, ringbuf, MAX_RSSI_LEN);
    arm_hamming_f32(Hammingwin, FFT_LENGTH_SAMPLES);
    filter_config_data_init();
    TAG_Init(PHASE_BOTH);

}


void softmax(float *x, float *y, int size)
{
    float max_val = x[0];
    float sum = 0.0f;

    for (int i = 1; i < size; i++)
    {
        if (x[i] > max_val) max_val = x[i];
    }

    for (int i = 0; i < size; i++)
    {
        y[i] = expf(x[i] - max_val);
        sum += y[i];
    }


    for (int i = 0; i < size; i++)
    {
        y[i] /= sum;
    }
}

float probability_dist(float *inbuf, uint8_t *index, float x)
{
    float std = 0.0f;
    float tempbuf[MAX_RSSI_LEN] = {0.0f};
    arm_sort_instance_f32 S;
    arm_sort_init_f32(&S, ARM_SORT_QUICK, ARM_SORT_DESCENDING);
    inbuf[index[0]++] = x;

    if(index[0] >= MAX_RSSI_LEN)  index[0] = 0;

//		arm_absmax_f32(inbuf,MAX_RSSI_LEN,&pResult,&pIndex);
//
//		pResult=fabs(pResult);
//		if(pResult!=0.0f)
//		{
//		arm_scale_f32(inbuf,1/pResult,tempbuf,MAX_RSSI_LEN);
//		}
    softmax(inbuf, ringbuf, MAX_RSSI_LEN);
    arm_sort_f32(&S, inbuf, tempbuf, MAX_RSSI_LEN);
    arm_std_f32(ringbuf, MAX_RSSI_LEN, &std);
    return tempbuf[3];
    //return std*1000 ;
}



void EPC_Tag_Para_update(TAGINFO *mobiletag, Tag_Data_f32 *Tagdata, PhaseAvailability phase_avail)
{
    Tagdata->Epclen      = mobiletag->Epclen;
    memcpy(Tagdata->EpcId, mobiletag->EpcId, mobiletag->Epclen);
    Tagdata->rssi        = (float)(mobiletag->RSSI - 0x100);
    Tagdata->freq        = 1000 * mobiletag->Frequency;     //unit:Hz

    if(phase_avail == PHASE_START_ONLY)
    {
        Tagdata->end_phase   = 0.0f;
        Tagdata->start_phase = degree2rad(mobiletag->Res);      //uint:rad
    }

    if(phase_avail == PHASE_BOTH)
    {
        Tagdata->end_phase   = phase2rad(mobiletag->CRC);
        Tagdata->start_phase = phase2rad(mobiletag->Res);      //uint:rad
    }
		
    Tagdata->deltaPH     = phase_difference(Tagdata->start_phase,Tagdata->end_phase);
    Tagdata->phase_avail = phase_avail;
    Tagdata->timestamp   = osKernelGetTickCount();
}


DistanceResult Mobile_Net(TAGINFO *mobiletag)
{
    DistanceResult result  = {0};
    Tag_Data_f32   EPCdata = {0};

    char  epcstr[32] = {0x30};
    EPC_Tag_Para_update(mobiletag, &EPCdata, PHASE_START_ONLY);
    TAG_AddData(&EPCdata);
		float distraw=TAG_GetAverageDistance(EPCdata.EpcId, EPCdata.Epclen);
 		float dist =TAG_GetRecentDistance(EPCdata.EpcId, EPCdata.Epclen);
        result = TAG_GetRecentDistance_Kalman(EPCdata.EpcId, EPCdata.Epclen);
    Hex2Str(EPCdata.EpcId, EPCdata.Epclen, epcstr);
  //  PRINT(plotter , "%.8f",result.rssi/50.0f);
	//	PRINT(deltaPH , "%.8f",dist);
    printf("{%s}%.8f,%.8f,%.8f,%.8f\n", epcstr,distraw,dist, result.distance,result.rssi/(-50.0f));

    return result;
}






