/**
 * @file  ota_recv.c
 * @brief 报警板侧 OTA1 接收端实现（A/B 双槽、无搬运）
 */
#include <string.h>
#include "ota_recv.h"
#include "ota_frame.h"     /* ota_frame_build / ota_frame_crc16 */
#include "ota_flash.h"
#include "hc32_ll_usart.h"
#include "bsp_rs485.h"     /* USART_UNIT */

/* 会话状态 */
static ota_rx_state_t s_state;
static uint32_t s_slot;          /* 目标槽（非活动槽） */
static uint32_t s_total;         /* 包总长（含 82B 包头） */
static uint32_t s_off;           /* 已写偏移（含包头） */
static uint32_t s_erased;        /* 目标槽内已擦除到（槽内相对偏移，扇区对齐） */
static uint32_t s_version;        /* 本包版本（OTA1 包头 [4:8]），写槽尾元数据用 */
static uint32_t s_last_ms;       /* 会话心跳（由 tick 补时基） */
static uint32_t s_sniff_ms;      /* 业务态嗅探半帧计时 */
static uint8_t  s_active;        /* 有流量 -> 下个 tick 补时基 */
static int      s_result;

/* 收帧状态机 */
static uint8_t  s_rx[OTA_RX_FRAME_MAX];
static uint16_t s_rxlen;
static uint8_t  s_sync;
static uint8_t  s_have;
static uint16_t s_plen;

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* USART_UART_Trans 的第 4 个参数是【自旋次数】不是时间 —— 见 DDL hc32_ll_usart.c:277
 * USART_WaitStatus 原文 "Maximum count of trying to get status"。原来传 100：Release 的循环
 * 比 Debug 紧凑得多，100 次自旋只有几 us，而 460800bps 下一个字节 ≈ 21.7us —— 会在
 * TX_EMPTY / TX_CPLT 置位前 break，帧被【静默截断】（返回值还被 (void) 丢了）。
 * 这里给 20000 次（≈0.5ms/字节，约 25 倍余量）；与 common.c 的 FRAME_TX_SPIN 同源同值。 */
#ifndef OTA_TX_SPIN
#define OTA_TX_SPIN     (20000UL)
#endif

/* 回一帧（ACK/RESUME，载荷 4B LE 偏移）。15B @460800 约 0.33ms，阻塞发送可接受 */
static void tx_frame(uint8_t type, const uint8_t *payload, uint16_t plen)
{
    uint8_t frame[9u + 4u + 2u];
    int     flen;

    flen = ota_frame_build(type, 0u, payload, plen, frame);
    if (flen > 0)
    {
        (void)USART_UART_Trans(USART_UNIT, frame, (uint32_t)flen, OTA_TX_SPIN);
    }
}

void ota_recv_send_ack(uint32_t offset)
{
    uint8_t p[4];
    put_u32(p, offset);
    tx_frame(OTA_FRAME_TYPE_ACK, p, 4u);
}

static void tx_resume(uint32_t offset)
{
    uint8_t p[4];
    put_u32(p, offset);
    tx_frame(OTA_FRAME_TYPE_RESUME, p, 4u);
}

static void rx_reset(void)
{
    s_rxlen = 0u;
    s_sync  = 0u;
    s_have  = 0u;
    s_plen  = 0u;
}

/* 只切会话状态，不碰收帧缓冲 —— 供「先解析后进入」在帧已收全时使用 */
static void enter_mode_only(void)
{
    s_state   = OTA_RX_WAIT_HDR;
    s_slot    = OTA_SLOT_A;
    s_total   = 0UL;
    s_off     = 0UL;
    s_erased  = 0UL;
    s_last_ms = 0UL;
    s_sniff_ms = 0UL;
    s_active  = 0u;
    s_result  = OTA_RX_RESULT_NONE;
}

