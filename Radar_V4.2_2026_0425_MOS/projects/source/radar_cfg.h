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

/* ============================ 雷达口目标波特率(唯一需要关心的项) ============================
 * 本产品统一用 460800。驱动上电流程(全自动、幂等):
 *   ① 自适应: 先不管模块当前是多少, 逐档试出它当前的波特率(判据 = 能收到上报帧);
 *   ② 已是 RADAR_BAUD_TARGET  -> 什么都不做;
 *      否则在这个波特率下发 0x00A1, 把**模块**的波特率改成 RADAR_BAUD_TARGET
 *      (写进模块 flash, 掉电不丢失);
 *   ③ 发 0x00A3 重启模块生效 -> 500ms 后重跑一遍自适应复检 -> 又锁定 460800 = 成功;
 *   ④ 从此每次上电: 模块自己就是 460800, 自适应第一档(460800)就命中, 不再写模块。
 * 填 0 = 不自动配置, 只自适应跟随模块当前的波特率。
 */
#define RADAR_BAUD_TARGET               (460800UL)

#define RADAR_BAUD_LOCK_FRAMES          (3U)

/* 上电自适应波特率探测顺序: 覆盖协议表 6 的全部 8 档。
 * 注意: 模块波特率是"掉电保存"的配置项(出厂默认 256000, 索引 0x0007), 一旦被
 * 上位机/APP/0x00A1 改过, 上电就是改过的值 —— 所以不能假设它是 256000。 */
#define RADAR_BAUD_TABLE                { 460800UL, 256000UL, 115200UL, 9600UL, 19200UL, \
                                          38400UL, 57600UL, 230400UL }
#define RADAR_BAUD_TABLE_CNT            (8U)        /* 协议表6 全部 8 档(无 921600);
                                                     * 460800 放第一档: 配置好之后一档命中,
                                                     * 其余档只用于兼容"还没配置过"的模块 */
#define RADAR_BAUD_FALLBACK             (256000UL)
/* 波特率 -> 协议表 6 索引(0x00A1 用), 索引 = 位置 + 1 */
#define RADAR_BAUD_IDX_TABLE            { 9600UL, 19200UL, 38400UL, 57600UL, 115200UL, 230400UL, 256000UL, 460800UL }
#define RADAR_BAUD_IDX_TABLE_CNT        (8U)

/* 产线配置(内部): 重启模块后等多久再复检(RADAR_BAUD_TARGET 非 0 时生效) */
#define RADAR_PROV_RESTART_MS           (500U)      /* 模块重启后等多久开始复检(复检=重跑自适应, 自带 1s 启动延时) */

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
#define RADAR_TX_TIMEOUT_MS             (50U)       /* 发送完成兜底: 超过该时间没等到完成中断就复位 TX 通路 */
#define RADAR_BAUD_PROBE_TIMEOUT_MS     (300U)      /* 每个波特率探测超时 */
#define RADAR_PROBE_BOOT_MS             (1000U)     /* 上电后等模块启动完成再开始探测 */
#define RADAR_PROBE_VERIFY_MS           (1000U)     /* 收到 ACK 后, 用"上报帧"验证该波特率的等待时间 */
#define RADAR_FRAME_GAP_MS              (20U)       /* 半包超时(帧内空闲复位) */
#define RADAR_REPORT_STALE_MS           (1000U)     /* 上报数据过期(串口在线判定) */

/* ============================ 设备数量 ============================ */
#define RADAR_DEV_CNT                   (1U)        /* 第一版只调通 1 路串口雷达 */

/* ============================ A. 探测行为参数(掉电保存, 按现场调) ============================
 * RADAR_PARAM_EN = 1: 上电(自适应锁定波特率后)自动把下面这组参数写进模块, **幂等** ——
 *   先 0x0061/0x00AE 读回当前配置与目标逐项比对, 只写不一致的项, 全一致则一条命令都不发;
 *   写完再读回复检, 结果见 radar_param_state() (4 = 成功/本来就一致, 5 = 失败)。
 * 数值按现场调; 每块模块只需配置一次(参数存在模块 flash 里)。
 * 注意: 距离分辨率(0x00AA)不在此列 —— 它要重启模块才生效, 用 radar_set_resolution() 手动设。
 * 光感辅助(0x00AD): 本产品不启用光感, 且 OUT 默认电平必须为 0(有人=高), 否则有人/无人会反。
 */
#define RADAR_PARAM_EN                  (0U)        /* 0 = 不自动配置(只读); 1 = 上电自动配置 */
#define RADAR_PARAM_MAX_MOVE_GATE       (3U)        /* 最大运动距离门 2~8 (0.75m/门) */
#define RADAR_PARAM_MAX_STILL_GATE      (3U)        /* 最大静止距离门 2~8 */
#define RADAR_PARAM_NO_BODY_SEC         (3U)        /* 无人持续时间(秒), 出厂 5 */
#define RADAR_PARAM_AUX_MODE            (0U)        /* 0 关闭光感辅助 / 1 光感<阈值 / 2 光感>阈值 */
#define RADAR_PARAM_AUX_THRESHOLD       (0x80U)     /* 光感阈值 0~255 */
#define RADAR_PARAM_AUX_OUT_LEVEL       (0U)        /* OUT 默认电平: 0 默认低(有人=高, 必须) */
/* 各距离门灵敏度 0~100 (100 = 忽略该门), 下标 0..8 */
#define RADAR_PARAM_MOVE_SENS           { 50U, 50U, 40U, 30U, 20U, 15U, 15U, 15U, 15U }
#define RADAR_PARAM_STILL_SENS          {  0U,  0U, 40U, 40U, 30U, 30U, 20U, 20U, 20U }  /* 门0/1 静止灵敏度不可设 */

/* C. 上电读回一次只读信息(读参数/分辨率/辅助控制/固件版本/MAC)到 s_dump, 供 Keil Watch 查看。
 * 只读不写, 无害; 出厂可置 0。 */
#define RADAR_DUMP_ONCE                 (1U)

/* ============================ 调试输出(上板验证用, 事后删除) ============================
 * RADAR_DBG_EN : 1 = 打开(临时, 见 docs/hc32_radar_bringup.md), 0 = 关闭(空实现/不占空间)
 *               验证通过后置 0, 或整体删除 radar_dbg.c/.h 并去掉 main.c 里的调用
 * 输出方式: 只有结构体 g_radar_dbg(Keil Watch 展开看, 纯数值)。
 *           本板没有连电脑的串口, 因此不做串口/printf/文本输出。
 */
#define RADAR_DBG_EN                    (0U)      /* 调试已关闭: 只剩空实现, 不占 Flash */
#define RADAR_DBG_PERIOD_MS             (500U)      /* 状态行刷新周期 */
#define RADAR_DBG_RX_STALL_MS           (3000U)     /* 收字节停滞多久报一次事件 */

#endif /* __RADAR_CFG_H__ */
