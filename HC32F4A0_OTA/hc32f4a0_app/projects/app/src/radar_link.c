/**
 * @file  radar_link.c
 * @brief F4A0 <-> HC32F460 报警板 业务链路实现（端口映射与协议见 radar_link.h）
 *
 * 移植来源：项目 1 的 STM32F0_linux_v4.31/BSP/app.c（那里叫 frCrc16 / sPortCom / radarPortFresh）。
 * 纯逻辑部分逐字对齐；收发部分按 F4A0 的 read(fd) 模型重写（驱动已用中断填缓冲）。
 */
#include <string.h>
#include "hc32f46_driver.h"     /* read / ioctl / Uart_RS485_send / COMMON_INTERFACE_* */
#include "radar_link.h"

/* ---------- 端口与协议常量 ---------- */
static const int s_link_fd[RADAR_LINK_NUM] = {
    COMMON_INTERFACE_RS485_1,   /* 天线1   */
    COMMON_INTERFACE_RS485_2,   /* 天线2,3 */
    COMMON_INTERFACE_RS485_3,   /* 天线4   */
};

#define FRAME_HDR_AA        0xAAu
#define FRAME_HDR_LEGACY    0xFFu
#define FRAME_MAX_PAYLOAD   251u
#define FRAME_BUF_MAX       (4u + FRAME_MAX_PAYLOAD + 2u)

#define CMD_QUERY           0x10u   /* 查询 -> 板子回 3B 状态 */
#define CMD_REPORT          0x81u   /* 报警上报 */

#define QUERY_PERIOD_MS     50UL    /* 查询周期。STM32F0 侧 200ms 判超时，这里取 1/4 留余量 */
#define RX_DRAIN_MAX        128     /* 每轮每口最多取多少字节，防饿死别的口 */

/* 查询帧的 addr 字段 = 本链路对应的地址。
 *
 * 【照搬 STM32F0，别再自己发明】STM32F0 的轮询是
 *     stmVarSend(sPortCom[i], 0x10u, (uint8_t)(i + 1u), 0, 0u);   // app.c:423
 * 即 cmd=0x10、addr = 端口序号 + 1（1/2/3/4/5）。我之前发的是广播 0x00，
 * 板子按地址过滤所以全都不应答 —— 三灯全灭就是这个原因。 */
static const uint8_t s_link_addr[RADAR_LINK_NUM] = { 1u, 2u, 3u };

typedef struct {
    radar_link_t st;
    uint8_t      rx[FRAME_BUF_MAX];
    uint16_t     rxlen;
    uint8_t      sync;          /* 1 = 已对齐帧头，正在攒帧 */
    uint16_t     plen;
    uint8_t      legacy;        /* 1 = 正在收 0xFF 定长 32B 帧 */
    uint32_t     next_query_ms;
    uint8_t      led_on;        /* 1 = 本口状态灯当前被我们占着（有人）。只释放自己占的通道，
                                 * 从不去灭别人的灯 —— 灯是共用脚，见 radar_link_poll 第 4 步 */
} radar_ctx_t;

static radar_ctx_t s_ctx[RADAR_LINK_NUM];

/* ---------- CRC16 / CCITT，与 F460 common.c 的 fr_crc16 逐位一致 ---------- */
static uint16_t fr_crc16(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i, j;

    for (i = 0u; i < n; i++) {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (j = 0u; j < 8u; j++) {
            if ((crc & 0x8000u) != 0u) { crc = (uint16_t)((crc << 1) ^ 0x1021u); }
            else                       { crc = (uint16_t)(crc << 1); }
        }
    }
    return crc;
}

static uint32_t now_ms(void)
{
    return (uint32_t)osKernelGetTickCount();
}

/* ---------- 发送：带自旋超时，避免驱动 Uart_RS485_send() 在串口异常时死等 ---------- */
/* 超时按【字节时间】折算成自旋次数：460800bps 下 1 字节 ≈ 21.7us；200MHz 下自旋约 4~8 周期/次，
 * 取 20000 次 ≈ 0.5ms/字节（约 25 倍余量）。发送正常时根本到不了上限。
 * （F460 侧曾因 USART_UART_Trans 的 timeout 被误当成"时间"而静默截断帧，故这里把口径写死。） */
#define RADAR_TX_SPIN_PER_BYTE   20000UL

/* 按口互斥：驱动的 Uart_RS485_send() 内部没有锁（对比 uart_send() 有 uart_tx_lock），
 * 多线程并发调会把两帧的字节交错。本模块所有发送都走这里，用一把全局锁串行化 ——
 * 三条总线共用一把锁是刻意的：发送是短操作，且轮询线程是唯一调用者，粒度粗一点更安全。 */
