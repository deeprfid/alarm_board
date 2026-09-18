# HC32F4A0 RFID 读写器固件

UHF RFID 读写器嵌入式固件，基于小华半导体 HC32F4A0PITB（Cortex-M4F @240MHz，2MB Flash，516KB SRAM，LQFP100），
支持以太网（W5100S）、MQTT/HTTP/TCP 上传、Modbus RTU、GPI/GPO、Wiegand、FlashDB 白名单存储等。

> 维护提示：本文件为系统框架速查；修改记录见 CHANGELOG.md；问题清单见 ISSUES.md（含真机验证清单 V1-V11）；数据手册 doc/DS_HC32F4A0系列数据手册_Rev1.50.pdf
> 最后更新：2026-08-15（含 OTA Phase 0-4 全部落地：服务端/上位机/RK Agent/验收闭环）

---

## 1. 芯片资源速查（HC32F4A0PITB）

| 项 | 值 |
|---|---|
| 内核 | Cortex-M4F（FPU+MPU+DSP），最高 240MHz（工程实际运行 240MHz） |
| Flash / OTP | 2MB dual-bank / 134KB |
| SRAM | 516KB = SRAMH(128K@0x1FFE0000) + SRAM1/2(各128K) + SRAM3(96K) + SRAM4(32K) + SRAMB(4K@0x200F0000) |
| 封装 | LQFP100（83 GPIO），-40~105℃，1.8~3.6V |
| 通信 | 10×UART、6×SPI、6×I2C、4×I2S、2×SDIO、1×QSPI、2×USB(HS/FS)、1×ETHMAC、CAN、EXMC |
| 安全/加速 | AES256、SHA256/HMAC、TRNG、DCU×8、FMAC×4、MAU、HRPWM×16 |

## 2. 目录结构

    HC32F4A020260320_RYK/
    ├── hc32f4a0_app/         应用工程（业务：user_main、HTTP/MQTT/Modbus、FlashDB、事件/IPC）
    │   ├── projects/app/src+inc   业务源码（app/src）
    │   ├── projects/MDK           Keil 工程（链接 driver_lib 库，输出 firmware.axf/hex）
    │   ├── projects/MQTT          Paho MQTT 移植
    │   ├── projects/flashDB       FlashDB(FAL) + 标签同步（QSPI_Sync.c）
    │   ├── projects/jsonlib       json-parser / yyjson / TagBuffer / mp_pool
    │   └── projects/middleware    http_parser
    ├── hc32f4a0_driver/      驱动工程（平台：main+时钟+外设+W5100S+USB，输出 hc32f4a_driver.lib）
    │   ├── drivers/hc32_ll_driver  华大 LL 库（全外设）
    │   ├── projects/source         板级（board.c 含 BSP_CLK_Init 240MHz、timer、flash、aes 等）
    │   ├── projects/user           用户驱动（common/io_stream/readercfg/uart/rs485/wiegand/usr_mod）
    │   └── projects/wizchip        W5100S（socket/wizchip_conf/port）
    ├── hc32f4a0_boot/        OTA Bootloader 工程（dual-bank swap + commit + boot count 回滚，0x0-0xFFFF）
    ├── driver_lib/            预编译库（hc32f4a_driver.lib、ModuleAPI_C_ARM.lib、RTX_CM4F.lib + 头文件）
    ├── tools/                 OTA 工具链（build_ota 一键构建/验收、ota_server 服务器、ota_pack 打包、
    │                           ota_upgrade_tool 上位机、ota_send/uart 本地升级、merge_hex 烧录、
    │                           ota_acceptance 验收、single_bak_sim、rk_ota_agent、ota_keygen 密钥）
    ├── doc/                   OTA 计划与接口（OTA开发提纲与计划.md、ota_interfaces/、OTA测试矩阵.md、
    │                           OTA网络升级演示.md、F460_FlashDB移植指南.md）
    ├── CHANGELOG.md / ISSUES.md
    └── 1090_src_20250313/     旧平台快照（HC32F460 bootloader/app/driver，参考用）

## 3. 统一 OTA 升级体系（Phase 0-4 已落地）

### 3.0 布局（F4A0 dual-bank）
    Bank A 0x00000000-0x000FFFFF       Bank B 0x00100000-0x001FFFFF
      Bootloader 0x00000-0x0FFFF         Bootloader 副本（commit 自复制）
      App        0x10000-0xEFFFF         App 新固件（commit 目标 0x11000）
      标志区     0xF0000-0xFFFFF         标志区副本
    升级流程：App 下载(QSPI 暂存)→验签(CRC32+SHA256+HMAC)→标志 NEED_COMMIT→复位
      →Bootloader commit(Bank B)→swap→新固件自检(boot count)→confirm；失败自动回滚

### 3.0.1 统一 OTA 包 / 三接口 / 工具链
    OTA 包："OTA1"+ver+platform+app+len+CRC32+SHA256+HMAC([0:50]+payload)+payload（82B 头）
    接口：POST /ota/check · GET /ota/pkg(+Range 断点续传) · POST /ota/report
    通道：HTTP+Range（网络）/ OTA1 帧（UART0+USB1-CDC，0x52 断点续传）
    安全等级：OTA_SECURITY_LEVEL 1/2/3（编译开关，LEVEL2/3 强制验签）
    一键：python tools/build_ota.py（编译 0E/0W→hex→体积监控→打包→版本库→合并烧录hex→验收 9/9）
    验收：python tools/ota_acceptance.py（9 项软件层 PASS；硬件 H1-H9 见 doc/OTA测试矩阵.md）

