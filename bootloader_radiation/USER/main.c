/**
 * @file main.c
 * @brief GD32F130F8P6 安全 Bootloader 主状态机。
 *
 * 新 APP 被标记为 TRIAL 后，必须在时钟、看门狗、配置和关键外设均成功
 * 初始化后，调用应用侧的 iap_confirm_app_boot() 等价实现，将状态改为
 * BOOT_STATE_NORMAL。若每次均在确认前复位，三次后本文件会恢复 Backup。
 */
#include "main.h"
#include "delay.h"
#include "ymodem.h"
#include "bootloader.h"

#define WAIT_TIMEOUT_SECONDS 1U
#define UPDATE_TIMEOUT_TICKS 200U
#define UPDATE_INACTIVITY_TICKS 10U

static uint8_t active_image_valid(boot_metadata_t *metadata)
{
    return (metadata != 0 && metadata->active_size != 0U &&
            boot_image_is_valid(APP_SECTOR_ADDR, metadata->active_size,
                                metadata->active_crc32)) ? 1U : 0U;
}

static uint8_t direct_image_valid(void)
{
    /* 兼容首次通过调试器直接烧录、尚没有 metadata 的 APP。 */
    return boot_image_is_valid(APP_SECTOR_ADDR, APP_IMAGE_MAX_SIZE,
                               0xFFFFFFFFU);
}

static uint8_t backup_image_valid(boot_metadata_t *metadata)
{
    return (metadata != 0 && metadata->backup_size != 0U &&
            boot_backup_is_valid(metadata->backup_size,
                                metadata->backup_crc32)) ? 1U : 0U;
}

static uint8_t prepare_update(boot_metadata_t *metadata)
{
    if (metadata == 0)
    {
        return 0U;
    }

    /*
     * 先落盘 BACKUP 状态，再复制旧 APP。复制中掉电时 Active 尚未擦除，
     * 下次启动会重新执行这一步，不会丢失最后一个已确认镜像。
     */
    if (active_image_valid(metadata))
    {
        metadata->state = BOOT_STATE_BACKUP;
        if (!boot_metadata_write(metadata) ||
            !boot_copy_image(APP_SECTOR_ADDR, APP_BACKUP_ADDR))
        {
            return 0U;
        }
        metadata->backup_size = metadata->active_size;
        metadata->backup_crc32 = metadata->active_crc32;
        metadata->trial_attempts = 0U;
    }
    else if (!backup_image_valid(metadata))
    {
        metadata->active_size = 0U;
        metadata->active_crc32 = 0U;
        metadata->backup_size = 0U;
        metadata->backup_crc32 = 0U;
        metadata->trial_attempts = 0U;
    }

    metadata->state = BOOT_STATE_RECEIVING;
    return boot_metadata_write(metadata);
}

static uint8_t recover_receiving(boot_metadata_t *metadata)
{
    if (metadata == 0)
    {
        return 0U;
    }
    /* 接收/擦写中断后，只要 Backup 有效就优先恢复 Active。 */
    if (!active_image_valid(metadata) && backup_image_valid(metadata))
    {
        if (!boot_copy_image(APP_BACKUP_ADDR, APP_SECTOR_ADDR))
        {
            return 0U;
        }
        metadata->active_size = metadata->backup_size;
        metadata->active_crc32 = metadata->backup_crc32;
        return boot_metadata_write(metadata);
    }
    return (active_image_valid(metadata) || backup_image_valid(metadata)) ? 1U : 0U;
}

static uint8_t enter_update_mode(boot_metadata_t *metadata)
{
    if (metadata == 0)
    {
        return 0U;
    }

    /* 不允许在有效 Active 尚未得到有效 Backup 保护时擦写 Active。 */
    if (active_image_valid(metadata) && !backup_image_valid(metadata))
    {
        return 0U;
    }

    metadata->state = BOOT_STATE_RECEIVING;
    if (!boot_metadata_write(metadata))
    {
        return 0U;
    }
    ymodem_reset_transfer();
    set_ymodem_status(UPDATE_PROGRAM);
    return 1U;
}

static uint8_t fallback_to_confirmed_app(boot_metadata_t *metadata)
{
    if (metadata == 0)
    {
        return 0U;
    }
    if (backup_image_valid(metadata) && !active_image_valid(metadata))
    {
        return boot_restore_backup(metadata);
    }
    if (active_image_valid(metadata))
    {
        metadata->state = BOOT_STATE_NORMAL;
        metadata->trial_attempts = 0U;
        return boot_metadata_write(metadata);
    }
    return 0U;
}

