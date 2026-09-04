

/*******************************************************************************
 * Include files
 ******************************************************************************/

#include "main.h"

/**
 * @addtogroup USART_UART_Interrupt
 * @{
 */

/**
 * @defgroup Ring_Buffer Ring Buffer
 * @{
 */
 /*使能配置命令*/
static const uint8_t config_enable_set[] ={0xFD,0xFC,0xFB,0xFA,0x04,0x00,0xFF,0x00,0x01,0x00,0x04,0x03,0x02,0x01};
/* 结束配置命令*/
static const uint8_t config_disable_set[]={0xFD,0xFC,0xFB,0xFA,0x02,0x00,0xFE,0x00,0x04,0x03,0x02,0x01};



//使能工程模式命令
static const uint8_t enable_engineer_mode[] ={0xFD,0xFC,0xFB,0xFA,0x02,0x00,0x62,0x00,0x04,0x03,0x02,0x01};
/*

雷达ACK(成功)：
FD FC FB FA 04 00 62 01 00 00 04 03 02 01

*/

//关闭工程模式命令
static const uint8_t disable_engineer_mode[] ={0xFD,0xFC,0xFB,0xFA,0x02,0x00,0x63,0x00,0x04,0x03,0x02,0x01};

/*
雷达ACK(成功)：
FD FC FB FA 04 00 63 01 00 00 04 03 02 01

*/
/*
最大距离门与无人持续时间参数配置命令
此命令设置雷达最大探测距离门（运动&静止）（配置范围2~8），以及无人持续时间参数（配置范
围0~65535秒）。具体参数字请参考下表。此配置值掉电不丢失。
命令字：0x0060
命令值：2 字节最大运动距离门字+ 4 字节最大运动距离门参数+ 2 字节最大静止距离门字+ 4 字
节最大静止距离门参数+ 2 字节无人持续时间字+ 4 字节无人持续时间参数
返回值：2字节ACK状态（0成功，1失败）
*/
static const uint8_t config_max_distance_duration[]={0xFD,0xFC,0xFB,0xFA,0x14,0x00,0x60,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x01,0x00,0x08,0x00,0x00,0x00,0x02,0x00,0x05,0x00,0x00,0x00,0x04,0x03,0x02,0x01};

/*读取参数命令
返回值：2 字节ACK 状态(0 成功， 1 失败) + 头（0xAA） + 最大距离门N(0x08) + 配
置最大运动距离门+配置最大静止距离门+ 距离门0 运动灵敏度（1字节） + ... +距离门N
运动灵敏度（1字节） + 距离门0 静止灵敏度1字节） + ...+距离门N 静止灵敏度（1字节）
+ 无人持续时间(2字节)    
*/
    
static const uint8_t config_para_read[]={0xFD,0xFC,0xFB,0xFA,0x02,0x00,0x61,0x00,0x04,0x03,0x02,0x01};
static const uint8_t config_para_read_result[]={0xFD,0xFC,0xFB,0xFA,0x1C,0x00,0x61,0x01,0x00,0x00,0xAA,0x08,0x08,0x08,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x19,0x19,0x19,0x19,0x19,0x19,0x19,0x19,0x19,0x05,0x00,0x04,0x03,0x02,0x01};

    
    
/*距离门灵敏度配置命令
此命令配置距离门的灵敏度，配置值掉电不丢失。既支持对各个距离门进行单独配置，也支持将所有
距离门同时配置成统一的数值。若同时设置所有距离门灵敏度为同一值，需将距离门值设置为0xFFFF。
命令字：0x0064
命令值：2字节距离门字+ 4字节距离门值+ 2字节运动灵敏度字+ 4字节运动灵敏度值+ 2字节静止
灵敏度字+ 4字节静止灵敏度值
返回值：2字节ACK状态（0成功， 1失败）    
*/

static const uint8_t distance_gate_sensitivity[]={0xFD,0xFC,0xFB,0xFA,0x14,0x00,0x64,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x01,0x00,0x28,0x00,0x00,0x00,0x02,0x00,0x28,0x00,0x00,0x00,0x04,0x03,0x02,0x01};  

/*
设置串口波特率
此命令用来设置模块串口的波特率，配置值掉电不丢失，配置值在重启模块后生效。
命令字：0x00A1
命令值：2字节波特率选择索引
返回值：2字节ACK状态（0成功，1失败）
波特率选择索引值波特率
0x0001 9600
0x0002 19200
0x0003 38400
0x0004 57600
0x0005 115200
0x0006 230400
0x0007 256000
0x0008 460800    
*/

