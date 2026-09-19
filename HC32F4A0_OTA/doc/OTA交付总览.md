# OTA 交付总览（2026-08-15，软件侧全闭环）

> 统一 OTA 升级系统（HC32F4A0 优先）Phase 0-4 + 工具链全部落地；F460/RK 方案就绪待部署确认与硬件验证。

## 一、固件交付

| 产物 | 位置 | 验证 |
|------|------|------|
| F4A0 Bootloader | hc32f4a0_boot/（uvprojx + boot/src） | 0E/0W，hex @0x0，6KB；dual-bank commit + boot count 回滚 |
| F4A0 App | hc32f4a0_app/（偏移 0x10000） | 0E/0W，hex @0x10000-0x6EF87；三通道 + 验签 + 标志页 |
| 合并烧录文件 | tools/merged_f4a0.hex | 字节级一致（boot+app 单文件） |
| F460 Bootloader | Decoder_20260326/bootloader/trunk（LL 精简，全新） | 0E/4W，hex @0x0-0x1980；引导+QSPI 覆盖写+备份恢复+boot count 回滚 |
| F460 App | Decoder_20260326/hc32f46_app/trunk（偏移 0x16000） | 0E/7W，hex @0x16000-0x61107；6 ota 模块 + 本地升级链路 |
| 合并烧录文件 | tools/merged_f460.hex | boot 0x0 + app 0x16000（314000B） |

## 二、统一 OTA 协议

- **包格式**：OTA1 头 82B（magic/ver/platform/app/len/CRC32/SHA256/HMAC([0:50]+payload)）+ payload
- **三接口**：POST /ota/check · GET /ota/pkg(+Range 断点续传) · POST /ota/report
- **本地帧**：OTA1 帧（0x50 DATA / 0x51 ACK / 0x52 RESUME，CRC16-CCITT，len=0 探测）
- **安全等级**：OTA_SECURITY_LEVEL 1/2/3（LEVEL2/3 强制验签）

## 三、工具链（tools/，全部自测通过）

| 工具 | 作用 | 验证 |
|------|------|------|
| build_ota.py | 一键构建+发布+验收（7 步） | 全流程通过 |
| ota_acceptance.py | 软件层验收（9 项） | 9/9 PASS |
| ota_server.py | 服务器（check/pkg+Range/report） | 三接口自测通过 |
| ota_upgrade_tool.py | 上位机（版本库/check/download/uart/devices 批量） | 自测通过 |
| ota_pack.py | 打包（--key/--key-file） | HMAC 正确性验证 |
| ota_keygen.py | 密钥管理（生成/显示/C 头） | 换 key 验签生效 |
| ota_selftest.py | 设备端状态机模拟 | 4 场景 PASS |
| single_bak_sim.py | F460 备份-覆盖-恢复算法 | 5 场景 PASS |
| rk_ota_agent.py | RK 应用原子升级 | 4 项 PASS |
| merge_hex.py | 烧录 hex 合并 | 字节级一致 |
| fw_size_monitor.py | 体积监控 | 3 场景 PASS |

## 四、平台无关核心（F460/RK 复用）

- **ota_frame**（帧协议：CRC16/组帧/解析/流式）——零硬件依赖
- **ota_security**（验签/SHA256，读回调注入）——F460 传 w25qxx 读即可

## 五、文档

| 文档 | 内容 |
|------|------|
| doc/OTA开发提纲与计划.md | 计划 V0.2 + 附录（全部落地记录） |
| doc/OTA测试矩阵.md | 软件 9 项 + 硬件 H1-H9（步骤+通过标准） |
| doc/OTA网络升级演示.md | 端到端操作手册（命令已验证） |
| doc/F460_FlashDB移植指南.md | FlashDB/FAL 移植清单+适配模板 |
| README / CHANGELOG / ISSUES | 总览 / 18 条记录 / 待办+真机清单 |

## 六、待办（非软件阻塞）

1. **F460 已完成**：App 0x16000 + LL 精简 bootloader + 6 ota 模块 + 本地升级链路（0E/7W，merged_f460.hex 备好）
2. **真机联调** H1-H12（需硬件，DAP-LINK 已具备）：F4A0（merged_f4a0.hex）→ 网络/串口升级 → 断点/断电/回滚/安全；F460（merged_f460.hex）→ 本地升级/断电/回滚；RK 板验证
3. **生产化**：正式密钥分发（ota_keygen）、服务器部署（HTTPS/域名）、灰度（多版本 manifest）

## 七、验证基线

- App 0E/0W（Code=361404，379.9KB/896KB）· Bootloader 0E/0W（Code=5596）
- ota_acceptance 9/9 PASS（打包/版本库/服务器/上位机/设备模拟/USB feed/RK）
- git：本地 main 含 18 个提交（远端 296eed4 起 17 个待推送：git push origin main）
