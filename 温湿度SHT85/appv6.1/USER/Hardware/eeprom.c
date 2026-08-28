#include "eeprom.h"
#include "stm32f10x_flash.h"
#include "usart.h"

/**
 * @brief 使用内部Flash存储地址，波特率，温度和湿度的校准值，Flash存储模块初始化
 * @return 0: 成功, 1: 失败
 */
uint8_t Flash_Storage_Init(void)
{
    uint8_t slave_addr;
    uint32_t baudrate;
    int16_t temp_offset, humi_offset;

    // 尝试从Flash加载配置
    if (Flash_Storage_Load_Config(&slave_addr, &baudrate, &temp_offset,
                                  &humi_offset) == 0) {
        printf("Flash config loaded: Slave=0x%02X, Baud=%d, Offset=%d/%d\r\n",
               slave_addr, baudrate, temp_offset, humi_offset);
        return 0;
    } else {
        // 配置无效，保存默认配置
        uint8_t result;
        printf("Flash config invalid, saving defaults\r\n");
        result = Flash_Storage_Save_Config(DEFAULT_SLAVE_ADDR, DEFAULT_BAUDRATE,
                                           DEFAULT_TEMP_OFFSET, DEFAULT_HUMI_OFFSET);
        if (result == 0) {
            printf("Default config saved successfully\r\n");
        } else {
            printf("Failed to save default config\r\n");
        }
        return result;
    }
}

/**
 * @brief 保存配置到Flash
 * @param slave_addr: 从机地址
 * @param baudrate: 波特率
 * @param parity: 校验位
 * @return 0: 成功, 1: 失败
 */
uint8_t Flash_Storage_Save_Config(uint8_t slave_addr, uint32_t baudrate,
                                  int16_t temp_offset, int16_t humi_offset)
{
    flash_config_t config;
    uint8_t result;
    uint8_t crc_data[12]; // 用于CRC计算的数据缓冲区

    // 参数有效性检查
    if (slave_addr == 0 || slave_addr > 247) {
        printf("Invalid slave address: %d\r\n", slave_addr);
        return 1;
    }
    if (baudrate != 9600 && baudrate != 19200 && baudrate != 115200) {
        printf("Invalid baudrate: %d\r\n", baudrate);
        return 1;
    }
    if (temp_offset < -999 || temp_offset > 999 ||
        humi_offset < -999 || humi_offset > 999) {
        printf("Invalid calibration offset\r\n");
        return 1;
    }

    // 填充配置结构
    config.slave_addr = slave_addr;
    config.baudrate = baudrate;
    //config.parity = parity;
    config.temp_offset = temp_offset;
    config.humi_offset = humi_offset;
    config.magic = FLASH_CONFIG_MAGIC;
    config.crc16 = 0; // 先将CRC设为0

    // 准备CRC16计算数据 (按照固定顺序排列数据字节)
    crc_data[0] = (uint8_t)(config.magic & 0xFF);
    crc_data[1] = (uint8_t)(config.magic >> 8);
    crc_data[2] = config.slave_addr;
    //crc_data[3] = config.parity;
    crc_data[3] = (uint8_t)(config.baudrate & 0xFF);
    crc_data[4] = (uint8_t)((config.baudrate >> 8) & 0xFF);
    crc_data[5] = (uint8_t)((config.baudrate >> 16) & 0xFF);
    crc_data[6] = (uint8_t)((config.baudrate >> 24) & 0xFF);
    crc_data[7] = (uint8_t)(config.temp_offset & 0xFF);
    crc_data[8] = (uint8_t)((config.temp_offset >> 8) & 0xFF);
    crc_data[9] = (uint8_t)(config.humi_offset & 0xFF);
    crc_data[10] = (uint8_t)((config.humi_offset >> 8) & 0xFF);
    crc_data[11] = 0;

    config.crc16 = Flash_Storage_CRC16(crc_data, 12); // 计算CRC16

    // 擦除页面
    result = Flash_Storage_Erase_Page(FLASH_CONFIG_ADDR);
    if (result != 0) {
        printf("Flash erase failed\r\n");
        return 1;
    }

    // 写入配置数据
    result = Flash_Storage_Write_Page(FLASH_CONFIG_ADDR, (uint8_t*)&config, sizeof(config));
    if (result != 0) {
        printf("Flash write failed\r\n");
        return 1;
    }

    printf("Config saved: Slave=0x%02X, Baud=%d\r\n", slave_addr, baudrate);
    return 0;
}

/**
 * @brief 从Flash加载配置
 * @param slave_addr: 从机地址指针
 * @param baudrate: 波特率指针
 * @param parity: 校验位指针
 * @return 0: 成功, 1: 失败
 */
