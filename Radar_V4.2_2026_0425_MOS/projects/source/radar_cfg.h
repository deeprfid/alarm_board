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

/* USART 挂的 PCLK1 频率(本工程 HCLK=200MHz、PCLK1=DIV2 -> 100MHz); 用于按波特率选分频 */
#define RADAR_UART_PCLK_HZ              (100000000UL)
/* ============================ 模块波特率目标值(产线配置, 可选) ============================
 * 0 = **只跟随模块**(出厂默认): 上电纯监听逐档探测, 模块当前是哪一档就用哪一档, 一个命令都不发。
 *     客户会用手机 APP 自由改模块波特率, 固件必须跟得上 —— 所以默认不写模块。
 * 非 0 = 产线配置: 探测到模块不是该值, 就发 0x00A1 把模块波特率改成它(写进模块 flash, 掉电保存),
 *        再 0x00A3 重启并复检。只在产线/调试时用, 平时不要开(会跟客户的 APP 设置打架)。
 * 禁止: 探测阶段永远**纯监听**, 绝不发 0x00FF/0x00FE(会让模块进配置态并停止上报)。
 */
#define RADAR_BAUD_TARGET               (0UL)

#define RADAR_BAUD_LOCK_FRAMES          (1U)        /* 监听阶段收到几个合法上报帧就算命中
                                                 * (帧头/长度/帧尾/帧内校验齐全, 乱码凑不出一整帧) */
#define RADAR_PROBE_LISTEN_MS           (300U)

/* 一整轮 8 档都没锁定时, 隔多久重扫一轮(避免'偶尔没锁上就永久失败', 需断电重启) */
#define RADAR_PROBE_RETRY_MS            (5000U)

/* 链路失联兜底: 锁定后连续多久一个字节都收不到 -> 做一轮兜底重扫(覆盖'模块被换到连乱码
 * 都收不到的跨档'的情形: 实测模块38400/我们在460800听 -> 全静默, 字节增量恒 0)。 */
#define RADAR_LINK_SILENT_MS            (3000U)
/* 兜底重扫的最小间隔(避免长时间失联/APP配置期间反复白扫; 只改我们自己的波特率, 不发命令) */
#define RADAR_LINK_SWEEP_MIN_MS         (10000U)     /* 每档只听多久(不发任何命令) */
#define RADAR_RX_TIMEOUT_BITS_HINT      (0U)        /* 占位: 见下方 RADAR_RX_TIMEOUT_BITS 说明 */

/* 上电自适应波特率探测顺序: 覆盖协议表 6 的全部 8 档。
 * 注意: 模块波特率是"掉电保存"的配置项(出厂默认 256000, 索引 0x0007), 一旦被
 * 上位机/APP/0x00A1 改过, 上电就是改过的值 —— 所以不能假设它是 256000。 */
#define RADAR_BAUD_TABLE                { 256000UL, 460800UL, 115200UL, 9600UL, 19200UL, \
                                          38400UL, 57600UL, 230400UL }
#define RADAR_BAUD_TABLE_CNT            (8U)        /* 协议表6 全部 8 档(无 921600);
                                                     * 460800 放第一档: 配置好之后一档命中,
                                                     * 其余档只用于兼容"还没配置过"的模块 */
#define RADAR_BAUD_FALLBACK             (256000UL)
/* ============================ 上电初始波特率(调试用) ============================
 * 0  = **自适应**(出厂口径): 先按 RADAR_BAUD_FALLBACK 收, 再由探测按 RADAR_BAUD_TABLE
 *      逐档**纯监听**, 听到合法上报帧就锁定该档 —— 客户 APP 把模块改成哪一档都能通。
 * 非 0 = 调试/单档定位: 上电就用该波特率(走初始化路径), 不做任何换档。
 */
