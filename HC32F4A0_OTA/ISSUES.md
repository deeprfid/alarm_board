# 问题 / Bug 检查清单

> 生成日期：2026-08-14（AI 辅助核查 + 多轮修复，供人工复核）
> 关联文档：CHANGELOG.md（修改记录/芯片时钟核查/编译基线）；README.md（系统框架）；
>          doc/DS_HC32F4A0系列数据手册_Rev1.50.pdf（官方数据手册）
> 编译基线：app 0 Error/0 Warning；driver 0 Error/0 Warning（UV4 命令行验证，2026-08-14 最终全量重建）
> 固件：hc32f4a0_app/projects/MDK/output/firmware.axf + rfidapp.hex
> 当前尺寸：Code=355296 RO-data=26116 RW-data=13288 ZI-data=52976
> 注：本文件曾两次因误操作被截断（附录/追加时误用 limit 读取结果），已按会话记录完整重建（2026-08-14）

---

## 〇、P0 紧急：OTA 双 bank 升级丢配置（2026-08-21 定案；已修复 v9.81r/s，待真机回归）

### 问题
- **9 项 RFID 配置存片内 Flash 0xFC000-0xFDFFF**（NetConfig/WifiConfig/HwType/Monet/Bluetooth/ActiveMode/WorkMode/Passive/Boot）
- HC32F4A0 dual-bank swap 是整 bank 地址重映射：swap 后 CPU 访问 0x0000 段命中物理 Bank B
- boot commit 只写固件（0x110000-0x171D20），**不复制配置区**；Bank B 配置区=0xFF
- 无任何配置迁移/备份逻辑 → **每次 OTA 升级后 9 项配置全部丢失**，系统回默认（IP 变/激活模式变 → 系统可能不工作）

### 证据链
1. driverconfig.h: RdrConfigPage_Addr=0xFC000（9 个配置块，8KB 区）
2. merged bin 到 0x71D20，配置区在固件之后（0xFC000）
3. ota_boot.c commit_to_other_bank 只搬 fw_len 到 BOOT_OTHER_APP_BASE
4. boot_cfg.h: swap 后 0x0000 段映射另一物理 bank
5. readercfg.c 启动读 0xFC000 → swap 后命中 Bank B 空区 → 0xFF → 无有效配置

### 解决方案（两案）
- **方案 A（推荐）**：配置迁到 FlashDB KV（QSPI fdb_kvdb1，2MB）——QSPI 不参与片内 swap，OTA 天然保留配置
- **方案 B**：boot commit 时把 Bank A 配置区复制到 Bank B（boot 复杂度↑）
- 详见 doc/OTA配置迁移方案.md

### 状态
- 已定案（代码证据充分）；**已修复 2026-08-21（v9.81r 方案 A + v9.81s 彻底 KV 化）**：
  9 项配置迁 QSPI FlashDB KV（fdb_kvdb1），QSPI 不参与片内 swap → OTA 后配置天然保留；
  配置区地址（0xFA000-0xFDFFF）由 hwport.c 拦截路由 KV，读取方零改动；rdr_cfg_migrate() 一次性迁移旧片内配置
- 编译通过，**待真机回归**：配置保存 + OTA 升级后配置保留（见 CHANGELOG 2026-08-21 v9.81r/s）

---

## 一、潜在风险（已修复项标注）

### R1. 堆区域覆盖 SRAM4/SRAMH，时钟门控无显式使能【已修复 2026-08-14】
- 位置：hc32f4a0_driver/projects/user/src/main.c（_init_alloc）+ projects/source/board.c（BSP_CLK_Init）
- 现象：堆 = 0x20000000~0x20060000（384KB）覆盖 SRAM1-4；SRAM 分区时钟门控（FCG0）未显式使能
- 【已实施】board.c BSP_CLK_Init 在 SRAM_Init() 前增加 FCG_Fcg0PeriphClockCmd(SRAMH|SRAM1-4|SRAMB, ENABLE)；
  driver 0E/0W、app 0E/0W，Code +8B。待真机确认堆访问 SRAM4 区域（V3）

