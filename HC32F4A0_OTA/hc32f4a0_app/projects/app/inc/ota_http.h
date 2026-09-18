/**
 * @file ota_http.h
 * @brief HTTP OTA 独立通道（v9.81j）：设备 HTTP 服务器，PC POST 固件包
 */
#ifndef OTA_HTTP_H
#define OTA_HTTP_H

void http_handle_conn(int fd);   /* v1.0: HTTP OTA conn handler (called by ota_dispatch_task in ota_integration.c) */

#endif /* OTA_HTTP_H */
