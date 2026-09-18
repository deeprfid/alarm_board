# 本地 OTA 串口工具（F4A0 Phase 3 实测）

与固件端 `ota_transport_uart.c` 的 "OTA1" 帧协议 + 0x52 断点续传配套。
无硬件时用 `ota_selftest.py` 验证协议一致性；有硬件按下方步骤实测。

## 文件
| 文件 | 作用 |
|------|------|
| ota_pack.py | bin/Intel-HEX → 统一 OTA 包（OTA1 头 + CRC32 + SHA256 + HMAC） |
| ota_send.py | 串口发送器：帧协议 + 断点续传 + 探测查询 + 进度显示 |
| ota_selftest.py | 设备端状态机模拟器（复刻固件 handle_frame），无硬件自测 |

## 1. 打包
    python ota_pack.py ..\hc32f4a0_app\projects\MDK\output\rfidapp.hex --version 0x01020000 --platform 2 -o fw.otapkg
    # 或 .bin：python ota_pack.py fw.bin --version 0x01020000 --platform 2 -o fw.otapkg

## 2. 无硬件自测（协议一致性）
    python ota_selftest.py fw.otapkg
    # 验证：全新下载 / 中断续传(10KB) / 半扇区续传(5000) / ACK丢失探测恢复，载荷 CRC32 全比对

## 3. 真机实测
前置：
- 设备刷入含 ota_transport_uart 的固件（UART0 同时监听 0xff 旧命令与 OTA1 帧，互不冲突）
- 设备 UART0（115200）经 USB 转串口接电脑，确认 COM 号（设备管理器）

发送（全新升级）：
    python ota_send.py fw.otapkg --port COM3 --baud 115200

断点续传实测（核心）：
    1) 发送约 50% 时按 Ctrl+C 中断（或直接拔串口线）
    2) 重新插线后原命令重跑：python ota_send.py fw.otapkg --port COM3
    3) 设备已有进度（iap_progress KV 掉电保留）→ 首帧回 0x52(偏移)
       → 工具自动从偏移续传，进度从 ~50% 继续
    4) 若完全重来：--offset 0 强制从头（会触发设备按全新下载处理）

预期结果：
- 进度到 100% → 设备读包头取版本/载荷 CRC32 → ota_storage_verify_payload 比对
- 通过 → ota_mark_ready → system_reset → 从新固件启动（Bank A 当前运行，commit 写 Bank B + SwapCmd，Phase 4 迁 bootloader）
- 失败 → 进度清零 + 设备记录 iap_result=1，可重新下发

## 协议速览（与 ota_transport_uart.c 一致）
    帧：[0:4]"OTA1" [4]type [5:7]seq(16LE) [7:9]len(16LE) [9:9+N]payload [末2]CRC16-CCITT-FALSE(LE)
    type: 0x50=DATA  0x51=ACK(设备,4B LE 已收偏移)  0x52=RESUME(设备,4B LE 续传偏移)
    CRC16-CCITT-FALSE: poly 0x1021, init 0xFFFF（逐位 MSB-first）
    len=0 的 DATA 帧 = 探测帧（上位机超时后查询设备进度，设备仅回 ACK(progress)）
    数据分片 ≤512B/帧；ACK/RESUME 载荷 = 统一 OTA 包内绝对偏移
