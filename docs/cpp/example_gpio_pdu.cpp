// =============================================================================
//  example_gpio_pdu.cpp  —  STM32F0 中继板上行帧(gpio_pdu)解析示例 (Linux)
//
//  编译:
//      g++ -std=c++11 -O2 -Wall -Wextra -I docs/cpp docs/cpp/example_gpio_pdu.cpp -o gpio_pdu
//  运行:
//      ./gpio_pdu --selftest                 # 无硬件自测(CRC 向量 + 组帧解析)
//      ./gpio_pdu -d /dev/ttyS0 -b 115200    # 直接读串口
//      cat /dev/ttyS0 | ./gpio_pdu           # 或从 stdin 读(不带 -d 时)
// =============================================================================
#include "stm32_gpio_pdu.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

using namespace alarm_board;

static std::uint64_t now_ms()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

static bool hex_dump(const std::uint8_t *p, std::size_t n, std::string &out)
{
    static const char *kHex = "0123456789ABCDEF";
    out.clear();
    for (std::size_t i = 0u; i < n; ++i)
    {
        if (i != 0u) { out += ' '; }
        out += kHex[(p[i] >> 4) & 0x0F];
        out += kHex[p[i] & 0x0F];
    }
    return true;
}

static int open_serial(const char *dev, int baud)
{
    const int fd = ::open(dev, O_RDONLY | O_NOCTTY);
    if (fd < 0) { std::fprintf(stderr, "open %s failed: %s\n", dev, std::strerror(errno)); return -1; }

    termios tio;
    std::memset(&tio, 0, sizeof(tio));
    if (::tcgetattr(fd, &tio) != 0) { std::perror("tcgetattr"); ::close(fd); return -1; }

    ::cfmakeraw(&tio);                       // 原始模式: 8N1, 无回显/无行缓冲
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 0;

    speed_t sp = B115200;
    switch (baud)
    {
        case 9600:   sp = B9600;   break;
        case 19200:  sp = B19200;  break;
        case 38400:  sp = B38400;  break;
        case 57600:  sp = B57600;  break;
        case 115200: sp = B115200; break;
        case 230400: sp = B230400; break;
        case 460800: sp = B460800; break;
        default: std::fprintf(stderr, "unsupported baud %d, use 115200\n", baud); sp = B115200; break;
    }
    ::cfsetispeed(&tio, sp);
    ::cfsetospeed(&tio, sp);

    if (::tcsetattr(fd, TCSANOW, &tio) != 0) { std::perror("tcsetattr"); ::close(fd); return -1; }
    ::tcflush(fd, TCIFLUSH);
    return fd;
}

