# F460 扫描盘读写器 OTA + 网页配置方案

> 2026-08-31 记录。F4A0 侧评估完成，F460 移植待工程就绪后实施。

## 1. 需求

另一项目（扫描盘读写器，芯片 **HC32F460**）需要：
- **OTA 升级**：只要 **HTTP**（设备做 HTTP 服务器，浏览器/上位机 POST 整包）
- **网页配置**：浏览器访问设备 IP 做参数设置 + 标签上传/查看 + OTA 一键升级
- 网页形态：**单文件纯 HTML**（index.html 内嵌 CSS/JS，几 KB~十几 KB）

## 2. 硬件条件（已确认）

| 项 | F4A0 | F460 | 结论 |
|---|---|---|---|
| 片内 SRAM | 512KB（静态127KB+堆384KB） | **192KB** | ⚠️ 差 2.7 倍，须流式瘦身 |
| 片内 Flash | 512KB | 512KB | 一致 |
| 片外 QSPI | 16MB（kvdb1 0-2M + tsdb2 2-4M + tsdb1 4-16M + OTA 暂存 0xE00000 1MB） | **16MB** | ✅ 同构，常量直接复用 |
| 网络 | W5100S SPI | **W5100S SPI** | ✅ 同款 |
| 外设接口 | — | 与 F4A0 **几乎完全一致** | ✅ port.c SPI 宏大概率直接可用 |
| FlashDB | 已移植 | 已移植（F460_FlashDB移植指南.md） | ✅ |

## 3. OTA 方案（HTTP-only）

### 移植文件清单（全部从 F4A0 拷贝）

**A. 平台无关 OTA 核心（直接拷，0 改动）**
- ota_frame.c/.h —— OTA1 帧格式 + CRC16 查表
- ota_security.c/.h —— HMAC-SHA256 验签 + 版本检查
- ota_state.c/.h —— 状态机 + FlashDB KV
- ota_storage.c/.h —— QSPI 暂存/校验/commit + 通道互斥
- ota_flag.c/.h —— 升级标志
- ota_agent.c/.h —— 下载→校验→置标志→复位
- ota_http.c/.h —— 设备做 HTTP 服务器收 POST 整包

**B. 网络栈（W5100S 标准代码）**
- wizchip/W5100S/w5100s.c/.h —— 0
- wizchip/wizchip_conf.h —— 0（已 _WIZCHIP_=W5100S）
- wizchip/socket.c/.h —— 0
- wizchip/port.c/.h —— 几乎 0（外设接口一致，仅核对板级 SPI 映射）
- wizchip/dhcp.c/.h —— 0（若用 DHCP）

**C. F460 侧需写（极少）**
1. OTA 监听任务 ~30 行（仅 HTTP 单通道）：
   ```c
   loop: apt_single_select_nob(SOCKET_x, OTA_PORT) → http_handle_conn(fd) → close → sleep_ms(1)
   ```
2. Flash 常量：OTA_QSPI_STAGE_BASE（0xE00000，同 F4A0）、OTA_FW_VERSION（F460 版本号）
3. 启动链 2 行：ota_init_http_only()（起监听 + ota_agent_boot）

**D. 上位机（0 改动）**
- tools/ota_http_send.py（POST 整包）；tools/ota_pack.py（同 OTA1 格式，密钥同源包通用）

### 网页端直接 OTA（已确认兼容）
- 设备 ota_http.c 解析 Content-Length → 收整包写 QSPI → 200 OK → 验签 → system_reset
- 浏览器 fetch POST /ota（body=File）自动带 Content-Length，与 PC 脚本等效
- 页面提示"升级中，设备将重启"即可

## 4. 网页配置方案（纯 HTML 单文件）

### 设备侧改动
1. HTTP 路由加 "/" → 从 QSPI FAT 流式发 index.html（1KB 缓冲 chunked，复用 http_serve_tagcsv 模式）
2. 参数设置：页面 fetch POST /api 传 command_type=set_xxx → 复用现有 json_remote_cmd 命令集，**0 新代码**
3. 标签上传：页面 fetch GET /tags?page=&size= → 新增流式路由（复制 tagcsv 改），或调现有 readtag API

### 网页文件进 QSPI
- 推荐：上位机 POST /web 推送一次（同推 TAG.CSV 流程）
- 或产线预置

### 浏览器 = 完整工具
| 功能 | 网页操作 | 设备侧 |
|---|---|---|
| 参数设置 | 表单 → fetch POST /api | 现有 json_remote_cmd（0 新代码） |
| 标签查看/上传 | fetch GET /tags?page=&size= | 新增流式路由（复制 tagcsv） |
| OTA 升级 | file input → fetch POST /ota | 现有 ota_http（0 新代码） |

## 5. RAM 策略（192KB 关键约束）

**核心原则：全程流式，绝不整文件/整包进 RAM**

| 项 | 预算 |
|---|---|
| 网页文件服务 | ~2KB（1KB 读缓冲 + 220B 头，http_serve_tagcsv 模式） |
| HTTP POST 缓冲 | 4-8KB（参数单条 JSON 很小；大推送走流式豁免） |
| 标签上传 | ~2KB（分页/流式，GET /tags?page=&size=） |
| **合计额外** | **< 20KB** |

**行动项（优先）**：编译 F460 看 map 的 RW+ZI——≤60KB → 堆 ~130KB 方案随便做；>100KB → 先砍静态缓冲。

## 6. 待现场确认

1. F460 的 W5100S SPI 引脚板级映射核对（外设接口一致，预计几乎零改）
2. F460 是否已有 RTOS（CMSIS-RTOS2/RTX5）——无则监听任务简化为 main 循环轮询
3. F460 编译后静态区（RW+ZI）实测大小 → 决定堆余量

## 7. 预估工作量

| 阶段 | 内容 | 工时 |
|---|---|---|
| 1 | OTA 移植（拷文件 + 30 行监听 + Flash 常量） | 2-3 天 |
| 2 | 网页 index.html（单文件） | 1-2 天（前端工作量） |
| 3 | 设备侧 2 个路由（/ 和 /tags）+ /web 推送 | 0.5-1 天 |
| **合计** | | **3-5 人·天** |