uint8_t Flash_Storage_Load_Config(uint8_t *slave_addr, uint32_t *baudrate,
                                  int16_t *temp_offset, int16_t *humi_offset)
{
    flash_config_t *config = (flash_config_t*)FLASH_CONFIG_ADDR;
    uint16_t calc_crc;
    uint8_t crc_data[12];

    // 魔数检查
    if (config->magic != FLASH_CONFIG_MAGIC) {
        printf("Flash magic error: 0x%04X\r\n", config->magic);
        return 1;
    }

    // 计算CRC16 (使用与保存时相同的顺序)
    crc_data[0] = (uint8_t)(config->magic & 0xFF);
    crc_data[1] = (uint8_t)(config->magic >> 8);
    crc_data[2] = config->slave_addr;
    //crc_data[3] = config->parity;
    crc_data[3] = (uint8_t)(config->baudrate & 0xFF);
    crc_data[4] = (uint8_t)((config->baudrate >> 8) & 0xFF);
    crc_data[5] = (uint8_t)((config->baudrate >> 16) & 0xFF);
    crc_data[6] = (uint8_t)((config->baudrate >> 24) & 0xFF);
    crc_data[7] = (uint8_t)(config->temp_offset & 0xFF);
    crc_data[8] = (uint8_t)((config->temp_offset >> 8) & 0xFF);
    crc_data[9] = (uint8_t)(config->humi_offset & 0xFF);
    crc_data[10] = (uint8_t)((config->humi_offset >> 8) & 0xFF);
    crc_data[11] = 0;

    calc_crc = Flash_Storage_CRC16(crc_data, 12);
    if (calc_crc != config->crc16) {
        printf("Flash CRC error: calc=0x%04X, stored=0x%04X\r\n", calc_crc, config->crc16);
        return 1;
    }

    // 参数有效性检查
    if (config->slave_addr == 0 || config->slave_addr > 247) {
        printf("Invalid stored slave address: %d\r\n", config->slave_addr);
        return 1;
    }
    if (config->baudrate != 9600 && config->baudrate != 19200 &&
        config->baudrate != 115200) {
        printf("Invalid stored baudrate: %d\r\n", config->baudrate);
        return 1;
    }
    if (config->temp_offset < -999 || config->temp_offset > 999 ||
        config->humi_offset < -999 || config->humi_offset > 999) {
        printf("Invalid stored calibration offset\r\n");
        return 1;
    }

    // 返回配置参数
    *slave_addr = config->slave_addr;
    *baudrate = config->baudrate;
    //*parity = config->parity;
    *temp_offset = config->temp_offset;
    *humi_offset = config->humi_offset;
    
    return 0;
}

/**
 * @brief 重置配置为默认值
 * @return 0: 成功, 1: 失败
 */
uint8_t Flash_Storage_Reset_Config(void)
{
    return Flash_Storage_Save_Config(DEFAULT_SLAVE_ADDR, DEFAULT_BAUDRATE,
                                     DEFAULT_TEMP_OFFSET, DEFAULT_HUMI_OFFSET);
}

/**
 * @brief 验证Flash中的配置有效性
 * @return 0: 有效, 1: 无效
 */
uint8_t Flash_Storage_Verify_Config(void)
{
    uint8_t slave_addr;
    uint32_t baudrate;
    int16_t temp_offset, humi_offset;

    return Flash_Storage_Load_Config(&slave_addr, &baudrate, &temp_offset,
                                     &humi_offset);
}

/**
 * @brief 计算CRC16校验值
 * @param data: 数据缓冲区
 * @param len: 数据长度
 * @return CRC16值
 */
uint16_t Flash_Storage_CRC16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc;
}

/**
 * @brief 擦除Flash页
 * @param addr: 页地址
 * @return 0: 成功, 1: 失败
 */
static uint8_t Flash_Storage_Erase_Page(uint32_t addr)
{
    FLASH_Status status;

    // 解锁Flash
    FLASH_Unlock();

    // 擦除页面
    status = FLASH_ErasePage(addr);

    // 锁定Flash
    FLASH_Lock();

    return (status == FLASH_COMPLETE) ? 0 : 1;
}

/**
 * @brief 写入Flash页
 * @param addr: 写入地址
 * @param data: 数据缓冲区
 * @param len: 数据长度
 * @return 0: 成功, 1: 失败
 */
static uint8_t Flash_Storage_Write_Page(uint32_t addr, uint8_t *data, uint16_t len)
{
    FLASH_Status status = FLASH_COMPLETE;
    uint16_t i;
    uint16_t *data16 = (uint16_t*)data;
    uint16_t len16 = (len + 1) / 2; // 转换为16位字长度

    // 解锁Flash
    FLASH_Unlock();

    // 按16位字写入
    for (i = 0; i < len16; i++) {
        status = FLASH_ProgramHalfWord(addr + i * 2, data16[i]);
        if (status != FLASH_COMPLETE) {
            break;
        }
    }

    // 锁定Flash
    FLASH_Lock();

    return (status == FLASH_COMPLETE) ? 0 : 1;
}


