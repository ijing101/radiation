/**
 * @file    bootloader.h
 * @brief   Bootloader 公共定义与接口
 * @note    GD32F130F8T6 具有 64KB 主 Flash（页大小 1KB，共 64 页）
 *          和 8KB SRAM，本头文件定义了 Flash 分区表和对外接口
 */
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include "gd32f1x0.h"
#include <string.h>

/**************************************************************************
 * Flash 分区表 (GD32F130F8 64KB Flash，页大小 1KB)
 *
 *  地址范围            分区       大小
 *  0x08000000 ~ 0x08003FFF  BOOT     16KB
 *  0x08004000 ~ 0x08004FFF  启动标志  4KB
 *  0x08005000 ~ 0x0800F7FF  APP      42KB
 *  0x0800F800 ~ 0x0800FBFF  CONFIG   1KB
 *  0x0800FC00 ~ 0x0800FFFF  保留      1KB
 **************************************************************************/
#define FLASH_SECTOR_SIZE           1024          /* Flash 页大小：1KB */
#define FLASH_SECTOR_NUM            64            /* 共 64 页 = 64KB */

#define FLASH_START_ADDR            0x08000000U   /* Flash 起始地址 */
#define FLASH_END_ADDR              (0x08000000U + FLASH_SECTOR_NUM * FLASH_SECTOR_SIZE)

/* Bootloader 区域 */
#define BOOT_SECTOR_ADDR            0x08000000U   /* BOOT 起始地址 */
#define BOOT_SECTOR_SIZE            0x4000U       /* BOOT 大小：16KB */

/* 启动标志区域（前置，避免与 APP 读写冲突） */
#define BOOT_FLAG_SECTOR_ADDR       0x08004000U   /* 启动标志起始地址 */
#define BOOT_FLAG_SECTOR_SIZE       0x1000U       /* 启动标志大小：4KB（实际只使用第 1 页 1KB） */

/* APP 区域 */
#define APP_SECTOR_ADDR             0x08005000U   /* APP 起始地址 */
#define APP_SECTOR_SIZE             0xA800U       /* APP 大小：42KB */

/* CONFIG 区域 */
#define CONFIG_SECTOR_ADDR          0x0800F800U   /* CONFIG 起始地址 */
#define CONFIG_SECTOR_SIZE          0x0400U       /* CONFIG 大小：1KB */

/* 保留区域（安全边界，防止越界写） */
#define RESERVE_SECTOR_ADDR         0x0800FC00U   /* 保留区起始地址 */
#define RESERVE_SECTOR_SIZE         0x0400U       /* 保留区大小：1KB */

/* 安全边界：APP 写入不允许超过此地址 */
#define APP_WRITE_MAX_ADDR          CONFIG_SECTOR_ADDR

/* APP 区域需要擦除的页数（42KB = 42 页） */
#define APP_ERASE_SECTORS           (APP_SECTOR_SIZE / FLASH_SECTOR_SIZE)

/* 擦除页数上限保护 */
#define APP_ERASE_SECTORS_MAX       APP_ERASE_SECTORS

/* 处理状态枚举 */
typedef enum
{
    NONE,
    WAIT_START_PROGRAM,      /* 等待启动应用程序 */
    START_PROGRAM,           /* 启动应用程序 */
    UPDATE_PROGRAM,          /* 升级应用程序 */
    UPDATE_SUCCESS,          /* 升级成功 */
    BUSY,                    /* 忙 */
} process_status;

/* 跳转函数指针类型 */
typedef void (*jump_callback)(void);

/* 启动标志定义 */
#define BOOT_FLAG_NORMAL        0x55AA5501U    /* 正常启动，快速跳转 */
#define BOOT_FLAG_NEED_UPDATE   0x55AA5502U    /* 需要升级 */
#define BOOT_FLAG_FIRST_BOOT    0x55AA5504U    /* 升级后首次启动，需校验 */

/* 启动标志信息结构体（存储于 BOOT_FLAG_SECTOR_ADDR 处） */
typedef struct {
    uint32_t magic;          /* 魔数 0x5AA5A55A */
    uint32_t boot_flag;      /* 启动标志 */
    uint32_t boot_count;     /* 启动计数（暂未使用） */
    uint32_t crc32;          /* 前 12 字节的 CRC32 校验值 */
} boot_flag_info_t;

/* Flash 操作接口 */
uint8_t jump_app(uint32_t appAddr);                                        /* 跳转到 APP */
void system_reboot(void);                                                  /* 系统复位 */
uint8_t mcu_flash_erase(uint32_t addr, uint8_t sector_num);                /* 擦除 Flash */
uint8_t mcu_flash_write(uint32_t addr, uint8_t *buffer, uint32_t length);  /* 写 Flash */
void mcu_flash_read(uint32_t addr, uint8_t *buffer, uint32_t length);      /* 读 Flash */

/* 启动标志操作接口 */
uint32_t read_boot_flag_from_flash(void);                   /* 读取启动标志 */
uint8_t write_boot_flag_to_flash(uint32_t flag);            /* 写入启动标志 */
uint32_t calculate_boot_flag_crc(boot_flag_info_t *info);   /* 计算启动标志 CRC */

#endif /* __BOOTLOADER_H */