### R2. SRAM4/SRAMB ECC 未配置【部分解决 2026-08-14】
- 现象：SRAM4/SRAMB 带 ECC（默认禁用 INVD 当普通 RAM 用）；tagtmpbuf(4KB) 原占用备份 SRAM
- 【已实施】确认 tagtmpbuf 为死缓冲（仅 memset 无读写），已删除 ipc.c 定义+memset、HttpModuleAPI.cpp extern、sct RW_IRAMB；
  ZI -4096B，备份 SRAM 4KB 释放。ECC 保持默认禁用（无风险）

### R3. 堆起点硬编码 0x20000000，浪费 SRAMH 高速区【已修复 2026-08-14】
- 【已实施】main.c heap_base = &Image$$RW_IRAM1$$ZI$$Limit（0x1FFF82D8→对齐 0x1FFF8300），堆 384→415KB（+31KB SRAMH）；
  driver/app 0E/0W。待真机确认 malloc 至 SRAMH 区读写（V2）

### R4. 主栈 64KB 偏大【已修复 2026-08-14】
- 【已实施】app startup Stack_Size 0x10000→0x8000（32KB）；ZI -32KB，堆 →457KB；
  待真机验证中断深度（V4）；如需更激进可再缩至 16KB

### R5. MQTTClient.c(146) 条件中赋值【已修复 2026-08-14】
- 【已实施】拆分条件（rc = 读; if (rc != rem_len)），Paho 语义保留；0W

---

## 一·补、协议核查与修复

### F1. flashdb() 调用官方示例污染生产 KVDB【已修复 2026-08-14】
- flashdb.c:79 kvdb_basic_sample(&AlarmDB) 每次启动写示例键 boot_count/boot_time(local_Rtc+=100)；
  已移除调用+extern，链接器移除 kvdb_basic_sample.o(-692B)，Code -1364B；如需重启计数请单独实现

### F2. FlashDB 分区与业务匹配【2026-08-14 评估，无需调整】
- 分区：fdb_kvdb1(0-2M) + fdb_tsdb2(2-4M) + fdb_tsdb1(4-16M)，16MB QSPI Flash；白名单容量余量约 20 倍
- 注意：kvdb_type_blob_sample.c/tsdb_sample.c 内含业务函数（save_tag_flashDB/save_tag_TSDB/Match_EPC_inTSDB 等），文件不可删

### F3. 流式白名单不落盘（重启丢失）【已修复 2026-08-14】
- httpSaveTagMethod 流式分支漏置 FlashDB_Sync_flag；FlashDB_Task 仅 flag>10 时触发落盘 → 流式白名单重启丢失
- 已补 FlashDB_Sync_flag=11；app 0E/0W；待真机验证（V5）

### F4/F4b. httpSaveTagMethod 防御【已修复 2026-08-14】
- method 缺失→strcmp(NULL) 崩溃：已加 name==NULL 检查（method missing）
- method 非 add/del→plist=NULL 崩溃：已加 else 分支（method not supported）
- EPC 数组元素 NULL/长度非法：已跳过（F4b）；app 0E/0W

### M1. Modbus 协议核查【2026-08-14，实现健全无 bug】
- 寄存器表 20 段（0x0000 盘点/0x0020 功率/0x0040 过滤/0x0060 附加/0x0070 配置/0x0080 GPI/0x0082 GPO/
  0x0084 天线/0x0086 重启/0x0088 状态/0x018E 保存/0x0190 API/0x0200 标签字节/0x0300-0x0400 标签内存/0x0600-0x0680 温度/0x0B00 短盘点），地址无重叠
- 分发健全：NULL handler→0x01、未知寄存器→0x02、CRC→0x04、写前校验、RTU+TCP、mberr==10 重启

### M2. MQTT 核查【2026-08-14，实现健全】
- 连接/重连（失败计数+设备复位兜底）、条件订阅（去重）、Yield 保活、Publish 上报、TLS 完整
- 小观察：Publish 前 SBuffer[dlen]=0，dlen==缓冲大小时越界 1 字节（低风险）
- 【已修复 2026-08-23 v9.81aj】mqtt_interface.c：dlen>0 && dlen<TagSendBufLen 才写终止符；app 编译 0E/0W

