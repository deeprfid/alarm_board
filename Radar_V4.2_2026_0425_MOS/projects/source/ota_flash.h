/**
 * @file  ota_flash.h
 * @brief 片内 Flash 擦写（OTA 用）—— 擦/写函数【必须 RAM 驻留】
 *
 * 为什么必须 RAM 驻留：EFM 擦/写期间 Flash 总线取指不可靠，执行代码须来自 RAM。
 * 本工程用 DDL 自带的 __RAM_FUNC（hc32_ll_def.h: __attribute__((section("RAMCODE")))），
 * 两个 scatter（HC32F460xC.sct / HC32F460xE.sct）的 RW_IRAM2 均已含 .ANY (RAMCODE)。
 */
#ifndef OTA_FLASH_H
#define OTA_FLASH_H

#include <stdint.h>
#include "ota_layout.h"

/* IEEE CRC32（poly 0xEDB88320, init/xorout 0xFFFFFFFF）—— 与 tools/ota_pack.py 的 zlib.crc32 一致 */
uint32_t ota_crc32(const uint8_t *buf, uint32_t len);
/* 直接对 Flash 内容算 CRC32（不落 RAM 缓冲） */
uint32_t ota_crc32_flash(uint32_t addr, uint32_t len);

/* 按扇区擦除；addr 须扇区对齐，size 向上取整到扇区。返回 0 成功 */
int32_t ota_flash_erase(uint32_t addr, uint32_t size);
/* 写入；addr 须 4 字节对齐。返回 0 成功 */
int32_t ota_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len);
/* 读出（仅 memcpy，不碰 EFM） */
void    ota_flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
/* 回读比较；返回 0 一致 */
int32_t ota_flash_verify(uint32_t addr, const uint8_t *buf, uint32_t len);

/* ---- 选择器标志（双份 + CRC32，取 seq 最大且 CRC 有效的一份） ---- */
/* 读出合并后的标志；返回 0 成功（找到有效份），-1 = 两份都无效 */
int32_t ota_flag_read(ota_flag_t *out);
/* 写入（两份都写，seq 递增）；返回 0 成功 */
int32_t ota_flag_write(const ota_flag_t *in);
/* 当前运行槽；标志无效时回退 OTA_SLOT_A */
uint32_t ota_flag_active_slot(void);

/* ---- 槽镜像头 ---- */
/* 校验某槽：槽尾元数据 magic / 长度 / TargetSlot / App 二进制 CRC32。返回 0 有效 */
int32_t ota_img_check(uint32_t slot);
/* 读槽尾元数据字段（不做校验） */
void    ota_img_read_hdr(uint32_t slot, uint32_t *version, uint32_t *img_len, uint32_t *crc32, uint8_t *slot_field);
/* 写槽尾元数据（CRC32 由 Flash 实际内容算出）；先由 App 写完二进制再调用。返回 0 成功 */
int32_t ota_img_write_trailer(uint32_t slot, uint32_t img_len, uint32_t version);

/* ===== OTA 总开关 =====
 * 0 = 关闭（默认）：App 启动不调用 ota_app_boot_confirm()，即不擦写标志扇区。
 *     此时 Boot 恒走兜底路径（按 A->B 扫第一个可用槽）-> 恒跳槽 A。
 *     用途：先单独验证「Boot 能否跳到 App」，把 OTA 这条链路的变量整体排除。
 * 1 = 打开：完整 OTA（App 自检后写标志把本槽置 RUNNABLE）。 */
#ifndef OTA_APP_ENABLE
#define OTA_APP_ENABLE   1
#endif

/* App 自检确认：把本槽置 RUNNABLE、清 NEED_CONFIRM、boot_count 归零。slot 传本槽号。
 *
 * 【为什么放在本文件而不是单独的 ota_app.c】
 *   它内部会擦写 Flash，所以【必须与 ota_flash.o 一起放进 scatter 的 RW_RAMCODE 执行区】。
 *   若单独成文件而该文件被链接器判为未使用（例如 OTA 关闭时），scatter 里的 ota_app.o 选择器就会落空，
 *   将来重开 OTA 时一旦忘了把它加回 RAMCODE，就会重现「擦写期间取指失败把 CPU 卡死」。
 *   并入本文件后，ota_flash.o 因 ota_recv 用到 ota_flag_read/ota_img_check 而【恒被链接】，不再有这个隐患。 */
int32_t ota_app_boot_confirm(uint32_t slot);

#endif /* OTA_FLASH_H */