static const uint8_t config_uart_baudrate[]={0xFD,0xFC,0xFB,0xFA,0x04,0x00,0xA1,0x00,0x07,0x00,0x04,0x03,0x02,0x01};


/*恢复出厂设置
此命令用来将所有配置值恢复未出厂值，配置值在重启模块后生效。
命令字：0x00A2
命令值：无
返回值：2字节ACK状态（0成功，1失败）*/


static const uint8_t restore_factory_settings[]={0xFD,0xFC,0xFB,0xFA,0x02,0x00,0xA2,0x00,0x04,0x03,0x02,0x01};
/*
重启模块
模块收到此命令，将会在应答发送完成后自动重启。
命令字：0x00A3
命令值：无
返回值：2字节ACK状态（0成功，1失败）
*/
static const uint8_t restart_radar_module[]={0xFD,0xFC,0xFB,0xFA,0x02,0x00,0xA3,0x00,0x04,0x03,0x02,0x01};


/*此命令用于查询MAC地址
命令字：0x00A5
命令值：0x0001
返回值：2字节ACK状态（0成功，1失败）+ 1字节固定类型（0x00）+3字节MAC地址（大端序）
*/

static const uint8_t get_radar_mac_addr[]={0xFD,0xFC,0xFB,0xFA,0x04,0x00,0xA5,0x00,0x01,0x00,0x04,0x03,0x02,0x01};


/*
距离分辨率设置
设置模块的距离分辨率，即每个距离门代表多远距离，配置值掉电不丢失，配置值在重启模块后生效。
可配置为每个距离门0.75m或0.2m，最大支持的距离门个数都是8。
命令字：0x00AA
命令值：2字节的距离分辨率选择索引
返回值：2字节ACK状态（0成功，1失败）
距离分辨率选择索引值         距离分辨率(每个距离门代表的距离)
     0x0000                       0.75m
     0x0001                       0.2m

*/
static const uint8_t distance_resolution_setting[]={0xFD,0xFC,0xFB,0xFA,0x04,0x00,0xAA,0x00,0x01,0x00,0x04,0x03,0x02,0x01};


/*
辅助控制功能设置
本模块自带光敏二极管，可用来检测输出光感值（请参考表15 工程模式目标数据组成），用户还可
配置开启光感辅助控制功能；
开启光感辅助控制功能，OUT脚的输出同时受雷达检测结果和光感控制逻辑的影响：
OUT脚输出从无人变为有人，需要满足：雷达检测到有人且光感辅助控制逻辑条件满足；
OUT脚输出从有人变为无人，需要满足：雷达检测到无人；
光感控制逻辑可选择检测到光感值小于设置的光感阈值，或者检测到光感值大于设置的光感阈值；
OUT脚的默认输出电平也可配置；
命令字：0x00AD
命令值：4字节的配置值
返回值：2字节ACK状态（0成功，1失败）

第一个字节                      说明
   0x00             关闭光感辅助控制功能，OUT脚输出不受光感影响
   0x01             开启光感辅助控功能，当检测光感值小于设置阈值时辅助控制条件满足第二个字节为要设置的光感阈值(范围0x00～0xFF)
   0x02             开启光感辅助控功能，当光感检测值大于设置阈值时辅助控制条件满足；第二个字节为要设置的光感阈值(范围0x00～0xFF)

第二个字节                      说明
0x00～0xFF          要设置的光感阈值(范围0～255)，默认为0x80

OUT脚默认电平配置
第三个字节配置值                说明
0x00                OUT脚默认为低电平，无目标触发时输出低电平，有目标触发时输出高电平
0x01                OUT脚默认为高电平，无目标触发时输出高电平，有目标触发时输出低电平


PS:出厂默认值为0x00，即关闭光感辅助控制功能
*/

static const uint8_t auxiliary_function_settings[]={0xFD,0xFC,0xFB,0xFA,0x06,0x00,0xAD,0x00,0x01,0x60,0x00,0x00,0x04,0x03,0x02,0x01};

/*
上报数据帧格式
帧头部      帧内数据长度 帧内数据     帧尾部
F4 F3 F2 F1   2字节       见下表A    F8 F7 F6 F5

A:帧内数据帧格式

数据类型    头部      目标数据    尾部      校验
1字节       0xAA       见表B      0x55      0x00

B:目标基本信息数据组成
目标状态     运动目标距离（厘米）   运动目标能量值    静止目标距离（厘米） 静止目标能量值     探测距离（厘米）
1字节             2字节                 1字节             2字节               1字节               2字节



目标状态值               说明
    0x00                无目标
    0x01                运动目标
    0x02                静止目标
    0x03                运动&静止目标
*/

