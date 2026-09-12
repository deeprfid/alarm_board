// =============================================================================
//  stm32_gpio_pdu.hpp —— STM32F0 中继板上行包 (gpio_pdu) 解析
//  仅数据包解析: 校验帧头/帧长/CRC 并取出各字段。不含收发、缓存、状态管理。
//  帧格式定义见 docs/stm32_uplink_interface.md
// =============================================================================
#ifndef STM32_GPIO_PDU_HPP
#define STM32_GPIO_PDU_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace alarm_board {

// 上行帧常量
constexpr std::uint8_t kFrameHead = 0xFFu;   // 帧头
constexpr std::size_t  kFrameSize = 32u;     // 帧长(字节)
constexpr std::size_t  kAntCount  = 8u;      // 天线数

// CRC-16/CCITT-FALSE: poly 0x1021, 初值 0xFFFF, 逐位 MSB 优先, 不反射, 无终值异或
// (与固件 ipcCrc() 一致; 自检向量 "123456789" -> 0x29B1)
inline std::uint16_t crc16_ccitt(const std::uint8_t *data, std::size_t len)
{
    std::uint16_t crc = 0xFFFFu;

    for (std::size_t i = 0u; i < len; ++i)
    {
        crc ^= static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[i]) << 8);

        for (int bit = 0; bit < 8; ++bit)
        {
            crc = ((crc & 0x8000u) != 0u)
                      ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021u)
                      : static_cast<std::uint16_t>(crc << 1);
        }
    }

    return crc;
}

// 上行包字段 (偏移见注释; 数组下标 0 = 天线1)
struct GpioPdu
{
    std::uint8_t device_id = 0u;                          // 偏移 2
    std::uint8_t ant_id    = 0u;                          // 偏移 3
    std::array<std::uint8_t, kAntCount> rad_status{};     // 偏移 4..11  天线 1..8: 1=有人 0=无人
    std::array<std::uint8_t, kAntCount> alarm_done{};     // 偏移 12..19 天线 1..8: 1=正在声光报警 0=未报警
    std::array<std::uint8_t, 10u>       gpio{};           // 偏移 20..29 预留
};

// 解析一帧。len 必须为 32; 帧头/帧长/CRC 任一不符返回 false(不修改 out)
inline bool parse_gpio_pdu(const std::uint8_t *data, std::size_t len, GpioPdu &out)
{
    if ((data == 0) || (len != kFrameSize))                { return false; }
    if (data[0] != kFrameHead)                             { return false; }
    if (data[1] != static_cast<std::uint8_t>(kFrameSize))  { return false; }

    const std::uint16_t crc_calc  = crc16_ccitt(data, kFrameSize - 2u);
    const std::uint16_t crc_frame = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[30]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[31]) << 8));
    if (crc_calc != crc_frame)                             { return false; }

    GpioPdu f;
    f.device_id = data[2];
    f.ant_id    = data[3];

    for (std::size_t i = 0u; i < kAntCount; ++i)
    {
        f.rad_status[i] = data[4u + i];
        f.alarm_done[i] = data[12u + i];
    }

    for (std::size_t i = 0u; i < f.gpio.size(); ++i)
    {
        f.gpio[i] = data[20u + i];
    }

    out = f;
    return true;
}

}   // namespace alarm_board

#endif  // STM32_GPIO_PDU_HPP
