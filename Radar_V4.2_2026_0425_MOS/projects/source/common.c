
/*******************************************************************************
 * Include files
 ******************************************************************************/
#include "main.h"

extern LED_T Board_LED_1;
extern LED_T Board_LED_2;


extern stc_radar_scan_data_t  HLKLD2410_Radar;
uint8_t rgb_led_status = 0, EAS_switch = 0, offline_flag = 0;
__align(64) alarm_confirm_package  HC32_RS485_corfirm_PDU;
extern stc_ring_buf_t m_stcRingBuf;
extern stc_ring_buf_t g_AlarmRing;
/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
unsigned short GetNumU16(uint8_t *p)
{
    return (p[0] << 8) | p[1];
}

unsigned int GetNumU32(uint8_t *p)
{
    return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
}

void SetNumU16(uint8_t *p, uint16_t num)
{
    p[0] = (num >> 8) & 0xff;
    p[1] = (num >> 0) & 0xff;
}

void SetNumU32(uint8_t *p, uint32_t num)
{
    p[0] = (num >> 24) & 0xff;
    p[1] = (num >> 16) & 0xff;
    p[2] = (num >> 8)  & 0xff;
    p[3] = (num >> 0)  & 0xff;
}

void CRC_calcCrc8(uint16_t *crcReg, uint16_t poly, uint16_t u8Data)
{
    uint16_t i;
    uint16_t xorFlag;
    uint16_t bit;
    uint16_t dcdBitMask = 0x80;

    for(i = 0; i < 8; i++)
    {
        xorFlag = *crcReg & 0x8000;
        *crcReg <<= 1;
        bit = ((u8Data & dcdBitMask) == dcdBitMask);
        *crcReg |= bit;

        if(xorFlag)
        {
            *crcReg = *crcReg ^ poly;
        }

        dcdBitMask >>= 1;
    }
}

uint16_t CalcCRC(uint8_t *msgbuf, uint8_t msglen)
{
    uint16_t calcCrc = MSG_CRC_INIT;
    uint8_t i;

    for (i = 0; i < msglen; i++)
        CRC_calcCrc8(&calcCrc, MSG_CCITT_CRC_POLY, msgbuf[i]);

    return calcCrc;
}

extern uint8_t buzz_duty;
uint16_t radar_range = 5;
int8_t Get_pdu_data(uint8_t *pdubuff)
{
    extern uint8_t alarm_duration;
    extern uint8_t m_au8DataBuf[RING_BUF_SIZE];
    uint16_t       crcdata, temp;
    uint16_t       idkey = 0;
    alarm_pdu      *getpdupack = (alarm_pdu *)pdubuff;
    temp           = getpdupack->crc;
    crcdata        = CalcCRC(pdubuff, getpdupack->Pdu_len - 2);

    if(getpdupack->AntID != 0)
    {
        buzz_duty      = getpdupack->Alarm_Duration[0];
        radar_range    = getpdupack->Alarm_Duration[1];
        rgb_led_status = getpdupack->Alarm_Duration[2];
        alarm_duration = getpdupack->Alarm_Duration[3];
        EAS_switch     = getpdupack->Alarm_Duration[4];



        if(alarm_duration == 0)
        {
            alarm_duration = 5;
        }
    }
    else
    {
        offline_flag   = getpdupack->Alarm_Duration[5];
    }

    idkey = Ucode_read(&HC32_RS485_corfirm_PDU.rngkey, &HC32_RS485_corfirm_PDU.uidkey);

    if(getpdupack->FrameHead == PDUHEAD && temp == crcdata && idkey && getpdupack->AntID != 0)
    {
        HC32_RS485_corfirm_PDU.framehead = PDUHEAD;
        HC32_RS485_corfirm_PDU.deviceID = getpdupack->DeviceID;
        HC32_RS485_corfirm_PDU.alarm_done = 1;
        memcpy(&HC32_RS485_corfirm_PDU.radar, &HLKLD2410_Radar.target_state, sizeof(HLKLD2410_Radar));
        HC32_RS485_corfirm_PDU.crc = CalcCRC((uint8_t *)&HC32_RS485_corfirm_PDU, sizeof(HC32_RS485_corfirm_PDU) -2);
        return LL_OK;
    }
    else if(getpdupack->FrameHead == PDUHEAD && temp == crcdata && idkey && getpdupack->AntID == 0 && offline_flag)
    {

        return LL_OK + offline_flag;
    }
    else if(getpdupack->FrameHead == GPIOHEAD && temp == crcdata && idkey)
    {
        // LED_Start(&Radar_LED,BOARD_GLED,1,5,1);
        Send_RadarStatus_to_Master();
        return  LL_ERR;

    }
    else
    {
        (void)BUF_Init(&m_stcRingBuf, m_au8DataBuf, sizeof(m_au8DataBuf));
        return  LL_ERR;
    }
}



void Send_RadarStatus_to_Master(void)
{
    alarm_pdu Get_Radar_Data;
    en_pin_state_t aicamsingal = switch_decoder_pio_read(AI_CAMERA);
    uint8_t radarsingal = bsp_get_radar_singal();
    memset(&Get_Radar_Data, 0, sizeof(Get_Radar_Data));
    Get_Radar_Data.FrameHead  = GPIOHEAD;
    Get_Radar_Data.Radarcfg[0] = (radarsingal || (PIN_RESET == aicamsingal)) ? 1U : 0U;
    Get_Radar_Data.crc = CalcCRC((uint8_t *)&Get_Radar_Data, sizeof(Get_Radar_Data) - 2);
    USART_UART_Trans(USART_UNIT, &Get_Radar_Data, sizeof(Get_Radar_Data), 100);
}

