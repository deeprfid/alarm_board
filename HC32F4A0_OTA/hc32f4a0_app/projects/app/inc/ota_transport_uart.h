/**
 * @file ota_transport_uart.h
 * @brief 本地通道 transport（UART/USB）："OTA1" 帧 + CRC16 + 0x52 断点续传
 * @version V0.2.1  2026-08-14  Phase 3
 *
 * 帧格式（与 doc/ota_interfaces/ota_transport.h 一致，V0.2.1 修订）：
 *   [0:4]  MAGIC "OTA1"   （与统一 OTA 包魔数一致；避让旧 0xEE/0xFF 协议）
 *   [4]    帧类型: 0x50=DATA(上位机→设备) 0x51=ACK(设备→上位机) 0x52=RESUME(设备→上位机)
 *   [5:7]  序号（uint16 LE，DATA 递增）
 *   [7:9]  载荷长度（uint16 LE，≤ OTA_FRAME_MAX_PAYLOAD）
 *   [9:9+N] 载荷（DATA=统一 OTA 包字节流分片；ACK/RESUME=4B LE 偏移）
 *   [末2]  CRC16-CCITT-FALSE（poly 0x1021, init 0xFFFF，覆盖 [0:9+N]）
 *
 * 断点续传：设备收到首帧 DATA 且已有进度 → 回 0x52(已收偏移)，
 * 上位机从该偏移续发；帧 CRC16 错误 → 设备回 0x52(当前偏移) 请求重发。
 */
#ifndef OTA_TRANSPORT_UART_H
#define OTA_TRANSPORT_UART_H

#include <stdint.h>

#define OTA_FRAME_MAGIC0        'O'
#define OTA_FRAME_MAGIC1        'T'
#define OTA_FRAME_MAGIC2        'A'
#define OTA_FRAME_MAGIC3        '1'
#define OTA_FRAME_TYPE_DATA     0x50
#define OTA_FRAME_TYPE_ACK      0x51
#define OTA_FRAME_TYPE_RESUME   0x52
#define OTA_FRAME_HDR_LEN       9      /* magic4 + type1 + seq2 + len2 */
#define OTA_FRAME_CRC_LEN       2
#define OTA_FRAME_MAX_PAYLOAD   4096   /* v9.81h: 2048->4096 串口提速（与 USB 帧对齐） */
#define OTA_FRAME_MAX_LEN       (OTA_FRAME_HDR_LEN + OTA_FRAME_MAX_PAYLOAD + OTA_FRAME_CRC_LEN)

/* 绑定输出 fd（COMMON_INTERFACE_UART0/USB1 等），ACK/RESUME 回发用 */
int  ota_transport_uart_init(int fd);

/* 设备→上位机：ACK(已收偏移) / RESUME(续传偏移)，偏移为统一 OTA 包内绝对字节 */
int  ota_transport_uart_send_ack(int fd, uint32_t offset);
int  ota_transport_uart_send_resume(int fd, uint32_t offset);

/* 上位机→设备：整帧处理（由 UART0 接收循环调用）
 *   frame 指向完整帧（含 magic..CRC16），flen=帧总长
 *   <0: 拒绝（CRC/长度错），0: 已处理，1: 下载完成并触发升级 */
int  ota_transport_uart_handle_frame(const uint8_t *frame, uint16_t flen);

/* 流式输入（USB1/网络等字节流通道）：内部组帧后调用整帧处理；
 *   fd 为回发通道（ACK/RESUME 用），与 UART0 互不干扰 */
int  ota_transport_uart_feed(int fd, const uint8_t *data, uint32_t len);

/* CRC16-CCITT-FALSE */
uint16_t ota_transport_uart_crc16(const uint8_t *buf, uint32_t len);

/* v9.81cm: 会话空闲超时（8s）——上位机中止后释放通道锁，返回 1=已释放 */
int  ota_transport_uart_idle_timeout(void);

#endif /* OTA_TRANSPORT_UART_H */
