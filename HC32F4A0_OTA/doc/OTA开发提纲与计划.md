# 统一 OTA 升级系统 · 开发提纲与实施计划

> 版本：V0.2（定稿草案）   日期：2026-08-14
> 依据：芯片资源核实、QSPI 驱动调通（F460/F4A0）、开源调研（FlashDB/EasyFlash/MicroBoot/MCUboot/SWUpdate/Mender/RAUC）、存储约束确认
> 适用：HC32F460 / HC32F4A0（MCU）、RK3506G / RK3566（MPU）、上位机（PC/本地工具）
> 原则：应用级升级、同一套升级协议、传输/存储可插拔、安全可裁剪、防变砖优先

---

## 一、目标与范围

### 1.1 目标
- 4 个平台（2 MCU + 2 MPU）统一升级体系：**同一协议、同一包格式、同一管理平台**
- **RK 只升级应用程序，不升级 Linux 系统**
- 支持网络升级（以太网/WiFi/4G）与**本地升级（UART/USB，同一协议）**
- 上位机可充当：传输代理（有云） / 本地 OTA 服务器（无云） / UART-USB 升级工具
- 彻底摆脱对 ModuleAPI.dll 与旧 0xff 帧升级协议的依赖
- 防变砖：任何时刻保留"第二份可启动固件" + boot count 三重兜底

### 1.2 范围
| 包含 | 不包含（明确排除） |
|------|---------------------|
| 统一协议与包格式定义 | RK 系统级 OTA（recovery/A-B 分区/差分包） |
| MCU OTA Agent（应用态） | ModuleAPI.dll 直连升级（仅产线/调试保留） |
| MCU Bootloader 升级逻辑重写 | 现有 bootloader 的引导跳转框架重写（保留复用） |
| RK 应用升级 Agent | Linux 内核/rootfs 升级 |
| 上位机（代理/本地服务器/串口工具） | 服务器管理平台完整实现（先最小化） |
| 服务器 OTA 接口（check/download/report） | 复杂灰度/统计报表（可后补） |

### 1.3 关键技术决策（已确认）
### 1.4 开源选型结论（2026-08-14 调研定稿）

**原则：分层借鉴 + 自研统一核心。** 不整体套用任何开源项目（HC32 无现成 port，且无跨 MCU+MPU 统一方案）：

| 模块 | 选型 | 来源 |
|------|------|------|
| 存储层 | **FlashDB + FAL + QSPI 驱动**（自研适配） | F4A0 已有；F460 同源移植（w25qxx 已调通） |
| 升级状态机/防变砖 | **借鉴 EasyFlash IAP 设计**（备份区+标志+掉电恢复），用 FlashDB KV 实现 | armink/EasyFlash |
| Bootloader 引导/魔术字 | **借鉴 MicroBoot**（NOINIT RAM 标志 + Flash 魔术字 + 固定地址 Flash 服务表） | Aladdin-Wang/MicroBootRom |
| 镜像头/签名/确认语义 | **对齐 MCUboot 设计**（镜像头 + RSA/ECDSA + confirm/回滚） | mcu-tools/mcuboot |
| 下载 Agent（网络） | **自研**（HTTP+Range 断点续传） | — |
| 下载 Agent（本地） | YModem 移植（EasyFlash demo）/ 自定义帧 | armink/EasyFlash demo |
| 服务器 | **先自研三接口**，规模大再评估 hawkBit | — |
| RK 应用升级 | **自研 rename 原子替换**（借鉴 Mender Update Modules 语义） | — |

### 1.5 MCU 端完整工作清单（6 大模块）

| # | 模块 | 内容 | 现有基础 |
|---|------|------|---------|
| ① | 存储层 | FlashDB(KV+TSDB) + FAL + QSPI 驱动 | F4A0 80% 已有；F460 移植 |
| ② | 升级状态机 | EasyFlash IAP 思想 → FlashDB KV（见 3.5） | 新增 |
| ③ | 下载 Agent | HTTP+Range / YModem，流式写 QSPI 暂存 | F4A0 有 http_callback 基础 |
| ④ | **Bootloader**（最大头） | 0x0 入口+标志检查+commit(QSPI→片内/双槽)+回滚+boot count 自检 | F4A0 需新建；F460 改造 |
| ⑤ | 安全层 | 包签名验签（硬件 SHA） | 新增 |
| ⑥ | 上报模块 | /ota/report（复用 MQTT/HTTP） | F4A0 通道现成 |


1. 传输层：**HTTP + Range 断点续传 + 包级签名**（MCU 用明文 HTTP + 包签名，省 TLS；RK 可 HTTPS）
2. 本地通道：UART/USB 自定义帧（长度+CRC+序号），载荷=同一 OTA 包
3. 存储后端：F4A0=dual_bank（首选）、F460=single_bak（QSPI 备份回滚）、预留 qspi_xip、RK=rename
4. 安全等级可裁剪：LEVEL1 网络+本地 / LEVEL2 仅本地 / LEVEL3 仅本地+强制签名
5. 升级状态机：check → download → verify → commit → (自检) → report；boot count 兜底回滚

---

## 二、总体架构

### 2.1 分层视图
    ┌────────────────────────────────────────────┐
    │ 管理平台：版本库 / 打包 / 灰度(可选) / 统计  │
    │  接口：POST /ota/check, GET /ota/pkg,       │
    │        POST /ota/report                     │
    └───────────────┬────────────────────────────┘
                    │ 统一协议（HTTPS / HTTP+签名）
        ┌───────────┼────────────┬──────────────┐
        ▼           ▼            ▼              ▼
    HC32F460    HC32F4A0      RK3506G         RK3566
    OTA Agent   OTA Agent     OTA Agent       OTA Agent
    ─────────    ─────────     ──────────      ──────────
    传输:eth/wifi/4g(HTTP) + uart/usb(帧)   （同左，MPU 用 HTTPS）
    存储:single_bak     dual_bank          rename（应用原子替换）
    Bootloader: 校验/commit/回滚(重写升级逻辑)

    ▸ 上位机角色（可插拔）：
      Proxy（有云，设备无网）/ 本地服务器（无云）/ UART-USB 工具（严格安全）