static osMutexId_t s_tx_mux = NULL;
static osRtxMutex_t s_tx_mux_cb;

static void tx_lock(void)
{
    if (s_tx_mux == NULL) {
        osMutexAttr_t attr = { NULL, osMutexPrioInherit, &s_tx_mux_cb, sizeof(s_tx_mux_cb) };
        s_tx_mux = osMutexNew(&attr);
        if (s_tx_mux == NULL) { s_tx_mux = (osMutexId_t)1; }   /* 失败退化为无锁，但不死锁 */
    }
    if (s_tx_mux != (osMutexId_t)1) { (void)osMutexAcquire(s_tx_mux, osWaitForever); }
}

static void tx_unlock(void)
{
    if ((s_tx_mux != NULL) && (s_tx_mux != (osMutexId_t)1)) { (void)osMutexRelease(s_tx_mux); }
}

static int radar_send_frame(uint8_t idx, const uint8_t *fr, uint16_t flen)
{
    int r;

    if (idx >= RADAR_LINK_NUM) { return -1; }

    tx_lock();
    r = Uart_RS485_send(s_link_fd[idx], fr, (uint32_t)flen);
    tx_unlock();

    if (r != (int)flen) {
        s_ctx[idx].st.tx_err++;
        return -1;
    }
    s_ctx[idx].st.tx_frames++;
    return 0;
}

static int radar_send_query(uint8_t idx)
{
    uint8_t  fr[6];
    uint16_t c;

    fr[0] = FRAME_HDR_AA;
    fr[1] = 2u;             /* lenv = plen(0) + 2 */
    fr[2] = CMD_QUERY;
    fr[3] = s_link_addr[idx];   /* addr = 本链路地址（1/2/3），与 STM32F0 一致 */
    c = fr_crc16(fr, 4u);
    fr[4] = (uint8_t)(c & 0xFFu);
    fr[5] = (uint8_t)(c >> 8);
    return radar_send_frame(idx, fr, 6u);
}

/* ---------- 收帧：0xAA 变长（主）+ 0xFF 定长 32B（兼容） ---------- */
static void handle_var_frame(uint8_t idx, const uint8_t *f, uint16_t total)
{
    uint16_t plen = (uint16_t)(f[1] - 2u);
    uint8_t  cmd  = f[2];
    radar_ctx_t *p = &s_ctx[idx];

    p->st.rx_frames++;
    p->st.last_rx_ms = now_ms();
    p->st.online = 1u;

    if (cmd == CMD_QUERY && plen >= 3u) {
        p->st.gpio_in    = f[4];
        p->st.work_mode  = f[5];
        p->st.alarm_done = f[6];
        p->st.radar_val  = ((p->st.gpio_in & 0x07u) != 0u) ? 1u : 0u;
        (void)total;
    }
    /* CMD_REPORT(0x81)：报警上报，本模块只记新鲜度，内容交上层（上报去处待定） */
}

static void feed_byte(uint8_t idx, uint8_t b)
{
    radar_ctx_t *p = &s_ctx[idx];

    if (p->sync == 0u) {
        if (b == FRAME_HDR_AA) {
            p->rx[0] = b; p->rxlen = 1u; p->sync = 1u; p->legacy = 0u;
        } else if (b == FRAME_HDR_LEGACY) {
            p->rx[0] = b; p->rxlen = 1u; p->sync = 1u; p->legacy = 1u;
        }
        return;
    }

    if (p->rxlen >= FRAME_BUF_MAX) { p->sync = 0u; p->rxlen = 0u; p->st.bad_frames++; return; }
    p->rx[p->rxlen++] = b;

    if (p->legacy != 0u) {
        if (p->rxlen >= 32u) {
            /* 老定长帧：0xFF + 30B 数据 + 2B CRC(CCITT)，
             * 布局见 F460 侧 main.h 的 alarm_confirm_package（正好 32B）。
             * 与 0xAA 一样必须校验 CRC —— 不校验的话线上噪声会被当成"板子在线"。 */
            uint16_t crc_rx = (uint16_t)(p->rx[30] | ((uint16_t)p->rx[31] << 8));
            if (crc_rx == fr_crc16(p->rx, 30u)) {
                p->st.leg_device_id  = p->rx[1];
                p->st.leg_alarm_done = p->rx[2];
                p->st.leg_got        = 1u;
                p->st.leg_frames++;
                p->st.gpio_in        = p->rx[2];      /* 新老口径统一：用 alarm_done 表达"本口有事件" */
                p->st.alarm_done     = p->rx[2];
                p->st.rx_frames++;
                p->st.last_rx_ms = now_ms();
                p->st.online     = 1u;
            } else {
                p->st.bad_frames++;
            }
            p->sync = 0u; p->rxlen = 0u;
        }
        return;
    }

    if (p->rxlen == 2u) {
        if ((p->rx[1] < 2u) || (p->rx[1] > (uint8_t)(FRAME_MAX_PAYLOAD + 2u))) {
            p->sync = 0u; p->rxlen = 0u; p->st.bad_frames++; return;
        }
    } else if (p->rxlen >= 4u) {
        uint16_t total = (uint16_t)(p->rx[1] + 4u);
        if (p->rxlen == total) {
            uint16_t crc_rx = (uint16_t)(p->rx[total - 2u] | ((uint16_t)p->rx[total - 1u] << 8));
            if (crc_rx == fr_crc16(p->rx, (uint16_t)(total - 2u))) {
                handle_var_frame(idx, p->rx, total);
            } else {
                p->st.bad_frames++;
            }
            p->sync = 0u; p->rxlen = 0u;
        }
    }
}

