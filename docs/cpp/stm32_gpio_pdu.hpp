// =============================================================================
//  stm32_gpio_pdu.hpp  —  STM32F0 中继板 -> Linux 上行帧 (gpio_pdu) 解析库
//
//  链路: STM32 COM1 (UART1) @115200 8N1  上行: PDUHEAD(0xFF) 定长 32B
//  帧结构 (与 STM32 侧 BSP/app.h 的 gpio_pdu / BSP/app.c 的 ipcReportBuild 完全一致):
//
//    偏移  长度  字段            说明
//     0     1    FrameHead       0xFF (PDUHEAD)
//     1     1    Pdu_len         32
//     2     1    DeviceID        设备号(STM32 侧当前填 0)
//     3     1    AntID           0 = 整机聚合上报(STM32 侧当前恒 0)
//     4..11  8    Rad_Status[8]   天线 1..8: 1 = 有人 / 0 = 无人
//    12..19  8    Alarm_Done[8]   天线 1..8: 1 = 该天线所属雷达板正在声光报警
//    20..29 10    GPIO[10]        预留(STM32 侧当前填 0)
//    30..31  2    crc             CRC-16/CCITT-FALSE(小端), 覆盖 [0..29]
//
//  CRC: poly 0x1021, init 0xFFFF, 不反射, 无终值异或 —— 自检向量 "123456789" -> 0x29B1
//  天线映射(5 路雷达板 -> 8 支 RFID 天线):
//        COM6 -> 天线1 ; COM2 -> 天线2,3 ; COM3 -> 天线4,5 ; COM4 -> 天线6,7 ; COM5 -> 天线8
//  发送时机: 状态变化即报 / 无变化 1s 心跳 / 收到 Linux 下发命令后立即回一帧
//
//  仅头文件, C++11 及以上, 无第三方依赖。
// =============================================================================
#ifndef STM32_GPIO_PDU_HPP
#define STM32_GPIO_PDU_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace alarm_board {

// ---------------------------------------------------------------- 常量
const std::uint8_t  kFrameHead = 0xFFu;   // PDUHEAD
const std::size_t   kFrameSize = 32u;     // APP_FRAME_LEN_MAX
const std::size_t   kAntCount  = 8u;      // RFID 模块 8 支天线
const std::size_t   kGpioCount = 10u;     // 预留 GPIO 字节数
const std::uint16_t kCrcPoly   = 0x1021u;
const std::uint16_t kCrcInit   = 0xFFFFu;

// ---------------------------------------------------------------- CRC
// 与 STM32 的 ipcCrc() (bsp/app.c) 逐位等价: CRC-16/CCITT-FALSE
inline std::uint16_t crc16_ccitt(const std::uint8_t *data, std::size_t len)
{
    std::uint16_t crc = kCrcInit;

    for (std::size_t i = 0u; i < len; ++i)
    {
        crc ^= static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[i]) << 8);

        for (int bit = 0; bit < 8; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = static_cast<std::uint16_t>((crc << 1) ^ kCrcPoly);
            }
            else
            {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }

    return crc;
}

// ---------------------------------------------------------------- 帧
struct GpioPdu
{
    std::uint8_t device_id = 0u;
    std::uint8_t ant_id    = 0u;
    std::array<std::uint8_t, kAntCount>  rad_status{};   // 下标 0 = 天线1
    std::array<std::uint8_t, kAntCount>  alarm_done{};
    std::array<std::uint8_t, kGpioCount> gpio{};
    std::uint16_t crc = 0u;                              // 帧内 CRC(小端)

    // ant: 1..8
    bool present(int ant) const
    {
        return (ant >= 1 && ant <= static_cast<int>(kAntCount)) ? (rad_status[static_cast<std::size_t>(ant - 1)] != 0u) : false;
    }

    bool alarming(int ant) const
    {
        return (ant >= 1 && ant <= static_cast<int>(kAntCount)) ? (alarm_done[static_cast<std::size_t>(ant - 1)] != 0u) : false;
    }

    // 有人/报警的天线号(1..8)列表, 例如 "1,3"
    std::string present_ants() const { return join_mask(rad_status); }
    std::string alarming_ants() const { return join_mask(alarm_done); }

