/**
 * @file    crc_check.c
 * @brief   CRC 校验实现（纯软件）
 * @details 提供 CRC16-CCITT（Ymodem 协议）与 CRC32（APP 完整性）计算
 */
#include "crc_check.h"
#include "bootloader.h"

/**
 * @brief  计算 CRC16-CCITT（XModem/YModem 标准，初值 0x0000）
 * @param  data: 数据指针
 * @param  length: 数据长度
 * @retval CRC16 值
 */
uint16_t crc16_ccitt(const uint8_t *data, uint16_t length)
{
    uint16_t crc = CRC16_CCITT_INIT;
    uint16_t i;
    uint8_t j;

    for (i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_POLY);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  增量更新 CRC16（逐字节）
 * @param  crc: 当前 CRC 值
 * @param  data: 新增数据字节
 * @retval 更新后的 CRC16 值
 */
uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    uint8_t i;

    crc ^= (uint16_t)data << 8;
    for (i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_POLY);
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief  校验 Ymodem 数据包的 CRC
 * @param  packet: 数据包指针（SOH/STX + 序号 + 序号取反 + 数据 + CRC 高 + CRC 低）
 * @param  data_len: 数据长度（128 或 1024）
 * @retval 1=校验通过, 0=校验失败
 */
uint8_t verify_ymodem_packet(const uint8_t *packet, uint16_t data_len)
{
    uint16_t received_crc, calculated_crc;

    if (packet == NULL || data_len == 0) {
        return 0;
    }

    /* 读取接收到的 CRC（高字节在前，位于数据之后） */
    received_crc = (uint16_t)((packet[data_len + 3] << 8) | packet[data_len + 4]);

    /* 计算数据部分 CRC（跳过帧头 3 字节：SOH/STX + 序号 + 序号取反） */
    calculated_crc = crc16_ccitt(&packet[3], data_len);

    return (received_crc == calculated_crc) ? 1 : 0;
}

/**
 * @brief  计算标准 CRC32（多项式 0xEDB88320，初值 0xFFFFFFFF，结果取反）
 * @param  data: 数据指针
 * @param  length: 数据长度
 * @retval CRC32 值
 */
uint32_t crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i;
    uint8_t j;

    for (i = 0; i < length; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

/**
 * @brief  计算 APP 区域 CRC32（不含末尾 4 字节 CRC 存储区）
 * @retval CRC32 值
 */
uint32_t calculate_app_crc32(void)
{
    return crc32((const uint8_t *)APP_SECTOR_ADDR, APP_SECTOR_SIZE - APP_CRC_SIZE);
}

/**
 * @brief  将 APP CRC 写入 Flash 尾部（APP 区域最后 4 字节）
 */
void store_app_crc(void)
{
    uint32_t app_crc = calculate_app_crc32();
    mcu_flash_write(APP_CRC_ADDR, (uint8_t *)&app_crc, APP_CRC_SIZE);
}

/**
 * @brief  校验 APP 完整性
 * @retval 1=通过, 0=失败
 * @note   若 CRC 区域为空（0xFFFFFFFF）说明是直接烧录，跳过校验
 */
uint8_t verify_app_integrity(void)
{
    uint32_t stored_crc = *(volatile uint32_t *)APP_CRC_ADDR;

    /* 未存储 CRC（直接烧录），跳过校验 */
    if (stored_crc == 0xFFFFFFFF) {
        return 1;
    }

    return (calculate_app_crc32() == stored_crc) ? 1 : 0;
}
