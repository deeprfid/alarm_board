/*******************************************************************************
 * radar_cfg.h -- LD2410C 雷达模块驱动配置(全部可调项集中在此文件)
 *
 * 分层: radar_cfg.h(配置) / radar_port.c(USART: 逐字节 RI 中断收 + 轮询 TXE 发) / radar_frame.c(分帧)
 *       / radar_proto.c(协议) / radar.c(服务与命令)
 ******************************************************************************/
#ifndef __RADAR_CFG_H__
#define __RADAR_CFG_H__

#include "hc32_ll.h"

/* ============================ 模块 OUT 引脚(三口) ============================
 * 高有效: 1 = 有人。三路 OUT 由 bsp_gpio.c / bsp_report.c / common.c 直接读,
 * radar.c 的 s_out_port[]/s_out_pin[] 也按下标引用它们(做串口/OUT 双路判定)。
 */
#define RADAR_PORT0                     (GPIO_PORT_C)
#define RADAR_PIN0                      (GPIO_PIN_14)
#define RADAR_PORT1                     (GPIO_PORT_C)
#define RADAR_PIN1                      (GPIO_PIN_13)
#define RADAR_PORT2                     (GPIO_PORT_H)
#define RADAR_PIN2                      (GPIO_PIN_02)

/* ============================ 雷达串口(三口, 每口 8 行) ============================
 * 雷达1/2/3 分别挂在 USART1/2/3 上。引脚功能号已按数据手册 Table 2-1/2-2 核对
 * (HC32F460 的功能号 32~63 按引脚所属功能组解释):
 *   PA2/PA3   属 Func_Grp1, 32/33 = USART1_TX/RX
 *   PA0/PA1   属 Func_Grp1, 36/37 = USART2_TX/RX
 *   PB14/PB15 属 Func_Grp2, 32/33 = USART3_TX/RX
 * 若与 PCB 实际不符, 只改下面这三组共 24 行, 其它代码不依赖具体引脚。
 * 注意: bsp_rs485.h 的 USART_UNIT 是 CM_USART4 / PB6 / PB7, 那是与 STM32 的
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



#define RADAR2_UART_UNIT                 (CM_USART2)
#define RADAR2_UART_FCG_ENABLE()         (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART2, ENABLE))

#define RADAR2_UART_TX_PORT              (GPIO_PORT_A)   /* PA0 */
#define RADAR2_UART_TX_PIN               (GPIO_PIN_00)
#define RADAR2_UART_TX_FUNC              (GPIO_FUNC_36)

#define RADAR2_UART_RX_PORT              (GPIO_PORT_A)   /* PA1 */
#define RADAR2_UART_RX_PIN               (GPIO_PIN_01)
#define RADAR2_UART_RX_FUNC              (GPIO_FUNC_37)


#define RADAR3_UART_UNIT                 (CM_USART3)
#define RADAR3_UART_FCG_ENABLE()         (FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_USART3, ENABLE))

#define RADAR3_UART_TX_PORT              (GPIO_PORT_B)   /* PB14 */
#define RADAR3_UART_TX_PIN               (GPIO_PIN_14)
#define RADAR3_UART_TX_FUNC              (GPIO_FUNC_32)

#define RADAR3_UART_RX_PORT              (GPIO_PORT_B)   /* PB15 */
#define RADAR3_UART_RX_PIN               (GPIO_PIN_15)
#define RADAR3_UART_RX_FUNC              (GPIO_FUNC_33)

/* ============================ 雷达2/雷达3 中断线(三口各自独立) ============================
 * 每口 **2 条**中断: RI(收满) / EI(帧错等)。发送走轮询 TXE, 所以**不需要 TCI**。
 * 定时器也不需要 —— 帧间隔用全局 m_u32Tickms。
 * 全工程中断线占用: INT000~INT004 RS485 / INT008,INT012 雷达1 / INT010~INT011 EXTINT /
 *                   INT013~INT014 雷达2 / INT016~INT017 雷达3。 */
#define RADAR2_UART_RX_IRQn             (INT013_IRQn)
#define RADAR2_UART_RX_INT_SRC          (INT_SRC_USART2_RI)
#define RADAR2_UART_RX_ERR_IRQn         (INT014_IRQn)
#define RADAR2_UART_RX_ERR_INT_SRC      (INT_SRC_USART2_EI)