### C1. 自定义命令长度字段无上限校验【已修复 2026-08-23 v9.81aj】
- Lan2Uart.c：IOS recvbuf[5]*2、SIO recvbuf[3]*2、透传 recvbuf[1]+5 无上限校验，异常输入可越界 recvbuf[255]
- 【已实施】IOS/SIO 长度字段 >120 拒绝（GPO 条数，6+2×120=246≤255）；透传 >250 拒绝（总长 ≤255，与 OTA 帧 >248 口径一致）；
  app 编译 0E/0W（Code=373500）。待真机回归 GPO 设置/透传

### R7. 上报协议核查【2026-08-14，实现健全，1 个低风险观察】
- 帧格式（AddMsgHeader2SockBuffer）：0xff + namelen + 2B预留 + mtype + 0x00 + 4B错误码(httpAPIErrCodeBase=100000 偏移) + 设备名
- send_evt_*（heartbeat/gpichan/tagcoming/tagbatch/reader_err/emptydata/synctimereq）覆盖完整；
  HTTP/MQTT 走 JSON、TCP/UART 走二进制帧；CRC（crc_Msg）与客户端 ACK（client_ack）支持
- 低风险观察：AddMsgHeader2SockBuffer 对设备名长度无边界校验（正常配置 <20B，配置损坏时可能溢出发送缓冲）；建议加校验
- 【已修复 2026-08-23 v9.81aj】新增 rdr_name_len()：有界读 name[129]（防未终止配置 strlen 越界）+ 钳位 ≤0xFE（1 字节长度字段）；
  AddMsgHeader2SockBuffer / SetMsgDatalen 统一使用；app 编译 0E/0W

---

## 二、死代码 / 未接线模块

