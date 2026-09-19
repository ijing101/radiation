/**
 * @file crc_check.c
 * @brief YMODEM CRC16 与精确长度镜像 CRC32/向量表校验。
 */
#include "crc_check.h"
#include "bootloader.h"

uint16_t crc16_ccitt(const uint8_t *data, uint16_t length)
{
    uint16_t crc = CRC16_CCITT_INIT;
    uint16_t i;
    uint8_t bit;

    if (data == 0)
    {
        return 0U;
    }
    for (i = 0U; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8U;
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ CRC16_CCITT_POLY)
                                  : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    uint8_t bit;

    crc ^= (uint16_t)data << 8U;
    for (bit = 0U; bit < 8U; bit++)
    {
        crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ CRC16_CCITT_POLY)
                              : (uint16_t)(crc << 1U);
    }
    return crc;
}

uint8_t verify_ymodem_packet(const uint8_t *packet, uint16_t data_len)
{
    uint16_t received_crc;

    if (packet == 0 || (data_len != 128U && data_len != 1024U))
    {
        return 0U;
    }
    received_crc = (uint16_t)((uint16_t)packet[data_len + 3U] << 8U) |
                   packet[data_len + 4U];
    return (received_crc == crc16_ccitt(&packet[3], data_len)) ? 1U : 0U;
}

uint32_t boot_image_crc32(uint32_t image_addr, uint32_t image_size)
{
    const uint8_t *data = (const uint8_t *)image_addr;
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < image_size; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
        }
    }
    return ~crc;
}

static uint8_t stack_pointer_valid(uint32_t stack_pointer)
{
    return ((stack_pointer & 3U) == 0U && stack_pointer >= SRAM_START_ADDR &&
            stack_pointer < SRAM_END_ADDR) ? 1U : 0U;
}

static uint8_t reset_vector_valid(uint32_t reset_vector, uint32_t code_start,
                                  uint32_t code_end)
{
    uint32_t reset_address;

    if ((reset_vector & 1U) == 0U)
    {
        return 0U;
    }
    reset_address = reset_vector & ~1U;
    return (reset_address >= code_start && reset_address < code_end) ? 1U : 0U;
}

uint8_t boot_image_is_valid(uint32_t image_addr, uint32_t image_size,
                            uint32_t image_crc32)
{
    uint32_t image_end;
    uint32_t stack_pointer;
    uint32_t reset_vector;

    if (image_addr != APP_SECTOR_ADDR || image_size < 8U ||
        image_size > APP_IMAGE_MAX_SIZE)
    {
        return 0U;
    }
    image_end = image_addr + image_size;
    stack_pointer = *(volatile uint32_t *)image_addr;
    reset_vector = *(volatile uint32_t *)(image_addr + 4U);
    if (!stack_pointer_valid(stack_pointer) ||
        !reset_vector_valid(reset_vector, image_addr, image_end))
    {
        return 0U;
    }

    /* 0xFFFFFFFF 仅用于首次由编程器直接烧录的旧 APP。 */
    return (image_crc32 == 0xFFFFFFFFU ||
            boot_image_crc32(image_addr, image_size) == image_crc32) ? 1U : 0U;
}

uint8_t boot_backup_is_valid(uint32_t image_size, uint32_t image_crc32)
{
    uint32_t stack_pointer;
    uint32_t reset_vector;

    if (image_size < 8U || image_size > APP_IMAGE_MAX_SIZE)
    {
        return 0U;
    }
    stack_pointer = *(volatile uint32_t *)APP_BACKUP_ADDR;
    reset_vector = *(volatile uint32_t *)(APP_BACKUP_ADDR + 4U);
    /* Backup 是 Active 的字节副本，复位向量仍应指向 Active 地址。 */
    if (!stack_pointer_valid(stack_pointer) ||
        !reset_vector_valid(reset_vector, APP_SECTOR_ADDR,
                            APP_SECTOR_ADDR + image_size))
    {
        return 0U;
    }
    return (image_crc32 == 0xFFFFFFFFU ||
            boot_image_crc32(APP_BACKUP_ADDR, image_size) == image_crc32) ? 1U : 0U;
}
