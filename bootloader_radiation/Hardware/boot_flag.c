/**
 * @file boot_flag.c
 * @brief 双页元数据、旧标志兼容和 Active/Backup 镜像复制。
 */
#include "bootloader.h"
#include "crc_check.h"

#define LEGACY_BOOT_FLAG_MAGIC 0x5AA5A55AU
#define LEGACY_BOOT_FLAG_UPDATE 0x55AA5502U

typedef struct
{
    uint32_t magic;
    uint32_t boot_flag;
    uint32_t boot_count;
    uint32_t crc32;
} legacy_boot_flag_t;

static uint8_t copy_buffer[FLASH_SECTOR_SIZE];

static uint32_t crc32_bytes(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t i;
    uint8_t bit;

    for (i = 0U; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
        }
    }
    return ~crc;
}

static uint8_t metadata_state_valid(uint32_t state)
{
    return (state == BOOT_STATE_NORMAL || state == BOOT_STATE_UPDATE_PENDING ||
            state == BOOT_STATE_BACKUP || state == BOOT_STATE_RECEIVING ||
            state == BOOT_STATE_TRIAL) ? 1U : 0U;
}

static uint8_t metadata_record_valid(uint32_t address, boot_metadata_t *metadata)
{
    if (metadata == 0)
    {
        return 0U;
    }

    memcpy(metadata, (const void *)address, sizeof(boot_metadata_t));
    if (metadata->magic != BOOT_METADATA_MAGIC ||
        !metadata_state_valid(metadata->state) ||
        metadata->active_size > APP_IMAGE_MAX_SIZE ||
        metadata->backup_size > APP_IMAGE_MAX_SIZE ||
        metadata->trial_attempts > BOOT_MAX_TRIAL_ATTEMPTS)
    {
        return 0U;
    }

    return (crc32_bytes((const uint8_t *)metadata, 32U) == metadata->record_crc32)
         ? 1U : 0U;
}

static uint8_t sequence_is_newer(uint32_t left, uint32_t right)
{
    return ((int32_t)(left - right) > 0) ? 1U : 0U;
}

static uint8_t legacy_record_valid(legacy_boot_flag_t *legacy)
{
    if (legacy == 0)
    {
        return 0U;
    }

    memcpy(legacy, (const void *)BOOT_FLAG_SECTOR_ADDR, sizeof(legacy_boot_flag_t));
    if (legacy->magic != LEGACY_BOOT_FLAG_MAGIC)
    {
        return 0U;
    }
    return (crc32_bytes((const uint8_t *)legacy, 12U) == legacy->crc32) ? 1U : 0U;
}

/* 首次替换 Bootloader 时兼容旧版单标志记录；不会擅自擦除 APP。 */
static uint8_t legacy_to_metadata(boot_metadata_t *metadata)
{
    legacy_boot_flag_t legacy;

    if (metadata == 0 || !legacy_record_valid(&legacy))
    {
        return 0U;
    }

    memset(metadata, 0, sizeof(boot_metadata_t));
    metadata->magic = BOOT_METADATA_MAGIC;
    metadata->active_size = APP_IMAGE_MAX_SIZE;
    metadata->active_crc32 = 0xFFFFFFFFU;

    if (legacy.boot_flag == LEGACY_BOOT_FLAG_UPDATE ||
        !boot_image_is_valid(APP_SECTOR_ADDR, metadata->active_size,
                             metadata->active_crc32))
    {
        metadata->state = BOOT_STATE_UPDATE_PENDING;
    }
    else
    {
        metadata->state = BOOT_STATE_NORMAL;
    }
    return 1U;
}

uint8_t boot_metadata_read(boot_metadata_t *metadata)
{
    boot_metadata_t page_a;
    boot_metadata_t page_b;
    uint8_t valid_a;
    uint8_t valid_b;

    if (metadata == 0)
    {
        return 0U;
    }
    valid_a = metadata_record_valid(BOOT_METADATA_PAGE_A, &page_a);
    valid_b = metadata_record_valid(BOOT_METADATA_PAGE_B, &page_b);

    if (!valid_a && !valid_b)
    {
        return legacy_to_metadata(metadata);
    }
    if (valid_a && valid_b)
    {
        memcpy(metadata, sequence_is_newer(page_b.sequence, page_a.sequence)
                         ? &page_b : &page_a, sizeof(boot_metadata_t));
    }
    else
    {
        memcpy(metadata, valid_a ? &page_a : &page_b, sizeof(boot_metadata_t));
    }
    return 1U;
}

