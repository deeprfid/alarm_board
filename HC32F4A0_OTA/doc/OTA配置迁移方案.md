# OTA 配置迁移方案：片内 Flash → FlashDB KV（QSPI）

> 日期：2026-08-21 | 状态：设计稿，待确认后实施
> 背景：P0 bug——OTA 双 bank swap 后片内配置区 0xFC000 丢失（见 ISSUES.md 〇）

---

## 一、现状与问题

### 配置区现状（片内 Flash 0xFC000-0xFDFFF，8KB）

| 配置块 | 地址 | 大小 | 内容 |
|---|---|---|---|
| NetConfig | 0xFC000 | 100B | 网络 IP/掩码/网关/MAC |
| WifiConfig | 0xFC080 | ? | WiFi 参数 |
| HwTypeConfig | 0xFC200 | ? | 硬件类型 |
| MonetConfig | 0xFC220 | ? | 移动网络 |
| BluetoothConfig | 0xFC300 | ? | 蓝牙参数 |
| ActiveModeConfig | 0xFC400 | 2048B | 激活模式（重点） |
| WorkModeParams | 0xFCF00 | ? | 工作模式参数 |
| PassiveModeConfig | 0xFD000 | ? | 被动模式配置 |
| BootConfig | 0xFDC00 | ? | 启动配置 |

### 问题本质
- HC32F4A0 dual-bank swap = 整 bank 地址重映射，swap 后 0x0000 段命中物理 Bank B
- boot commit 只写固件，不复制配置区 → Bank B 配置区 0xFF → 升级丢配置

---

## 二、方案选择

| 方案 | 机制 | 优点 | 缺点 | 结论 |
|---|---|---|---|---|
| A：迁 FlashDB KV | 配置存 QSPI fdb_kvdb1 | QSPI 不参与 swap，天然保留；KV 管理科学 | 依赖 QSPI；boot 不能读 | **推荐** |
| B：boot 复制配置区 | commit 时 BankA→B 拷贝 | 保持片内存储 | boot 复杂度↑；每次升级都复制 | 备选 |
| C：双写（片内+KV） | 两处都写 | 兼容过渡 | 双份维护 | 过渡用 |

---

## 三、方案 A 详细设计（FlashDB KV）

### 3.1 KV 键规划

| KV 键 | 内容 | 类型 | 大小 |
|---|---|---|---|
| rdr.netcfg | NetConfig | blob | 100B |
| rdr.wificfg | WifiConfig | blob | ~200B |
| rdr.hwtype | HwTypeConfig | blob | ~50B |
| rdr.monetcfg | MonetConfig | blob | ~200B |
| rdr.btcfg | BluetoothConfig | blob | ~200B |
| rdr.activemode | ActiveModeConfig | blob | 2048B |
| rdr.workmode | WorkModeParams | blob | ~500B |
| rdr.passivemode | PassiveModeConfig | blob | ~500B |
| rdr.bootcfg | BootConfig | blob | ~300B |
| rdr.cfgver | 配置版本/迁移标志 | u32 | 4B |

### 3.2 读写路径改造

```c
/* readercfg.c 替代 flash_bytes_read */
static int cfg_kv_get(const char *key, void *buf, size_t len) {
    struct fdb_blob blob;
    size_t n = fdb_kv_get_blob(&AlarmDB, key, fdb_blob_make(&blob, buf, len));
    return (n == len) ? 0 : -1;
}

/* set_config_to_flash 替代 flash 擦写 */
static int cfg_kv_set(const char *key, const void *buf, size_t len) {
    struct fdb_blob blob;
    return (fdb_kv_set_blob(&AlarmDB, key, fdb_blob_make(&blob, buf, len)) == FDB_NO_ERR) ? 0 : -1;
}
```

### 3.3 一次性迁移逻辑（升级到新固件首次启动）

```c
void rdr_cfg_migrate(void) {
    uint32_t ver = 0;
    if (fdb_kv_get_blob(&AlarmDB, "rdr.cfgver", fdb_blob_make(&blob, &ver, 4)) == 4)
        return;  /* 已迁移过 */
    /* 片内旧配置存在且有效 → 复制到 KV */
    uint8_t buf[2048];
    if (flash_bytes_read(ActiveModeConfig_Addr, buf, sizeof(buf)) == 0 &&
        !is_all_ff(buf, sizeof(buf))) {
        cfg_kv_set("rdr.netcfg", ...);   /* 逐块复制 */
        ...
    }
    cfg_kv_set("rdr.cfgver", &CFG_VER, 4);
}
```

### 3.4 兼容策略

- 新固件：读写全走 KV（fdb_kv_*）
- 片内 0xFC000 保留不写（或只读兜底）
- 迁移函数在 flashdb() 初始化后、set_network_config() 前调用

---

## 四、方案 B 备选（boot 复制配置区）

```c
/* ota_boot.c commit_to_other_bank 追加：复制配置区 */
/* 在步骤 2（App 搬运）后、步骤 3（标志）前 */
for (off = 0; off < 0x2000; off += 1024) {   /* 8KB 配置区 */
    memcpy(buf, (void*)(0xFC000 + off), n);   /* 当前 bank 读 */
    efm_program(BOOT_OTHER_APP_BASE + 0xEC000 + off, buf, n);  /* 另一 bank 写 */
}
```

### 方案 B 注意
- 配置区在另一 bank 的物理地址 = BOOT_OTHER_APP_BASE + (0xFC000-0x10000) = 0x11EC000
- 需 8KB 扇区对齐擦除（另一 bank 配置区）
- boot 复杂度↑，但配置保留在片内（不依赖 QSPI）

---

## 五、建议与决策点

1. **推荐方案 A**（FlashDB KV）：架构更优，OTA 天然保留，与现有 KV 体系统一
2. **决策点 1**：是否保留片内配置区兼容？（建议保留只读，双保险）
3. **决策点 2**：boot 是否需要读配置？（若不需要，方案 A 无冲突）
4. **决策点 3**：配置写入频率？（决定是否需要 KV 缓存/批量写）

---

## 六、实施步骤（方案 A）

1. readercfg.h 增加 KV 键宏 + cfgver 常量
2. readercfg.c：flash_bytes_read/write → cfg_kv_get/set（9 处）
3. 新增 rdr_cfg_migrate()（一次性迁移）+ 挂到 flashdb() 后
4. common.c/custom_ee_commond.c 的网络配置读写同步改
5. 编译 + 真机回归：配置保存/OTA 升级后保留验证

> 预计工作量：1-2 天（不含回归）

## 相关文档
- ISSUES.md 〇（P0 bug 记录）
- doc/工程体检与优化建议.md（RAM/架构背景）
- doc/PATCHES.md（DDL 补丁清单）