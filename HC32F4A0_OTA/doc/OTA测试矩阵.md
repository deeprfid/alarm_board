# OTA 测试矩阵（验收标准落地）

> 对应 doc/OTA开发提纲与计划.md 第八节验收标准；软件项由 ota_acceptance.py 一键验证（9/9 PASS），硬件项待真机。

## 一、一键验收（软件层）

| 编号 | 验收项 | 覆盖内容 | 结果 |
|------|--------|---------|------|
| A | 打包 + 版本库 | OTA 包生成（OTA1 头+CRC32+SHA256+HMAC）、add/list | PASS |
| B | 服务器三接口 | check 新旧版本 / pkg 完整+Range / report / 目录穿越 | PASS |
| C | 上位机工具 | check 有新版 / download+三重校验 / 断点续传（半文件→Range→字节一致） | PASS |
| D | 设备端模拟 | 全新下载 / 中断续传(10KB) / 半扇区续传(5000) / ACK 丢失探测恢复 | PASS |
| E | USB 通道 | feed 状态机（整帧/逐字节/随机分块/半帧恢复/连续/非法长度） | PASS |
| F | RK Agent | 包校验 / rename 原子替换 / 回滚 / 坏包拒绝 | PASS |

## 二、硬件项（待真机，标注步骤与通过标准）

| 编号 | 验收项 | 步骤 | 通过标准 | 状态 |
|------|--------|------|---------|------|
| H1 | F4A0 首烧引导 | 用 merge_hex.py 合并 boot.hex+app hex → 烧 merged_f4a0.hex → 上电 | 正常跳 App，RFID 功能正常 | 待硬件 |
| H2 | 网络升级全链路 | ota_server + ota_upgrade_tool download → 设备 HTTP 升级 | 下载→验签→commit→swap→新固件启动→confirm | 待硬件 |
| H3 | 本地 UART 升级 | ota_send/upgrade_tool uart 连 UART0 | OTA1 帧全程 ACK，100% 后重启 | 待硬件 |
| H4 | 本地 USB1 升级 | 同上连 USB1(CDC) | 同 H3 | 待硬件 |
| H5 | 断点续传实测 | 升级 50% 中断 → 重跑 | 从偏移续传，不重复不丢数据 | 待硬件 |
| H6 | 断电测试 | 下载/commit/自检各阶段断电 | 恢复或保持旧版，不砖 | 待硬件 |
| H7 | 坏固件回滚 | 注入坏包 → 升级 | boot count 3 次内自动回滚旧版 | 待硬件 |
| H8 | 安全拒收 | 篡改包 / 无签名包 | LEVEL2/3 拒收 | 待硬件 |
| H9 | RK 板验证 | rk_ota_agent.py 在 RK3506G/RK3566 运行 | 应用原子替换 + 服务重启 + 回滚 | 待硬件 |

## 三、回归说明
- 每次固件/工具改动后：先跑 ota_acceptance.py（软件层回归），再决定是否动硬件
- 固件体积监控：F4A0 超 0.9MB 预警（当前 0.36MB，余量充足）
- 烧录：python tools/merge_hex.py boot.hex app.hex -o merged_f4a0.hex（字节级一致性已自测 PASS）