#define RADAR3_UART_RX_IRQn             (INT016_IRQn)
#define RADAR3_UART_RX_INT_SRC          (INT_SRC_USART3_RI)
#define RADAR3_UART_RX_ERR_IRQn         (INT017_IRQn)
#define RADAR3_UART_RX_ERR_INT_SRC      (INT_SRC_USART3_EI)




/* USART 挂的 PCLK1 频率(本工程 HCLK=200MHz、PCLK1=DIV2 -> 100MHz); 用于按波特率选分频 */
#define RADAR_UART_PCLK_HZ              (100000000UL)
/* ============================ 模块波特率目标值(产线配置, 可选) ============================
 * 0 = **只跟随模块**(出厂默认): 上电纯监听逐档探测, 模块当前是哪一档就用哪一档, 一个命令都不发。
 *     客户会用手机 APP 自由改模块波特率, 固件必须跟得上 —— 所以默认不写模块。
 * 非 0 = 产线配置: 探测到模块不是该值, 就发 0x00A1 把模块波特率改成它(写进模块 flash, 掉电保存),
 *        再 0x00A3 重启并复检。只在产线/调试时用, 平时不要开(会跟客户的 APP 设置打架)。
 * 禁止: 探测阶段永远是**纯监听**, **绝不发 0x00FF**(它才会把模块推进配置态并停止上报)。
 *       裸 0x00FE 只在"重扫急救包"里、且**只针对我们自己把模块留在配置态的那一口**补发(靶向),
 *       见下面 RADAR_PROBE_FAILSAFE。
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
#define RADAR_LINK_SWEEP_MIN_MS         (10000U)

/* 上电自适应波特率探测顺序: 覆盖协议表 6 的全部 8 档。
 * 注意: 模块波特率是"掉电保存"的配置项(出厂默认 256000, 索引 0x0007), 一旦被
 * 上位机/APP/0x00A1 改过, 上电就是改过的值 —— 所以不能假设它是 256000。 */
#define RADAR_BAUD_TABLE                { 256000UL, 460800UL, 115200UL, 9600UL, 19200UL, \
                                          38400UL, 57600UL, 230400UL }
#define RADAR_BAUD_TABLE_CNT            (8U)        /* 协议表6 全部 8 档(无 921600);
                                                     * 256000 放第一档: 模块出厂/APP 默认值,
                                                     * 也是端口上电初值(RADAR_BAUD_FALLBACK),
                                                     * 出厂模块第 1 个窗口即命中;
                                                     * 460800 退第二档, 其余档只用于兼容已被改过
                                                     * 波特率的模块(客户会用 APP 自由改档) */
#define RADAR_BAUD_FALLBACK             (256000UL)
/* ============================ 上电初始波特率(调试用) ============================
 * 0  = **自适应**(出厂口径): 先按 RADAR_BAUD_FALLBACK 收, 再由探测按 RADAR_BAUD_TABLE
 *      逐档**纯监听**, 听到合法上报帧就锁定该档 —— 客户 APP 把模块改成哪一档都能通。
 * 非 0 = 调试/单档定位: 上电就用该波特率(走初始化路径), 不做任何换档。
 */
#define RADAR_BAUD_INIT_FIXED           (0UL)
/* ==================== 雷达口时钟分频 / 过采样 ====================
 * 实现见 radar_port.c 的 radar_pick_clk_div(): **分频随波特率自动选**
 *     baud < 115200 -> DIV64 (C = 1.5625MHz);   否则 -> DIV1 (C = 100MHz)
 * (照抄扫描台主板同款 HC32F460 的量产写法, 取代早期写死 DIV4 的版本。)
 * 过采样固定 8 倍(USART_OVER_SAMPLE_8BIT), CKOutput 关闭。
 *
 * 为什么必须随波特率选 —— 依据 hc32_ll_usart.c 源码(非推测):
 *   - PR.PSC 是 2 位, 实际分频 = 4^PSC, 只有 /1 /4 /16 /64 四档;
 *   - BRR 整数分频只有 8 位: DIV_Integer = C/(B*8*(2-OVER8)) - 1 必须 <= 255,
 *     否则 USART_SetBaudrate() 返回错误, 且根本**不写 BRR**(端口静默停在旧波特率)。
 * 本工程 PCLK1 = 100MHz, 四条约束下协议表 6 的可表示性(8 倍过采样):
 *   C = 100MHz   (DIV1) : 仅 57600~460800, 9600/19200/38400 算不出;
 *   C = 25MHz    (DIV4) : 9600 需 324>255 算不出, 其余可;
 *   C = 6.25MHz  (DIV16): 8 档全能算, 但 460800 整数分频为 0(靠小数凑, 不推荐);
 *   C = 1.5625MHz(DIV64): 仅 9600~115200, >=230400 三档全部算不出。
 *   => 单一分频无法覆盖全表, 所以按波特率二分(没有"固定某一档"这回事)。
 *   SDK 例程选 DIV64 是单档低波特率的精度优化, 不是通用答案; 取舍详见
 *   docs/radar_baud_debug_notes.md 第 5 节。
 *
 * 现场实测(多口化后三路同时在线, 各自锁定不同档): 460800/115200/9600 的 BRR 整数
 * 分别为 26/107/19, 与公式预测逐一吻合, 均稳定 10Hz 出帧 —— 见同文档 §8.5 与 §9。
 */
