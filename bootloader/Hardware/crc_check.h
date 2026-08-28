/**
 * @file    crc_check.h
 * @brief   CRC 校验接口
 * @details 提供 CRC16-CCITT（Ymodem 协议校验）与 CRC32（APP 完整性校验）
 */
#ifndef __CRC_CHECK_H
#define __CRC_CHECK_H

#include "gd32f1x0.h"
#include "bootloader.h"

/* CRC16-CCITT 多项式与初值（Ymodem/Xmodem 标准） */
#define CRC16_CCITT_POLY    0x1021U
#define CRC16_CCITT_INIT    0x0000U

/* 应用程序 CRC 存储地址：APP 区域末尾 4 字节 */
#define APP_CRC_ADDR        (APP_SECTOR_ADDR + APP_SECTOR_SIZE - 4U)
#define APP_CRC_SIZE        4U

uint16_t crc16_ccitt(const uint8_t *data, uint16_t length);               /* 计算 CRC16-CCITT */
uint16_t crc16_update(uint16_t crc, uint8_t data);                        /* 增量更新 CRC16 */
uint32_t crc32(const uint8_t *data, uint32_t length);                     /* 计算 CRC32（标准） */
uint32_t calculate_app_crc32(void);                                       /* 计算 APP 区域 CRC32 */
uint8_t verify_app_integrity(void);                                       /* 校验 APP 完整性 */
void store_app_crc(void);                                                 /* 存储 APP CRC 到 Flash */
uint8_t verify_ymodem_packet(const uint8_t *packet, uint16_t data_len);   /* 校验 Ymodem 数据包 CRC */

#endif /* __CRC_CHECK_H */
