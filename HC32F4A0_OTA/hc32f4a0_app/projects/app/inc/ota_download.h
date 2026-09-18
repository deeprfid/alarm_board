/**
 * @file ota_download.h
 * @brief 下载 Agent：HTTP GET + Range 断点续传 → QSPI 暂存区（F4A0 首个实现）
 * @version V0.2  2026-08-14  Phase 3
 */
#ifndef OTA_DOWNLOAD_H
#define OTA_DOWNLOAD_H

#include <stdint.h>

/* 下载固件包到 QSPI 暂存区（断点续传：从 ota_get_progress() 续传）
 * url: http(s)://host[:port]/path/file.ota1
 * 返回 0=成功（暂存区含完整包，可验签/commit） */
int ota_download_start(const char *url);

#endif /* OTA_DOWNLOAD_H */