static const uint8_t upload_data_frames[]={0xF4,0xF3,0xF2,0xF1,0x0D,0x00,0x02,0xAA,0x02,0x51,0x00,0x00,0x00,0x00,0x3B,0x00,0x00,0x55,0x00,0xF8,0xF7,0xF6,0xF5};

//static const uint8_t engineer_model_EN_frames[] ={0xFD, 0xFC ,0xFB ,0xFA, 0x02 ,0x00, 0x62, 0x00, 0x04 ,0x03 ,0x02, 0x01};
//static const uint8_t engineer_model_DIS_frames[]={0xFD, 0xFC ,0xFB ,0xFA, 0x02 ,0x00, 0x63, 0x00, 0x04 ,0x03 ,0x02, 0x01};    
#define RADAR_LL_PERIPH_SEL                   (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | \
                                               LL_PERIPH_EFM | LL_PERIPH_SRAM)

/* DMA definition */
#define RADAR_RX_DMA_UNIT                     (CM_DMA2)
#define RADAR_RX_DMA_CH                       (DMA_CH1)
#define RADAR_RX_DMA_FCG_ENABLE()             (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA2, ENABLE))
#define RADAR_RX_DMA_TRIG_SEL                 (AOS_DMA2_1)
#define RADAR_RX_DMA_TRIG_EVT_SRC             (EVT_SRC_USART1_RI)
#define RADAR_RX_DMA_RECONF_TRIG_SEL          (AOS_DMA_RC)
#define RADAR_RX_DMA_RECONF_TRIG_EVT_SRC      (EVT_SRC_AOS_STRG)
#define RADAR_RX_DMA_TC_INT                   (DMA_INT_TC_CH1)
#define RADAR_RX_DMA_TC_FLAG                  (DMA_FLAG_TC_CH1)
#define RADAR_RX_DMA_TC_IRQn                  (INT005_IRQn)
#define RADAR_RX_DMA_TC_INT_SRC               (INT_SRC_DMA2_TC1)

#define RADAR_TX_DMA_UNIT                     (CM_DMA2)
#define RADAR_TX_DMA_CH                       (DMA_CH0)
#define RADAR_TX_DMA_FCG_ENABLE()             (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA2, ENABLE))
#define RADAR_TX_DMA_TRIG_SEL                 (AOS_DMA2_0)
#define RADAR_TX_DMA_TRIG_EVT_SRC             (EVT_SRC_USART1_TI)
#define RADAR_TX_DMA_TC_INT                   (DMA_INT_TC_CH0)
#define RADAR_TX_DMA_TC_FLAG                  (DMA_FLAG_TC_CH0)
#define RADAR_TX_DMA_TC_IRQn                  (INT006_IRQn)
#define RADAR_TX_DMA_TC_INT_SRC               (INT_SRC_DMA2_TC0)

/* Timer0 unit & channel definition */
#define RADAR_TMR0_UNIT                       (CM_TMR0_1)
#define RADAR_TMR0_CH                         (TMR0_CH_A)
#define RADAR_TMR0_FCG_ENABLE()               (FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMR0_1, ENABLE))

/* USART RX/TX pin definition */
/* USART RX/TX pin definition */
#define RADAR_USART_RX_PORT                   (GPIO_PORT_A)   /* PB9: USART1_RX */
#define RADAR_USART_RX_PIN                    (GPIO_PIN_03)
#define RADAR_USART_RX_GPIO_FUNC              (GPIO_FUNC_33)

#define RADAR_USART_TX_PORT                   (GPIO_PORT_A)   /* PE6: USART1_TX */
#define RADAR_USART_TX_PIN                    (GPIO_PIN_02)
#define RADAR_USART_TX_GPIO_FUNC              (GPIO_FUNC_32)

/* USART unit definition */
#define RADAR_USART_UNIT                      (CM_USART1)
#define RADAR_USART_FCG_ENABLE()              (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART1, ENABLE))

/* USART baudrate definition */
#define RADAR_USART_BAUDRATE                  (460800UL)

/* USART timeout bits definition */
#define RADAR_USART_TIMEOUT_BITS              (200U)

/* USART interrupt definition */
#define RADAR_USART_TX_CPLT_IRQn              (INT007_IRQn)
#define RADAR_USART_TX_CPLT_INT_SRC           (INT_SRC_USART1_TCI)

#define RADAR_USART_RX_ERR_IRQn               (INT008_IRQn)
#define RADAR_USART_RX_ERR_INT_SRC            (INT_SRC_USART1_EI)