### D1. driver 工程【已归档 2026-08-14 → _archive_deadcode/，见 MANIFEST.txt】
| 文件 | 大小 | 说明 |
|------|------|------|
| user/src/sysinit.c | 5.3KB | 168MHz 时钟死代码（sysinit() 零调用；F460 DDL 风格） |
| user/src/hwport.c | 3.1KB | GPIO_Configuration 已注释；实际在 common.c:152 |
| user/src/adc.c / flash.c / flash_op.c / hpm6340.c / qspi_flash.c / Retarget.c | — | 未接入或与工程内文件重复 |
| source/usart.c | 6.3KB | 与 user/usart_driver.c（在工程）重复 |
| projects/jsonlib/* | ~460KB | **非死代码（修正）**：.h 为 readercfg.c 编译依赖（IncludePath 含 jsonlib）；.c 未编译，实现由 app jsonlib 提供 → 保留 |
| MDK/startup_hc32f4a0.s | 21.8KB | 与工程所用 cmsis startup 版本不同 |
| MDK/RTE/Device/HC32F4A0PITB/*、RTE/MicroBoot/* | — | RTE 生成物未用（含 include path 风险，未动） |

### D2. app 工程【已归档 2026-08-14 → _archive_deadcode/】
| 文件 | 大小 | 说明 |
|------|------|------|
| app/src/EASConfigHandler.cpp + inc/EASConfigHandler.h | 23.7KB | 全工程零引用；EAS 配置由 HttpModuleAPI::httpEascfg() 实现 |
| app/src/hpm6340.c / w25qxx.c | 30KB | 未接入 |
| projects/backup/*（6） | ~90KB | 已知死代码（2026-06-29 归档） |
| projects/httpServer/*（5） | ~40KB | 已知死代码 |

### D3. SDK 保留目录【2026-08-14 评估：不建议直接归档】
- 体积约 4.6MB（driver usb_lib 777KB / app usb_lib 779KB / midwares 1115KB×2 / bsp components 262KB / bsp ev 247KB / flashDB demos 308KB）
- driver usb_lib 部分在工程、midwares 可能未来用 → 保持现状；若瘦身优先 app usb_lib 与 flashDB demos/tests
- 移动前须逐个核实 uvprojx 引用（jsonlib 教训）

---

## 三、待确认 / 待硬件验证

- T1. 240MHz 主频真机验证（SystemCoreClock=240000000）
- T2. 堆在 SRAMH/SRAM4 区域读写（R3/R1 后）
- T3. 编译 Warning 清零【已完成 2026-08-14，5→0】
- T4. FPU 精度确认（uvprojx FPU2 vs M4 单精度，map 含软浮点库）
- T5. board.c（LQFP176 EV BSP）与量产 LQFP100 的 GPIO 兼容性核对
- T6. EASConfigHandler 接线决策（已归档，接入或删除）
- T7. driver_lib 操作注意（After Build 自动同步，勿手工改）

---

- T8. 【F460 Phase 5 待确认】App 最终链接地址 / 片内分区（bootloader-App-标志区）/ 升级执行者（bootloader 覆盖写 vs App IAP）/ btparams 复用或新标志页
- T9. 【F460 bootloader 依赖】RTX_CM4F.lib、json 库、堆描述符等缺失库路径（bootloader 工程编译需补齐）
- T10. 【真机】按 doc/OTA测试矩阵.md H1-H9 执行（F4A0 首烧/网络/UART/USB1/断点/断电/回滚/安全/RK）
## 四、编译基线速查（回归用）

    D:\Keil_v5\UV4\UV4.exe -r "<app.uvprojx>" -o 日志   # app 全量重建 → 0E/0W
    D:\Keil_v5\UV4\UV4.exe -b "<driver.uvprojx>" -o 日志  # driver → 0E/0W（自动同步 driver_lib）
    当前尺寸：Code=355296 RO-data=26116 RW-data=13288 ZI-data=52976
    编译日志存档：doc/_build_*.log

## 五、SRAM 布局（hc32_ll_sram.h 官方注释）

| 分区 | 地址 | 大小 | 特性 | 当前用途 |
|------|------|------|------|----------|
| SRAMH | 0x1FFE0000~0x1FFFFFFF | 128KB | 高速 0 等待 | 静态区+32KB 主栈（R4 后）+ 堆前段 |
| SRAM1 | 0x20000000~0x2001FFFF | 128KB | 偶校验 | 堆 |
| SRAM2 | 0x20020000~0x2003FFFF | 128KB | 偶校验 | 堆 |
| SRAM3 | 0x20040000~0x20057FFF | 96KB | 偶校验 | 堆 |
| SRAM4 | 0x20058000~0x2005FFFF | 32KB | ECC（默认禁用） | 堆 |
| SRAMB | 0x200F0000~0x200F0FFF | 4KB | ECC+VBAT 保持 | 空闲（R2 释放） |

堆：0x1FFF0300~0x20060000 ≈ 457KB（R3+R4 优化，原 384KB）

---

## 六、真机验证清单（硬件实测用）

| # | 验证项 | 方法 | 预期 | 对应条目 |
|---|--------|------|------|---------|
| V1 | 主频 240MHz | 读 SystemCoreClock 或 CLK_GetBusClockFreq(HCLK) | 240000000 | T1 |
| V2 | 堆在 SRAMH 高速区读写 | malloc 大块写入读回校验 | 0x1FFF8300 起正常 | R3/T2 |
| V3 | 堆触及 SRAM4（0x20058000+） | malloc 至地址 ≥0x20058000 读写 | 正常 | R1/R2/T2 |
| V4 | 主栈 32KB 中断深度 | 高负载（盘点+网络+USB）长时间运行 | 无 HardFault | R4 |
| V5 | 流式白名单持久化 | 流式推送 5 万条 add 后重启 | 白名单保留 | F3 |
| V6 | HTTP 异常输入 | 发缺 method/坏 epc JSON | 错误码不崩溃 | F4/F4b |
| V7 | 备份 SRAM 空闲 | 确认 0x200F0000 无访问 | 无引用 | R2 |
| V8 | FPU 双精度确认 | 编译选项 vs 硬件浮点实测 | 无精度意外 | T4 |
| V9 | LQFP100 引脚覆盖 | 核对 board.c/自定义 GPIO 引脚在 LQFP100 均存在 | 全部存在 | T5 |
| V10 | 240MHz 外设时序 | UART 波特率/SPI 实测 | 无错码 | — |
| V11 | Flash 写损耗 | 记录 FlashDB 写入频次 | F1 后显著下降 | F1 |

---

| H1 | OTA 首烧引导 | merge_hex 合并 boot+app → 烧 merged_f4a0.hex → 上电 | 跳 App，RFID 正常 | OTA P4 |
| H2 | OTA 网络升级 | ota_update 命令（doc/OTA网络升级演示.md） | 下载→commit→swap→新固件→confirm | OTA |
| H3 | OTA UART 升级 | ota_upgrade_tool uart COM3 | OTA1 帧全 ACK→重启 | OTA |
| H4 | OTA USB1 升级 | uart COM9（CDC） | 同 H3 | OTA |
| H5 | OTA 断点续传 | 50% 中断→重跑 | 从偏移续传 | OTA |
| H6 | OTA 断电测试 | 下载/commit/自检各阶段断电 | 恢复或保持旧版 | OTA |
| H7 | OTA 坏固件回滚 | 注入坏包 | boot count 3 次内回滚 | OTA |
| H8 | OTA 安全拒收 | 篡改/无签名包 | LEVEL2/3 拒收 | OTA |
| H9 | OTA RK 板验证 | rk_ota_agent.py on RK | 原子替换+重启+回滚 | OTA P2 |
## 七、当前代码状态（2026-08-14 最终验证）

- 基线编译（最终全量重建）：driver 0E/0W、app 0E/0W（UV4 命令行可复现）
- 固件尺寸：Code=355296 RO-data=26116 RW-data=13288 ZI-data=52976
- 已修复/完成：R1-R5、T3、D1/D2、F1、F3、F4/F4b、M1、M2 + README + 编译基线 + 架构核查
- 
### OTA 体系（2026-08-15，Phase 0-4 + 工具链全部落地）
- 软件侧全部完成并有自动回归：服务端三接口 / 签名+安全等级 / USB1 通道 / 上位机(含批量) / RK Agent / 一键构建验收 / 体积监控 / 密钥管理
- 编译基线：App 0E/0W（Code=361404）、Bootloader 0E/0W（Code=5596）；验收 ota_acceptance 9/9
- 待硬件：H1-H9（OTA测试矩阵.md）；待确认：T8（F460 部署模型）、T9（bootloader 缺失库）

### OTA 三通道真机闭环（2026-08-21，v9.81o 64KB 块擦除提速）
- 串口/COM3、USB/COM8、HTTP/设备:8081 三通道**各自独立**，互斥共用暂存区（ota_channel_* mutex），
  同一 verify(CRC32)+mark_ready+reset 闭环；v9.81o 三通道回归全通过（用户实测）
- **v9.81o 提速实测**：HTTP t_erase 3646ms→1169ms（-68%，64KB 块擦 0xD8 替代 4KB 扇区擦），
  total 8296ms→5749ms（-31%）；USB 4.27s=91KB/s（v9.81g 的 75KB/s → +30%）
- **踩坑记录**：OTA 包版本必须与设备固件版本一致（0x01150000）；v9.81o 首次打包误用 0x01020000
  → 串口通道版本检查拒绝（pkg ver != fw ver）；HTTP 通道不检查版本故能通过。已统一修复
- **当前瓶颈**（v9.81o 之后）：HTTP TCP 网络接收 ~3.9s（400KB ≈ 100KB/s）占 total 5749ms 的 68%；
  后续优化方向：加大设备 HTTP read 块 / W5100S socket buffer / 发送器并发
- **待办**：H2 网络 OTA 回归（下载方向，非 PC→设备 POST）；KV 写 off-frame 路径检查；多轮稳定性测试；
  HTTP 网络接收提速；推送 CHANGELOG/ISSUES 已记录、git push 待用户执行
- **2026-08-23 v9.81aj 加固落地**：C1（Lan2Uart 长度校验）、R7（设备名有界长度）、M2（MQTT 终止符越界）全部修复，app 编译 0E/0W（Code=373500）；详见 CHANGELOG 2026-08-23
- 记录未改：D3(SDK目录)、T4-T7、V1-V11 真机验证；git push 待用户执行
