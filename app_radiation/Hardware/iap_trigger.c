#include "main.h"
#include "iap_trigger.h"
#include "wdg.h"
#include <string.h>

/*
 * 必须与 bootloader_radiation/Hardware/bootloader.h 保持字节级一致。
 * APP 只写元数据，不直接操作 Active/Backup 槽；备份、镜像校验和回滚均由
 * Bootloader 在复位后的受控流程中完成。
 */
#define BOOT_METADATA_MAGIC       0x4D455441U
#define BOOT_METADATA_PAGE_A      0x08004000U
#define BOOT_METADATA_PAGE_B      0x08004400U
#define APP_IMAGE_MAX_SIZE        0x53FCU
#define BOOT_MAX_TRIAL_ATTEMPTS   3U

#define BOOT_STATE_NORMAL         0x13572401U
#define BOOT_STATE_UPDATE_PENDING 0x13572402U
#define BOOT_STATE_BACKUP         0x13572403U
#define BOOT_STATE_RECEIVING      0x13572404U
#define BOOT_STATE_TRIAL          0x13572405U

/* 仅用于从旧版单标志 APP/Boot 过渡到双页元数据协议。 */
#define LEGACY_BOOT_FLAG_MAGIC    0x5AA5A55AU
#define LEGACY_BOOT_FLAG_UPDATE   0x55AA5502U

/* 原业务代码沿用该历史函数名；映射到 CMSIS 的标准复位实现。 */
void nvic_system_reset(void)
{
    NVIC_SystemReset();
}

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

typedef struct
{
    uint32_t magic;
    uint32_t boot_flag;
    uint32_t boot_count;
    uint32_t crc32;
} legacy_boot_flag_t;

/* 与 Bootloader 相同的标准 CRC-32（多项式 0xEDB88320）。 */
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
    /* 有符号差值可处理 sequence 从 0xFFFFFFFF 回绕到 0 的情况。 */
    return ((int32_t)(left - right) > 0) ? 1U : 0U;
}

static uint8_t read_metadata(boot_metadata_t *metadata)
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
        return 0U;
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

static void restore_interrupt_state(uint32_t primask)
{
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
}

/*
 * 写入较旧的一页，另一页始终保留最后一份有效记录。
 * metadata 页不在 APP 执行区，因而不会擦除正在运行的应用程序。
 */
static uint8_t write_metadata(boot_metadata_t *metadata)
{
    boot_metadata_t page_a;
    boot_metadata_t page_b;
    boot_metadata_t verify;
    uint8_t valid_a;
    uint8_t valid_b;
    uint32_t target_address;
    uint32_t sequence;
    uint32_t i;
    uint32_t primask;

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

    /* Flash 擦写期间不允许中断服务程序访问 Flash 或再次使用 FMC。 */
    IWDG_Feed();
    primask = __get_PRIMASK();
    __disable_irq();
    fmc_unlock();

    /* GD32 固件库的 fmc_page_erase()/fmc_word_program() 内部已等待完成。 */
    if (fmc_page_erase(target_address) != FMC_READY)
    {
        goto error_exit;
    }

    for (i = 0U; i < sizeof(boot_metadata_t) / sizeof(uint32_t); i++)
    {
        if (fmc_word_program(target_address + i * sizeof(uint32_t),
                             ((uint32_t *)metadata)[i]) != FMC_READY)
        {
            goto error_exit;
        }
        IWDG_Feed();
    }

    fmc_lock();
    restore_interrupt_state(primask);

    return (metadata_record_valid(target_address, &verify) &&
            memcmp(&verify, metadata, sizeof(boot_metadata_t)) == 0) ? 1U : 0U;

error_exit:
    fmc_lock();
    restore_interrupt_state(primask);
    return 0U;
}

static uint8_t write_legacy_update_flag(void)
{
    legacy_boot_flag_t legacy;
    uint32_t i;
    uint32_t primask;

    legacy.magic = LEGACY_BOOT_FLAG_MAGIC;
    legacy.boot_flag = LEGACY_BOOT_FLAG_UPDATE;
    legacy.boot_count = 0U;
    legacy.crc32 = crc32_bytes((const uint8_t *)&legacy, 12U);

    IWDG_Feed();
    primask = __get_PRIMASK();
    __disable_irq();
    fmc_unlock();
    if (fmc_page_erase(BOOT_METADATA_PAGE_A) != FMC_READY)
    {
        goto error_exit;
    }

    for (i = 0U; i < sizeof(legacy_boot_flag_t) / sizeof(uint32_t); i++)
    {
        if (fmc_word_program(BOOT_METADATA_PAGE_A + i * sizeof(uint32_t),
                             ((uint32_t *)&legacy)[i]) != FMC_READY)
        {
            goto error_exit;
        }
        IWDG_Feed();
    }

    fmc_lock();
    restore_interrupt_state(primask);
    return 1U;

error_exit:
    fmc_lock();
    restore_interrupt_state(primask);
    return 0U;
}

static uint8_t write_update_pending(void)
{
    boot_metadata_t metadata;

    if (!read_metadata(&metadata))
    {
        /* 首次替换 Bootloader 时，仍可唤起其旧标志兼容处理。 */
        return write_legacy_update_flag();
    }

    metadata.state = BOOT_STATE_UPDATE_PENDING;
    metadata.trial_attempts = 0U;
    return write_metadata(&metadata);
}

void trigger_iap_update(void)
{
    IWDG_Feed();
    if (!write_update_pending())
    {
        return;             /* 元数据未安全落盘，绝不复位进入升级流程。 */
    }

    IWDG_Feed();
    delay_ms(10U);
    __disable_irq();
    NVIC_SystemReset();

    while (1)
    {
    }
}

void iap_confirm_app_boot(void)
{
    boot_metadata_t metadata;

    if (!read_metadata(&metadata) || metadata.state != BOOT_STATE_TRIAL)
    {
        return;
    }

    /* 只有业务完成关键初始化后才调用本函数，确认试运行版本可正常工作。 */
    metadata.state = BOOT_STATE_NORMAL;
    metadata.trial_attempts = 0U;
    (void)write_metadata(&metadata);
}