void radar_link_init(void)
{
    commonUartPara para;
    int nb = 1;      /* 1 = 非阻塞（见 ota_integration.c 的同款用法） */
    int tmo = 10;
    uint8_t i;

    (void)memset(s_ctx, 0, sizeof(s_ctx));

    /* 【次序：先 open、再 ioctl —— 与 ota_integration.c:102-104 的既有写法一致】
     *
     * 这三个口本来由 ipc.c:271 Tag_update_thread() -> Usart_RS485_init() 以 【O_BLOCK】 打开，
     * 我们只补 ioctl 的话会有一个次序洞：若我们的 ioctl 跑在它前面，随后的 uart_open()
     * 会用 O_BLOCK 把 isBlock 覆盖回去，read() 就走阻塞分支（§9.3.3 第 3 条风险）。
     *
     * 所以这里自己也 uart_open() 一次，参数与 ipc 对齐、isBlock 取 O_NONBLOCK：
     *   · io_stream.c:291 的 uart_open() 对【已打开】的口是 no-op（`if (paraLoc->isOpen != 1)`），
     *     所以谁先跑都不会被覆盖 —— 它先开就沿用它的，我们先开就沿用我们的，两边结果都是非阻塞。
     *   · 之前注释里担心的"再开一次会被改回 O_BLOCK / read() 永久阻塞"不会发生：
     *     覆盖只发生在【首次】打开时；而 Usart_RS485_init() 给的是 timeout=20，
     *     即使真的落到 O_BLOCK 分支也只会 ~25ms 后返回 -2，不会死等。
     *   · 结论：两条时序路径都收敛到"非阻塞"，SET_ISBLOCK(1) 一定生效。 */
    (void)memset(&para, 0, sizeof(para));
    para.baudrate = 460800;        /* 必须与 Usart_RS485_init() 的 460800 一致，否则静默错帧 */
    para.isBlock  = O_NONBLOCK;
    para.timeout  = 10;
    para.isRdam   = 0;
    para.isPrintf = 1;

    for (i = 0u; i < RADAR_LINK_NUM; i++) {
        (void)uart_open(s_link_fd[i], &para);
        (void)ioctl(s_link_fd[i], COMMON_INTERFACE_SET_ISBLOCK, &nb);
        (void)ioctl(s_link_fd[i], COMMON_INTERFACE_SET_TIMEOUT, &tmo);
        (void)ioctl(s_link_fd[i], COMMON_INTERFACE_CLEAR_REVBUF, NULL);
        s_ctx[i].next_query_ms = now_ms();

        /* 上电【不】去灭这三盏灯 —— 它们是共用脚，一进来就写电平等于抢别人的指示。
         * led_on 由上面 memset(s_ctx) 清零：表示"这三盏灯不是我们点亮的，别去收"，
         * 只有雷达真的"有人"时我们才会碰它们（见 radar_link_poll 第 4 步）。 */
        s_ctx[i].led_on = 0u;
    }
    TRACE("radar_link: 3 links up (485_1 ant1 / 485_2 ant2,3 / 485_3 ant4)\n");
}