### 2.2 关键抽象（可插拔接口）
    ota_transport_t { check(); download(); report(); }
      实现：eth_http / wifi_http / 4g_http / uart_usb_frame

    ota_storage_t { prepare(); write_stage(); verify();
                    commit(); rollback(); get_boot_count(); set_boot_ok(); }
      实现：dual_bank(F4A0) / single_bak(F460) / qspi_xip(预留) / rk_rename

    ota_security_t { verify_signature(); verify_sha(); }
      实现：hmac_sha256（MCU 用硬件 HASH）/ openssl（RK）

---

## 三、统一协议规范（V0.2）

### 3.1 统一固件包（字节级）
    [0:4]    MAGIC "OTA1"
    [4:8]    版本号（uint32，如 0x01020000）
    [8]      平台ID（1=HC32F460, 2=HC32F4A0, 3=RK3506G, 4=RK3566）
    [9]      app_id（应用标识，0=主固件，1..N=子应用）
    [10:14]  载荷长度（uint32 LE）
    [14:18]  CRC32（载荷）
    [18:50]  SHA256（载荷）
    [50:82]  HMAC-SHA256 签名（覆盖 [0:50]+载荷；服务器私钥，设备内置公钥）
    [82:]    载荷（MCU=固件 bin；RK=应用包 tar.gz）

### 3.2 三接口（语义一致，网络/本地复用）
    POST /ota/check
      req  {device_id, platform, app_id, version}
      resp {upgrade:bool, url?, new_version?, size?, sha256?, sign?}
    GET  /ota/pkg?id=xxx      （HTTPS / HTTP+Range；本地=帧传输同语义）
    POST /ota/report
      req  {device_id, platform, app_id, version, result(0=ok,1=fail,2=rollback), error}

### 3.3 升级状态机
    IDLE → CHECK(有新版) → DOWNLOAD(→QSPI暂存/临时文件) → VERIFY(CRC+SHA+签名)
        → COMMIT(写标志/槽切换/rename) → REBOOT → SELF_CHECK(心跳/自检)
        → CONFIRM(set_boot_ok) / ROLLBACK(boot count 超限→恢复备份→重启)
    ▶ 任何阶段失败：保持旧版本运行，report 上报，可重试/续传

### 3.4 本地通道帧格式（UART/USB）

> **V0.2.1 修订（2026-08-14）**：帧魔数 0xEE → "OTA1"。
> 原因：0xEE 与现有上位机协议帧头撞车（reader_msg.c/ipc.c，TCP/UDP 0xEE 帧）；
> 0xFF 亦被旧升级/自定义命令帧占用（customcmd.c/Lan2Uart.c，ModuleAPI.dll FirmwareLoadEx）。
> 新 OTA 本地通道必须与旧协议**零冲突**，故帧头直接复用统一 OTA 包魔数 "OTA1"。

    [0:4]  MAGIC "OTA1" | [4] 帧类型(0x50=数据,0x51=ACK,0x52=续传请求) | [5:7] 序号(16LE)
    [7:9]  载荷长度(16LE) | [9:9+N] 载荷（=统一 OTA 包字节流，仍以 "OTA1" 开头） | [末2] CRC16（覆盖全帧）
    断点续传：设备回 0x52(已收偏移) → 上位机从偏移续发

### 3.5 MCU 升级状态机设计（EasyFlash IAP → FlashDB KV 映射）

**KV 键设计**（FlashDB KVDB，F4A0/F460 一致）：

| KV 键 | 值 | 说明 |
|-------|-----|------|
| iap_need_copy | 0/1 | 固件已下载完、待 commit（掉电恢复核心标志） |
| iap_copy_size | 固件字节数 | 待 commit 的固件大小 |
| iap_version | 版本号 | 目标版本 |
| iap_progress | 已下载字节 | 断点续传进度 |
| iap_sign_ok | 0/1 | 验签结果（commit 前必须 1） |
| iap_result | ok/fail/rollback | 最近一次升级结果（上报用） |

**状态机**（与 EasyFlash 流程等价）：

    IDLE
      │ ① 下载（HTTP+Range / YModem 流式写 QSPI 暂存）
      ▼
    DOWNLOADING（iap_progress 持续更新，掉电续传）
      │ ② 下载完成 + CRC + 验签（iap_sign_ok=1）
      ▼
    READY（置 iap_need_copy=1, iap_copy_size；写 RAM 魔术字 → system_reset）
      │ ③ bootloader 读标志（RAM 或 KV）
      ▼
    COMMIT（bootloader：QSPI 暂存 → 片内；F4A0 写 Bank B / F460 覆盖写）
      │ ④ 完成 → 清 iap_need_copy=0 → 跳新固件
      ▼
    SELF_CHECK（新固件启动，boot count 自检）
      ├─ 正常 → set_boot_ok，iap_result=ok，上报
      └─ N 次失败 → 回滚（F4A0 清标志回 A；F460 从 QSPI 备份恢复），iap_result=rollback

**掉电恢复**：任意阶段掉电 → 上电 bootloader 读 KV：
- need_copy=1 → 继续 commit（备份区/QSPI 暂存完整）
- 下载中（progress<size）→ 重新下载或 Range 续传
- commit 中 → 重试 commit（F460 先恢复 QSPI 备份再重写，双保险）