int main(void)
{
    boot_metadata_t metadata;
    uint8_t metadata_valid;
    uint16_t wait_ticks = WAIT_TIMEOUT_SECONDS;
    uint16_t update_timeout = 0U;
    uint8_t transfer_idle_ticks = 0U;

    delay_init();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    /* ymodem_init() 内部初始化 USART0 PA9/PA10 为 9600 baud。 */
    ymodem_init();

    metadata_valid = boot_metadata_read(&metadata);
    if (!metadata_valid)
    {
        memset(&metadata, 0, sizeof(metadata));
        if (direct_image_valid())
        {
            metadata.active_size = APP_IMAGE_MAX_SIZE;
            metadata.active_crc32 = 0xFFFFFFFFU;
            metadata.state = BOOT_STATE_NORMAL;
        }
        else
        {
            metadata.state = BOOT_STATE_UPDATE_PENDING;
        }
        (void)boot_metadata_write(&metadata);
        metadata_valid = boot_metadata_read(&metadata);
    }
    else if (metadata.sequence == 0U)
    {
        /* 读取到旧单标志时，将其升级为双页 metadata 格式。 */
        (void)boot_metadata_write(&metadata);
        metadata_valid = boot_metadata_read(&metadata);
    }

    if (!metadata_valid)
    {
        memset(&metadata, 0, sizeof(metadata));
        metadata.state = BOOT_STATE_RECEIVING;
        enter_update_mode(&metadata);
    }
    else
    {
        switch (metadata.state)
        {
        case BOOT_STATE_NORMAL:
            if (active_image_valid(&metadata))
            {
                set_ymodem_status(WAIT_START_PROGRAM);
            }
            else if (direct_image_valid())
            {
                metadata.active_size = APP_IMAGE_MAX_SIZE;
                metadata.active_crc32 = 0xFFFFFFFFU;
                metadata.trial_attempts = 0U;
                (void)boot_metadata_write(&metadata);
                set_ymodem_status(WAIT_START_PROGRAM);
            }
            else if (fallback_to_confirmed_app(&metadata))
            {
                set_ymodem_status(WAIT_START_PROGRAM);
            }
            else if (prepare_update(&metadata))
            {
                enter_update_mode(&metadata);
            }
            else
            {
                enter_update_mode(&metadata);
            }
            break;

        case BOOT_STATE_UPDATE_PENDING:
        case BOOT_STATE_BACKUP:
            if (prepare_update(&metadata))
            {
                enter_update_mode(&metadata);
            }
            else
            {
                enter_update_mode(&metadata);
            }
            break;

        case BOOT_STATE_RECEIVING:
            (void)recover_receiving(&metadata);
            enter_update_mode(&metadata);
            break;

        case BOOT_STATE_TRIAL:
            if (!active_image_valid(&metadata) ||
                metadata.trial_attempts >= BOOT_MAX_TRIAL_ATTEMPTS)
            {
                if (fallback_to_confirmed_app(&metadata))
                {
                    set_ymodem_status(WAIT_START_PROGRAM);
                }
                else
                {
                    metadata.state = BOOT_STATE_RECEIVING;
                    enter_update_mode(&metadata);
                }
            }
            else
            {
                metadata.trial_attempts++;
                if (boot_metadata_write(&metadata))
                {
                    set_ymodem_status(WAIT_START_PROGRAM);
                }
                else
                {
                    metadata.state = BOOT_STATE_RECEIVING;
                    enter_update_mode(&metadata);
                }
            }
            break;

        default:
            metadata.state = BOOT_STATE_UPDATE_PENDING;
            if (prepare_update(&metadata))
            {
                enter_update_mode(&metadata);
            }
            else
            {
                enter_update_mode(&metadata);
            }
            break;
        }
    }

    while (1)
    {
        switch (get_ymodem_status())
        {
        case WAIT_START_PROGRAM:
            /*正常启动窗口不接受任意字节触发升级。 */
            delay_ms(1000U);
            if (wait_ticks > 0U)
            {
                wait_ticks--;
            }
            if (wait_ticks == 0U)
            {
                set_ymodem_status(START_PROGRAM);
            }
            update_timeout = 0U;
            break;

        case START_PROGRAM:
            if (!jump_app(APP_SECTOR_ADDR))
            {
                if (boot_metadata_read(&metadata) &&
                    fallback_to_confirmed_app(&metadata))
                {
                    wait_ticks = WAIT_TIMEOUT_SECONDS;
                    set_ymodem_status(WAIT_START_PROGRAM);
                }
                else
                {
                    if (boot_metadata_read(&metadata))
                    {
                        metadata.state = BOOT_STATE_RECEIVING;
                        (void)boot_metadata_write(&metadata);
                    }
                    ymodem_reset_transfer();
                    set_ymodem_status(UPDATE_PROGRAM);
                    update_timeout = 0U;
                }
            }
            break;

        case UPDATE_PROGRAM:
            /* 已开始数据传输时禁止继续发 C，避免 9600 baud 下与数据包冲突。 */
            if (ymodem.status == 0U)
            {
                ymodem_c();
            }

            /*
             * 收到 block 0 后不能继续发 C，以免与正常 1K 包冲突；但若通信
             * 线拔掉，5 秒无任何接收活动就丢弃半包并重新发送 C。metadata
             * 仍为 RECEIVING，已经保存的旧 APP Backup 不会受影响。
             */
            if (ymodem.status != 0U)
            {
                if (ymodem_take_rx_activity())
                {
                    transfer_idle_ticks = 0U;
                    update_timeout = 0U;
                }
                else if (++transfer_idle_ticks >= UPDATE_INACTIVITY_TICKS)
                {
                    ymodem_abort_transfer();
                    transfer_idle_ticks = 0U;
                    update_timeout = 0U;
                    ymodem_c();
                }
            }
            else
            {
                transfer_idle_ticks = 0U;
                (void)ymodem_take_rx_activity();
            }
            delay_ms(500U);
            update_timeout++;
            if (update_timeout >= UPDATE_TIMEOUT_TICKS)
            {
                if (boot_metadata_read(&metadata) &&
                    fallback_to_confirmed_app(&metadata))
                {
                    wait_ticks = WAIT_TIMEOUT_SECONDS;
                    set_ymodem_status(WAIT_START_PROGRAM);
                }
                else
                {
                    /* 没有任何可启动镜像时持续等待主机修复，不跳转到空 Flash。 */
                    ymodem_reset_transfer();
                    set_ymodem_status(UPDATE_PROGRAM);
                    update_timeout = 0U;
                }
            }
            break;

        case UPDATE_SUCCESS:
            delay_ms(1000U);
            system_reboot();
            break;

        default:
            break;
        }
    }
}
