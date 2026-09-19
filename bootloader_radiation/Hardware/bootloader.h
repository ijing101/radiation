/**
 * @file bootloader.h
 * @brief GD32F130F8P6 安全 IAP 的公共定义。
 *
 * 64KB Flash 被划分为 Active/Backup 两个等大的 APP 槽。升级前必须先把
 * 已确认的 Active 镜像复制到 Backup；因此本方案不支持超过
 * APP_IMAGE_MAX_SIZE 的应用程序。
 */
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include "gd32f1x0.h"
#include <string.h>

#define FLASH_SECTOR_SIZE           1024U
#define FLASH_SECTOR_NUM            64U
#define FLASH_START_ADDR            0x08000000U
#define FLASH_END_ADDR              0x08010000U

/* GD32F130F8P6: 8KB SRAM。跳转前必须拒绝这个范围外的 MSP。 */
#define SRAM_START_ADDR             0x20000000U
#define SRAM_END_ADDR               0x20002000U

/*
 * 0x08000000 - 0x08003FFF : Bootloader               16KB
 * 0x08004000 - 0x080043FF : metadata A                1KB
 * 0x08004400 - 0x080047FF : metadata B                1KB
 * 0x08004800 - 0x08004FFF : metadata reserve          2KB
 * 0x08005000 - 0x0800A3FF : Active APP                21KB
 * 0x0800A400 - 0x0800F7FF : Backup APP                21KB
 * 0x0800F800 - 0x0800FBFF : application configuration 1KB
 * 0x0800FC00 - 0x0800FFFF : reserve                   1KB
 */
#define BOOT_SECTOR_ADDR            0x08000000U
#define BOOT_SECTOR_SIZE            0x4000U
#define BOOT_FLAG_SECTOR_ADDR       0x08004000U
#define BOOT_FLAG_SECTOR_SIZE       0x1000U
#define BOOT_METADATA_PAGE_A        0x08004000U
#define BOOT_METADATA_PAGE_B        0x08004400U
#define BOOT_METADATA_PAGE_SIZE     0x0400U

#define APP_SECTOR_ADDR             0x08005000U
#define APP_SLOT_SIZE               0x5400U
#define APP_SECTOR_SIZE             APP_SLOT_SIZE
#define APP_BACKUP_ADDR             0x0800A400U
/* 保留槽尾 4 字节，兼容原工程的镜像容量约束。 */
#define APP_IMAGE_MAX_SIZE          (APP_SLOT_SIZE - 4U)
#define APP_ERASE_SECTORS           (APP_SLOT_SIZE / FLASH_SECTOR_SIZE)
#define APP_WRITE_MAX_ADDR          (APP_SECTOR_ADDR + APP_SLOT_SIZE)

/* Bootloader 与上位机 YMODEM 工具统一使用 USART0 的 9600-8-N-1。 */
#define BOOT_USART_BAUD             9600U

#define CONFIG_SECTOR_ADDR          0x0800F800U
#define CONFIG_SECTOR_SIZE          0x0400U
#define RESERVE_SECTOR_ADDR         0x0800FC00U
#define RESERVE_SECTOR_SIZE         0x0400U

typedef enum
{
    NONE,
    WAIT_START_PROGRAM,
    START_PROGRAM,
    UPDATE_PROGRAM,
    UPDATE_SUCCESS,
    BUSY,
} process_status;

typedef void (*jump_callback)(void);

#define BOOT_METADATA_MAGIC       0x4D455441U
#define BOOT_STATE_NORMAL         0x13572401U
#define BOOT_STATE_UPDATE_PENDING 0x13572402U
#define BOOT_STATE_BACKUP         0x13572403U
#define BOOT_STATE_RECEIVING      0x13572404U
#define BOOT_STATE_TRIAL          0x13572405U
#define BOOT_MAX_TRIAL_ATTEMPTS   3U

/* 保留旧名称，供旧 APP 的升级触发逻辑迁移时使用。 */
#define BOOT_FLAG_NORMAL          BOOT_STATE_NORMAL
#define BOOT_FLAG_NEED_UPDATE     BOOT_STATE_UPDATE_PENDING
#define BOOT_FLAG_FIRST_BOOT      BOOT_STATE_TRIAL

/* 两页中始终至少保留一页有效记录；record_crc32 覆盖前 32 字节。 */
typedef struct
{
    uint32_t magic;
    uint32_t sequence;
    uint32_t state;
    uint32_t active_size;
    uint32_t active_crc32;
    uint32_t backup_size;
    uint32_t backup_crc32;
    uint32_t trial_attempts;
    uint32_t record_crc32;
} boot_metadata_t;

uint8_t jump_app(uint32_t app_addr);
void system_reboot(void);
uint8_t mcu_flash_erase(uint32_t addr, uint8_t sector_num);
uint8_t mcu_flash_write(uint32_t addr, uint8_t *buffer, uint32_t length);
void mcu_flash_read(uint32_t addr, uint8_t *buffer, uint32_t length);

uint8_t boot_metadata_read(boot_metadata_t *metadata);
uint8_t boot_metadata_write(boot_metadata_t *metadata);
uint32_t boot_image_crc32(uint32_t image_addr, uint32_t image_size);
uint8_t boot_image_is_valid(uint32_t image_addr, uint32_t image_size,
                            uint32_t image_crc32);
uint8_t boot_backup_is_valid(uint32_t image_size, uint32_t image_crc32);
uint8_t boot_copy_image(uint32_t source_addr, uint32_t destination_addr);
uint8_t boot_restore_backup(boot_metadata_t *metadata);

#endif /* __BOOTLOADER_H */