#define RADAR_USART_RX_TIMEOUT_IRQn           (INT009_IRQn)
#define RADAR_USART_RX_TIMEOUT_INT_SRC        (INT_SRC_USART1_RTO)

/* Application frame length max definition */
#define RADAR_RINGBUF_MAX               (900U)
#define RADAR_UART_FRAME_MAX            (45U)





/*******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/*******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/*******************************************************************************
 * Local variable definitions ('static')
 ******************************************************************************/

static __IO uint16_t radar_RxLen;
static __align(64) uint8_t radarscandata[RADAR_RINGBUF_MAX];
static __align(64) uint8_t radar_uart_input[RADAR_UART_FRAME_MAX];
static __align(64) uint8_t radar_pdu_lentg[32];
stc_radar_scan_data_t  HLKLD2410_Radar;
static stc_ring_buf_t g_radarRingBuf;

void radar_init_by_uart(void)
{
    uint8_t recvbuf[256];
    memset(recvbuf,0,sizeof(recvbuf));
    USART_UART_Trans(RADAR_USART_UNIT,config_enable_set,sizeof(config_enable_set),5000);
    USART_UART_Receive(RADAR_USART_UNIT,recvbuf,18,1000);
    DDL_DelayUS(100);
    USART_UART_Trans(RADAR_USART_UNIT,enable_engineer_mode,sizeof(enable_engineer_mode),5000);
    DDL_DelayUS(100);
    USART_UART_Receive(RADAR_USART_UNIT,recvbuf,14,1000);
    DDL_DelayUS(100);
    
    
    USART_UART_Trans(RADAR_USART_UNIT,config_disable_set,sizeof(config_disable_set),5000);
    DDL_DelayUS(100);
    USART_UART_Receive(RADAR_USART_UNIT,recvbuf,14,1000);
    DDL_DelayUS(100);
    
    Radar_DMA_Trans(config_enable_set,sizeof(config_enable_set));
    DDL_DelayMS(150);
    Radar_DMA_Trans(enable_engineer_mode,sizeof(enable_engineer_mode));
    DDL_DelayMS(150);
    Radar_DMA_Trans(config_disable_set,sizeof(config_disable_set));

}    
void Board_LED_Init(void)
{

    stc_gpio_init_t stcGpioInit;

    /* configuration structure initialization */
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinState = PIN_STAT_RST;
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    stcGpioInit.u16PinDrv = PIN_MID_DRV;
    GPIO_SetDebugPort(GPIO_PIN_SWO,DISABLE);
    /* Initialize LED pin */
    (void)GPIO_Init(RADAR_BOARD_LED_G_PORT,RADAR_BOARD_LED_G_PIN,&stcGpioInit);
	  (void)GPIO_Init(BOARD_LED_1_PORT,BOARD_LED_1_PIN,&stcGpioInit);
	  (void)GPIO_Init(BOARD_LED_1_PORT,BOARD_LED_2_PIN,&stcGpioInit);
    
    
}

void Board_LED_On(void)
{
   GPIO_SetPins(RADAR_BOARD_LED_G_PORT,RADAR_BOARD_LED_G_PIN);
} 

void Board_LED_Off(void)
{
   GPIO_ResetPins(RADAR_BOARD_LED_G_PORT,RADAR_BOARD_LED_G_PIN);
}

void Board_LED_Toggle(void)
{
   GPIO_TogglePins(RADAR_BOARD_LED_G_PORT,RADAR_BOARD_LED_G_PIN);
}


/*******************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/**
 * @brief  USART RX timeout IRQ callback.
 * @param  None
 * @retval None
 */
static void RADAR_USART_RxTimeout_IrqCallback(void)
{
        radar_RxLen =(uint16_t)DMA_GetTransCount(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH);
        BUF_Write(&g_radarRingBuf,radar_uart_input,sizeof(radar_uart_input));
        /* Trigger for re-config USART RX DMA */
        AOS_SW_Trigger();
        TMR0_Stop(RADAR_TMR0_UNIT, RADAR_TMR0_CH);
        USART_ClearStatus(RADAR_USART_UNIT, USART_FLAG_RX_TIMEOUT);
}

/**
 * @brief  USART TX complete IRQ callback function.
 * @param  None
 * @retval None
 */
static void RADAR_USART_TxComplete_IrqCallback(void)
{
    USART_FuncCmd(RADAR_USART_UNIT, (USART_TX | USART_INT_TX_CPLT), DISABLE);

    TMR0_Stop(RADAR_TMR0_UNIT, RADAR_TMR0_CH);

    USART_ClearStatus(RADAR_USART_UNIT, USART_FLAG_RX_TIMEOUT);

    USART_FuncCmd(RADAR_USART_UNIT, USART_RX_TIMEOUT, ENABLE);

    USART_ClearStatus(RADAR_USART_UNIT, USART_FLAG_TX_CPLT);
}

