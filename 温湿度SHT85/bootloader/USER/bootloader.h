#ifndef __BOOTLOARDER_H
#define __BOOTLOARDER_H
#include "string.h"
#include "stm32f10x.h"
#include "main.h"


/******************************************
内存布局 (STM32F103 64KB Flash):
| 0x08000000 | 0x08004000 | 0x0800F800 | 0x0800FC00 | 0x08010000 |
-------------------------------------------------------------------
|  BOOT(16K) |  APP(46KB) | CONFIG(1K) |  FLAG(1K)  |    END     |
-------------------------------------------------------------------
*******************************************/

//#define BOOT_DEBUG
#ifdef BOOT_DEBUG
#define BOOT_LOG printf
#else
#define BOOT_LOG
#endif

#define FLASH_SECTOR_SIZE           1024
#define FLASH_SECTOR_NUM            64   // 64K

#define FLASH_START_ADDR            ((uint32_t)0x8000000)
#define FLASH_END_ADDR              ((uint32_t)(0x8000000 + FLASH_SECTOR_NUM * FLASH_SECTOR_SIZE))

/******************************************
优化后的Flash布局 (STM32F103 64KB Flash):
| 0x08000000 | 0x08004000 | 0x08005000 | 0x0800F800 | 0x0800FC00 | 0x08010000 |
--------------------------------------------------------------------------------
|  BOOT(16K) |  FLAG(4K)  |  APP(42KB) | CONFIG(1K) | RESERVE(1K)|    END     |
--------------------------------------------------------------------------------
说明：
- Flag前置：避免APP擦除/写入时误伤
- Reserve区：最后1KB作为缓冲区，防止越界写入
- APP减小4KB：从46KB减为42KB，牺牲少量空间换取安全性
*******************************************/

// Bootloader区域
#define BOOT_SECTOR_ADDR            0x08000000     // BOOT起始地址
#define BOOT_SECTOR_SIZE            0x4000         // BOOT大小: 16KB

// 启动标志区域（前置保护）
#define BOOT_FLAG_SECTOR_ADDR       0x08004000     // 启动标志起始地址
#define BOOT_FLAG_SECTOR_SIZE       0x1000         // 启动标志大小: 4KB (增加至4页提高可靠性)

// APP区域
#define APP_SECTOR_ADDR             0x08005000     // APP起始地址
#define APP_SECTOR_SIZE             0xA800         // APP大小: 42KB (0x0800F800 - 0x08005000)

// CONFIG区域
#define CONFIG_SECTOR_ADDR          0x0800F800     // CONFIG起始地址
#define CONFIG_SECTOR_SIZE          0x0400         // CONFIG大小: 1KB

// 预留区域（安全缓冲）
#define RESERVE_SECTOR_ADDR         0x0800FC00     // 预留区起始地址
#define RESERVE_SECTOR_SIZE         0x0400         // 预留区大小: 1KB

// 安全边界定义
#define APP_WRITE_MAX_ADDR          CONFIG_SECTOR_ADDR  // APP写入不能超过此地址

// APP区域需要擦除的页数 (42KB = 42页)
#define APP_ERASE_SECTORS        (APP_SECTOR_SIZE / FLASH_SECTOR_SIZE)
#define MaxQueueSize                1200





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

// 启动标志定义 (三种必要状态)
#define BOOT_FLAG_NORMAL        0x55AA5501    // 正常启动，快速跳转
#define BOOT_FLAG_NEED_UPDATE   0x55AA5502    // 需要升级
#define BOOT_FLAG_FIRST_BOOT    0x55AA5504    // 升级后首次启动，需要验证

// 启动标志信息结构体
typedef struct {
    uint32_t magic;        // 魔数：0x5AA5A55A
    uint32_t boot_flag;    // 启动标志
    uint32_t boot_count;   // 启动次数
    uint32_t crc32;        // CRC32校验
} boot_flag_info_t;

// Flash操作函数
uint8_t jump_app(uint32_t appAddr);
void system_reboot(void);
uint8_t mcu_flash_erase(uint32_t addr, uint8_t sector_num);
uint8_t mcu_flash_write(uint32_t addr, uint8_t *buffer, uint32_t length);
void mcu_flash_read(uint32_t addr, uint8_t *buffer, uint32_t length);

// 启动标志操作函数
uint32_t read_boot_flag_from_flash(void);
uint8_t write_boot_flag_to_flash(uint32_t flag);
uint32_t calculate_boot_flag_crc(boot_flag_info_t *info);
uint32_t get_boot_count(void);

// 旧接口（保留兼容性）
void set_boot_state(process_status process);
process_status get_boot_state(void);
uint8_t read_setting_boot_state(void);
uint8_t write_setting_boot_state(uint8_t boot_state);

#endif
