#ifndef __CRC_CHECK_H
#define __CRC_CHECK_H

#include "stm32f10x.h"

// CRC16校验常量定义
#define CRC16_CCITT_POLY    0x1021
#define CRC16_CCITT_INIT    0x0000

// 应用程序CRC存储地址：应用末尾4字节
#define APP_CRC_ADDR        (APP_SECTOR_ADDR + APP_SECTOR_SIZE - 4)
#define APP_CRC_SIZE        4

// ????????
uint16_t crc16_ccitt(const uint8_t *data, uint16_t length);
uint16_t crc16_update(uint16_t crc, uint8_t data);
uint32_t calculate_app_crc32(void);
uint8_t verify_app_integrity(void);
void store_app_crc(void);
uint8_t verify_ymodem_packet(const uint8_t *packet, uint16_t data_len);

#endif /* __CRC_CHECK_H */
