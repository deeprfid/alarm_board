# ota/f4a0/ —— F4A0 上位机侧的 OTA 代码（抢救存档）

## 为什么在这里

这些文件原本只存在于 `J:\dsh\alarm_board\HC32F4A0_OTA\`（为便于我写入而复制的 F4A0 工程副本）。
该副本 910MB / 18346 文件，**真身仓库是 `J:\dsh\HC32F4A020260320_RYK`（远端 `uhf-hc32f4a0.git`）**，
而副本里的这几份**真身仓库里完全没有**。

副本一旦删除且未推送，这份工作就永久丢失 —— 故抢救到这里（共 ~24KB）。

## 文件

| 文件 | 是什么 | 去向 |
| --- | --- | --- |
| `ota_host.c/h` | **上位机端 OTA1 发送器**：包头握手 / 分块 / ACK 跟随 / 探测 / 卡死保护。移植自 C# `OtaUpdater.cs` 的串口分支 + `OtaProtocol.cs` | F4A0 工程 `hc32f4a0_app/projects/app/{inc,src}/` |
| `ota_dist.c/h` | **F4A0 集成层**：给 `ota_host` 提供 4 个 IO 回调（`read/write(fd)` + `osKernelGetTickCount()`）+ 分发状态 + **业务帧闸门 `ota_dist_busy()`** | 同上 |
| `gates_and_project.patch` | 两处 `ipc_hpm_message()` 加的业务帧闸门（`alarm.c` / `mqtt_interface.c`）+ `hc32f4a0_app.uvprojx` 的文件注册 | `git apply` 到 F4A0 工程 |

## 与 `ota/` 根目录那份的关系

`ota/` 根下的 `ota_host.c/h` 是**可移植核心**（LF 行尾，原始版本）；
`ota/f4a0/` 这份是**合进 F4A0 工程时的版本**（CRLF 行尾，内容与根目录那份相同）。
两者都不含硬件依赖 —— 只依赖「写字节 / 读字节 / 取毫秒」三个回调。

## 恢复用法（将来要用 F4A0 侧时）

```
# 1) 拷进真身仓库（J:\dsh\HC32F4A020260320_RYK）
copy ota\f4a0\ota_host.*  <真身>\hc32f4a0_app\projects\app\inc|src\
copy ota\f4a0\ota_dist.*  <真身>\hc32f4a0_app\projects\app\inc|src\
# 2) 打闸门补丁
cd <真身> && git apply <此目录>\gates_and_project.patch
```

## 状态

**从未在真机上跑过。** 编译验证过（F4A0 工程 0 Error，唯一警告是既有的 `ota_usb_stream.c(47)`），
但 OTA 整链（下载 / 激活 / 回退 / 槽 B）一次都没跑过。