// ------------------------------------------------------------------ 自测
static int selftest()
{
    int fail = 0;

    // 1) CRC 自检向量: CRC-16/CCITT-FALSE("123456789") = 0x29B1
    const char *vec = "123456789";
    const std::uint16_t c = crc16_ccitt(reinterpret_cast<const std::uint8_t *>(vec), 9u);
    std::printf("[1] CRC \"123456789\" = 0x%04X (期望 0x29B1) %s\n", c, (c == 0x29B1u) ? "OK" : "FAIL");
    if (c != 0x29B1u) { ++fail; }

    // 2) 组帧 + 解析(整帧 / 逐字节 / 带噪声与半帧)
    std::array<std::uint8_t, kAntCount> rad{};
    std::array<std::uint8_t, kAntCount> alarm{};
    rad[0] = 1u; rad[2] = 1u;          // 天线1、天线3 有人
    alarm[2] = 1u;                     // 天线3 报警中

    std::uint8_t frame[kFrameSize];
    build_frame(rad, alarm, frame);

    std::string hex;
    hex_dump(frame, kFrameSize, hex);
    std::printf("[2] 模拟上行帧: %s\n", hex.c_str());

    GpioPduParser parser;
    parser.feed(frame, kFrameSize);
    GpioPdu f;
    if (parser.pop(f))
    {
        std::printf("    解析: %s\n", f.to_string().c_str());
        const bool ok = f.present(1) && f.present(3) && !f.present(2) && f.alarming(3) && !f.alarming(1);
        std::printf("    字段校验: %s\n", ok ? "OK" : "FAIL");
        if (!ok) { ++fail; }
    }
    else { std::printf("    解析: FAIL (无输出帧)\n"); ++fail; }

    // 3) 逐字节喂入
    GpioPduParser p2;
    for (std::size_t i = 0u; i < kFrameSize; ++i) { p2.feed_byte(frame[i]); }
    std::printf("[3] 逐字节喂入: frames_ok=%u crc_err=%u\n", p2.frames_ok(), p2.crc_errors());
    if (p2.frames_ok() != 1u) { ++fail; }

    // 4) 噪声 + 半帧 + 完整帧, 应重同步并只解出 2 帧(坏帧计数 >= 1)
    const std::uint8_t junk[] = {0x00, 0x12, 0xFF, 0xAA, 0x55, 0xFF};
    GpioPduParser p3;
    p3.feed(junk, sizeof(junk));
    p3.feed(frame, kFrameSize);
    p3.feed(frame, 17u);
    p3.feed(frame, kFrameSize);
    std::printf("[4] 噪声/半帧重同步: frames_ok=%u crc_err=%u resync=%u\n",
                p3.frames_ok(), p3.crc_errors(), p3.resync_drops());
    if (p3.frames_ok() != 2u) { ++fail; }

    // 5) 坏 CRC 必须被丢弃
    std::uint8_t bad[kFrameSize];
    std::memcpy(bad, frame, kFrameSize);
    bad[5] ^= 0x01u;
    GpioPduParser p4;
    p4.feed(bad, kFrameSize);
    std::printf("[5] 坏 CRC 丢弃: frames_ok=%u crc_err=%u\n", p4.frames_ok(), p4.crc_errors());
    if (p4.frames_ok() != 0u || p4.crc_errors() == 0u) { ++fail; }

    // 6) 状态缓存: 相同内容不算变化, 不同内容算变化
    GpioPduStatus st;
    GpioPdu a1, a2;
    parser.reset();
    parser.feed(frame, kFrameSize); parser.pop(a1);
    parser.feed(frame, kFrameSize); parser.pop(a2);
    const bool c1 = st.update(a1, 1000u);
    const bool c2 = st.update(a2, 1200u);
    std::printf("[6] 变化判定: 首帧 changed=%d 重复帧 changed=%d\n", static_cast<int>(c1), static_cast<int>(c2));
    if (!c1 || c2) { ++fail; }

    std::printf("== 自测结果: %s ==\n", (fail == 0) ? "全部通过" : "有失败项");
    return (fail == 0) ? 0 : 1;
}

// ------------------------------------------------------------------ 主循环
int main(int argc, char **argv)
{
    const char *device = 0;
    int baud = 115200;
    bool st = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--selftest") == 0) { st = true; }
        else if (std::strcmp(argv[i], "-d") == 0 && (i + 1) < argc) { device = argv[++i]; }
        else if (std::strcmp(argv[i], "-b") == 0 && (i + 1) < argc) { baud = std::atoi(argv[++i]); }
        else { std::fprintf(stderr, "usage: %s [--selftest] [-d /dev/ttySx] [-b baud]\n", argv[0]); return 2; }
    }

    if (st) { return selftest(); }

    int fd = 0;                                  // 0 = stdin
    if (device != 0)
    {
        fd = open_serial(device, baud);
        if (fd < 0) { return 1; }
        std::printf("已打开 %s @%d 8N1, 等待上行帧...\n", device, baud);
    }
    else
    {
        std::printf("从 stdin 读取(可直接 cat /dev/ttyS0 | %s)\n", argv[0]);
    }

    GpioPduParser parser;
    GpioPduStatus status;
    std::uint8_t buf[256];
    std::uint64_t last_report = 0u;
    bool link_reported_down = true;

    for (;;)
    {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n < 0)
        {
            if (errno == EINTR) { continue; }
            std::perror("read");
            break;
        }
        if (n == 0) { break; }                       // EOF(stdin/串口关闭)

        parser.feed(buf, static_cast<std::size_t>(n));

        GpioPdu f;
        while (parser.pop(f))
        {
            const std::uint64_t t = now_ms();
            const bool changed = status.update(f, t);
            std::printf("[%llu ms] %s %s\n",
                        static_cast<unsigned long long>(t),
                        changed ? "(变化)" : "(心跳)",
                        f.to_string().c_str());
        }

        const std::uint64_t t = now_ms();
        if ((t - last_report) >= 1000u)
        {
            last_report = t;
            const bool alive = status.link_alive(t);
            if (!alive && !link_reported_down)
            {
                link_reported_down = true;
                std::printf("[%llu ms] !! 链路超时: 超过 2.5s 未收到上行帧\n",
                            static_cast<unsigned long long>(t));
            }
            else if (alive && link_reported_down)
            {
                link_reported_down = false;
            }
            std::printf("    (统计: 帧=%u CRC错=%u 重同步丢字节=%u 链路=%s)\n",
                        parser.frames_ok(), parser.crc_errors(), parser.resync_drops(),
                        alive ? "在线" : "断线");
        }
    }

    if (device != 0) { ::close(fd); }
    return 0;
}
