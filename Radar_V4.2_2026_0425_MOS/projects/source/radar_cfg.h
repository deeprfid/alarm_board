/*******************************************************************************
 * radar_cfg.h -- LD2410C 雷达模块驱动配置(全部可调项集中在此文件)
 *
 * 分层: radar_cfg.h(配置) / radar_port.c(USART+DMA) / radar_frame.c(分帧)
 *       / radar_proto.c(协议) / radar.c(服务与命令)
 ******************************************************************************/
#ifndef __RADAR_CFG_H__
#define __RADAR_CFG_H__

#include "hc32_ll.h"

/* ============================ 模块 OUT 引脚 ============================
 * 高有效: 1 = 有人。三路 OUT 现由 bsp_gpio.c / bsp_report.c / common.c 使用,
 * 宏名与旧工程保持一致, 便于平滑过渡。
 */
#define RADAR_PORT0                     (GPIO_PORT_C)
#define RADAR_PIN0                      (GPIO_PIN_14)
#define RADAR_PORT1                     (GPIO_PORT_C)
#define RADAR_PIN1                      (GPIO_PIN_13)
#define RADAR_PORT2                     (GPIO_PORT_H)
#define RADAR_PIN2                      (GPIO_PIN_02)

/* 与串口连接同一模块的 OUT 脚(供串口/OUT 双路判定; 默认第 1 路) */
#define RADAR_UART_DEV_OUT_PORT         (RADAR_PORT0)
#define RADAR_UART_DEV_OUT_PIN          (RADAR_PIN0)

/* ============================ 雷达串口 ============================
 * 说明: 端口/引脚按旧工程 bsp_radar.c 的宏定义(PA2 = TX, PA3 = RX)。
 *       若与 PCB 实际不符, 只改下面这 8 行即可, 其它代码不依赖具体引脚。
 *       注意: bsp_rs485.h 里的 USART_UNIT(CM_USART4 / PB6 / PB7) 是与 STM32 的
 *       RS485 主机口, 与雷达口无关, 两者不能混用。
 */
#define RADAR_UART_UNIT                 (CM_USART1)
#define RADAR_UART_FCG_ENABLE()         (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART1, ENABLE))

#define RADAR_UART_TX_PORT              (GPIO_PORT_A)   /* PA2 */
#define RADAR_UART_TX_PIN               (GPIO_PIN_02)
#define RADAR_UART_TX_FUNC              (GPIO_FUNC_32)

#define RADAR_UART_RX_PORT              (GPIO_PORT_A)   /* PA3 */
#define RADAR_UART_RX_PIN               (GPIO_PIN_03)
#define RADAR_UART_RX_FUNC              (GPIO_FUNC_33)

/* 上电自适应波特率探测顺序(LD2410C 出厂默认 256000) */
#define RADAR_BAUD_TABLE                { 256000UL, 460800UL, 115200UL }
#define RADAR_BAUD_TABLE_CNT            (3U)
#define RADAR_BAUD_FALLBACK             (256000UL)

/* ============================ RX DMA: USART1_RI -> DMA2 CH1 ============================ */
#define RADAR_RX_DMA_UNIT               (CM_DMA2)
#define RADAR_RX_DMA_CH                 (DMA_CH1)
#define RADAR_RX_DMA_FCG_ENABLE()       (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA2, ENABLE))
#define RADAR_RX_DMA_TRIG_SEL           (AOS_DMA2_1)
#define RADAR_RX_DMA_TRIG_EVT_SRC       (EVT_SRC_USART1_RI)
#define RADAR_RX_DMA_RECONF_TRIG_SEL    (AOS_DMA_RC)
#define RADAR_RX_DMA_RECONF_TRIG_EVT_SRC (EVT_SRC_AOS_STRG)
#define RADAR_RX_DMA_TC_INT             (DMA_INT_TC_CH1)
#define RADAR_RX_DMA_TC_FLAG            (DMA_FLAG_TC_CH1)
#define RADAR_RX_DMA_TC_IRQn            (INT005_IRQn)
#define RADAR_RX_DMA_TC_INT_SRC         (INT_SRC_DMA2_TC1)

/* ============================ TX DMA: DMA2 CH0 -> USART1_TI ============================ */
#define RADAR_TX_DMA_UNIT               (CM_DMA2)
#define RADAR_TX_DMA_CH                 (DMA_CH0)
#define RADAR_TX_DMA_FCG_ENABLE()       (FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_DMA2, ENABLE))
#define RADAR_TX_DMA_TRIG_SEL           (AOS_DMA2_0)
#define RADAR_TX_DMA_TRIG_EVT_SRC       (EVT_SRC_USART1_TI)
#define RADAR_TX_DMA_TC_INT             (DMA_INT_TC_CH0)
#define RADAR_TX_DMA_TC_FLAG            (DMA_FLAG_TC_CH0)
#define RADAR_TX_DMA_TC_IRQn            (INT006_IRQn)
#define RADAR_TX_DMA_TC_INT_SRC         (INT_SRC_DMA2_TC0)

/* ============================ TMR0: 接收超时(帧间隔) ============================ */
#define RADAR_TMR0_UNIT                 (CM_TMR0_1)
#define RADAR_TMR0_CH                   (TMR0_CH_A)
#define RADAR_TMR0_FCG_ENABLE()         (FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMR0_1, ENABLE))
#define RADAR_RX_TIMEOUT_BITS           (100U)      /* 约 100 bit 时间的空闲判定 */

/* ============================ USART 中断 ============================ */
#define RADAR_UART_TX_CPLT_IRQn         (INT007_IRQn)
#define RADAR_UART_TX_CPLT_INT_SRC      (INT_SRC_USART1_TCI)
#define RADAR_UART_RX_ERR_IRQn          (INT008_IRQn)
#define RADAR_UART_RX_ERR_INT_SRC       (INT_SRC_USART1_EI)
#define RADAR_UART_RX_TIMEOUT_IRQn      (INT009_IRQn)
#define RADAR_UART_RX_TIMEOUT_INT_SRC   (INT_SRC_USART1_RTO)

/* ============================ 缓冲与超时 ============================ */
#define RADAR_RX_WIN                    (256U)      /* DMA 接收窗口 */
#define RADAR_RX_RING_SIZE              (512U)      /* 软件环形缓冲 */
#define RADAR_TX_MAX                    (32U)       /* 单条命令帧最大长度 */
#define RADAR_FRAME_MAX                 (64U)       /* 单帧最大长度(工程模式 45B) */
#define RADAR_CMD_TIMEOUT_MS            (200U)      /* 命令等待 ACK 超时 */
#define RADAR_BAUD_PROBE_TIMEOUT_MS     (300U)      /* 每个波特率探测超时 */
#define RADAR_FRAME_GAP_MS              (20U)       /* 半包超时(帧内空闲复位) */
#define RADAR_REPORT_STALE_MS           (1000U)     /* 上报数据过期(串口在线判定) */

/* ============================ 设备数量 ============================ */
#define RADAR_DEV_CNT                   (1U)        /* 第一版只调通 1 路串口雷达 */

#endif /* __RADAR_CFG_H__ */