uint8_t boot_metadata_write(boot_metadata_t *metadata)
{
    boot_metadata_t page_a;
    boot_metadata_t page_b;
    boot_metadata_t verify;
    uint8_t valid_a;
    uint8_t valid_b;
    uint32_t target_address;
    uint32_t sequence;
    uint32_t i;

    if (metadata == 0 || !metadata_state_valid(metadata->state) ||
        metadata->active_size > APP_IMAGE_MAX_SIZE ||
        metadata->backup_size > APP_IMAGE_MAX_SIZE ||
        metadata->trial_attempts > BOOT_MAX_TRIAL_ATTEMPTS)
    {
        return 0U;
    }

    valid_a = metadata_record_valid(BOOT_METADATA_PAGE_A, &page_a);
    valid_b = metadata_record_valid(BOOT_METADATA_PAGE_B, &page_b);
    if (!valid_a && !valid_b)
    {
        target_address = BOOT_METADATA_PAGE_A;
        sequence = 1U;
    }
    else if (valid_a && valid_b)
    {
        if (sequence_is_newer(page_b.sequence, page_a.sequence))
        {
            target_address = BOOT_METADATA_PAGE_A;
            sequence = page_b.sequence + 1U;
        }
        else
        {
            target_address = BOOT_METADATA_PAGE_B;
            sequence = page_a.sequence + 1U;
        }
    }
    else if (valid_a)
    {
        target_address = BOOT_METADATA_PAGE_B;
        sequence = page_a.sequence + 1U;
    }
    else
    {
        target_address = BOOT_METADATA_PAGE_A;
        sequence = page_b.sequence + 1U;
    }

    metadata->magic = BOOT_METADATA_MAGIC;
    metadata->sequence = sequence;
    metadata->record_crc32 = crc32_bytes((const uint8_t *)metadata, 32U);

    /* 先写入另一页；掉电时旧页仍保留为最后一份有效记录。 */
    fmc_unlock();
    if (fmc_page_erase(target_address) != FMC_READY)
    {
        fmc_lock();
        return 0U;
    }
    for (i = 0U; i < sizeof(boot_metadata_t) / 4U; i++)
    {
        if (fmc_word_program(target_address + i * 4U,
                             ((uint32_t *)metadata)[i]) != FMC_READY)
        {
            fmc_lock();
            return 0U;
        }
    }
    fmc_lock();

    return (metadata_record_valid(target_address, &verify) &&
            memcmp(&verify, metadata, sizeof(boot_metadata_t)) == 0) ? 1U : 0U;
}

uint8_t boot_copy_image(uint32_t source_addr, uint32_t destination_addr)
{
    uint32_t offset;

    if ((source_addr != APP_SECTOR_ADDR && source_addr != APP_BACKUP_ADDR) ||
        (destination_addr != APP_SECTOR_ADDR && destination_addr != APP_BACKUP_ADDR) ||
        source_addr == destination_addr ||
        !mcu_flash_erase(destination_addr, APP_ERASE_SECTORS))
    {
        return 0U;
    }

    for (offset = 0U; offset < APP_SLOT_SIZE; offset += FLASH_SECTOR_SIZE)
    {
        mcu_flash_read(source_addr + offset, copy_buffer, FLASH_SECTOR_SIZE);
        if (!mcu_flash_write(destination_addr + offset, copy_buffer,
                             FLASH_SECTOR_SIZE) ||
            memcmp((const void *)(destination_addr + offset), copy_buffer,
                   FLASH_SECTOR_SIZE) != 0)
        {
            return 0U;
        }
    }
    return 1U;
}

uint8_t boot_restore_backup(boot_metadata_t *metadata)
{
    if (metadata == 0 || metadata->backup_size == 0U ||
        !boot_backup_is_valid(metadata->backup_size, metadata->backup_crc32) ||
        !boot_copy_image(APP_BACKUP_ADDR, APP_SECTOR_ADDR))
    {
        return 0U;
    }

    metadata->active_size = metadata->backup_size;
    metadata->active_crc32 = metadata->backup_crc32;
    metadata->state = BOOT_STATE_NORMAL;
    metadata->trial_attempts = 0U;
    return boot_metadata_write(metadata);
}