/**
 * @brief  USART RX error IRQ callback.
 * @param  None
 * @retval None
 */
static void RADAR_USART_RxError_IrqCallback(void)
{
    (void)USART_ReadData(RADAR_USART_UNIT);

    USART_ClearStatus(RADAR_USART_UNIT, (USART_FLAG_PARITY_ERR | USART_FLAG_FRAME_ERR | USART_FLAG_OVERRUN));
}
/**
 * @brief  DMA transfer complete IRQ callback function.
 * @param  None
 * @retval None
 */
static void RADAR_RX_DMA_TC_IrqCallback(void)
{
   // Rardar_RxFrameEnd = SET;
   // radar_RxLen = RADAR_APP_FRAME_LEN_MAX;

   // USART_FuncCmd(RADAR_USART_UNIT, USART_RX_TIMEOUT, DISABLE);

    DMA_ClearTransCompleteStatus(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_TC_FLAG);
}

/**
 * @brief  DMA transfer complete IRQ callback function.
 * @param  None
 * @retval None
 */
static void RADAR_TX_DMA_TC_IrqCallback(void)
{
    USART_FuncCmd(RADAR_USART_UNIT, USART_INT_TX_CPLT, ENABLE);

    DMA_ClearTransCompleteStatus(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_FLAG);
}

/**
 * @brief  Initialize DMA.
 * @param  None
 * @retval int32_t:
 *           - LL_OK:                   Initialize successfully.
 *           - LL_ERR_INVD_PARAM:       Initialization parameters is invalid.
 */
