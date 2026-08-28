/**
 * @file    iap_trigger.c
 * @brief   IAP升级触发功能
 * @details 从APP中触发进入Bootloader升级模式
 */

#include "stm32f10x.h"
#include "iap_trigger.h"
#include "delay.h"
#include "rs485.h"
#include "wdg.h"

// 启动标志区域地址（与Bootloader保持一致）
// 注意：已调整为新的Flash布局，Flag区域前置到0x08004000
#define BOOT_FLAG_SECTOR_ADDR       0x08004000  // 从0x0800FC00调整为0x08004000
#define BOOT_FLAG_MAGIC             0x5AA5A55A
#define BOOT_FLAG_NEED_UPDATE       0x55AA5502

// 启动标志结构体
typedef struct {
    uint32_t magic;
    uint32_t boot_flag;
    uint32_t boot_count;
    uint32_t crc32;
} boot_flag_info_t;

/**
 * @brief  计算CRC32（简化版）
 */
static uint32_t calculate_crc32_simple(uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint16_t i, j;
    
    for (i = 0; i < len; i++) {
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
 * @brief  写入升级标志到Flash（精简版：无串口输出）
 */
static uint8_t write_update_flag(void)
{
    FLASH_Status status;
    boot_flag_info_t info;
    boot_flag_info_t *verify;
    uint32_t *src;
    int i;
    uint32_t timeout;
    
    // 构建数据
    info.magic = BOOT_FLAG_MAGIC;
    info.boot_flag = BOOT_FLAG_NEED_UPDATE;
    info.boot_count = 0;
    info.crc32 = calculate_crc32_simple((uint8_t*)&info, 12);
    
    // 喂狗
    IWDG_ReloadCounter();
    
    // 预防性清理Flash状态
    FLASH_Unlock();
    FLASH_Lock();
    
    // 关闭中断
    __disable_irq();
    
    // 解锁Flash
    FLASH_Unlock();
    
    // 等待Flash就绪
    timeout = 0x10000;
    while(FLASH_GetStatus() != FLASH_COMPLETE && timeout--);
    if (timeout == 0) {
        goto error_exit;
    }
    
    // 擦除整页
    status = FLASH_ErasePage(BOOT_FLAG_SECTOR_ADDR);
    if (status != FLASH_COMPLETE) {
        goto error_exit;
    }
    
    // 等待擦除完成
    timeout = 0x100000;
    while(FLASH_GetStatus() != FLASH_COMPLETE && timeout--);
    if (timeout == 0) {
        goto error_exit;
    }
    
    // 擦除后喂狗（Flash擦除可能耗时较长）
    IWDG_ReloadCounter();
    
    // 写入数据
    src = (uint32_t*)&info;
    for (i = 0; i < 4; i++) {
        status = FLASH_ProgramWord(BOOT_FLAG_SECTOR_ADDR + i * 4, src[i]);
        if (status != FLASH_COMPLETE) {
            goto error_exit;
        }
        
        timeout = 0x10000;
        while(FLASH_GetStatus() != FLASH_COMPLETE && timeout--);
        if (timeout == 0) {
            goto error_exit;
        }
    }
    
    // 验证数据（在中断恢复前，避免被打断）
    verify = (boot_flag_info_t*)BOOT_FLAG_SECTOR_ADDR;
    
    if (verify->magic != info.magic || 
        verify->boot_flag != info.boot_flag || 
        verify->crc32 != info.crc32) {
        // BUG修复：验证失败时重新擦除页，恢复到未初始化状态
        // 避免下次启动时读到错误标志
        FLASH_ErasePage(BOOT_FLAG_SECTOR_ADDR);
        goto error_exit;
    }
    
    // 锁定Flash
    FLASH_Lock();
    
    // 恢复中断
    __enable_irq();
    
    return 1;

error_exit:
    FLASH_Lock();
    __enable_irq();
    return 0;
}

/**
 * @brief  触发进入Bootloader升级模式
 * @retval 不会返回（系统复位）
 * @note   BUG修复：使用真正的硬件复位而非跳转，确保外设状态完全清零
 */
void trigger_iap_update(void)
{
    // 喂狗
    IWDG_ReloadCounter();
    
    // 写入升级标志
    if (!write_update_flag()) {
        // 写入失败，返回正常运行
        return;
    }
    
    // 再次喂狗
    IWDG_ReloadCounter();
    
    // 短暂延时，确保Flash写入完全稳定
    delay_ms(10);
    
    // 禁用中断，准备复位
    __disable_irq();
    
    // BUG修复：使用真正的硬件复位（NVIC_SystemReset）
    // 而不是软跳转（boot_soft_reset_do），确保：
    // 1. 所有外设状态完全清零（定时器、串口、DMA等）
    // 2. RAM数据完全清除
    // 3. 时钟配置恢复到复位状态
    // 4. 看门狗状态重新初始化
    // 5. Bootloader在干净环境中启动
    NVIC_SystemReset();
    
    // 永远不会执行到这里
    while(1);
}
