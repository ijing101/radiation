#include "crc_check.h"
#include "bootloader.h"
#include "usart.h"
#include <stdio.h>

/**
 * @brief  计算CRC16-CCITT校验值
 * @param  data: 数据指针
 * @param  length: 数据长度
 * @retval CRC16校验值
 */
uint16_t crc16_ccitt(const uint8_t *data, uint16_t length)
{
    uint16_t crc = CRC16_CCITT_INIT;
    uint16_t i, j;
    
    for (i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_CCITT_POLY;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief  增量更新CRC16数值
 * @param  crc: 当前CRC值
 * @param  data: 新增数据字节?
 * @retval 更新后的CRC值
 */
uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    uint8_t i;
    crc ^= (uint16_t)data << 8;
    for (i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ CRC16_CCITT_POLY;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief  校验Ymodem数据包的CRC
 * @param  packet: 数据包指针
 * @param  data_len: 数据长度(128或1024)
 * @retval 1: 校验通过, 0: 校验失败
 */
uint8_t verify_ymodem_packet(const uint8_t *packet, uint16_t data_len)
{
    uint16_t received_crc, calculated_crc;

    /* 安全检查：确保不会越界访问 */
    if (packet == NULL || data_len == 0) {
        uart_log("CRC Check: Invalid parameters\r\n");
        return 0;
    }

    /* 读取接收到的CRC（高字节在前） */
    received_crc = ((uint16_t)packet[data_len + 3] << 8) | packet[data_len + 4];

    /* 计算数据部分CRC（跳过包头3字节：SOH/STX + 包号 + 包号取反） */
    calculated_crc = crc16_ccitt(&packet[3], data_len);

    uart_log("CRC Check: Received=0x%04X, Calculated=0x%04X\r\n", received_crc, calculated_crc);

    if (received_crc == calculated_crc) {
        uart_log("CRC verification PASSED\r\n");
        return 1;
    } else {
        uart_log("CRC verification FAILED\r\n");
        return 0;
    }
}

/**
 * @brief  使用STM32硬件CRC模块计算应用程序CRC32
 * @param  None
 * @retval 计算得到的CRC32值
 */
uint32_t calculate_app_crc32(void)
{
    uint32_t crc_value;
    uint32_t *app_data = (uint32_t*)APP_SECTOR_ADDR;
    uint32_t word_count = (APP_SECTOR_SIZE - APP_CRC_SIZE) / 4;  // 不含CRC尾部的字数
    
    // ???CRC???
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
    
    // ????CRC??????
    CRC_ResetDR();
    
    // ????CRC32
    crc_value = CRC_CalcBlockCRC(app_data, word_count);
    
    uart_log("App CRC32: 0x%08X (Words: %d)\r\n", crc_value, word_count);
    
    return crc_value;
}

/**
 * @brief  将计算得到的CRC写入Flash尾部
 * @param  None
 * @retval None
 */
void store_app_crc(void)
{
    uint32_t app_crc = calculate_app_crc32();
    
    uart_log("Storing App CRC: 0x%08X at 0x%08X\r\n", app_crc, APP_CRC_ADDR);
    
    // ????CRC??Flash???
    mcu_flash_write(APP_CRC_ADDR, (uint8_t*)&app_crc, APP_CRC_SIZE);
    
    uart_log("App CRC stored successfully\r\n");
}

/**
 * @brief  校验应用完整性
 * @param  None
 * @retval 1: 通过, 0: 失败
 * @note   如果CRC区域为空(0xFFFFFFFF)，说明是直接烧录的，跳过校验
 */
uint8_t verify_app_integrity(void)
{
    uint32_t stored_crc, calculated_crc;
    char buf[64];
    
    // 读取存储的CRC
    stored_crc = *(uint32_t*)APP_CRC_ADDR;
    
    // 如果CRC区域为空，说明是直接烧录的（非IAP升级），跳过校验
    if (stored_crc == 0xFFFFFFFF) {
        uart_log("No CRC stored (direct flash), skipping integrity check\r\n");
        Usart2_SendString("No CRC stored\n");
        return 1;  // 直接烧录时跳过校验
    }
    
    // IAP升级的情况，进行CRC校验
    calculated_crc = calculate_app_crc32();
    
    uart_log("App Integrity Check: Stored=0x%08X, Calculated=0x%08X\r\n", 
             stored_crc, calculated_crc);
    
    // 通过Usart2输出CRC对比信息
    sprintf(buf, "Stored CRC: 0x%08X\n", stored_crc);
    Usart2_SendString(buf);
    sprintf(buf, "Calculated CRC: 0x%08X\n", calculated_crc);
    Usart2_SendString(buf);
    
    if (stored_crc == calculated_crc) {
        uart_log("App integrity verification PASSED\r\n");
        Usart2_SendString("CRC verification PASSED\n");
        return 1;
    } else {
        uart_log("App integrity verification FAILED\r\n");
        Usart2_SendString("CRC verification FAILED\n");
        return 0;
    }
}
