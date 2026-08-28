/**
 * @file    boot_flag.c
 * @brief   启动标志管理模块
 * @details 使用Flash存储启动标志，支持多槽位轮换减少Flash擦写次数
 *          槽位设计：1KB可存储64个标志结构体(每个16字节)
 *          寿命提升：10万次擦除 × 64 = 640万次写入
 */

#include "bootloader.h"
#include "usart.h"
#include <string.h>

// 槽位配置
#define BOOT_FLAG_SLOT_SIZE    16          // 每个槽位16字节
#define BOOT_FLAG_SLOT_COUNT   64          // 1KB可容纳64个槽位
#define BOOT_FLAG_MAGIC        0x5AA5A55A  // 魔数验证

/**
 * @brief  计算启动标志结构体的CRC32（软件实现）
 * @param  info: 启动标志结构体指针
 * @retval CRC32值
 */
uint32_t calculate_boot_flag_crc(boot_flag_info_t *info)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t *data = (uint8_t*)info;//
    uint16_t i, j;
    
    // 只计算前12字节（不包括crc32字段本身）
    for (i = 0; i < 12; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

/**
 * @brief  从Flash读取启动标志（简化版：固定地址）
 * @retval 启动标志值，0xFFFFFFFF表示未初始化
 */
uint32_t read_boot_flag_from_flash(void)
{
    boot_flag_info_t *flag = (boot_flag_info_t*)BOOT_FLAG_SECTOR_ADDR;
    uint32_t calculated_crc;
    
    // 检查magic
    if (flag->magic != BOOT_FLAG_MAGIC) {
        uart_log("Boot flag uninitialized (magic: 0x%08X)\r\n", flag->magic);
        return 0xFFFFFFFF;
    }
    
    // 验证CRC
    calculated_crc = calculate_boot_flag_crc(flag);
    if (calculated_crc != flag->crc32) {
        uart_log("Boot flag CRC failed (calc: 0x%08X, stored: 0x%08X)\r\n", 
                 calculated_crc, flag->crc32);
        return 0xFFFFFFFF;
    }
    
    uart_log("Boot flag: 0x%08X\r\n", flag->boot_flag);
    return flag->boot_flag;
}

/**
 * @brief  写入启动标志到Flash（简化版：固定地址+每次擦除）
 * @param  flag: 启动标志值
 * @retval 1=成功, 0=失败
 */
uint8_t write_boot_flag_to_flash(uint32_t flag)
{
    FLASH_Status status;
    boot_flag_info_t info;
    boot_flag_info_t *verify;
    uint32_t *src;
    int i;
    
    // 构建新数据（去掉boot_count）
    info.magic = BOOT_FLAG_MAGIC;
    info.boot_flag = flag;
    info.boot_count = 0;  // 不再使用
    info.crc32 = calculate_boot_flag_crc(&info);
    
    uart_log("Writing boot flag 0x%08X to 0x%08X\r\n", flag, BOOT_FLAG_SECTOR_ADDR);
    
    FLASH_Unlock();
    
    // 每次都擦除整页
    status = FLASH_ErasePage(BOOT_FLAG_SECTOR_ADDR);
    if (status != FLASH_COMPLETE) {
        uart_log("Erase boot flag page failed: %d\r\n", status);
        FLASH_Lock();
        return 0;
    }
    
    // 写入到固定地址（页起始地址）
    src = (uint32_t*)&info;
    for (i = 0; i < 4; i++) {
        status = FLASH_ProgramWord(BOOT_FLAG_SECTOR_ADDR + i * 4, src[i]);
        if (status != FLASH_COMPLETE) {
            uart_log("Flash write failed at offset %d: %d\r\n", i * 4, status);
            FLASH_Lock();
            return 0;
        }
    }
    
    FLASH_Lock();
    
    // 回读验证
    verify = (boot_flag_info_t*)BOOT_FLAG_SECTOR_ADDR;
    if (verify->magic != info.magic || 
        verify->boot_flag != info.boot_flag ||
        verify->crc32 != info.crc32) {
        uart_log("Verify failed! magic=0x%08X, flag=0x%08X, crc=0x%08X\r\n",
                 verify->magic, verify->boot_flag, verify->crc32);
        return 0;
    }
    
    uart_log("Boot flag written and verified successfully\r\n");
    return 1;
}