> 对照 EasyFlash：ef_write_data_to_bak→QSPI 暂存写；ef_erase_user_app+ef_copy_app_from_bak→bootloader commit；env(iap_need_copy)→FlashDB KV。**介质从片内备份区换到 QSPI（片内无双份空间，见 4.1.1）**。

---

## 四、分平台模块设计

### 4.1 HC32F4A0（优先实现，驱动最全）
| 模块 | 说明 | 依赖现有 |
|------|------|---------|
| OTA Agent（应用态） | check/download(HTTP+Range)/report；置升级标志+重启 | http_callback、mqtt、qspi_flash |
| QSPI 暂存 | 分区 4~8MB 固件暂存槽 + 进度记录页 | qspi_flash.c（已调通） |
| Bootloader（升级逻辑重写） | 读 QSPI 暂存→验签→写 Bank B→切标志；boot count 兜底回滚 | dual-bank Flash、EFM |
| 升级标志页 | 双标志+反码+boot count+槽位（独立于旧 btparams） | 新设计 |

> MCU 端实现按 1.5 的 6 大模块展开；本节省略重复，聚焦平台差异。

### 4.1.1 存储策略硬约束（2026-08-14 确认）

| 平台 | 片内 Flash | 固件上限 | 双份/备份区空间 | 存储后端 |
|------|-----------|---------|----------------|---------|
| HC32F460 | ~512KB | ≤512KB | ❌ 片内无任何双份空间 | **single_bak_qspi**（QSPI 暂存+备份回滚，片内仅覆盖写） |
| HC32F4A0 | 2MB dual-bank（每 bank 1MB） | ≤2MB | ✅ 固件 ≤1MB 时每 bank 1MB 可用 | **dual_bank**（默认，当前 0.4MB）；**>1MB 时切换** single_bak_qspi / qspi_xip |

关键约定：
- F460：片内 A/B 与片内备份区均不可行（512KB 装不下两份固件）→ **QSPI(8MB) 是唯一暂存/备份介质**
- F4A0：当前固件 0.4MB，dual-bank A/B 完全可行（余量 60%+）；**固件 >0.9MB 预警（编译脚本检查），>1MB 触发策略切换**
- 存储后端抽象 ota_storage_t 不变，实现按平台/体积选择：F460=single_bak_qspi，F4A0=dual_bank（默认）/single_bak_qspi（超限）
- 两者 QSPI 均为核心：F460 全部依赖（暂存+备份），F4A0 用于暂存（下载不碰运行区）

### 4.2 HC32F460（复用 F4A0，驱动差异）
| 差异点 | 处理 |
|--------|------|
| 存储后端 | single_bak：QSPI 备份旧固件 → 覆盖写 → 失败恢复 |
| QSPI 驱动进 bootloader | 把 w25qxx.c + hc32f460_qspi.c 加入 bootloader 工程（当前缺） |
| 其余 | 与 F4A0 共用协议/Agent/标志页（仅存储后端不同） |

### 4.3 RK3506G / RK3566（只升应用）
| 模块 | 说明 |
|------|------|
| OTA Agent（Linux 服务/守护） | check → 下载到 /ota/staging → 验签 → 备份旧应用 → rename 原子替换 → 重启服务 |
| 进程管理 | systemd unit 承载应用；升级后 systemctl restart |
| 安全 | HTTPS + 包签名（OpenSSL） |
| 多应用 | app_id 区分，独立版本链 |

### 4.4 上位机（PC/本地工具）
| 角色 | 职责 |
|------|------|
| 传输代理 | 有云：云→上位机→设备（串口/USB/TCP） |
| 本地 OTA 服务器 | 无云：本地版本库 + check/download/report + 批量管理 + 日志 |
| UART/USB 升级工具 | 单台直连：帧协议发送同一 OTA 包 + 断点续传 + 结果展示 |

---

## 五、开发阶段与具体步骤

### Phase 0：协议与设计定稿（0.5~1 周）
- [ ] 3.1 包格式、3.2 三接口、3.3 状态机、3.4 帧格式文档定稿
- [ ] 4.1-4.4 模块接口头文件（ota_transport/ota_storage/ota_security）设计
- [ ] 服务器接口最小实现定义（check/download/report 返回结构）
- 交付：协议文档 + 接口头文件 + 打包工具原型

### Phase 1：服务器与打包（1~2 周，可并行）
- [ ] 服务器：版本库（表结构）、check/download/report 三接口（最小实现）
- [ ] 打包工具：MCU bin / RK 应用 → 统一 OTA 包（签名密钥管理）
- [ ] 测试：打包-上传-下载回路验证
- 交付：可部署的最小 OTA 服务器 + 打包 CLI

### Phase 2：RK Agent（1 周，最简单先跑通全链路）
- [ ] ota_transport_http（HTTPS）+ ota_storage_rename + 验签
- [ ] 应用以 systemd 服务运行；升级→替换→restart→自检→report
- [ ] 断点续传（Range）与回滚（恢复旧文件）
- 交付：RK3566 Agent 跑通"服务器→设备"完整链路

### Phase 3：F4A0 Agent + QSPI 暂存 + 升级状态机（1~2 周）
- [ ] FlashDB KV 键落地（iap_need_copy/iap_size/progress/sign_ok，见 3.5）
- [ ] QSPI 分区规划（暂存槽+进度页）与读写封装（复用 qspi_flash.c）
- [ ] ota_transport_eth_http（HTTP+Range 续传，复用 http_callback 流式）
- [ ] 下载完成 → CRC + SHA256 + 验签（硬件 HASH）→ 置 iap_need_copy → 写 RAM 魔术字 → reboot
- [ ] 掉电恢复：上电读 KV → 续传/继续 commit
- [ ] QSPI 分区规划（暂存槽+进度页）与读写封装（复用 qspi_flash.c）
- [ ] ota_transport_eth_http（HTTP+Range 续传，复用 http_callback 流式）
- [ ] 升级标志页（双标志+反码+boot count）读写
- [ ] Agent 主流程：check/download/verify/set-flag/reboot/report
- 交付：F4A0 应用态升级到 QSPI 暂存完成 + 断点续传

