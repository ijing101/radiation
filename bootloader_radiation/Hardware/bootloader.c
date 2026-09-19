/**
 * @file bootloader.c
 * @brief GD32 Flash 操作、环境清理和安全跳转。
 */
#include "bootloader.h"
#include "ymodem.h"
#include "usart.h"

static void cleanup_bootloader_environment(void)
{
    uint8_t i;

    __disable_irq();
    timer_interrupt_disable(TIMER2, TIMER_INT_UP);
    timer_disable(TIMER2);
    usart_interrupt_disable(USART0, USART_INT_RBNE);
    usart_disable(USART0);

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    SCB->VTOR = 0U;
}

uint8_t jump_app(uint32_t app_addr)
{
    boot_metadata_t metadata;
    uint32_t stack_pointer;
    uint32_t reset_vector;
    jump_callback callback;

    if (app_addr != APP_SECTOR_ADDR || !boot_metadata_read(&metadata) ||
        (metadata.state != BOOT_STATE_NORMAL &&
         metadata.state != BOOT_STATE_TRIAL) ||
        !boot_image_is_valid(app_addr, metadata.active_size,
                             metadata.active_crc32))
    {
        return 0U;
    }

    stack_pointer = *(volatile uint32_t *)app_addr;
    reset_vector = *(volatile uint32_t *)(app_addr + 4U);
    if ((reset_vector & 1U) == 0U)
    {
        return 0U;
    }

    cleanup_bootloader_environment();
    SCB->VTOR = app_addr;
    callback = (jump_callback)reset_vector;
    __set_MSP(stack_pointer);
    callback();
    return 1U;
}

void system_reboot(void)
{
    __set_FAULTMASK(1U);
    NVIC_SystemReset();
}

static uint8_t flash_region_valid(uint32_t addr, uint32_t length)
{
    uint32_t region_start;
    uint32_t region_end;

    if (length == 0U)
    {
        return 0U;
    }
    if (addr >= APP_SECTOR_ADDR && addr < APP_SECTOR_ADDR + APP_SLOT_SIZE)
    {
        region_start = APP_SECTOR_ADDR;
        region_end = APP_SECTOR_ADDR + APP_SLOT_SIZE;
    }
    else if (addr >= APP_BACKUP_ADDR && addr < APP_BACKUP_ADDR + APP_SLOT_SIZE)
    {
        region_start = APP_BACKUP_ADDR;
        region_end = APP_BACKUP_ADDR + APP_SLOT_SIZE;
    }
    else
    {
        return 0U;
    }

    return (addr >= region_start && length <= region_end - addr) ? 1U : 0U;
}

uint8_t mcu_flash_erase(uint32_t addr, uint8_t sector_num)
{
    uint8_t i;

    /* 仅允许完整擦除 Active 或 Backup 槽，永不触碰 Boot/配置区。 */
    if ((addr != APP_SECTOR_ADDR && addr != APP_BACKUP_ADDR) ||
        sector_num == 0U || sector_num > APP_ERASE_SECTORS)
    {
        return 0U;
    }

    fmc_unlock();
    for (i = 0U; i < sector_num; i++)
    {
        if (fmc_page_erase(addr + (uint32_t)i * FLASH_SECTOR_SIZE) != FMC_READY)
        {
            fmc_lock();
            return 0U;
        }
    }
    fmc_lock();
    return 1U;
}

uint8_t mcu_flash_write(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    uint32_t i;
    uint16_t data;

    if (buffer == 0 || length == 0U || (length & 1U) != 0U ||
        !flash_region_valid(addr, length))
    {
        return 0U;
    }

    fmc_unlock();
    for (i = 0U; i < length; i += 2U)
    {
        data = (uint16_t)buffer[i] | (uint16_t)((uint16_t)buffer[i + 1U] << 8U);
        if (fmc_halfword_program(addr + i, data) != FMC_READY ||
            *(volatile uint16_t *)(addr + i) != data)
        {
            fmc_lock();
            return 0U;
        }
    }
    fmc_lock();
    return 1U;
}

void mcu_flash_read(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    if (buffer != 0 && length != 0U)
    {
        memcpy(buffer, (const void *)addr, length);
    }
}