## 3. 系统架构

### 3.1 双工程 · 单固件模型（重要）

    driver 工程（"创建库"模式）──编译──> hc32f4a_driver.lib（含 main/init_thread/dhcp_func/BSP_CLK_Init/平台驱动）
    app 工程（"可执行"模式）──链接 driver_lib + 业务源码 ──> firmware.axf（单固件）
        ├─ 入口 main() 来自驱动库（app 工程无 main.c）
        ├─ init_thread 创建 user_main 线程（app 业务入口）、DHCP 线程、固件升级线程
        └─ user_main（app/src/user_main.c）按工作模式分派：
             ├─ 主动模式 user_main_active()：自主盘点→入库→上报；GPI 触发；心跳；错误自愈
             └─ 被动模式 user_main_passive()（Lan2Uart.c）：TCP/UART/USB 服务器，
                  命令分发：自定义 ASCII / 0xEE / Modbus RTU / POS(HTTP API) / 模块透传

> 修改 driver 源码后必须重编 driver 工程（After Build 自动同步 driver_lib/），再重编 app。

### 3.2 线程模型（RTX5 / CMSIS-RTOS2）

    main() → osKernelStart
      ├─ init_thread(1.5KB)    网口/DHCP 检测、创建业务线程、固件升级线程、广播线程
      ├─ dhcp_func(1KB)        以太网 DHCP（失败回退默认 IP）
      ├─ user_main(4KB)        业务主线程（主动/被动模式）
      ├─ send_tags(4KB)        事件上报线程（心跳/标签/批量/GPI/错误）
      ├─ Tag_update_thread(2KB) 标签入库/报警处理（ipc.c）
      ├─ send_func/sendthread(被动模式) 串口/TCP 响应缓冲
      └─ 软看门狗：task_monitor.c（SoftWdtFed + 线程终止检测自动复位）

### 3.3 标签数据流（主动模式）

    RFID 模块(UART) → ModuleAPI.lib → TagInventory → TAGINFO
      → tagInsert_wp(user_main) → put_tag_que(ipc.c 队列)
          ├─ Tag_update_thread → tag_package(alarm.c)：白名单匹配(ADDlist/DELlist/TSDB) / EAS bit → UDP/RS485/GPO
          └─ put_evt_que(event_mq.c) → send_tags → send_evt_*(reader_msg.c) → TCP/HTTP/MQTT 上报

### 3.4 存储布局

    Flash（片内 2MB）：代码 ~0.39MB（~19%）
    外部 QSPI Flash（FAL/FlashDB）：
        fdb_kvdb1(0~2MB) + fdb_tsdb2(2~4MB) + fdb_tsdb1(4~16MB)
        fal_flash_sfud_port.c read/write/erase → QSPI_FLASH_*(预编译库)
    片内 SRAM（2026-08-14 优化后）：
        静态区+32KB 主栈：0x1FFE0000~0x1FFF02D8（SRAMH 内；R4 主栈 64K→32K）
        堆：0x1FFF0300~0x20060000 ≈ 457KB（R3 动态跟随静态区 + R4 主栈缩小；覆盖 SRAMH 剩余+SRAM1-4）
        备份 SRAM：0x200F0000 4KB（空闲；R2 已删除 tagtmpbuf 死缓冲）

## 4. 编译 / 烧录

    # driver 库（改动 driver 源码后必须重编）
    D:\Keil_v5\UV4\UV4.exe -r "hc32f4a0_driver\projects\MDK\hc32f4a0_driver.uvprojx" -o 日志
    # app 固件
    D:\Keil_v5\UV4\UV4.exe -r "hc32f4a0_app\projects\MDK\hc32f4a0_app.uvprojx" -o 日志
    # 基线（2026-08-14）：driver 0E/0W；app 0E/0W
    # 产物：hc32f4a0_app/projects/MDK/output/firmware.axf + rfidapp.hex
    # 烧录：J-Link/ST-Link 烧录 rfidapp.hex（或通过 bootloader HTTP 升级，见 1090_src bootloader）

## 5. 关键配置开关

    app_conf.h：Custom_By_SZBMA=1（深圳 BMA 定制）；Custom_By_Caipan/ZHXX_ZSYH/CDZNWL/GZTD/HZWXZN=0
    driverconfig.h：IS_RTOS2_SUPPORT=1、ENABLE_ICG_TABLE=0
    时钟：board.c BSP_CLK_Init 8MHz×120÷4 = 240MHz（sysinit.c 的 168MHz 配置为死代码，勿接入）

## 6. 文档索引

| 文档 | 内容 |
|------|------|
| CHANGELOG.md | 修改记录：流式 EPC 扫描器、芯片/时钟核查、编译基线、Warning 清零、R1/R3 修复 |
| ISSUES.md | 问题清单：风险 R1-R5、死代码 D1-D3、待验证 T1-T7（含已修复标记） |
| doc/DS_HC32F4A0系列数据手册_Rev1.50.pdf | 官方数据手册（命名规则、引脚、电气特性） |

## 7. 已知注意事项

- 预编译库 driver_lib/ 由 driver 工程 After Build 自动同步，勿手工修改
- 5 万条白名单 HTTP 推送走流式扫描（stream_epc_scanner.c），内存恒定 ~150B
- 备份 SRAM(4KB) 当前被 tagtmpbuf 占用（临时缓冲），如需掉电保持功能需调整（见 ISSUES R2）
- 240MHz 主频与堆扩展建议真机验证（ISSUES T1/T2）
