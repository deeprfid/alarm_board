/* usb_msc_ota.h - USB 虚拟盘符升级检测 (v9.81ci)
 * 启动时检测 FAT 卷的 fw.bin（完整 OTA 包），校验后搬移到 OTA 暂存区并标记升级。
 * 依赖: FatFs (driver_lib/ff.h) + OTA 存储/安全/标志 (app 层)
 */
#ifndef USB_MSC_OTA_H
#define USB_MSC_OTA_H

/* 检测 FAT 卷 fw.bin 并准备升级；0=无升级文件 / 1=升级已就绪(待重启) / <0=错误 */
int usb_msc_ota_check(void);

#endif /* USB_MSC_OTA_H */