    // 单行摘要
    std::string to_string() const
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "Dev=%u Ant=%u 有人=[%s] 报警=[%s] GPIO0..9=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                      static_cast<unsigned>(device_id), static_cast<unsigned>(ant_id),
                      present_ants().c_str(), alarming_ants().c_str(),
                      gpio[0], gpio[1], gpio[2], gpio[3], gpio[4],
                      gpio[5], gpio[6], gpio[7], gpio[8], gpio[9]);
        return std::string(buf);
    }

    // 多行明细(8 支天线逐行)
    std::string dump() const
    {
        std::string s = to_string();
        s += "\n";
        for (std::size_t i = 0u; i < kAntCount; ++i)
        {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "    天线%zu: 有人=%u 报警=%u\n",
                          i + 1u,
                          static_cast<unsigned>(rad_status[i]),
                          static_cast<unsigned>(alarm_done[i]));
            s += buf;
        }
        return s;
    }

private:
    static std::string join_mask(const std::array<std::uint8_t, kAntCount> &m)
    {
        std::string s;
        for (std::size_t i = 0u; i < kAntCount; ++i)
        {
            if (m[i] != 0u)
            {
                if (!s.empty()) { s += ","; }
                char n[4];
                std::snprintf(n, sizeof(n), "%zu", i + 1u);
                s += n;
            }
        }
        return s;
    }
};

// ---------------------------------------------------------------- 流式解析器
// 用法:
//   GpioPduParser parser;
//   parser.feed(buf, n);          // 串口读到多少喂多少(允许任意切分)
//   GpioPdu f;
//   while (parser.pop(f)) { ... }  // 取出一帧(内部队列 4 帧, 溢出丢最旧)
// 非法字节/坏 CRC 自动逐字节重同步, 统计见 frames_ok()/crc_errors()/resync_drops()。
class GpioPduParser
{
public:
    typedef void (*FrameCallback)(const GpioPdu &frame, void *user);
    static const std::size_t kQueueDepth = 4u;

    void set_callback(FrameCallback cb, void *user = 0) { cb_ = cb; user_ = user; }

    void feed(const std::uint8_t *data, std::size_t len)
    {
        for (std::size_t i = 0u; i < len; ++i) { feed_byte(data[i]); }
    }

    void feed_byte(std::uint8_t b)
    {
        if (len_ < kFrameSize) { buf_[len_++] = b; }
        try_parse();
    }

    bool pop(GpioPdu &out)
    {
        if (q_count_ == 0u) { return false; }
        out = queue_[q_head_];
        q_head_ = (q_head_ + 1u) % kQueueDepth;
        --q_count_;
        return true;
    }

    std::uint32_t frames_ok()    const { return frames_ok_; }
    std::uint32_t crc_errors()   const { return crc_errors_; }
    std::uint32_t resync_drops() const { return resync_drops_; }
    std::size_t   pending()      const { return len_; }

    void reset()
    {
        len_ = 0u;
        q_head_ = 0u;
        q_count_ = 0u;
    }

private:
    void drop_front(std::size_t n)
    {
        if (n >= len_) { len_ = 0u; return; }
        std::memmove(buf_.data(), buf_.data() + n, len_ - n);
        len_ -= n;
    }

