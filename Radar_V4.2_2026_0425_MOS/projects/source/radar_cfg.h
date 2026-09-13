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

/* 波特率策略:
 *   0      = 自适应探测(依次试下面 3 个候选, 谁能回 ACK 就用谁)
 *   非 0   = 固定使用该值, 跳过探测(已知模块波特率时用, 最省事)
 * 现场若怀疑模块不在候选波特率里, 直接在这里填 460800UL / 256000UL 逐个试。
 */
#define RADAR_BAUD_FORCE                (0UL)

/* 自适应时: 没等到 ACK 但已收到这么多"合法帧"也算波特率正确(如 TX 方向没通) */
#define RADAR_BAUD_LOCK_FRAMES          (3U)

/* 上电自适应波特率探测顺序: 覆盖协议表 6 的全部 8 档。
 * 注意: 模块波特率是"掉电保存"的配置项(出厂默认 256000, 索引 0x0007), 一旦被
 * 上位机/APP/0x00A1 改过, 上电就是改过的值 —— 所以不能假设它是 256000。 */
#define RADAR_BAUD_TABLE                { 256000UL, 460800UL, 115200UL, 9600UL, 19200UL, \
                                          38400UL, 57600UL, 230400UL }
#define RADAR_BAUD_TABLE_CNT            (8U)        /* 协议表6 全部 8 档(无 921600) */
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

/* ============================ 调试输出(上板验证用, 事后删除) ============================
 * RADAR_DBG_EN : 1 = 打开(临时, 见 docs/hc32_radar_bringup.md), 0 = 关闭(空实现/不占空间)
 *               验证通过后置 0, 或整体删除 radar_dbg.c/.h 并去掉 main.c 里的调用
 * SINK_KEIL    : 结构体 g_radar_dbg + 字符串 g_radar_dbg_line, Keil Watch 直接看(默认开, 零成本)
 * SINK_ITM     : SWO/ITM 输出, 需 Keil 里打开 Trace 并接 SWO 线(默认关)
 * SINK_RS485   : 每 RADAR_DBG_PERIOD_MS 从 RS485 主机口(USART4, 460800)打一行 ASCII(默认关)
 *               —— 与 STM32 挂同一总线时属抢总线, 只在单独给本板上电+USB-RS485 直连时开
 */
#define RADAR_DBG_EN                    (1U)
#define RADAR_DBG_PERIOD_MS             (500U)      /* 状态行刷新周期 */
#define RADAR_DBG_RX_STALL_MS           (3000U)     /* 收字节停滞多久报一次事件 */
#define RADAR_DBG_SINK_ITM              (0U)
#define RADAR_DBG_SINK_RS485            (0U)
/* 把模块波特率改成 460800(一次性, 默认关) —— 效果等同于"改出厂波特率":
 *   协议 §2.2.9/§2.2.11: 0x00A1 写索引(0x0008=460800) -> 掉电不丢失, 重启后生效。
 *   填 0 = 不做; 填 8 = 上电后自动执行: 使能配置 -> 0x00A1(8) -> 0x00A3 重启模块
 *   -> 800ms 后驱动切到 RADAR_DBG_SET_BAUD_VALUE -> 自检 2.5s(看有无上报帧)
 *   -> 成功记 "baud verify OK 460800"; 失败自动回退旧波特率并记事件。
 *   全程结果看 g_radar_dbg_evt。每块模块只需做一次, 成功后本项改回 0。
 *   注意: 厂家固件里的"出厂默认 256000"改不了(0x00A2 恢复出厂会回到 256000);
 *         我们只是把 460800 写进模块 flash, 让这块模块每次上电都从 460800 开始。 */
#define RADAR_DBG_SET_BAUD_IDX          (0U)
#define RADAR_DBG_SET_BAUD_VALUE        (460800UL)

#endif /* __RADAR_CFG_H__ */