### Phase 4：F4A0 Bootloader 新建/重写（1~2 周，防变砖核心）
- [ ] **新建 bootloader 工程**（App 现占 0x0 → 偏移到 0x10000；bootloader 放 0x0）
- [ ] 保留引导框架；重写升级协议段
- [ ] 标志检查：RAM NOINIT（软件复位触发）+ Flash KV（掉电恢复）双通道
- [ ] QSPI 读（暂存固件）+ 验签（SHA256/HMAC，硬件 HASH）
- [ ] commit：写 Bank B + 切换标志 + boot count 兜底回滚（dual-bank）
- [ ] 断电测试：各阶段掉电 → 恢复验证（含 commit 后自检回滚）
- [ ] 保留引导框架；重写升级协议段
- [ ] QSPI 读（暂存固件）+ 验签（SHA256/HMAC，用硬件 HASH）
- [ ] 写 Bank B + 切换标志 + boot count 兜底回滚
- [ ] 断电测试：各阶段掉电 → 恢复验证
- 交付：F4A0 全链路（下载→暂存→commit→启动→回滚）通过，含断电/看门狗测试

### Phase 5：F460 迁移（1 周）
- [ ] FlashDB/FAL 移植 + w25qxx.c 适配 fal_flash（同源拷贝 + 分区表）
- [ ] w25qxx.c + hc32f460_qspi.c 加入 F460 bootloader 工程（当前缺）
- [ ] 存储后端 single_bak_qspi（QSPI 暂存→覆盖写→失败从 QSPI 备份恢复）
- [ ] Agent 复用 F4A0（仅驱动/地址差异）；确认 App 链接地址偏移
- 交付：F460 全链路通过（重点验证变砖恢复：commit 掉电/坏固件 boot count 回滚）
- [ ] w25qxx.c + hc32f460_qspi.c 加入 F460 bootloader 工程
- [ ] 存储后端 single_bak（QSPI 备份→覆盖写→失败恢复）
- [ ] Agent 复用 F4A0（仅驱动/地址差异）
- 交付：F460 全链路通过（重点验证变砖恢复）

### Phase 6：上位机 + 安全等级 + 收尾（1~2 周）
- [ ] 上位机：本地服务器（版本库+批量+日志）与 UART/USB 升级工具（帧+续传）
- [ ] 安全等级编译开关（LEVEL1/2/3）+ 强制签名
- [ ] 服务器灰度/统计（可选后补）
- [ ] 全平台回归：网络/本地/断点/回滚/断电 测试矩阵
- 交付：可发布版本 + 测试报告 + 运维文档

---

## 六、开发计划（时间线，建议 6~9 周）

    W1    W2    W3    W4    W5    W6    W7    W8    W9
    ├P0┤
    ├───P1────┤
    │    ├P2──┤
    │         ├────P3────┤
    │              ├─────P4─────┤
    │                    ├─P5──┤
    │                         ├────P6────┤
    ▸ M1(第2周末)：协议定稿+服务器最小可跑
    ▸ M2(第3周末)：RK 全链路 Demo
    ▸ M3(第5周末)：F4A0 应用态下载到 QSPI 完成
    ▸ M4(第7周末)：F4A0 全链路含断电/回滚测试通过
    ▸ M5(第8周末)：F460 全链路通过
    ▸ M6(第9周末)：上位机+安全等级+回归，可发布

    并行建议：P0/P1 与 P2 可并行；P3/P4 为关键路径（F4A0 先行）；
    P5 依赖 P3/P4 成果；P6 可与 P4/P5 部分并行。

---

## 七、关键风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| 片内 Flash 擦写变砖（F460 前科） | 设备不可用 | dual_bank 优先；single_bak 必须"先备份后覆盖"+boot count 恢复；断电测试矩阵 |
| 固件 >1MB 双 bank 失效 | A/B 不可用 | 已确认 F4A0 当前 0.4MB（≤1MB 可行）；>0.9MB 预警、>1MB 切 single_bak_qspi / qspi_xip（4.1.1） |
| MCU TLS 资源不足 | HTTPS 不可用 | HTTP+包级签名（HMAC-SHA256 硬件）替代 TLS |
| 上位机中断/掉线 | 下载中断 | QSPI 进度持久化 + 断点续传（网络 Range / 本地偏移续传） |
| 旧 bootloader 兼容 | 升级协议不兼容 | 新协议与旧 bootloader 并存过渡期：先发"双模式"固件 |
| 签名密钥泄露 | 固件被伪造 | 密钥管理流程；产线/受控环境签名；设备端公钥可轮换 |
| 多客户安全要求差异 | 需求冲突 | 安全等级编译开关（LEVEL1/2/3） |
| 4 平台联调 | 周期延长 | 协议先行定稿；RK 先跑通验证协议；MCU 分平台落地 |

---

## 八、验收标准（Definition of Done）

1. 4 平台均通过：check→download→verify→commit→自检→report 全链路
2. 断电测试：下载/校验/commit/切换各阶段断电，设备可恢复或保持旧版本
3. 回滚测试：注入坏固件，boot count 3 次内自动回滚
4. 断点续传：下载中断 50% 后续传成功
5. 安全：篡改包/无签名包被拒收（LEVEL2/3 强制）
6. 本地通道：UART/USB 升级与网络升级使用同一 OTA 包、同一校验/回滚逻辑
7. 上位机：本地服务器批量升级 ≥10 台成功 + 日志完整
8. 固件体积监控：F4A0 超 0.9MB 预警机制（编译脚本输出体积）

