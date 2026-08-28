#ifndef _EEPROM_H
#define _EEPROM_H

#include "stm32f10x.h"

// Flash存储数据结构，使用内部Flash存储地址，波特率，温度和湿度的校准值
typedef struct {
    uint8_t slave_addr;     // 从机地址 (1-247)
    uint32_t baudrate;      // 波特率
    int16_t temp_offset;    // 温度校准值 (单位0.1°C, 范围-999~+999)
    int16_t humi_offset;    // 湿度校准值 (单位0.1%RH, 范围-999~+999)
    uint16_t crc16;         // CRC16校验值
    uint16_t magic;         // 魔数标识，用于验证数据有效性
} flash_config_t;

// 配置数据有效性验证魔数
#define FLASH_CONFIG_MAGIC      0x5A5C

// Flash存储地址 (IAP应用程序区域)
// IAP区域: 0x08005000-0x0800FFFF (44KB), 最后预留2KB用于配置
#define FLASH_CONFIG_ADDR       0x0800F800  // IAP应用程序区域最后1KB
#define FLASH_PAGE_SIZE         1024

// 默认配置参数
#define DEFAULT_SLAVE_ADDR      0x01
#define DEFAULT_BAUDRATE        9600
//#define DEFAULT_PARITY          0       // 无校验
#define DEFAULT_TEMP_OFFSET     0       // 温度偏移量为0
#define DEFAULT_HUMI_OFFSET     0       // 湿度偏移量为0

// 函数声明
uint8_t Flash_Storage_Init(void);
uint8_t Flash_Storage_Save_Config(uint8_t slave_addr, uint32_t baudrate,
                                 int16_t temp_offset, int16_t humi_offset);
uint8_t Flash_Storage_Load_Config(uint8_t *slave_addr, uint32_t *baudrate,
                                 int16_t *temp_offset, int16_t *humi_offset);
uint8_t Flash_Storage_Reset_Config(void);
uint8_t Flash_Storage_Verify_Config(void);

// 内部辅助函数
uint16_t Flash_Storage_CRC16(uint8_t *data, uint16_t len);
static uint8_t Flash_Storage_Write_Page(uint32_t addr, uint8_t *data, uint16_t len);
static uint8_t Flash_Storage_Erase_Page(uint32_t addr);

#endif
