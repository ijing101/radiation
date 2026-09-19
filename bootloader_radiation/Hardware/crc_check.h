/** @file crc_check.h */
#ifndef __CRC_CHECK_H
#define __CRC_CHECK_H

#include "gd32f1x0.h"

#define CRC16_CCITT_POLY 0x1021U
#define CRC16_CCITT_INIT 0x0000U

uint16_t crc16_ccitt(const uint8_t *data, uint16_t length);
uint16_t crc16_update(uint16_t crc, uint8_t data);
uint8_t verify_ymodem_packet(const uint8_t *packet, uint16_t data_len);
uint32_t boot_image_crc32(uint32_t image_addr, uint32_t image_size);
uint8_t boot_image_is_valid(uint32_t image_addr, uint32_t image_size,
                            uint32_t image_crc32);
uint8_t boot_backup_is_valid(uint32_t image_size, uint32_t image_crc32);

#endif /* __CRC_CHECK_H */