static int32_t RADAR_DMA_Config(void)
{
    int32_t i32Ret;
    stc_dma_init_t stcDmaInit;
    stc_dma_llp_init_t stcDmaLlpInit;
    stc_irq_signin_config_t stcIrqSignConfig;
    static stc_dma_llp_descriptor_t stcLlpDesc;

    /* DMA&AOS FCG enable */
    RADAR_RX_DMA_FCG_ENABLE();
    RADAR_TX_DMA_FCG_ENABLE();
    FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

    /* USART_RX_DMA */
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32IntEn = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize = 1UL;
    stcDmaInit.u32TransCount = ARRAY_SZ(radar_uart_input);
    stcDmaInit.u32DataWidth = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr = (uint32_t)radar_uart_input;
    stcDmaInit.u32SrcAddr = (uint32_t)(&RADAR_USART_UNIT->RDR);
    stcDmaInit.u32SrcAddrInc = DMA_SRC_ADDR_FIX;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_INC;
    i32Ret = DMA_Init(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, &stcDmaInit);
    if (LL_OK == i32Ret) {
        (void)DMA_LlpStructInit(&stcDmaLlpInit);
        stcDmaLlpInit.u32State = DMA_LLP_ENABLE;
        stcDmaLlpInit.u32Mode  = DMA_LLP_WAIT;
        stcDmaLlpInit.u32Addr  = (uint32_t)&stcLlpDesc;
        (void)DMA_LlpInit(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, &stcDmaLlpInit);

        stcLlpDesc.SARx   = stcDmaInit.u32SrcAddr;
        stcLlpDesc.DARx   = stcDmaInit.u32DestAddr;
        stcLlpDesc.DTCTLx = (stcDmaInit.u32TransCount << DMA_DTCTL_CNT_POS) | (stcDmaInit.u32BlockSize << DMA_DTCTL_BLKSIZE_POS);;
        stcLlpDesc.LLPx   = (uint32_t)&stcLlpDesc;
        stcLlpDesc.CHCTLx = stcDmaInit.u32SrcAddrInc | stcDmaInit.u32DestAddrInc | stcDmaInit.u32DataWidth |  \
                            stcDmaInit.u32IntEn      | stcDmaLlpInit.u32State    | stcDmaLlpInit.u32Mode;

        DMA_ReconfigLlpCmd(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, ENABLE);
        DMA_ReconfigCmd(RADAR_RX_DMA_UNIT, ENABLE);
        AOS_SetTriggerEventSrc(RADAR_RX_DMA_RECONF_TRIG_SEL, RADAR_RX_DMA_RECONF_TRIG_EVT_SRC);

        stcIrqSignConfig.enIntSrc = RADAR_RX_DMA_TC_INT_SRC;
        stcIrqSignConfig.enIRQn  = RADAR_RX_DMA_TC_IRQn;
        stcIrqSignConfig.pfnCallback = &RADAR_RX_DMA_TC_IrqCallback;
        (void)INTC_IrqSignIn(&stcIrqSignConfig);
        NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
        NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
        NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

        AOS_SetTriggerEventSrc(RADAR_RX_DMA_TRIG_SEL, RADAR_RX_DMA_TRIG_EVT_SRC);

        DMA_Cmd(RADAR_RX_DMA_UNIT, ENABLE);
        DMA_TransCompleteIntCmd(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_TC_INT, ENABLE);
        (void)DMA_ChCmd(RADAR_RX_DMA_UNIT, RADAR_RX_DMA_CH, ENABLE);
    }

    /* USART_TX_DMA */
    stcDmaInit.u32IntEn = DMA_INT_ENABLE;
    stcDmaInit.u32BlockSize = 1UL;
    (void)DMA_StructInit(&stcDmaInit);
    stcDmaInit.u32TransCount = ARRAY_SZ(radar_uart_input);
    stcDmaInit.u32DataWidth = DMA_DATAWIDTH_8BIT;
    stcDmaInit.u32DestAddr = (uint32_t)(&RADAR_USART_UNIT->TDR);
    stcDmaInit.u32SrcAddr = (uint32_t)radar_uart_input;
    stcDmaInit.u32SrcAddrInc = DMA_SRC_ADDR_INC;
    stcDmaInit.u32DestAddrInc = DMA_DEST_ADDR_FIX;
    i32Ret = DMA_Init(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_CH, &stcDmaInit);
    if (LL_OK == i32Ret) {
        stcIrqSignConfig.enIntSrc = RADAR_TX_DMA_TC_INT_SRC;
        stcIrqSignConfig.enIRQn  = RADAR_TX_DMA_TC_IRQn;
        stcIrqSignConfig.pfnCallback = &RADAR_TX_DMA_TC_IrqCallback;
        (void)INTC_IrqSignIn(&stcIrqSignConfig);
        NVIC_ClearPendingIRQ(stcIrqSignConfig.enIRQn);
        NVIC_SetPriority(stcIrqSignConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
        NVIC_EnableIRQ(stcIrqSignConfig.enIRQn);

        AOS_SetTriggerEventSrc(RADAR_TX_DMA_TRIG_SEL, RADAR_TX_DMA_TRIG_EVT_SRC);

        DMA_Cmd(RADAR_TX_DMA_UNIT, ENABLE);
        DMA_TransCompleteIntCmd(RADAR_TX_DMA_UNIT, RADAR_TX_DMA_TC_INT, ENABLE);
    }

    return i32Ret;
}

/**
 * @brief  Configure TMR0.
 * @param  [in] u16TimeoutBits:         Timeout bits
 * @retval None
 */
static void RADAR_TMR0_Config(uint16_t u16TimeoutBits)
{
    uint16_t u16Div;
    uint16_t u16Delay;
    uint16_t u16CompareValue;
    stc_tmr0_init_t stcTmr0Init;

    RADAR_TMR0_FCG_ENABLE();

    /* Initialize TMR0 base function. */
    stcTmr0Init.u32ClockSrc = TMR0_CLK_SRC_XTAL32;
    stcTmr0Init.u32ClockDiv = TMR0_CLK_DIV8;
    stcTmr0Init.u32Func     = TMR0_FUNC_CMP;
    if (TMR0_CLK_DIV1 == stcTmr0Init.u32ClockDiv) {
        u16Delay = 7U;
    } else if (TMR0_CLK_DIV2 == stcTmr0Init.u32ClockDiv) {
        u16Delay = 5U;
    } else if ((TMR0_CLK_DIV4 == stcTmr0Init.u32ClockDiv) || \
               (TMR0_CLK_DIV8 == stcTmr0Init.u32ClockDiv) || \
               (TMR0_CLK_DIV16 == stcTmr0Init.u32ClockDiv)) {
        u16Delay = 3U;
    } else {
        u16Delay = 2U;
    }

    u16Div = (uint16_t)1U << (stcTmr0Init.u32ClockDiv >> TMR0_BCONR_CKDIVA_POS);
    u16CompareValue = ((u16TimeoutBits + u16Div - 1U) / u16Div) - u16Delay;
    stcTmr0Init.u16CompareValue = u16CompareValue;
    (void)TMR0_Init(RADAR_TMR0_UNIT, RADAR_TMR0_CH, &stcTmr0Init);

    TMR0_HWStartCondCmd(RADAR_TMR0_UNIT, RADAR_TMR0_CH, ENABLE);
    TMR0_HWClearCondCmd(RADAR_TMR0_UNIT, RADAR_TMR0_CH, ENABLE);
}



/**
 * @brief  Main function of UART DMA project
 * @param  None
 * @retval int32_t return value, if needed
 */
void Rardar_init(void)
{
    stc_usart_uart_init_t stcUartInit;
    stc_irq_signin_config_t stcIrqSigninConfig;

    /* Initialize DMA. */
    
    (void)RADAR_DMA_Config();

    /* Initialize TMR0. */
    RADAR_TMR0_Config(RADAR_USART_TIMEOUT_BITS);

    /* Configure USART RX/TX pin. */
    GPIO_SetFunc(RADAR_USART_RX_PORT, RADAR_USART_RX_PIN, RADAR_USART_RX_GPIO_FUNC);
    GPIO_SetFunc(RADAR_USART_TX_PORT, RADAR_USART_TX_PIN, RADAR_USART_TX_GPIO_FUNC);

    /* Enable peripheral clock */
    RADAR_USART_FCG_ENABLE();

    /* Initialize UART. */
    (void)USART_UART_StructInit(&stcUartInit);
    stcUartInit.u32ClockDiv = USART_CLK_DIV4;
    stcUartInit.u32CKOutput = USART_CK_OUTPUT_ENABLE;
    stcUartInit.u32Baudrate = RADAR_USART_BAUDRATE;
    stcUartInit.u32OverSampleBit = USART_OVER_SAMPLE_8BIT;
    if (LL_OK != USART_UART_Init(RADAR_USART_UNIT, &stcUartInit, NULL)) {
       // BSP_LED_On(LED_RED);
        for (;;) {
        }
    }

    /* Register TX complete IRQ handler. */
    stcIrqSigninConfig.enIRQn = RADAR_USART_TX_CPLT_IRQn;
    stcIrqSigninConfig.enIntSrc = RADAR_USART_TX_CPLT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &RADAR_USART_TxComplete_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* Register RX error IRQ handler. */
    stcIrqSigninConfig.enIRQn = RADAR_USART_RX_ERR_IRQn;
    stcIrqSigninConfig.enIntSrc = RADAR_USART_RX_ERR_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &RADAR_USART_RxError_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

    /* Register RX timeout IRQ handler. */
    stcIrqSigninConfig.enIRQn = RADAR_USART_RX_TIMEOUT_IRQn;
    stcIrqSigninConfig.enIntSrc = RADAR_USART_RX_TIMEOUT_INT_SRC;
    stcIrqSigninConfig.pfnCallback = &RADAR_USART_RxTimeout_IrqCallback;
    (void)INTC_IrqSignIn(&stcIrqSigninConfig);
    NVIC_ClearPendingIRQ(stcIrqSigninConfig.enIRQn);
    NVIC_SetPriority(stcIrqSigninConfig.enIRQn, DDL_IRQ_PRIO_DEFAULT);
    NVIC_EnableIRQ(stcIrqSigninConfig.enIRQn);

   
    
    Ext_Init();
    (void)BUF_Init(&g_radarRingBuf, radarscandata, sizeof(radarscandata));
    USART_FuncCmd(RADAR_USART_UNIT, (USART_TX | USART_RX | USART_INT_RX | USART_RX_TIMEOUT | \
                               USART_INT_RX_TIMEOUT), ENABLE);

   radar_init_by_uart();

   
        
    
}

void Radar_DMA_Trans(const void *pvBuf, uint32_t u32Len)
{
  DMA_SetSrcAddr    (RADAR_TX_DMA_UNIT,RADAR_TX_DMA_CH,(uint32_t)pvBuf);
  DMA_SetTransCount (RADAR_TX_DMA_UNIT,RADAR_TX_DMA_CH, u32Len);
  (void)DMA_ChCmd   (RADAR_TX_DMA_UNIT,RADAR_TX_DMA_CH, ENABLE);
  USART_FuncCmd     (RADAR_USART_UNIT ,USART_TX       , ENABLE);

} 
void radar_scan_update(void)
{
 uint32_t head=RADARFRAMEHEAD;
 uint32_t tail=RADARFRAMEEND;     
 uint8_t radartempring[RADAR_UART_FRAME_MAX]={0};
 BUF_Read(&g_radarRingBuf,radartempring,RADAR_UART_FRAME_MAX);  
 engineer_frame_t *radar_pdu_package=(engineer_frame_t *)radartempring ;
if((memcmp(&radar_pdu_package->framehead[0],&head,4)==0 && memcmp(&radar_pdu_package->frameend[0],&tail,4)==0))
 {  
   /*
     HLKLD2410_Radar.target_state=radar_pdu_package->target_state;  
     HLKLD2410_Radar.moving_target_distance     =  (radar_pdu_package->moving_distance[1])|(radar_pdu_package->moving_distance[0]);
     HLKLD2410_Radar.moving_target_energy       =  radar_pdu_package->moving_energy;
     HLKLD2410_Radar.stationary_target_distance =  (radar_pdu_package->sta_distance[1])| (radar_pdu_package->sta_distance[0]);
     HLKLD2410_Radar.stationary_target_energy   =  radar_pdu_package->sta_energy;    
     HLKLD2410_Radar.detection_distance         =  (radar_pdu_package->detection_range[1])| (radar_pdu_package->detection_range[0]);
     HLKLD2410_Radar.pinout                     =  GPIO_ReadInputPins(Radar_PORT,Radar_PIN);
     //if(HLKLD2410_Radar.pinout && HLKLD2410_Radar.target_state>=2 && HLKLD2410_Radar.detection_distance>20)
     HLKLD2410_Radar.targeted                   =  GPIO_ReadInputPins(Radar_PORT,Radar_PIN);
   */
     MEM_ZERO_STRUCT(HLKLD2410_Radar); 
     }
else
    {
    // radar_init_by_uart();
     MEM_ZERO_STRUCT(HLKLD2410_Radar);
     (void)BUF_Init(&g_radarRingBuf, radarscandata, sizeof(radarscandata));   



    }    
/*    
 if(HLKLD2410_Radar.pinout)    
 {
  HLKLD2410_Radar.targeted=1;
     
 } 
else
 {
  HLKLD2410_Radar.targeted=0;
 
 } 
*/
}
void Check_Radar_state(void)
{
    
   if (BUF_UsedSize(&g_radarRingBuf)>=RADAR_UART_FRAME_MAX)
    {
        radar_scan_update();

    }
   else
   {
  // Radar_Led_update();
 
   }       

}    
   
int32_t get_pdu_len(uint8_t *pdulen,uint8_t flag)
{

   
    pdulen[0]=sizeof(config_enable_set);
    pdulen[1]=sizeof(config_disable_set);
    pdulen[2]=sizeof(config_max_distance_duration);
    pdulen[3]=sizeof(config_para_read);
    pdulen[4]=sizeof(config_para_read_result);
    pdulen[5]=sizeof(distance_gate_sensitivity);
    pdulen[6]=sizeof(config_uart_baudrate);
    pdulen[7]=sizeof(restore_factory_settings);
    pdulen[8]=sizeof(restart_radar_module);
    pdulen[9]=sizeof(get_radar_mac_addr);
    pdulen[10]=sizeof(distance_resolution_setting);
    pdulen[11]=sizeof(auxiliary_function_settings);
    pdulen[12]=sizeof(upload_data_frames);
    pdulen[13]=sizeof(enable_engineer_mode);
    pdulen[14]=sizeof(disable_engineer_mode);
  if(flag)
  {  
    USART_UART_Trans(USART_UNIT,config_enable_set,                 pdulen[0],500);
    USART_UART_Trans(USART_UNIT,config_disable_set,                pdulen[1],500);
    USART_UART_Trans(USART_UNIT,config_max_distance_duration,      pdulen[2],500);
    USART_UART_Trans(USART_UNIT,config_para_read,                  pdulen[3],500);
    USART_UART_Trans(USART_UNIT,config_para_read_result,           pdulen[4],500);
    USART_UART_Trans(USART_UNIT,distance_gate_sensitivity,         pdulen[5],500);
    USART_UART_Trans(USART_UNIT,config_uart_baudrate,              pdulen[6],500);
    USART_UART_Trans(USART_UNIT,restore_factory_settings,          pdulen[7],500);
    USART_UART_Trans(USART_UNIT,restart_radar_module,              pdulen[8],500);
    USART_UART_Trans(USART_UNIT,get_radar_mac_addr,                pdulen[9],500);
    USART_UART_Trans(USART_UNIT,distance_resolution_setting,       pdulen[10],500);
    USART_UART_Trans(USART_UNIT,auxiliary_function_settings,       pdulen[11],500);
    USART_UART_Trans(USART_UNIT,upload_data_frames,                pdulen[12],500);
    USART_UART_Trans(USART_UNIT,enable_engineer_mode,              pdulen[13],500);  
    USART_UART_Trans(USART_UNIT,disable_engineer_mode,             pdulen[14],500);  
  }
    
   return LL_OK;
} 

void config_frame_init(void)
{

  get_pdu_len(radar_pdu_lentg,0);

}  
  
/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/

/*******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