void Check_alarm_state(void)
{

    alarm_thread();
}

uint8_t bsp_get_radar_detection(void)
{
    uint8_t key = 0, flag = 0;

    do
    {
        key = bsp_GetKey();

        if((key == KEY_DOWN_K1) || (key == KEY_LONG_K1))
        {
            flag = flag | 0x1;
        }

        if((key == KEY_DOWN_K2) || (key == KEY_LONG_K2))
        {
            flag = flag | 0x2;
        }

        if((key == KEY_DOWN_K3) || (key == KEY_LONG_K3))
        {
            flag = flag | 0x4;
        }

    }
    while(key != KEY_NONE);

    return flag;
}

uint8_t bsp_get_radar_singal(void)
{
    uint8_t flag = false;

    if(     PIN_SET == GPIO_ReadInputPins(RADAR_PORT0, RADAR_PIN0) || \
            PIN_SET == GPIO_ReadInputPins(RADAR_PORT1, RADAR_PIN1) || \
            PIN_SET == GPIO_ReadInputPins(RADAR_PORT2, RADAR_PIN2) \
      )
    {
        flag = true;
    }

    return flag;
}

void Check_Uart_Pdu(void)
{

    if (BUF_UsedSize(&m_stcRingBuf) >= APP_FRAME_LEN_MAX)
    {
        uint8_t alarm_databuf[APP_FRAME_LEN_MAX];
        uint8_t	radarsingal = bsp_get_radar_singal();
        BUF_Read(&m_stcRingBuf, alarm_databuf, APP_FRAME_LEN_MAX);
        en_pin_state_t p_Easmode   = switch_decoder_pio_read(EAS_MODE);
        en_pin_state_t aicamsingal	= switch_decoder_pio_read(AI_CAMERA);
        int8_t pduflag = Get_pdu_data(alarm_databuf);

        if(pduflag >= 0)
        {
            LED_Start(&Board_LED_1, BOARDLED1, 2, 1, 1);
        }

        if((LL_OK == pduflag) &&  ((p_Easmode == PIN_RESET) || (radar_range == 0)) )
        {
            uint8_t intid = MSG_485_TAG_RTU;
            radar_range = 0xFF;
            BUF_Write(&g_AlarmRing, &intid, 1);
            return;

        }

        if(LL_OK == pduflag && (radarsingal || PIN_RESET == aicamsingal )) //&& EAS_switch==AUX_EAS_CODE
        {
            uint8_t intid = MSG_485_TAG_RTU;
            BUF_Write(&g_AlarmRing, &intid, 1);
            return;
        }

        if((1 == pduflag) && (offline_flag == 1) )
        {
            uint8_t intid = MSG_NETWORK_OFFLINE;
            offline_flag = 0;
            BUF_Write(&g_AlarmRing, &intid, 1);
            return;

        }

        if((2 == pduflag) && (offline_flag == 2) )
        {
            uint8_t intid = MSG_LEDTEST;
            offline_flag = 0;
            BUF_Write(&g_AlarmRing, &intid, 1);
            return;

        }

    }


}

void Relay_status_check(void)
{
    extern uint8_t Relay_input_flag;

    if(Relay_input_flag == PIN_RESET)
    {
        return;
    }

    // if	((PIN_RESET==switch_decoder_pio_read(INput_RELAY1)) || (PIN_RESET==switch_decoder_pio_read(INput_RELAY2)))
    if	(PIN_RESET == switch_decoder_pio_read(INput_RELAY))
    {
        uint8_t intid = MSG_485_TAG_RTU;
        rgb_led_status = ALARM_R_CODE;
        BUF_Write(&g_AlarmRing, &intid, 1);
        Relay_input_flag = PIN_RESET;
    }

}

uint16_t Ucode_read(uint32_t *rngkey, uint16_t *uidkey)
{
    uint16_t crcdata = 0, crcdata1 = 0;
    uint32_t UniqueID = 0;
    uint32_t magic_code = 0xA5A55A5A;
    stc_efm_unique_id_t efm_unique_id = {0};

    uint8_t Hash_table[HASH_MSG_DIGEST_SIZE];

    EFM_GetUID(&efm_unique_id);

    UniqueID = (efm_unique_id.u32UniqueID0) ^ (efm_unique_id.u32UniqueID1) ^ (efm_unique_id.u32UniqueID2);

    HASH_Calculate((uint8_t*)&UniqueID, sizeof(UniqueID), Hash_table);

    crcdata  = CalcCRC(Hash_table, HASH_MSG_DIGEST_SIZE / 2);
    crcdata1 = CalcCRC(Hash_table + (HASH_MSG_DIGEST_SIZE / 2), HASH_MSG_DIGEST_SIZE / 2);

    UniqueID = UniqueID ^ (crcdata1 << 16 | crcdata) ^ 0x0C32F460;

    magic_code = UniqueID ^ ((crcdata << 16) | (crcdata1));

    #if Custom_By_SZBMA
    return (magic_code == 0xC1A53979) ? 1 : 0;
    #else
    return 1;

    #endif

}



void HashConfig(void)
{
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_HASH, ENABLE);
    HASH_DeInit();
}

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
