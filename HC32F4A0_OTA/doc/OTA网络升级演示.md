# OTA 网络升级端到端演示

> 覆盖：统一 OTA 体系全链路（服务器 → 上位机 → 设备）。所有命令在本机（Windows + Python3.12）验证可跑。

## 0. 拓扑

```
[打包] ota_pack.py ──> [版本库] server_data/ ──> [服务器] ota_server.py:8080
                                              │
[设备 F4A0] ◄── HTTP+Range ── [上位机] ota_upgrade_tool.py download
     │  ◄── UART0/USB1 OTA1 帧 ── ota_upgrade_tool.py uart / ota_send.py
     └── 验签 → commit → swap → 新固件自检 → confirm
```

## 1. 打包固件（构建产物 → OTA 包）

```bat
cd J:\dsh\HC32F4A020260320_RYK\tools
:: 一键构建（编译 0E/0W + 体积监控 + 打包 + 版本库 + 合并烧录hex + 验收 9/9）
python build_ota.py

:: 或单独打包
python ota_pack.py ..\hc32f4a0_app\projects\MDK\output\firmware_p4.hex ^
    --version 0x01020000 --platform 2 --app 0 -o server_data\firmware\fw_01020000.otapkg
```

## 2. 启动服务器 + 版本库

```bat
:: 版本库管理（add 自动校验 OTA 包 + 更新 latest）
python ota_upgrade_tool.py add server_data\firmware\fw_01020000.otapkg ^
    --platform 2 --version 0x01020000 --app 0 --dir server_data
python ota_upgrade_tool.py list --dir server_data

:: 启动服务器（0.0.0.0:8080，支持 Range 断点续传）
python ota_server.py --port 8080 --dir server_data
```

## 3. 上位机网络升级（HTTP+Range 断点续传）

```bat
:: 查询是否有新版（当前 0x01010000）
python ota_upgrade_tool.py check --server http://127.0.0.1:8080 ^
    --platform 2 --app 0 --version 0x01010000

:: 下载并校验（CRC32/SHA256/HMAC 三重；本地已有部分自动 Range 续传）
python ota_upgrade_tool.py download --server http://127.0.0.1:8080 ^
    --platform 2 --app 0 --version 0 --out fw_new.otapkg
```

## 4. 设备端触发升级（F4A0）

### 4.1 网络通道（HTTP 命令）
设备作为 HTTP Server，远程命令下发（http_callback.c json_remote_cmd）：
```json
{
  "cmd": "ota_update",
  "command_data": "http://<服务器IP>:8080/ota/pkg?id=fw_01020000.otapkg"
}
```
设备收到后 `ota_agent_run(url)`：HTTP GET + Range 续传 → QSPI 暂存 → 验签(CRC32+SHA256+HMAC)
→ 标志 NEED_COMMIT → 复位 → bootloader commit(Bank B) → swap → 新固件自检 → confirm。

### 4.2 本地通道（UART0 / USB1-CDC）
```bat
:: UART0
python ota_upgrade_tool.py uart --port COM3 --baud 115200 --pkg fw_new.otapkg
:: USB1(CDC)（同 OTA1 帧协议）
python ota_upgrade_tool.py uart --port COM9 --baud 115200 --pkg fw_new.otapkg
:: 中断后续传：Ctrl+C → 重跑同命令（设备 iap_progress 保留）
```

## 5. 一键验收（软件层回归）

```bat
python ota_acceptance.py        :: 9/9 PASS
```

## 6. 真机联调清单（测试矩阵 H1-H9）

| 步骤 | 操作 | 通过标准 |
|------|------|---------|
| 首烧 | merge_hex.py 合并 → 烧 merged_f4a0.hex | 上电跳 App，RFID 正常 |
| 网络升级 | 4.1 命令 → 设备升级 | 下载→commit→swap→新固件→confirm |
| 本地升级 | 4.2 串口 | OTA1 帧全程 ACK → 重启 |
| 断点 | 50% 中断 → 重跑 | 从偏移续传 |
| 断电/回滚 | 各阶段断电 / 坏包 | 恢复旧版，不砖 |

## 7. 常见问题

- 服务器 8080 被占用：--port 换端口，设备命令 URL 同步
- 设备连不上服务器：确认设备网络（以太网/WiFi/4G）与服务器可达；MCU 用 HTTP+包签名（省 TLS）
- 验签失败：确认打包 key 与设备内置 OTA_SEC_KEY 一致（demo key，正式化按客户密钥替换）