/* 波特率 -> 协议表 6 索引(0x00A1 用), 索引 = 位置 + 1 */
#define RADAR_BAUD_IDX_TABLE            { 9600UL, 19200UL, 38400UL, 57600UL, 115200UL, 230400UL, 256000UL, 460800UL }
#define RADAR_BAUD_IDX_TABLE_CNT        (8U)

/* 产线配置(内部): 重启模块后等多久再复检(RADAR_BAUD_TARGET 非 0 时生效) */
#define RADAR_PROV_RESTART_MS           (500U)      /* 模块重启后等多久开始复检(复检=重跑自适应, 自带 1s 启动延时) */

/* ============================ USART 中断 ============================
 * 注: 接收走**逐字节 RI 中断**、发送走**轮询 TXE**(见 radar_port.c),
 *     所以既不用 DMA, 也不注册发送完成(TCI)中断。
 * 原先的 RX DMA(USART1_RI -> DMA2 CH1)、TX DMA(DMA2 CH0 -> USART1_TI)与
 * RADAR_UART_TX_CPLT_* 已随"改逐字节中断方案"删除, 这里不再保留死宏
 * (留着的后果: 会让人以为还在用 DMA2/INT006/INT007)。 */
#define RADAR_UART_RX_ERR_IRQn          (INT008_IRQn)
#define RADAR_UART_RX_ERR_INT_SRC       (INT_SRC_USART1_EI)

/* 逐字节接收中断(RI): 收满一个字节触发(见 radar_port.c) */
#define RADAR_UART_RX_IRQn              (INT012_IRQn)
#define RADAR_UART_RX_INT_SRC           (INT_SRC_USART1_RI)

/* ============================ 缓冲与超时 ============================ */
#define RADAR_RX_RING_SIZE              (512U)      /* 软件环形缓冲 */
#define RADAR_TX_MAX                    (32U)       /* 单条命令帧最大长度 */
#define RADAR_FRAME_MAX                 (64U)       /* 单帧最大长度(工程模式 45B) */
#define RADAR_CMD_TIMEOUT_MS            (200U)      /* 命令等待 ACK 超时(非阻塞事务里 = 每帧最多等多久) */
#define RADAR_TXN_TX_WAIT_MS            (50U)       /* 事务里发下一帧前, 等上一帧发完的最长时间 */
#define RADAR_TX_TIMEOUT_MS             (50U)       /* 发送兜底: 超时仍未发完就复位 s_tx_busy 放行后续发送(见 radar_port_tx_watchdog) */
#define RADAR_READ_TRY                  (2U)        /* radar_read_all() 每项最多试几次 */
#define RADAR_READ_GAP_MS               (50U)       /* 每项之间的间隔(模块连续命令间需要喘口气) */
#define RADAR_PROBE_BOOT_MS             (1000U)     /* 上电后等模块启动完成再开始探测 */
#define RADAR_FRAME_GAP_MS              (20U)       /* 半包超时(帧内空闲复位) */
#define RADAR_REPORT_STALE_MS           (1000U)     /* 上报数据过期(串口在线判定) */

/* ============================ 设备数量 ============================ */
#define RADAR_DEV_CNT                  (3U)   /* 与 RADAR_PORT_CNT 对齐(三个雷达口); 设备号 == 口号 */

