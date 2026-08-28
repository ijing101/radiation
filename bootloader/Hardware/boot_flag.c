/**
 * @file    boot_flag.c
 * @brief   启动标志管理模块
 * @details 使用 Flash 存储启动标志，支持多次位翻转（Flash 写特性）
 */
#include "bootloader.h"

#define BOOT_FLAG_SLOT_SIZE    16          /* 每个槽位 16 字节 */
#define BOOT_FLAG_SLOT_COUNT   64          /* 1KB 页可存 64 个槽位 */
#define BOOT_FLAG_MAGIC        0x5AA5A55A  /* 魔数验证 */

/**
 * @brief  计算启动标志结构体的 CRC32（软件实现）
 * @param  info: 启动标志结构体指针
 * @retval CRC32 值
 */
uint32_t calculate_boot_flag_crc(boot_flag_info_t *info)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t *data = (uint8_t *)info;
    uint16_t i;
    uint8_t j;

    /* 只计算前 12 字节（不含 crc32 字段本身） */
    for (i = 0; i < 12; i++) {
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
 * @brief  从 Flash 读取启动标志（固定地址）
 * @retval 启动标志值；0xFFFFFFFF 表示未初始化
 */
uint32_t read_boot_flag_from_flash(void)
{
    boot_flag_info_t *flag = (boot_flag_info_t *)BOOT_FLAG_SECTOR_ADDR;

    /* 校验魔数 */
    if (flag->magic != BOOT_FLAG_MAGIC) {
        return 0xFFFFFFFF;
    }

    /* 校验 CRC */
    if (calculate_boot_flag_crc(flag) != flag->crc32) {
        return 0xFFFFFFFF;
    }

    return flag->boot_flag;
}

/**
 * @brief  写启动标志到 Flash（固定地址，每次先擦除页）
 * @param  flag: 启动标志值
 * @retval 1=成功, 0=失败
 */
uint8_t write_boot_flag_to_flash(uint32_t flag)
{
    boot_flag_info_t info;
    boot_flag_info_t *verify;
    uint32_t *src;
    int i;

    /* 组装数据（boot_count 暂未使用） */
    info.magic = BOOT_FLAG_MAGIC;
    info.boot_flag = flag;
    info.boot_count = 0;
    info.crc32 = calculate_boot_flag_crc(&info);

    fmc_unlock();

    /* 擦除启动标志页 */
    if (fmc_page_erase(BOOT_FLAG_SECTOR_ADDR) != FMC_READY) {
        fmc_lock();
        return 0;
    }

    /* 以字为单位写入（4 字 = 16 字节） */
    src = (uint32_t *)&info;
    for (i = 0; i < 4; i++) {
        if (fmc_word_program(BOOT_FLAG_SECTOR_ADDR + i * 4, src[i]) != FMC_READY) {
            fmc_lock();
            return 0;
        }
    }

    fmc_lock();

    /* 回读校验 */
    verify = (boot_flag_info_t *)BOOT_FLAG_SECTOR_ADDR;
    if (verify->magic != info.magic ||
        verify->boot_flag != info.boot_flag ||
        verify->crc32 != info.crc32) {
        return 0;
    }

    return 1;
}
