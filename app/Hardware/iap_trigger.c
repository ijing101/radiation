#include "main.h"
#include "iap_trigger.h"
#include "wdg.h"

#define BOOT_FLAG_SECTOR_ADDR       0x08004000
#define BOOT_FLAG_MAGIC             0x5AA5A55A
#define BOOT_FLAG_NEED_UPDATE       0x55AA5502

typedef struct {
    uint32_t magic;
    uint32_t boot_flag;
    uint32_t boot_count;
    uint32_t crc32;
} boot_flag_info_t;

static uint32_t calculate_crc32_simple(uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint16_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static uint8_t write_update_flag(void)
{
    boot_flag_info_t info;
    boot_flag_info_t *verify;
    uint32_t *src;
    int i;

    /* 注意：此函数执行期间 CPU 从 0x08004000 擦写数据。
       若 0x08004000 与你 APP 代码区重叠会损坏固件——APP 位于 0x08005000，安全。
       写 Flash 前先解锁，成功后一定记 lock。 */
    info.magic      = BOOT_FLAG_MAGIC;
    info.boot_flag  = BOOT_FLAG_NEED_UPDATE;
    info.boot_count = 0;
    info.crc32      = calculate_crc32_simple((uint8_t *)&info, 12);

    fmc_unlock();

    if (fmc_ready_wait(0x100000) != FMC_READY) {
        goto error_exit;
    }

    if (fmc_page_erase(BOOT_FLAG_SECTOR_ADDR) != FMC_READY) {
        goto error_exit;
    }
    if (fmc_ready_wait(0x100000) != FMC_READY) {
        goto error_exit;
    }

    IWDG_Feed();

    src = (uint32_t *)&info;
    for (i = 0; i < 4; i++) {
        if (fmc_word_program(BOOT_FLAG_SECTOR_ADDR + i * 4, src[i]) != FMC_READY) {
            goto error_exit;
        }
        if (fmc_ready_wait(0x10000) != FMC_READY) {
            goto error_exit;
        }
    }

    verify = (boot_flag_info_t *)BOOT_FLAG_SECTOR_ADDR;
    if (verify->magic != info.magic ||
        verify->boot_flag != info.boot_flag ||
        verify->crc32 != info.crc32) {
        fmc_page_erase(BOOT_FLAG_SECTOR_ADDR);
        goto error_exit;
    }

    fmc_lock();
    return 1;

error_exit:
    fmc_lock();
    return 0;
}

void trigger_iap_update(void)
{
    IWDG_Feed();

    if (!write_update_flag()) {
        return;     /* 写标志失败则继续运行，不复位 */
    }

    IWDG_Feed();
    delay_ms(10);
    __disable_irq();
    nvic_system_reset();

    while (1) {     /* 复位后不会执行到这里 */
    }
}