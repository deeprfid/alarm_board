# docs/cpp -- STM32 上行帧解析库（Linux 侧）

接口说明见上级目录文档: docs/stm32_uplink_interface.md

| 文件 | 说明 |
| --- | --- |
| stm32_gpio_pdu.hpp | 仅头文件解析库: CRC、帧结构、流式解析器、状态缓存、自测组帧 |
| example_gpio_pdu.cpp | 示例程序: 自测 / 读串口 / 读 stdin |

## 编译

    g++ -std=c++11 -O2 -Wall -Wextra -I docs/cpp docs/cpp/example_gpio_pdu.cpp -o gpio_pdu

仅需标准库 + POSIX termios, 无第三方依赖。Windows 下示例程序的串口部分需换成 Win32 串口 API,
解析库本身(头文件)与平台无关。

## 运行

    ./gpio_pdu --selftest                  # 无硬件自测
    ./gpio_pdu -d /dev/ttyS0 -b 115200     # 直接读串口
    cat /dev/ttyS0 | ./gpio_pdu            # 从 stdin 读

--selftest 会检查: CRC 自检向量 0x29B1、整帧解析、逐字节喂入、噪声/半帧重同步、坏 CRC 丢弃、
心跳帧不算变化。

## 集成要点

1. 串口必须 raw 模式 (cfmakeraw), 否则 0xFF/0x0A 会被行规程处理掉;
2. 解析器是流式的, 任意切分都能接受: parser.feed(buf, n) 之后 while (parser.pop(frame)) 取帧;
3. 用 GpioPduStatus::link_alive(now_ms) (默认 2.5s) 判断链路是否在线 —— 正常时每 1s 至少一帧心跳;
4. 天线号 1..8 与 STM32 的 5 路雷达口映射固定: COM6->1, COM2->2,3, COM3->4,5, COM4->6,7, COM5->8。