/* ============================ A. 探测行为参数(掉电保存, 按现场调) ============================
 * **逐口独立**: 每个雷达口各跑一套状态机, 且**只对已锁定(在线)的口执行**, 未锁定的口跳过。
 * 幂等: 先 0x0061/0x00AE 读回当前配置与目标逐项比对, **只写不一致的项**, 全一致则一条命令都不发;
 *       写完再读回复检。每块模块的参数存在它自己的 flash 里, 所以三口必须分别配。
 *
 * 进度与结果**不看 radar_param_state()**, 而是编码进 g_radar_comm 的 **bit11..15**
 * (编码表见 radar.c 文件头) —— 这样不新增 Watch 变量, 也不占用已被波特率用掉的那 3 个标量。
 *
 * 注意: 距离分辨率(0x00AA)不在此列 —— 它要重启模块才生效, 用 radar_set_resolution_port() 手动设。
 * 光感辅助(0x00AD): 本产品不启用光感, 且 OUT 默认电平必须为 0(有人=高), 否则有人/无人会反。
 */
/* **出厂默认 0(关闭)**: 本功能要和模块做命令事务(0x00FF -> 命令 -> 0x00FE), 而客户会用手机
 * APP 同时配同一个模块 —— APP 的配置会话走的是同一套事务, 两边交叉下发就会互相打断
 * (现场实测: APP 报"设置距离门灵敏度失败 返回码 6401", 且时好时坏)。参数配置是**产线/现场调试**
 * 动作、不是运行时需求, 所以默认关掉: 要配参数时改成 2, 逐口读数确认后改回 0 再出货。
 * **灵敏度不在这里配**: 现场灵敏度由**报警下行包**逐次下发(见下面 RADAR_DL_SENS_EN),
 * 两边都配会互相覆盖 —— 所以本功能默认关, 灵敏度只认下行那一个来源。 */
#define RADAR_PARAM_EN                  (0U)        /* 0=关闭(**出厂默认**) / 1=只读自检(**不写模块**) / 2=完整幂等配置 */
#define RADAR_PARAM_MAX_MOVE_GATE       (3U)        /* 最大运动距离门 2~8 (0.75m/门) */
#define RADAR_PARAM_MAX_STILL_GATE      (3U)        /* 最大静止距离门 2~8 */
#define RADAR_PARAM_NO_BODY_SEC         (0U)        /* 无人持续时间(秒), 出厂 5 */
#define RADAR_PARAM_AUX_MODE            (0U)        /* 0 关闭光感辅助 / 1 光感<阈值 / 2 光感>阈值 */
#define RADAR_PARAM_AUX_THRESHOLD       (0x80U)     /* 光感阈值 0~255 */
#define RADAR_PARAM_AUX_OUT_LEVEL       (0U)        /* OUT 默认电平: 0 默认低(有人=高, 必须) */
/* 各距离门灵敏度 0~100 (100 = 忽略该门), 下标 0..8 */
#define RADAR_PARAM_MOVE_SENS           { 100U, 100U, 100U, 100U, 100U, 100U, 100U, 100U, 30U }
#define RADAR_PARAM_STILL_SENS          { 30U, 30U, 30U, 30U, 30U, 30U, 30U, 30U, 30U }  /* 门0/1 静止灵敏度不可设 */

/* 编译期拦住写错的目标值(手册 2.2.3: 运动/静止距离门配置范围 2~8)。
 * 写错了宁可编不过, 也别把非法参数发给模块 —— 运行时 radar_set_max_gate_port() 还有一道。 */
#if ((RADAR_PARAM_MAX_MOVE_GATE < 2U) || (RADAR_PARAM_MAX_MOVE_GATE > 8U))
#error "RADAR_PARAM_MAX_MOVE_GATE must be 2..8 (LD2410C 协议 2.2.3)"
#endif
#if ((RADAR_PARAM_MAX_STILL_GATE < 2U) || (RADAR_PARAM_MAX_STILL_GATE > 8U))
#error "RADAR_PARAM_MAX_STILL_GATE must be 2..8 (LD2410C 协议 2.2.3)"
#endif

