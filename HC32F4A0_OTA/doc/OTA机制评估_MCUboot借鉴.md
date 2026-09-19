# OTA 升级机制评估：现有自研实现 vs MCUboot 借鉴点

> 版本：v1.0（2026-08-28）
> 依据：`hc32f4a0_boot/projects/boot/src/ota_boot.c`、`hc32f4a0_app/projects/app/src/ota_agent.c`、`ota_state.c`、`ota_security.c`
> 目的：评估 MCUboot 对现有自研双 bank OTA 的借鉴意义，形成改进清单。

---

## 1. 现有 OTA 机制总览（已实现）

### 1.1 升级流程（App 侧）

```
传输通道（串口/USB-CDC/HTTP 8081/USB 虚拟U盘）
  → 接收 OTA1 包（82B 包头 + 载荷）
  → 流式搬移到 QSPI 暂存区（0xE00000，8MB FAT 卷）
  → 验签 + CRC 校验（ota_security_verify_staged）
  → ota_mark_ready(fw_len, version)  标记 NEED_COMMIT
  → 重启
```

### 1.2 引导决策（Bootloader 侧，ota_boot.c）

```
boot_run():
  当前 bank 标志 NEED_COMMIT=1
    → 校验 QSPI 暂存 → commit 写另一 bank（boot 自复制 + App 拷贝）
    → 清标志 → EFM swap → 复位
  当前 bank 标志 NEED_CONFIRM=1
    → boot_count++
    → 超限 → swap 回滚旧镜像；否则跳 App
  否则 → 跳 App（当前 bank）
```

### 1.3 已实现机制清单

| 机制 | MCUboot 术语 | 现有实现 | 状态 |
|---|---|---|---|
| 双镜像区 | primary/secondary slot | 双 bank（0x0 + 0x100000） | ✅ |
| 升级确认 | image confirm | NEED_CONFIRM + boot_count 自检回滚 | ✅ |
| 回滚 | swap 回滚 | boot_count 超限 swap 回滚 | ✅ |
| 待提交标志 | image pending | NEED_COMMIT（QSPI 暂存→commit） | ✅ |
| 镜像校验 | magic + 签名/哈希 | OTA1 82B 头 + CRC32 + 验签 | ✅ |
| 交换算法 | swap/overwrite | EFM 硬件 swap（优于软件 swap） | ✅ |
| 版本检查 | downgrade prevention | 版本号校验 | ✅ |
| 多通道传输 | -（MCUboot 不管传输） | 串口/USB/HTTP/U盘 4 通道 | ✅ |

### 1.4 现有实现优于 MCUboot 的点

1. **QSPI 暂存中转**：新固件先落 QSPI（8MB），commit 时才写另一 bank——镜像区不提前被占，MCUboot 是直接写 secondary slot
2. **EFM 硬件 swap**：HC32 flash swap 硬件功能，比 MCUboot 软件 swap（需 scratch 区 + 反复拷贝）快且省 Flash
3. **4 通道传输已打通**，MCUboot 本身不含传输

---

## 2. MCUboot 值得借鉴的点（对照缺口）

### 2.1 掉电安全 swap 协议（最高优先级）

**MCUboot 设计**：secondary slot 尾部有 trailer 区，记录 swap 进度（每扇区交换状态），
**任意时刻掉电可恢复**——下次启动从断点继续或回滚。

**现有风险**：commit 写另一 bank 过程中掉电（boot 自复制 + App 拷贝 + 清标志 + swap 时序中），
另一 bank 处于半写状态 + 标志页状态——重启后能否正确恢复？

**行动项**：审查 ota_boot.c 的 commit 时序与标志页设计，确认：
- [ ] commit 写另一 bank 期间掉电，重启后 boot 是否能识别半写状态
- [ ] swap 执行中掉电，能否恢复（硬件 swap 的原子性确认）
- [ ] 是否需要增加"进度记录"（类似 MCUboot trailer）

### 2.2 镜像头标准化（中优先级）

MCUboot 镜像头固定格式（magic/版本/大小/签名偏移/依赖），可对接 imgtool 等官方工具链。

现有 OTA1 自研 82B 头。**借鉴点**：
- 若将来对接标准工具链/多镜像管理，参考其头部字段设计
- 现在保持 OTA1 自研格式（已与上位机/桥包工具链一致），不做破坏性变更

### 2.3 多镜像支持（低优先级，按需）

MCUboot 支持 App + bootloader + 协处理器固件各自独立升级。
现有：单 App 升级。
若以后有 FPGA/协处理器固件（如 RFID 前端），可参考其多镜像管理。

### 2.4 加密签名体系（中优先级）

MCUboot：RSA/ECDSA 签名 + 密钥吊销 + 防回滚。
现有：ota_security 验签（HMAC/签名）。
**借鉴点**：密钥管理流程、防回滚（版本单调递增）设计是否完备。

### 2.5 Zephyr 生态集成（未来选项）

若将来迁 Zephyr，MCUboot 是标配（MCUboot + Zephyr 原生集成）。
届时"借鉴"变"直接采用"。

---

## 3. 结论与行动清单

### 结论

**核心 OTA 机制（双 bank/confirm/回滚/校验）已与工业标准 MCUboot 同构**，
设计思路正确。MCUboot 的借鉴价值集中在**细节补强**而非**整体替换**。

### 行动清单（按优先级）

| 优先级 | 行动 | 对应缺口 |
|---|---|---|
| **P0** | 审查 commit/swap 掉电安全性，必要时加进度记录 | 2.1 |
| **P1** | 检查验签/防回滚（密钥管理、版本单调）完备性 | 2.4 |
| **P2** | 评估镜像头标准化（对接工具链） | 2.2 |
| **P3** | 记录多镜像需求（协处理器固件时启用） | 2.3 |
| **P3** | Zephyr 迁移评估（若立项）→ MCUboot 直接采用 | 2.5 |

---

*文档结束。依据源码：hc32f4a0_boot（bootloader）+ hc32f4a0_app（App OTA 侧）。*