---

## 九、待确认事项（开工前）

- [ ] F460 是否支持 dual-bank（决定 P5 存储后端选型）
- [ ] F460 QSPI 是否支持 XIP（决定是否预留 qspi_xip 路线）
- [ ] RK3506G 运行系统（Linux vs OpenHarmony，决定 Agent 形态）
- [ ] 服务器部署环境（云厂商/自建，HTTPS 证书）
- [ ] 签名密钥管理责任方（产线/研发）
- [ ] 优先落地平台（建议 F4A0 先行验证协议）

---

（本文档为开发提纲草案，Phase 0 定稿后作为正式开发依据）


---

## 附录：统一 OTA 接口头文件（Phase 0 交付）

### 设计基准（doc/ota_interfaces/，未接入编译）

| 文件 | 内容 |
|------|------|
| ota_pkg.h | 统一固件包格式（OTA1 头 + CRC32 + SHA256 + HMAC） |
| ota_transport.h | 传输后端抽象（check/download/report + 本地帧） |
| ota_storage.h | 存储后端抽象（prepare/write_stage/verify/commit/rollback/boot_count） |
| ota_state.h | 升级状态机 + FlashDB KV 键 + RAM 魔术字 |
| ota_security.h | 验签 + 安全等级裁剪 |

### F4A0 落地（Phase 3 起点，2026-08-14 编译验证 0E/0W）

| 文件 | 位置 | 说明 |
|------|------|------|
| ota_state.h/c | hc32f4a0_app/projects/app/{inc,src} | 状态机 + FlashDB KV（AlarmDB）持久化；掉电安全标志序（先数据后 need_copy） |
| ota_storage.h/c | 同左 | QSPI 暂存（QSPI_FLASH_*）+ 片内 dual-bank commit（EFM_Program 写 Bank B + EFM_SwapCmd 引导交换） |
| 接入 | hc32f4a0_app.uvprojx user group | 已加入工程，编译 0E/0W（模块未调用，链接按需包含） |

**已验证的硬件能力**：
- HC32F4A0 支持硬件引导交换：EFM_SWAP_ADDR(0x03002000) + EFM_SwapCmd(ENABLE) → 复位后从另一 bank 启动
- QSPI：QSPI_FLASH_Write/Read/EraseSector（driver_lib）
- FlashDB KV：fdb_kv_set_blob/get_blob（AlarmDB，fdb_kvdb1 分区）

**下载 Agent + 触发/闭环/启动提交（已接入，2026-08-14 编译 0E/0W）**：
- ota_download.h/c：HTTP GET + Range 续传 → QSPI 暂存（ota_storage_write_stage）+ ota_set_progress
- 复用 CheckServerConnection/mbedtls/http_parser；独立 parser 实例
- ota_agent.h/c：ota_agent_run(url) 闭环 = 下载→verify_staged_pkg→ota_mark_ready→system_reset
- ota_agent_boot()：启动时若状态=READY → commit(OTA头82B偏移, size)→自检→system_reset
- 触发命令：http_callback.c json_remote_cmd 新增 "ota_update" 分支（解析 command_data url → ota_agent_run）
- 接入点：user_main.c wait_fin_init() 后调用 ota_agent_boot()

**本地通道 transport（已接入，2026-08-14 编译 0E/0W）**：
- ota_transport_uart.h/c："OTA1" 帧 + CRC16-CCITT-FALSE + 0x52 断点续传（帧格式见 3.4 V0.2.1）
- 接入点：Lan2Uart.c send_func UART0 接收循环，首字节 'O' 且后续 "TA1" → 整帧交给 ota_transport_uart_handle_frame；0xff 仍走旧命令（同口共存，魔数区分）
- 帧处理：懒擦除（写到哪个扇区擦哪个，避免 4MB 一次性擦除阻塞）→ ota_storage_write_stage → 进度 KV；
  首帧已有进度 → 回 0x52(偏移) 续传；CRC16 错 → 回 0x52(当前偏移) 重发
- 下载完成（82+payload_len）→ ota_storage_verify_payload（载荷 CRC32 与包头比对）→ ota_mark_ready → system_reset
- ota_storage 新增 ota_storage_verify_payload()（载荷区 CRC32 比对）

**当前固件体积**：Code=360180 RO=26120 RW=13328 ZI=55120（本地 transport 净增 +1608B，累计 +4904B）

**上位机工具 + 协议自测（已交付，2026-08-14）**：
- tools/ota_pack.py：bin/Intel-HEX → 统一 OTA 包（OTA1 头 + CRC32 + SHA256 + HMAC demo key）
- tools/ota_send.py：串口发送器（"OTA1" 帧 + 0x52 断点续传 + len=0 探测查询 + 进度显示）
- tools/ota_selftest.py：设备端状态机模拟器（复刻 handle_frame），无硬件验证协议一致性
- 自测结果（真实固件 378KB 包）：全新下载/中断续传(10KB)/半扇区续传(5000)/ACK丢失探测恢复 全部通过
- 设备端新增：len=0 DATA 帧 = 探测帧（仅回 ACK(progress)，不写存储），供上位机超时后查询进度
- 实测步骤见 tools/README.md（打包 → 连 UART0@115200 → 发送 → Ctrl+C 中断 → 重跑续传）