/* ==================== 报警下行下发"雷达灵敏度"(每次报警下发) ====================
 * 来源: 报警下行 32B PDU(alarm_pdu)的 **Alarm_Duration[1]** —— 即 common.c 里的 radar_range。
 *   0      = **不设置**: 保持模块现状(连待办都不建, 一个字节都不发)
 *   1 ~ 10 = 映射到**动态(运动)灵敏度** 10 ~ 100, 即 灵敏度 = 值 × 10
 *   其它   = 非法值直接忽略(common.c 的报警流程会把 radar_range 改成 0xFF, 那不该下发)
 * 静态(静止)灵敏度**恒为 RADAR_DL_SENS_STILL = 100**(该门不参与判定)。
 * 写/比哪些门: 0 ~ RADAR_DL_SENS_GATE_MAX(0x0064 是按门逐个写的, 所以最多 9 条命令)。
 *
 * **幂等(核心要求)**: 这个值每次报警都会带下来, 但**只有值真的变了才动模块**:
 *   ① 值没变 -> 一条命令都不发;
 *   ② 值变了 -> **先读回模块现存的灵敏度逐门比对, 只写不一致的门**(模块自己会把灵敏度存 flash,
 *      所以固件重启后第一次下发同样先读后写, 不会白写一遍)。
 *
 * 与 RADAR_PARAM_EN 的关系: 灵敏度**只认这一个来源**。参数自动配置默认关闭(PARAM_EN = 0),
 *   它那套 RADAR_PARAM_MOVE_SENS/STILL_SENS 宏不参与; 两个都开就会互相覆盖, 不要同时开。
 *
 * **不在报警期间动雷达(重要)**: 这条参数是**跟着报警包一起下来的**, 而设置/读取参数的整个
 *   事务期间模块会**停止上报**(0x00FF 让它进配置态) —— 偏偏报警期间最需要雷达数据。所以:
 *   ① 值没变 -> **零流量**(见上面的幂等);
 *   ② 值变了 -> **先只登记, 不动雷达**; 一直等到"距最近一次报警下行 ≥ RADAR_DL_SENS_QUIET_MS"
 *      的**平静期**才去做"读回 + 只写不一致的门"。等待期间状态码是 14。
 *   不设强制超时: **宁可晚生效, 也不在报警期间把雷达打哑**。连续不断报警时会一直等第一个空隙。
 *
 * 现场读法: g_radar_comm 的 bit11..15(参数自动配置关闭时就是本功能的逐口状态码) ——
 *   0 还没下发过 / 1..10 **已生效档位**(= 下行原值, 灵敏度 = 值×10) / 11 正在写 /
 *   12 写失败已放弃 / 13 该口未锁定(不在线), 等它上线 / 14 在等平静期(报警期间不动雷达)。
 */
#define RADAR_DL_SENS_EN                (1U)        /* 0 = 整条路径关闭(不发任何命令) */
#define RADAR_DL_SENS_GATE_MAX          (8U)        /* 0~8: 手册 2.2.7 距离门范围; 不得超过 RADAR_GATE_MAX */
#define RADAR_DL_SENS_STILL             (100U)      /* 静态阈值恒为 100 */
#define RADAR_DL_SENS_TRY_MAX           (3U)        /* 同一步最多试几次(失败就放弃, 绝不无限重试) */
#define RADAR_DL_SENS_QUIET_MS          (5000U)     /* 距**最近一次报警下行包**多久才算"平时":
                                                     * 到点才允许去读/写模块(参数就是跟报警一起来的,
                                                     * 所以这条同时也定义了"报警期间"的长度) */

#if (RADAR_DL_SENS_GATE_MAX > 8U)
#error "RADAR_DL_SENS_GATE_MAX must be 0..8 (LD2410C 0x0064 gate range)"
#endif
/* 两边都会写 0x0064(灵敏度)、也都要占用 g_radar_comm 的 bit11..15, 同时开必然互相覆盖。
 * 灵敏度只认下行这一个来源, 所以这里直接拦住 —— 写错宁可编不过(同本文件其它 #error 的口径)。 */
#if ((RADAR_PARAM_EN != 0U) && (RADAR_DL_SENS_EN != 0U))
#error "RADAR_PARAM_EN and RADAR_DL_SENS_EN must not both be enabled (both write 0x0064 / bit11..15)"
#endif

/* C. 上电读回一次只读信息(读参数/分辨率/辅助控制/固件版本/MAC)到 s_dump, 供 Keil Watch 查看。
 * 只读不写, 无害; 出厂可置 0。 */