#define RADAR_BAUD_INIT_FIXED           (0UL)
/* ==================== 雷达口时钟分频 / 过采样(不可随意改) ====================
 * 固定 DIV4 + 8 倍过采样, 与现场确认可用的版本一致。依据(hc32_ll_usart.c 源码):
 *   - PR.PSC 是 2 位, 实际分频 = 4^PSC, 只有 /1 /4 /16 /64 四档;
 *   - BRR 整数分频只有 8 位: DIV_Integer = C/(B*8*(2-OVER8)) - 1 必须 <= 255,
 *     否则 USART_SetBaudrate() 返回错误, 且根本**不写 BRR**(端口保持复位波特率)。
 * 本工程 PCLK1 = 100MHz, 于是:
 *   C = 25MHz    (DIV4) : 8 倍过采样算不出 9600(需 324>255), 但 460800 正常 —— 本产品用这一档;
 *   C = 6.25MHz  (DIV16): 8 倍过采样 8 档全能算, 但 460800 的整数分频为 0(靠小数凑, 不推荐);
 *   C = 1.5625MHz(DIV64): SDK 例程里 115200 / 9600 用的就是这一档(只跑一个低波特率时精度最好),
 *                         但 >=230400 三档全部算不出来 —— 那是单档优化, 不是通用答案。
 * 结论: 本产品只用 460800 -> DIV4 + 8 倍。若要支持 9600 等低波特率必须同时改分频和过采样;
 *       但 2026-09-14 实测已证明低波特率收不到帧与分频/BRR 无关(BRR 回读完全正确), 该方向已放弃,
 *       详见 docs/radar_baud_debug_notes.md 第 5、6 节。
 */
/* 波特率 -> 协议表 6 索引(0x00A1 用), 索引 = 位置 + 1 */
#define RADAR_BAUD_IDX_TABLE            { 9600UL, 19200UL, 38400UL, 57600UL, 115200UL, 230400UL, 256000UL, 460800UL }
#define RADAR_BAUD_IDX_TABLE_CNT        (8U)

/* 产线配置(内部): 重启模块后等多久再复检(RADAR_BAUD_TARGET 非 0 时生效) */
#define RADAR_PROV_RESTART_MS           (500U)      /* 模块重启后等多久开始复检(复检=重跑自适应, 自带 1s 启动延时) */

/* ============================ RX DMA: USART1_RI -> DMA2 CH1 ============================ */
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

/* ============================ USART 中断 ============================ */
#define RADAR_UART_TX_CPLT_IRQn         (INT007_IRQn)
#define RADAR_UART_TX_CPLT_INT_SRC      (INT_SRC_USART1_TCI)
#define RADAR_UART_RX_ERR_IRQn          (INT008_IRQn)
#define RADAR_UART_RX_ERR_INT_SRC       (INT_SRC_USART1_EI)

/* 逐字节接收中断(RI): 本工程原来没用到 INT012, 分配给它 */
#define RADAR_UART_RX_IRQn              (INT012_IRQn)
#define RADAR_UART_RX_INT_SRC           (INT_SRC_USART1_RI)

/* ============================ 缓冲与超时 ============================ */
#define RADAR_RX_RING_SIZE              (512U)      /* 软件环形缓冲 */
#define RADAR_TX_MAX                    (32U)       /* 单条命令帧最大长度 */
#define RADAR_FRAME_MAX                 (64U)       /* 单帧最大长度(工程模式 45B) */
#define RADAR_CMD_TIMEOUT_MS            (200U)      /* 命令等待 ACK 超时 */
#define RADAR_TX_TIMEOUT_MS             (50U)       /* 发送完成兜底: 超过该时间没等到完成中断就复位 TX 通路 */
#define RADAR_READ_TRY                  (2U)        /* radar_read_all() 每项最多试几次 */
#define RADAR_READ_GAP_MS               (50U)       /* 每项之间的间隔(模块连续命令间需要喘口气) */
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
#define RADAR_DUMP_ONCE                 (0U)        /* 0 = 上电不读回(不发任何命令); 需要读时手动调 radar_read_all() */
#endif /* __RADAR_CFG_H__ */