**Phase 4 Bootloader（已交付，2026-08-15 编译 0E/0W）**：
- 新建独立工程 hc32f4a0_boot（Code=5596，64KB 空间余量大），LL 驱动源码编译（无 RTX 依赖）
- **布局**：bootloader 0x0-0xFFFF；App 偏移 0x10000-0xEFFFF（896KB）；标志区 0xF0000（每 bank 一份）
- **标志页**（boot_flag_t，24B）：magic+"OTA1"+flags(NEED_COMMIT/NEED_CONFIRM)+size+version+boot_count+crc32
- **commit 迁入 bootloader**：读标志 NEED_COMMIT → 校验 QSPI 暂存（magic+载荷 CRC32）→ 自复制 boot 到另一 bank + 写 App(0x11000) + 目标标志(NEED_CONFIRM) → 清当前标志 → swap → 复位
- **boot count 自检回滚**：新固件首启（NEED_CONFIRM）→ boot_count++；>3 次 → swap 回滚旧 bank；App 自检通过 → ota_boot_confirm 清标志
- **App 改动**：sct/VTOR 偏移 0x10000；ota_mark_ready 写标志页 NEED_COMMIT；ota_agent_boot 改 confirm-only（commit 已迁出）；新增 ota_flag.h/c
- 编译验证：boot 0E/0W（hex @0x0, 6KB），App 0E/0W（hex @0x10000-0x6EB10, 388KB）

**首次部署（烧录器）**：
1. 烧 hc32f4a0_boot.hex（0x0）→ Bank A bootloader
2. 烧 firmware_p4.hex（0x10000 起）→ Bank A App（标志区 0xF0000 保持 0xFF = 正常跳 App）
3. 升级测试：tools/ota_pack.py 打包新 App hex → 串口/网络下发 → App mark_ready(标志) → 复位 → bootloader commit → swap → 新固件自检 → confirm
4. 断电/坏固件测试：commit 中掉电 → 上电重试 commit；新固件起不来 → boot count 3 次后自动回滚