#define RADAR_DUMP_ONCE                 (0U)        /* 0 = 上电不读回(不发任何命令); 需要读时手动调 radar_read_all() */
/* ==================== 探测重扫的"急救包"(补发裸 0x00FE) ====================
 * 背景: LD2410C 一旦进了配置态(收到 0x00FF 而没收到 0x00FE 收尾)**就彻底停止上报**,
 *       纯监听方案无法自救。固件的 cfg 事务已做到"无条件发 0x00FE"(见 radar.c), 但那帧也可能丢,
 *       所以重扫时再补一帧。
 * 现场实测(2026-09-15, 数据见 docs/radar_baud_debug_notes.md §10), 两次受控实验:
 *   - 给**正常态**模块裸发 0x00FE -> 回 ACK 但 status!=0(模块判"无效"), **上报完全不受影响**(安全);
 *   - 给**卡在配置态**的模块裸发 0x00FE -> 回 ACK 且 status=0, **恢复上报**(有效)。
 *   => 它**只在需要它的时候生效**, 这正是我们要的。
 *
 * **绝不发 0x00FF**: 那才会把模块推进配置态, 是现场明令禁止的; 0x00FE 没有这个语义。
 *
 * **门控 = 靶向 + 全静默(2026-09-16 现场把第一版否掉了)**:
 *   ① 第一版是"长时间全静默就补发"(RADAR_FAILSAFE_SILENT_MS = 30s), 假设"APP 会话期间线上一直有字节"。
 *      **假设是错的**: 客户在 APP 参数页**盯着不放**时线上一个字节都没有, 30 秒一到门控放行,
 *      0x00FE 照样插进 APP 的会话把模块拉出配置态 -> 现场"设置参数 6401 + 雷达 lock=0"。
 *   ② 现在先问 **"是谁把模块留在配置态的"**: 只有**我们自己的**配置事务(结束帧 0x00FE 没被确认, 或
 *      0x00FF 发出后事务中途崩了)才武装急救。客户 APP 的会话**永远不会**武装它 —— 客户在参数页
 *      停留多久都与我们无关, 一帧都不发。(APP 自己会收尾; APP 崩了也不该由我们去打断它。)
 *      全静默只是第二道, 用来避免在别人正常通信时插嘴。
 *
 *   0 = 关闭(不补发)
 *   1 = **只有重扫才补**(当前默认) —— 上电首次探测仍保持纯监听, 符合现场口径的字面要求
 *   2 = 上电首次探测也补 —— 还能救"上一次遗留的卡死模块"; 实测无害, 但要现场点头才改
 */
#define RADAR_PROBE_FAILSAFE            (1U)

/* 急救包的第二道门控(第一道是"靶向": 只有我们自己把模块留在配置态才武装, 见 radar.c):
 * 该口连续多久收不到**任何字节**才允许补发裸 0x00FE —— 用来避免在"别人正在通信"时插嘴。
 * 注意: 这道门**不再是**主要保护。别指望它挡住 APP 会话(客户盯着页面不动时线上就是没字节)。 */
#define RADAR_FAILSAFE_SILENT_MS        (30000UL)

/* ==================== 长时兜底: APP 会话被遗弃 ====================
 * 现场场景(2026-09-16): 客户用手机 APP(蓝牙)配完参数, **不退出参数页直接最小化或关掉 APP**。
 *   BLE 会话就此断开, 那帧 0x00FE **永远不会发出来** -> 模块永远卡在配置态、彻底停止上报。
 *   固件侧看到的就是"某口长时间零字节", 于是不停重扫 8 档、锁不上、回落到 256000 再循环 ——
 *   雷达从此死掉, 只能给模块断电才能救回来。
 *
 * 判据: 该口**彻底静默**超过 RADAR_RESCUE_ABANDON_MS, 就认为"这个会话已经没人管了", 补发裸 0x00FE。
 *   (靶向那条只认"我们自己搞的", 救不了这一种; 两条并存。)
 *
 * **取值的代价是不对称的**:
 *   调小 -> 救得快, 但可能打断"还停在参数页上操作的客户"(他下一条设置命令会失败, 重试即可);
 *   调大 -> 不误伤还在操作的客户, 但模块卡死的时间更长(这段时间该路雷达等于没有)。
 * 误伤 = 客户重试一次; 漏救 = 一路雷达一直死到断电 —— 所以宁可偏保守地调大一点。
 * 0 = 关闭长时兜底(只保留靶向那条)。
 * 调节点: 现场说"客户在页面上待很久" 就调大; 说"卡死后等太久" 就调小。 */
#define RADAR_RESCUE_ABANDON_MS         (300000UL)  /* 5 分钟 */

#endif /* __RADAR_CFG_H__ */