void radar_link_poll(void)
{
    uint8_t  buf[RX_DRAIN_MAX];
    uint32_t t = now_ms();
    uint8_t  i;
    int      n, k;

    /* 【临时诊断已删】原来这里的三级 LED 定位（LED2 心跳 / LED3 发查询 / LED4 收到字节）
     * 已在链路打通后移除（handoff §4.2）。现在 LED2/3/4 只做一件事：本口雷达"有人"常亮。 */

    for (i = 0u; i < RADAR_LINK_NUM; i++) {
        radar_ctx_t *p = &s_ctx[i];

        /* 1) 先收：非阻塞取数，喂帧解析 */
        for (k = 0; k < 4; k++) {
            n = read(s_link_fd[i], buf, (uint32_t)sizeof(buf));
            if (n <= 0) { break; }
            {
                int j;
                for (j = 0; j < n; j++) { feed_byte(i, buf[j]); }
            }
        }

        /* 2) 新鲜度：STM32F0 的 radarPortFresh 等价物 —— 200ms 无应答即按离线 */
        if ((p->st.online != 0u) && ((t - p->st.last_rx_ms) > RADAR_LINK_FRESH_MS)) {
            p->st.online   = 0u;
            p->st.radar_val = 0u;
            p->st.gpio_in   = 0u;
        }

        /* 3) 周期发查询 */
        if ((int32_t)(t - p->next_query_ms) >= 0) {
            (void)radar_send_query(i);
            p->next_query_ms = t + QUERY_PERIOD_MS;
        }

        /* 4) 雷达状态灯：BOARD_LED2/3/4 = 链路 485_1/485_2/485_3。
         *    【为什么从 LED2 起】BOARD_LED1 已被 app 占用（现场确认），故依次用 2/3/4。
         *
         *    口径（现场指定）：雷达"有人"就【常亮】，人走了【灭】—— 不闪。
         *
         *    【这三个脚是和 app 其它功能复用的，所以走共用 API，不直接写引脚】
         *      ipc.c(586-588) / Lan2Uart.c(526) / alarm.c(110) / bsp_led.c(567 报警路径)
         *      都用 Alarm_Output / Alarm_Disable 这套通道。我们也用它，理由：
         *        · 每个通道有 ucEnalbe 门 + 互斥锁：占用期间别人的 Alarm_Output 直接早退，
         *          不会出现"我写引脚、它按状态机翻转"的对打；
         *        · 释放用 Alarm_Disable（别人随后就能正常申请），不长期霸占；
         *        · Alarm_Off() 只停 Buzz/Relay/RGB，不碰 LED1..4，不会误清我们的通道。
         *
         *      参数用 (1, 0, 0) 而不是 (5, 5, 1)：
         *        (5,5,1) = 50ms 亮 / 50ms 灭、循环 1 次 —— 配合每拍重调就是【一直闪】，
         *        正是现场要去掉的那个现象。而 GPIO_Pro() 在 usStopTime==0 时直接 return
         *        （bsp_led.c:223 的判断），所以这里传 OFF=0、Cycle=0 就能：
         *          · 通道保持占用（别人抢不走）；
         *          · 引脚一直亮、不翻转 —— 真正的"有人就常亮"，且全程在共用状态机里。
         *      每拍重调是幂等的：ucEnalbe==1 时 GPIO_Start 直接早退，只在别人显式
         *      Alarm_Disable 抢走通道后才会重新占回来（有人期间以雷达为准）。 */
        if (p->st.radar_val != 0u) {
            Alarm_Output((uint8_t)(BOARD_LED2 + i), 1u, 0u, 0u);
            p->led_on = 1u;
        }
        else if (p->led_on != 0u) {
            Alarm_Disable((uint8_t)(BOARD_LED2 + i));
            p->led_on = 0u;
        }
    }
}

void radar_link_task(void *arg)
{
    (void)arg;

    /* 与 send_tags 一致：等系统初始化完成再动外设。
     * Usart_RS485_init() 是在 Tag_update_thread 里调的，早于它去配置这三个口会被覆盖。 */
    wait_init_ok();

    radar_link_init();
    for (;;) {
        radar_link_poll();
        sleep_ms(10);
    }
}

const radar_link_t *radar_link_get(uint8_t idx)
{
    return (idx < RADAR_LINK_NUM) ? &s_ctx[idx].st : NULL;
}

int radar_link_of_antenna(uint8_t ant)
{
    switch (ant) {
        case 1u: return 0;
        case 2u: return 1;
        case 3u: return 1;
        case 4u: return 2;
        default: return -1;
    }
}

uint8_t radar_link_antenna_alarm(uint8_t ant)
{
    int i = radar_link_of_antenna(ant);
    if (i < 0) { return 0u; }
    /* 一口多天线时（天线2,3 共用 RS485_2），本口 radarVal 只能表达"这条总线上有人" */
    return s_ctx[i].st.radar_val;
}