**服务端三接口（已交付，2026-08-15 自测 6 项全 PASS）**：
- tools/ota_server.py：POST /ota/check（版本比对）、GET /ota/pkg（+HTTP Range 断点续传）、POST /ota/report（落盘）
- 版本库：server_data/manifest.json + firmware/*.otapkg（启动自动补 sha256；id 防目录穿越）
- tools/ota_server_selftest.py：check 新旧版本 / pkg 完整+Range 拼接 / report / 404 安全 全通过
- 与设备端 ota_download.c（HTTP+Range，iap_progress 续传）协议对齐
- 启动：python tools/ota_server.py --port 8080 --dir tools/server_data

**安全层正式化（已交付，2026-08-15 编译 0E/0W）**：
- app/inc/ota_security.h + src/ota_security.c：HMAC-SHA256 验签（覆盖 [0:50]+payload）+ payload SHA256 完整性
  - mbedtls md 流式计算（分块读 QSPI 暂存，不占大 RAM）
  - 设备内置共享密钥 OTA_SEC_KEY（与 tools/ota_pack.py DEMO_HMAC_KEY 一致；正式化按客户密钥替换）
- 安全等级编译开关 OTA_SECURITY_LEVEL（app_conf 可覆盖）：1=网络+本地（默认，有签名必验、全 0 兼容放行）/ 2=仅本地（强制验签）/ 3=仅本地+强制签名
- ota_agent.c verify_staged_pkg 接入验签（CRC32 保留 + HMAC + SHA256）
- 修复 ota_pack.py HMAC 覆盖范围 bug（原 [0:82]+payload → 协议要求 [0:50]+payload），重新打包并验证 magic/crc32/sha256/hmac 全匹配
- 编译：App Code=360756 0E/0W；版本库包 fw_01020000.otapkg 388666B（含新验签固件）

**USB1(CDC) 本地通道（已交付，2026-08-15 编译 0E/0W）**：
- ota_transport_uart 新增流式组帧 ota_transport_uart_feed(fd, data, len)（状态机：魔数同步/续接/重同步/非法长度防护，与 UART0 整帧入口共存，fd 参数化回发互不干扰）
- 新增 ota_usb.h/c：USB1(CDC) 轮询线程（read→feed），ACK/RESUME 经 write(USB1) 回发
- user_main 启动时调用 ota_usb_start()（wait_fin_init 后）
- 自测：Python 复刻状态机 6 项 PASS（整帧/逐字节/随机分块/半帧恢复/连续多帧/非法长度防护）
- 说明：UART0 与 USB1 为同一 OTA1 帧协议的两个本地通道；半帧被吞由停止等待+RESUME 重发兜底
- 编译：App Code=361176 0E/0W；版本库包 fw_01020000.otapkg 389082B

**上位机升级工具（已交付，2026-08-15 自测通过）**：
- tools/ota_upgrade_tool.py：统一 CLI
  - 版本库管理：list / add / remove（add 自动校验 OTA 包 + 更新 manifest latest，版本号 8 位十六进制规范化）
  - 网络升级：check（查新版）/ download（HTTP+Range 断点续传，本地已有部分自动续传；下载后校验 CRC32/SHA256/HMAC）
  - 串口升级：uart（复用 ota_send OTA1 帧协议，带进度；Ctrl+C 可中断续传）
- 自测：list/add/check/download 全通过；断点续传（预置半文件→Range 续传→与源字节一致+校验通过）
- 与 ota_server.py（check/pkg+Range/report）组成完整网络升级链路

**RK Agent（已交付，2026-08-15 自测 4 项 PASS）**：
- tools/rk_ota_agent.py：RK3506G/RK3566 应用升级 Agent（只升应用不升系统）
  - 流程：check → download(断点续传) → verify(CRC32/SHA256/HMAC) → 备份旧应用 → **rename 原子替换**（os.replace 单次原子提交）→ systemd restart → 记录版本
  - 失败回滚：从备份恢复旧应用；坏包/校验失败不触碰旧应用
  - 配置 rk_agent.json（server/device_id/platform/app_id/app_path/service/staging/backup/key/state_file）
- 自测（--selftest，临时目录模拟）：包校验 / 原子替换+版本记录 / 失败回滚 / 坏包拒绝 全 PASS
- 部署：应用由 systemd 承载（myapp.service），Agent 由 timer/手动触发

**F460 Phase 5 勘察与基线（2026-08-15）**：
- F460 App 基线编译通过：hc32f46_app/trunk 0E/7W，Code=283404（F460 工具链可用）
- F460 bootloader 工程修复：hc32f46_driver.lib 内含 main.o 与 bootloader 源码 main 冲突 →
  剥离 main.o 生成 bootloader 专用 boot_driver.lib（armar -d），main 冲突解除
- bootloader 剩余依赖缺口（既有工程环境问题）：RTX_CM4F.lib（osDelay/osMutex/osKernel*）、
  json 库（json_parse 等，readercfg.o 引用）、堆描述符（__rt_heap_descriptor）、
  u32ICG/active_http_post/httpapi_hander 等——需用户提供这些库/配置的完整路径
- **F460 部署模型待确认**（Phase 5 前提）：
  1) App 最终链接地址（当前 Firmware.sct=0x0；bootloader JustJump2App 用 0x16000，需统一）
  2) 片内分区：bootloader 区 / App 区 / 标志区（512KB 单 bank，无 A/B）
  3) 升级执行者：bootloader 覆盖写 vs App 内 IAP（RAM 例程）——single_bak_qspi 需"先备份后覆盖"
  4) btparams 升级标志复用或新建标志页（对齐 F4A0 方案）
- 待确认后实施：QSPI 暂存/备份布局（w25qxx 已有）、F460 ota 模块移植（hc32f46x 驱动适配）、
  w25qxx+hc32f460_qspi 进 bootloader（当前缺）、FlashDB/FAL 移植（如需 KV 进度）

**全链路验收（已交付，2026-08-15 9/9 PASS）**：
- tools/ota_acceptance.py：一键串联全部软件层验证（打包→版本库→服务器三接口→上位机 check/download/断点续传→设备端模拟 4 场景→USB feed 状态机→RK Agent）
- doc/OTA测试矩阵.md：验收标准落地——软件项 9 项自动验证；硬件项 H1-H9（首烧/网络/本地/断点/断电/回滚/安全/RK）标注步骤+通过标准+待硬件

**固件体积监控（已交付，2026-08-15 自测通过）**：
- tools/fw_size_monitor.py：解析 Keil .map 的 Total ROM Size → 对比阈值
  - F4A0 App 区硬上限 896KB、预警阈值 761.6KB（0.85×）；退出码 0=正常/1=预警/2=超上限
  - 当前固件 379.9KB（剩余 516KB），远低于预警
- 用法：python tools/fw_size_monitor.py --map output/firmware.map（编译后自动检查）

**一键构建/发布/验收（已交付，2026-08-15 全流程通过）**：
- tools/build_ota.py：python build_ota.py 一条命令完成
  编译 App(0E/0W) → 编译 Bootloader(0E/0W) → fromelf 生成 hex → 体积监控 →
  打包 OTA 包+版本库 add → 合并烧录 hex（merged_f4a0.hex）→ 全链路验收(9/9)
- 选项：--skip-build / --skip-acceptance；退出码 0=通过
- 实测：全流程一次通过（App 379.9KB、merged 0x0-0x6EF87、验收 9/9）

**安全层平台无关化（已交付，2026-08-15 编译 0E/0W + 回归 9/9）**：
- ota_security 抽象为读回调接口：ota_security_verify_ex(ota_sec_read_fn read, void *arg)
  - read 回调：off=暂存区相对偏移（0=包头）；F4A0 传 QSPI 读，F460 可传 w25qxx 读
  - ota_security_verify_staged() 保留为 F4A0 便捷入口（内部用 QSPI 回调）
- 意义：验签/SHA256/CRC 核心为平台无关纯逻辑，F460/RK MCU 移植直接复用
- 回归：App 0E/0W（Code=361220）；ota_acceptance 9/9 PASS

**帧协议核心平台无关化（已交付，2026-08-15 编译 0E/0W + 回归 9/9）**：
- app/inc/ota_frame.h + src/ota_frame.c：OTA1 帧协议纯逻辑核心（零硬件依赖）
  - ota_frame_crc16 / ota_frame_build（组帧）/ ota_frame_parse（解析+CRC 校验）/ ota_frame_feed（流式状态机）
- ota_transport_uart.c 重构复用核心（删重复实现，对外 API 不变）；F460 帧协议直接复用
- 回归：App 0E/0W（Code=361404）；ota_acceptance 9/9 PASS

**端到端演示文档（已交付，2026-08-15 命令链验证通过）**：
- doc/OTA网络升级演示.md：打包→版本库→服务器→上位机 check/download（Range 续传）→
  设备端触发（HTTP ota_update 命令 / UART0 / USB1）→ 一键验收 → 真机清单 → FAQ
- 本地可跑命令链全部验证通过（list/check/download/断点续传/验收 9/9）

**single_bak_qspi 策略算法验证（已交付，2026-08-15 5 场景 PASS）**：
- tools/single_bak_sim.py：F460 单 bank 备份-覆盖-恢复策略模拟（片内 512KB + QSPI 8MB）
  - 正常升级 / 暂存损坏 / 备份前断电 / 覆盖中断电（从备份恢复）/ 覆盖后校验失败 全 PASS
  - F460 移植直接按此实现（含断电注入语义：备份完整→覆盖→校验→恢复）
- 配合已交付的平台无关核心（ota_frame / ota_security 读回调），F460 Phase 5 主要拼图齐备

**F460 FlashDB/FAL 移植指南（已交付，2026-08-15）**：
- doc/F460_FlashDB移植指南.md：文件清单（同源拷贝 F4A0 flashDB）+ 适配点模板
  - fal_cfg.h 分区表模板 / fal_flash 驱动（nor_flash0 → w25qxx API 映射骨架）/
    flashdb.c 初始化 / fdb_cfg.h 配置 / ota_state 对接
  - 验证路径：编译 0E/0W → KV 读写自测 → ota_state 联调
  - bootloader 建议用独立标志页（F4A0 Phase 4 方案），不引入 FlashDB

**上位机批量设备管理（已交付，2026-08-15 自测通过）**：
- ota_upgrade_tool.py 新增 devices 命令：CSV 设备清单（device_id,platform,app_id,version）
  - 批量 check 汇总（需升级/最新）→ devices_report.csv
  - --download：逐台下载 + 校验（devices_out/）
- 自测：3 台（2 旧版+1 最新）→ 2/3 需升级，下载校验通过
- 配合 ota_server reports.log，满足验收标准 7（批量升级+日志）

**密钥管理正式化（已交付，2026-08-15 自测通过）**：
- tools/ota_keygen.py：生成 32B HMAC 密钥 / --show 显示 / --c-header 输出设备端替换片段
- ota_pack.py 支持 --key-file（32B 密钥文件，优先于 --key 字符串）
- 自测：新密钥打包→同 key 验签通过；错误 key 验签失败（密钥生效）
- 安全：密钥文件已加入 .gitignore（*.key/key_*.bin）；正式流程：keygen→安全分发（产线/服务器/设备编译）→轮换时重生成+设备端替换 OTA_SEC_KEY

**交付总览（已交付，2026-08-15）**：
- doc/OTA交付总览.md：固件/协议/工具链/平台无关核心/文档/待办/验证基线全览（21/21 交付物核对存在）

**F460 Phase 5 主体（已实施，2026-08-15 编译验证）**：
- 部署模型确认：App@0x16000 / bootloader 0x0-0x7FFF / 标志区 0x7F000 / QSPI 暂存 0x0(1MB)+备份 0x100000(512KB) / 方案A(bootloader 覆盖写) / 方案1(独立标志页) / 不保留网络升级
- **F460 App**：Firmware.sct 偏移 0x16000（0E/7W，hex 0x16000-0x60210）；新增 ota_flag.h/c（双标志+反码+boot count @0x7F000，flash_sector_erase/flash_bytes_write）
- **F460 精简 Bootloader**（全新，不依赖旧工程）：LL 库源码编译（hc32_ll_clk/efm/gpio/pwc/qspi/sram/utility，官方 DDL Rev3.3.0）
  - 时钟 MPLL 200MHz；QSPI 内存映射读（PortB 引脚，STD_RD）；片内 EFM 覆盖写
  - 流程：NEED_COMMIT→校验暂存(CRC32)→覆盖写 App→置 NEED_CONFIRM→跳 App；boot count>3→从 QSPI 备份恢复
  - 备份由 App 侧升级前写 QSPI（bootloader 只读暂存+备份）；编译 0E/4W（Code=4856，hex 0x0-0x1980）
- 待真机：F460 首烧（boot hex + App hex）+ 升级/断电/回滚实测

**F460 App 侧 ota 模块（已交付，2026-08-15 编译 0E/7W）**：
- ota_flag.h/c（标志页 0x7F000 双标志+反码+boot count）+ ota_storage.h/c（QSPI 暂存写/备份旧固件/进度持久化）
  + ota_state.h/c（mark_ready=备份+写标志 / boot_confirm）+ ota_agent.h/c（run_local=校验暂存→备份→置 NEED_COMMIT→复位；boot=启动确认）
- 流程闭环（本地升级）：下载到 QSPI 暂存 → ota_agent_run_local → 备份旧 App → NEED_COMMIT → 复位 → bootloader 覆盖写 → NEED_CONFIRM → 新固件自检 → confirm
- 编译：F460 App 0E/7W（原有 warning）；4 模块全编译（含修复 uvprojx IncludeInBuild=0 排除条目）
- 待接线：下载通道（HTTP+Range 或本地 OTA1 帧）+ user_main 调 ota_agent_boot

**F460 本地升级链路（已交付，2026-08-15 编译 0E/7W）**：
- ota_frame（平台无关核心，复用）+ ota_transport_uart（OTA1 帧 → 懒擦除写 QSPI 暂存 → 进度 → 完成触发升级）
- Lan2Uart UART0 循环接入 OTA1 magic 分支（与 0xff 旧命令共存）
- user_main 调 ota_agent_boot（启动自检确认）
- 全链路：上位机 OTA1 帧发送 → F460 暂存 → 完成 → 备份旧固件 → NEED_COMMIT → 复位 → bootloader 覆盖写 → 自检 → confirm
- 编译：F460 App 0E/7W（Code=286196，ota 模块已链接）
- 待真机：F460 首烧 + 本地升级/断电/回滚实测（DAP-LINK 已具备）

**F460 真机联调准备（已交付，2026-08-15）**：
- F460 OTA 包打包验证（platform=1，307546B，magic/CRC/SHA/HMAC 全过）
- 烧录文件 merged_f460.hex（boot 0x0 + app 0x16000，314000B）
- 测试矩阵 +H10-H12：F460 首烧 / 本地升级 / 断电回滚（待真机，DAP-LINK 已具备）

**下一步**：
- 真机联调（F4A0 H1-H9 + F460 H10-H12；merged_f4a0.hex / merged_f460.hex 已备；F460 用 ota_upgrade_tool uart 测本地链路）