/* ---------------- 完成：校验 + 激活（写选择器标志） ---------------- */
static int32_t ota_recv_finish(void)
{
    ota_flag_t flag;
    uint32_t   other;
    uint32_t   img_len = s_total - OTA_RX_PKG_HDR_LEN;

    /* 1) 先写槽尾元数据（CRC32 由 Flash 实际内容算出），再回读校验 ——
     *    校验的是 Flash 里真实的内容，不是我们以为写进去的内容 */
    if (0 != ota_img_write_trailer(s_slot, img_len, s_version))
    {
        return -1;
    }
    if (0 != ota_img_check(s_slot))
    {
        return -2;   /* 元数据 magic/长度/TargetSlot/App 二进制 CRC32 任一不过 */
    }

    if (0 != ota_flag_read(&flag))
    {
        (void)memset(&flag, 0, sizeof(flag));
        flag.magic   = OTA_FLAG_MAGIC;
        flag.active  = OTA_SLOT_A;
        flag.state_a = (uint32_t)OTA_SLOT_RUNNABLE;
        flag.state_b = (uint32_t)OTA_SLOT_EMPTY;
    }

    other = OTA_SLOT_OTHER(s_slot);
    flag.active = s_slot;
    if (s_slot == OTA_SLOT_A) { flag.state_a = (uint32_t)OTA_SLOT_TRIAL; flag.fail_a = 0UL; }
    else                      { flag.state_b = (uint32_t)OTA_SLOT_TRIAL; flag.fail_b = 0UL; }

    /* 另一槽若还停在 TRIAL（上一版没确认成功），回落为 RUNNABLE 作为回退 */
    if (other == OTA_SLOT_A)
    {
        if (flag.state_a == (uint32_t)OTA_SLOT_TRIAL) { flag.state_a = (uint32_t)OTA_SLOT_RUNNABLE; }
    }
    else
    {
        if (flag.state_b == (uint32_t)OTA_SLOT_TRIAL) { flag.state_b = (uint32_t)OTA_SLOT_RUNNABLE; }
    }

    flag.boot_count = 0UL;
    flag.flags      = OTA_FLAG_NEED_CONFIRM;

    if (0 != ota_flag_write(&flag)) { return -2; }
    return 0;
}

/* ---------------- 数据帧 ---------------- */
static void handle_data(const uint8_t *payload, uint16_t plen)
{
    uint32_t img_off;
    uint32_t end;
    uint32_t to;
    uint32_t plen_pkg;

    if (s_state == OTA_RX_WAIT_HDR)
    {
        /* 包头帧：payload 必为 82B 统一 OTA 包头 */
        if (plen != (uint16_t)OTA_RX_PKG_HDR_LEN) { tx_resume(0UL); return; }
        if (payload[0] != (uint8_t)OTA_FRAME_MAGIC0 || payload[1] != (uint8_t)OTA_FRAME_MAGIC1 ||
            payload[2] != (uint8_t)OTA_FRAME_MAGIC2 || payload[3] != (uint8_t)OTA_FRAME_MAGIC3)
        {
            tx_resume(0UL);
            return;
        }
        (void)memcpy(&plen_pkg, &payload[10], 4);   /* 包头 [10:14] = payload 长度 */
        if ((plen_pkg < 64UL) || (plen_pkg > OTA_IMG_MAX)) { tx_resume(0UL); return; }
        (void)memcpy(&s_version, &payload[4], 4);    /* 包头 [4:8] = 版本，写槽尾元数据用 */

        s_total  = OTA_RX_PKG_HDR_LEN + plen_pkg;
        s_slot   = OTA_SLOT_OTHER(ota_flag_active_slot());   /* 目标 = 非活动槽 */
        s_off    = OTA_RX_PKG_HDR_LEN;
        s_erased = 0UL;
        s_result = OTA_RX_RESULT_NONE;
        s_state  = OTA_RX_DATA;
        ota_recv_send_ack(s_off);          /* ACK(82)：包头已受理，请从 82 起发 */
        return;
    }

    if (s_state != OTA_RX_DATA) { return; }
    if (plen == 0u || plen > (uint16_t)OTA_RX_MAX_PAYLOAD) { tx_resume(s_off); return; }
    if ((s_off + (uint32_t)plen) > s_total) { tx_resume(s_off); return; }

    img_off = s_off - OTA_RX_PKG_HDR_LEN;
    end     = img_off + (uint32_t)plen;

    /* 惰性擦除：写到哪个扇区擦哪个（避免一次性擦整槽） */
    if (end > s_erased)
    {
        to = ((end + OTA_FLASH_SECTOR - 1UL) / OTA_FLASH_SECTOR) * OTA_FLASH_SECTOR;
        if (0 != ota_flash_erase(OTA_SLOT_BASE(s_slot) + s_erased, to - s_erased))
        {
            s_state  = OTA_RX_FAIL;
            s_result = OTA_RX_RESULT_FAIL;
            return;
        }
        s_erased = to;
    }

    if (0 != ota_flash_write(OTA_SLOT_BASE(s_slot) + img_off, payload, plen))
    {
        s_state  = OTA_RX_FAIL;
        s_result = OTA_RX_RESULT_FAIL;
        return;
    }
    if (0 != ota_flash_verify(OTA_SLOT_BASE(s_slot) + img_off, payload, plen))
    {
        tx_resume(s_off);   /* 回读不一致：请主机重发本块 */
        return;
    }

    s_off += (uint32_t)plen;
    ota_recv_send_ack(s_off);

    if (s_off >= s_total)
    {
        if (0 == ota_recv_finish())
        {
            ota_recv_send_ack(s_total);
            s_state  = OTA_RX_DONE;
            s_result = OTA_RX_RESULT_OK;
        }
        else
        {
            s_state  = OTA_RX_FAIL;
            s_result = OTA_RX_RESULT_FAIL;
        }
    }
}

