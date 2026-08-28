/**
 * @file    main.c
 * @brief   Bootloader 主程序
 * @details 实现 GD32F130F8T6 单串口（PA9/PA10）Ymodem IAP 升级流程：
 *          1. 系统初始化（时钟由 SystemInit 完成，48MHz）
 *          2. 根据启动标志选择启动模式
 *          3. 等待按键或超时后启动 APP
 *          4. 支持 Ymodem 协议在线升级
 */
#include "main.h"
#include "delay.h"
#include "ymodem.h"
#include "usart.h"
#include "bootloader.h"
#include "crc_check.h"

#define WAIT_TIMEOUT    5   /* 等待用户操作的默认超时时间（秒） */

/**
 * @brief  显示 Bootloader 启动信息与 Flash 分区表
 */
static void print_boot_message(void)
{
    console_printf("---------- Enter BootLoader ----------\r\n");
    console_printf("======== flash partition table ========\r\n");
    console_printf("| name    | offset     | size       |\r\n");
    console_printf("| boot    | 0x%08X | 0x%08X |\r\n", BOOT_SECTOR_ADDR, BOOT_SECTOR_SIZE);
    console_printf("| flag    | 0x%08X | 0x%08X |\r\n", BOOT_FLAG_SECTOR_ADDR, BOOT_FLAG_SECTOR_SIZE);
    console_printf("| app     | 0x%08X | 0x%08X |\r\n", APP_SECTOR_ADDR, APP_SECTOR_SIZE);
    console_printf("| config  | 0x%08X | 0x%08X |\r\n", CONFIG_SECTOR_ADDR, CONFIG_SECTOR_SIZE);
    console_printf("| reserve | 0x%08X | 0x%08X |\r\n", RESERVE_SECTOR_ADDR, RESERVE_SECTOR_SIZE);
    console_printf("=======================================\r\n");
}

/**
 * @brief  主函数
 * @retval 正常情况下不会返回
 */
int main(void)
{
    process_status process;
    uint32_t boot_flag;
    uint16_t wait_timeout = WAIT_TIMEOUT;   /* 等待超时（秒） */
    uint16_t timerout = 0;                  /* 等待计时 */
    uint16_t update_timeout = 0;            /* 升级超时计时 */

    /* 初始化：延时 -> NVIC 优先级分组 -> Ymodem（队列+定时器） -> USART0 */
    delay_init();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
    ymodem_init();
    usart0_init(115200);

    print_boot_message();

    /* 读取启动标志，决定启动模式 */
    boot_flag = read_boot_flag_from_flash();
    if (boot_flag == 0xFFFFFFFF) {
        /* 标志未初始化：写入 NORMAL */
        write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
        boot_flag = BOOT_FLAG_NORMAL;
    }

    switch (boot_flag)
    {
    case BOOT_FLAG_NORMAL:
        /* 正常启动：快速跳转（1 秒超时，期间任意按键可进入升级） */
        wait_timeout = 1;
        set_ymodem_status(WAIT_START_PROGRAM);
        console_printf("Normal boot: jump to APP in 1s, press any key to update.\r\n");
        break;

    case BOOT_FLAG_NEED_UPDATE:
        /* 需要升级：直接进入升级模式 */
        set_ymodem_status(UPDATE_PROGRAM);
        console_printf("Update requested, enter update mode.\r\n");
        break;

    case BOOT_FLAG_FIRST_BOOT:
        /* 升级后首次启动：跳转前校验 APP 完整性 */
        wait_timeout = 2;
        set_ymodem_status(WAIT_START_PROGRAM);
        console_printf("First boot after update: will verify APP integrity.\r\n");
        break;

    default:
        /* 非法标志：强制恢复为 NORMAL */
        write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
        wait_timeout = 1;
        set_ymodem_status(WAIT_START_PROGRAM);
        console_printf("Invalid boot flag, reset to normal.\r\n");
        break;
    }

    /* 主循环 */
    while (1)
    {
        process = get_ymodem_status();
        switch (process)
        {
        case WAIT_START_PROGRAM:
            /* 检测用户按键：任意按键进入升级模式 */
            if (queue_not_empty(&rx_queue)) {
                queue_initiate(&rx_queue);      /* 清空队列，丢弃按键字节 */
                console_printf("Key pressed, enter update mode.\r\n");
                set_ymodem_status(UPDATE_PROGRAM);
                update_timeout = 0;
                timerout = 0;
                break;
            }

            delay_ms(1000);
            timerout++;

            /* 超时后自动启动 APP */
            if (timerout >= wait_timeout) {
                set_ymodem_status(START_PROGRAM);
            }
            update_timeout = 0;
            break;

        case START_PROGRAM:
            console_printf("start app...\r\n");
            delay_ms(50);

            /* 尝试跳转到 APP */
            if (!jump_app(APP_SECTOR_ADDR)) {
                console_printf("start app failed, enter update mode.\r\n");
                delay_ms(2000);

                /* 跳转失败：写升级标志并进入升级模式 */
                write_boot_flag_to_flash(BOOT_FLAG_NEED_UPDATE);
                set_ymodem_status(UPDATE_PROGRAM);
                timerout = 0;
                update_timeout = 0;
            }
            break;

        case UPDATE_PROGRAM:
            if (update_timeout == 0) {
                /* 刚进入升级模式：清除 NEED_UPDATE 标志，避免掉电后重复进入 */
                write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
                console_printf("=== Enter UPDATE mode ===\r\n");
                console_printf("Waiting for Ymodem file...\r\n");
            }

            /* 发送 'C' 请求 CRC 模式传输 */
            ymodem_c();
            delay_ms(500);
            update_timeout++;

            /* 100 秒超时保护，避免永久卡住 */
            if (update_timeout >= 200) {
                console_printf("Update timeout, back to APP.\r\n");
                set_ymodem_status(START_PROGRAM);
                update_timeout = 0;
                timerout = 0;
            }
            break;

        case UPDATE_SUCCESS:
            console_printf("update success, system reboot...\r\n");
            delay_ms(1000);
            system_reboot();
            break;

        default:
            break;
        }
    }
}
