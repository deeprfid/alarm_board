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

#endif /* OTA_FLASH_H */