/* 版本 / 槽位查询（RESUME 帧，载荷为 4 字节 ASCII） */
static void handle_query(const uint8_t *payload, uint16_t plen)
{
    if (plen != 4u) { return; }
    if (payload[0] == (uint8_t)'V' && payload[1] == (uint8_t)'E' &&
        payload[2] == (uint8_t)'R' && payload[3] == (uint8_t)'1')
    {
        ota_recv_send_ack(OTA_RX_FW_VERSION);
    }
    else if (payload[0] == (uint8_t)'S' && payload[1] == (uint8_t)'L' &&
             payload[2] == (uint8_t)'O' && payload[3] == (uint8_t)'T')
    {
        ota_recv_send_ack(ota_flag_active_slot());   /* 主机据此选 A/B 镜像 */
    }
}

/* 一帧收全 */
static void handle_frame(void)
{
    uint16_t crc_rx;
    uint16_t crc_calc;
    uint8_t  type;
    uint16_t plen;

    if (s_rxlen < (uint16_t)(9u + 2u)) { return; }
    plen = (uint16_t)(s_rx[7] | ((uint16_t)s_rx[8] << 8));
    if (s_rxlen != (uint16_t)(9u + plen + 2u)) { return; }

    crc_rx   = (uint16_t)(s_rx[9u + plen] | ((uint16_t)s_rx[9u + plen + 1u] << 8));
    crc_calc = ota_frame_crc16(s_rx, (uint32_t)(9u + plen));
    if (crc_rx != crc_calc)
    {
        /* 帧 CRC 错：请主机从当前偏移重发（与设备端既有语义一致） */
        if (s_state == OTA_RX_DATA) { tx_resume(s_off); }
        return;
    }

    type = s_rx[4];

    /* 业务态自动进入：先解析、后进入 —— 只有 CRC16 合法且长度合规的 DATA 帧才切升级模式。
     * 不因「见到 'O'」或「半帧/坏帧」而进入，避免误触发让业务停摆 10s。 */
    if (s_state == OTA_RX_OFF)
    {
        if ((type != (uint8_t)OTA_FRAME_TYPE_DATA) || (plen == 0u) || (plen > (uint16_t)OTA_RX_MAX_PAYLOAD))
        {
            return;
        }
        enter_mode_only();   /* 保留已收帧，交给 handle_data 处理 */
    }

    if (type == (uint8_t)OTA_FRAME_TYPE_DATA)
    {
        handle_data(&s_rx[9], plen);
    }
    else if (type == (uint8_t)OTA_FRAME_TYPE_RESUME)
    {
        handle_query(&s_rx[9], plen);
    }
    /* ACK 帧由主机发出，设备端忽略 */
}

/* ---------------- 对外 ---------------- */
void ota_recv_init(void)
{
    (void)memset(s_rx, 0, sizeof(s_rx));
    rx_reset();
    s_state  = OTA_RX_OFF;
    s_slot   = OTA_SLOT_A;
    s_total  = 0UL;
    s_off    = 0UL;
    s_erased = 0UL;
    s_last_ms = 0UL;
    s_active  = 0u;
    s_result = OTA_RX_RESULT_NONE;
}

void ota_recv_enter(void)
{
    rx_reset();
    enter_mode_only();
}

void ota_recv_exit(void)
{
    rx_reset();
    s_state = OTA_RX_OFF;
}

uint8_t ota_recv_busy(void)
{
    return ((s_state == OTA_RX_WAIT_HDR) || (s_state == OTA_RX_DATA)) ? 1u : 0u;
}

ota_rx_state_t ota_recv_state(void)        { return s_state; }
uint32_t       ota_recv_progress(void)     { return s_off; }
uint32_t       ota_recv_total(void)        { return s_total; }
int            ota_recv_result(void)       { return s_result; }
void           ota_recv_result_clear(void) { s_result = OTA_RX_RESULT_NONE; }

