usb_new_winusb.inf 使用说明（F460 扫描盘 WinUSB 通道）
=========================================================

0. 先确认：多数情况根本不需要装驱动
-----------------------------------
设备端已实现微软 OS 描述符：
  字符串描述符 0xEE = "MSFT100"，Vendor 请求 wIndex=0x04 返回 Compatible ID "WINUSB"，
  wIndex=0x05 返回 DeviceInterfaceGUID {13EB360B-BC1E-46CB-AC8B-EF3DA47B4062}。
Windows 8 以上会自动把该接口绑到**内置** winusb.sys（免 INF、免签名）。

先用这条命令确认（不需要装任何 Python 包）：
    python tools\usb_winusb_test.py --list
  * 打印出 vid_2e88&pid_4608 + interface class=0xFF + bulk 0x82/0x03  → 免驱已生效，收数即可：
    python tools\usb_winusb_test.py --read
  * 打印 "no USB device matched ..." 或 "not bound to WinUSB"           → 继续往下看

1. 先清掉 Windows 的缓存（最常见原因）
-------------------------------------
设备管理器 → 找到该设备（可能在"其他设备"或"通用串行总线设备"下）→
  右键"卸载设备"（**勾选"删除此设备的驱动程序"**）→ 拔掉 USB → 重新插入。
（Windows 会按 VID/PID 缓存上次的绑定结果，F4A0 那边就遇到过 4607 缓存失败、换 4608 才好。）

2. 首选方案：Zadig（不用处理签名）
---------------------------------
下载 Zadig（https://zadig.akeo.ie/）→ Options 勾选 "List All Devices" →
下拉框选 "F460 Scanner (WinUSB)" 或 VID 2E88 / PID 4608 的接口 →
目标驱动选 **WinUSB** → "Replace Driver"/"Install Driver"。
装完再跑 --list 验证。Zadig 用的是系统内置 winusb.sys，通常不需要关签名强制。

3. 兜底方案：用本 INF 手动安装（需要一份签名后的目录文件）
---------------------------------------------------------
本 INF 只引用系统内置 winusb.sys，但 x64/arm64 上**第三方 INF 必须带签名**才能安装，
否则会报"第三方 INF 不包含数字签名信息"（错误 0xE000024B）。两种做法：

(1) 自签名（需要管理员 + WDK 的 Inf2Cat/signtool，或 PowerShell 自带命令）
    :: 生成证书（管理员 PowerShell）
    New-SelfSignedCertificate -Type Custom -Subject "CN=HDSC Test" `
        -KeyUsage DigitalSignature -CertStoreLocation "Cert:CurrentUserMy" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
    :: 生成目录文件（需 WDK 的 Inf2Cat）
    Inf2Cat /driver:"<本目录>" /os:10_X64
    :: 签名
    signtool sign /fd sha256 /a /n "HDSC Test" /tr http://timestamp.digicert.com /td sha256 usb_new_winusb.cat
    :: 把证书装到"受信任的根证书颁发机构"和"受信任的发布者"（本机，管理员）后：
    pnputil /add-driver usb_new_winusb.inf /install
    或 设备管理器 → 更新驱动程序 → 浏览我的电脑 → 选择本目录。

(2) 关闭签名强制（仅实验室机器，需要关 Secure Boot）
    bcdedit /set testsigning on     然后重启
    之后可直接用 INF 安装。

4. C# 上位机怎么找设备
----------------------
- 免驱或 INF 两条路都注册了同一个 DeviceInterfaceGUID：
    {13EB360B-BC1E-46CB-AC8B-EF3DA47B4062}
  用 SetupDiGetClassDevs(GUID_DEVINTERFACE_...) 枚举，或按 VID/PID 打开：
    VID 0x2E88 / PID 0x4608（含 WinUSB 的组合）
  现有 OtaUpdater/WinUsbChannel 就是按 0x2E88/0x4608 打开的，与设备端 PID 规则一致。
- HID / CDC / 键盘组合（PID 0x4605）走的是系统 HID/CDC 类驱动，不需要本 INF；
  老上位机按 0x2E88/0x4605 打开 HID 接口即可。

5. 排查小抄
-----------
- 设备管理器里是"未知设备" → 免驱没生效：先做第 1 步，再考虑 Zadig / INF。
- 设备管理器正常但 --list 打不开 → 多半是绑到了别的驱动（如 usbser.sys）：
  用 Zadig 把该接口改成 WinUSB。
- 换过 PID 之后死活认不到 → 一定是缓存：卸载设备(含驱动) + 拔插，必要时
  pnputil /enum-drivers 找到对应的 oemNN.inf 再 pnputil /delete-driver oemNN.inf /uninstall。

6. 一键脚本（补充安装：不用手工做签名那套）
------------------------------------------
本目录已带好脚本，直接双击 .cmd 即可（会自己申请管理员权限）：

  install_winusb.cmd            生成自签名证书 -> 信任证书 -> 生成并签名 .cat -> pnputil 安装 -> 打印设备状态
  install_winusb.cmd -NoTimestamp          离线机器用（不联网打时间戳）
  install_winusb.cmd -UseTestSigning       先开启 bcdedit testsigning（需关 Secure Boot，重启后再装）
  uninstall_winusb.cmd          卸载驱动包（pnputil /delete-driver）并移除测试证书
  clear_usb_cache.cmd           清掉 USB\VID_2E88&PID_4608 的设备节点（解决“缓存了错误绑定”）

手工等价命令（想自己一步步做时）：
    powershell -ExecutionPolicy Bypass -File install_winusb.ps1
    powershell -ExecutionPolicy Bypass -File install_winusb.ps1 -UseTestSigning
    powershell -ExecutionPolicy Bypass -File uninstall_winusb.ps1 -KeepCertificate
    powershell -ExecutionPolicy Bypass -File clear_usb_cache.ps1 -RemoveDevice

脚本做了什么（可审计，全部是系统自带命令）：
  New-SelfSignedCertificate / Export-Certificate / Import-Certificate  -> 建证书并放进
       LocalMachine\Root 与 LocalMachine\TrustedPublisher
  Inf2Cat（若装了 WDK）或 New-FileCatalog                              -> 生成 usb_new_winusb.cat
  Set-AuthenticodeSignature（SHA256，可加 DigiCert 时间戳）            -> 给 .cat 签名
  pnputil /add-driver usb_new_winusb.inf /install                      -> 安装驱动包
  （脚本自己不写注册表、不动设备，只用上述官方命令）

注意：
- 只在这台测试机上装；证书是自签的，别拷到别的机器装（会被当成不受信任的根）。
- 未提权运行只会打印 “[X] must run elevated” 并退出，不改动系统（已实测）。
- 装完仍打不开，优先回去用 Zadig 把该接口改成 WinUSB（第 2 节），它不涉及签名。