    void try_parse()
    {
        while (len_ > 0u)
        {
            if (buf_[0] != kFrameHead)
            {
                drop_front(1u);
                ++resync_drops_;
                continue;
            }

            if (len_ < kFrameSize) { return; }        // 等更多字节

            const std::uint16_t got = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(buf_[30]) | static_cast<std::uint16_t>(static_cast<std::uint16_t>(buf_[31]) << 8));
            const std::uint16_t calc = crc16_ccitt(buf_.data(), kFrameSize - 2u);

            if ((calc != got) || (buf_[1] != static_cast<std::uint8_t>(kFrameSize)))
            {
                drop_front(1u);                        // 坏帧: 丢 1 字节后重新找帧头
                ++crc_errors_;
                continue;
            }

            GpioPdu f;
            decode(buf_.data(), f);
            ++frames_ok_;
            deliver(f);
            drop_front(kFrameSize);
        }
    }

    static void decode(const std::uint8_t *p, GpioPdu &f)
    {
        f.device_id = p[2];
        f.ant_id    = p[3];
        for (std::size_t i = 0u; i < kAntCount; ++i)
        {
            f.rad_status[i] = p[4u + i];
            f.alarm_done[i] = p[12u + i];
        }
        for (std::size_t i = 0u; i < kGpioCount; ++i) { f.gpio[i] = p[20u + i]; }
        f.crc = static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[30]) |
                                           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[31]) << 8));
    }

    void deliver(const GpioPdu &f)
    {
        if (q_count_ == kQueueDepth)                  // 满了丢最旧
        {
            q_head_ = (q_head_ + 1u) % kQueueDepth;
            --q_count_;
        }
        queue_[(q_head_ + q_count_) % kQueueDepth] = f;
        ++q_count_;

        if (cb_ != 0) { cb_(f, user_); }
    }

    std::array<std::uint8_t, kFrameSize> buf_{};
    std::size_t len_ = 0u;

    std::array<GpioPdu, kQueueDepth> queue_{};
    std::size_t q_head_  = 0u;
    std::size_t q_count_ = 0u;

    std::uint32_t frames_ok_    = 0u;
    std::uint32_t crc_errors_   = 0u;
    std::uint32_t resync_drops_ = 0u;

    FrameCallback cb_  = 0;
    void         *user_ = 0;
};

// ---------------------------------------------------------------- 状态缓存 / 链路判活
// STM32 侧: 变化即报 + 1s 心跳。若 timeout_ms 内一帧都没收到, 视为链路断开。
class GpioPduStatus
{
public:
    static const std::uint32_t kDefaultTimeoutMs = 2500u;   // 2.5 x 心跳周期

    // 返回 true 表示这一帧相对上一帧内容有变化(心跳帧会返回 false)
    bool update(const GpioPdu &f, std::uint64_t now_ms)
    {
        const bool changed = (frames_ == 0u) ||
                             (std::memcmp(&f.rad_status, &last_.rad_status, kAntCount) != 0) ||
                             (std::memcmp(&f.alarm_done, &last_.alarm_done, kAntCount) != 0) ||
                             (f.device_id != last_.device_id) || (f.ant_id != last_.ant_id);
        last_ = f;
        last_rx_ms_ = now_ms;
        ++frames_;
        if (changed) { last_change_ms_ = now_ms; }
        return changed;
    }

    bool link_alive(std::uint64_t now_ms, std::uint32_t timeout_ms = kDefaultTimeoutMs) const
    {
        if (frames_ == 0u) { return false; }
        return (now_ms - last_rx_ms_) <= static_cast<std::uint64_t>(timeout_ms);
    }

    bool present(int ant) const { return last_.present(ant); }
    bool alarming(int ant) const { return last_.alarming(ant); }

    const GpioPdu &last() const { return last_; }
    std::uint64_t last_rx_ms() const { return last_rx_ms_; }
    std::uint64_t last_change_ms() const { return last_change_ms_; }
    std::uint32_t frames() const { return frames_; }

private:
    GpioPdu       last_{};
    std::uint64_t last_rx_ms_     = 0u;
    std::uint64_t last_change_ms_ = 0u;
    std::uint32_t frames_         = 0u;
};

// ---------------------------------------------------------------- 组帧(仅测试/模拟用)
// 生成与 STM32 ipcReportBuild() 相同格式的上行帧, 便于无硬件自测。
inline void build_frame(const std::array<std::uint8_t, kAntCount> &rad,
                        const std::array<std::uint8_t, kAntCount> &alarm,
                        std::uint8_t *out,
                        std::uint8_t device_id = 0u,
                        std::uint8_t ant_id = 0u)
{
    std::memset(out, 0, kFrameSize);
    out[0] = kFrameHead;
    out[1] = static_cast<std::uint8_t>(kFrameSize);
    out[2] = device_id;
    out[3] = ant_id;
    for (std::size_t i = 0u; i < kAntCount; ++i)
    {
        out[4u + i]  = rad[i];
        out[12u + i] = alarm[i];
    }
    const std::uint16_t c = crc16_ccitt(out, kFrameSize - 2u);
    out[30] = static_cast<std::uint8_t>(c & 0xFFu);
    out[31] = static_cast<std::uint8_t>(c >> 8);
}

}   // namespace alarm_board

#endif  // STM32_GPIO_PDU_HPP