/* 本字节是否由 OTA 独占（调用方据此跳过业务解析）。
 *
 * 【关键不变式】只有【真的处于一次升级会话中】(OTA_RX_WAIT_HDR / OTA_RX_DATA) 才独占线路。
 * 判据与 ota_recv_busy() 完全一致，两端必须一致 —— 否则会出两类事故，都踩过：
 *
 *   ① 业务态（OTA_RX_OFF）返回 1 → 吞字节：业务流里任何一个 0x4F（'O'）都会被吞掉
 *      （OTA1 的 magic 是 "OTA1" = 4F 54 41 31，0x4F 是再普通不过的载荷字节）。
 *      业务帧因此少一个字节，整帧 CRC16 失败被丢弃：N 字节的帧命中概率约 1-(255/256)^N，
 *      20~40 字节载荷就是 8~15% 丢帧；若载荷里恰好出现 4F 54 41 31 连串，则整帧剩余全被吞。
 *
 *   ② 会话【结束后】(OTA_RX_DONE / OTA_RX_FAIL) 返回 1 → 链路被永久占死：
 *      ota_recv_busy() 此时已是 0，Check_Uart_Pdu 不会再把字节转给 ota_recv_feed，
 *      于是每个字节都走 sniff() 被吞 —— 而 FAIL 之后没有任何地方会自动回到 OFF
 *      （只有 tick 里的空闲超时那条路会 exit），业务通信就再也回不来了。 */
static uint8_t claim_byte(void)
{
    return ((s_state == OTA_RX_WAIT_HDR) || (s_state == OTA_RX_DATA)) ? 1u : 0u;
}

/* 消费 1 字节。
 * 返回值由 claim_byte() 决定 —— 业务态一律返回 0（字节照样喂给业务解析器），
 * 升级态返回 1（本链路只跑 OTA1）。缓冲与 CRC 判定不受影响，仍照常推进。 */
static uint8_t consume_byte(uint8_t b)
{
    static const uint8_t magic[4] = {
        (uint8_t)OTA_FRAME_MAGIC0, (uint8_t)OTA_FRAME_MAGIC1,
        (uint8_t)OTA_FRAME_MAGIC2, (uint8_t)OTA_FRAME_MAGIC3
    };

    s_active = 1u;   /* 有流量：由下个 tick 补时基 */

    if (s_sync == 0u)
    {
        if (b == magic[s_have])
        {
            s_rx[s_have] = b;
            s_have++;
            if (s_have >= 4u) { s_rxlen = 4u; s_sync = 1u; s_have = 0u; }
        }
        else
        {
            s_have = 0u;
        }
        /* 部分匹配中也照常返回 claim_byte()：业务态下这字节必须继续交给业务解析器 */
        return claim_byte();
    }

    if (s_rxlen >= (uint16_t)sizeof(s_rx)) { rx_reset(); return claim_byte(); }
    s_rx[s_rxlen++] = b;

    if (s_rxlen == 9u)
    {
        s_plen = (uint16_t)(s_rx[7] | ((uint16_t)s_rx[8] << 8));
        if (s_plen > (uint16_t)OTA_RX_MAX_PAYLOAD) { rx_reset(); return claim_byte(); }
    }
    else if ((s_rxlen >= 11u) && (s_rxlen == (uint16_t)(9u + s_plen + 2u)))
    {
        handle_frame();   /* 合法 DATA 帧会在这里把 s_state 切出 OTA_RX_OFF */
        rx_reset();
    }
    return claim_byte();
}

uint8_t ota_recv_sniff(uint8_t byte)
{
    return consume_byte(byte);
}

void ota_recv_feed(const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    if ((buf == NULL) || (len == 0UL)) { return; }
    for (i = 0u; i < len; i++)
    {
        (void)consume_byte(buf[i]);
    }
}

void ota_recv_tick(uint32_t now_ms)
{
    if (s_state == OTA_RX_OFF)
    {
        /* 业务态嗅探：半帧 50ms 未补齐则丢弃，避免卡在半个魔数上 */
        if ((s_sync != 0u) || (s_have > 0u))
        {
            if ((s_sniff_ms == 0UL) || (s_active != 0u)) { s_sniff_ms = now_ms; s_active = 0u; }
            else if ((now_ms - s_sniff_ms) > OTA_RX_GUARD_MS) { rx_reset(); s_sniff_ms = 0UL; }
        }
        else
        {
            s_sniff_ms = 0UL;
            s_active   = 0u;
        }
        return;
    }
    if ((s_state == OTA_RX_DONE) || (s_state == OTA_RX_FAIL)) { return; }
    if ((s_last_ms == 0UL) || (s_active != 0u))
    {
        s_last_ms = now_ms;   /* 首次进入，或本拍有流量 */
        s_active  = 0u;
        return;
    }
    if ((now_ms - s_last_ms) > OTA_RX_IDLE_MS)
    {
        ota_recv_exit();
        s_result = OTA_RX_RESULT_FAIL;
    }
}
