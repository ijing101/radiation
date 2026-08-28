/**
 * @file    main.c
 * @brief   Bootloader主程序文件
 * @details 实现IAP在线升级功能，支持Ymodem协议和CRC校验
 * @author  IAP开发团队
 * @date    2024
 */

#include "delay.h"
#include "usart.h"
#include "ymodem.h"
#include "rs485.h"
#include "bsp_clkconfig.h"
#include "bootloader.h"

#define WAIT_TIMEOUT   5    // 等待用户操作的超时时间(秒)

uint8_t buf[1024 * 20];     // 数据缓冲区，用于存储升级文件
//const uint8_t buf[1024 * 20];

/**
 * @brief  显示Bootloader启动信息
 * @details 显示Flash分区表信息和Bootloader版本
 * @param  None
 * @retval None
 */
void print_boot_message(void)
{
    uart_log("---------- Enter BootLoader ----------\r\n");
    uart_log("\r\n");
    uart_log("======== flash partition table ========\r\n");
    uart_log("| name     | offset     | size       |\r\n");
    uart_log("---------------------------------------\r\n");
    uart_log("| boot     | 0x%08X | 0x%08X |\r\n", BOOT_SECTOR_ADDR, BOOT_SECTOR_SIZE);
    uart_log("| flag     | 0x%08X | 0x%08X |\r\n", BOOT_FLAG_SECTOR_ADDR, BOOT_FLAG_SECTOR_SIZE);
    uart_log("| app      | 0x%08X | 0x%08X |\r\n", APP_SECTOR_ADDR, APP_SECTOR_SIZE);
    uart_log("| config   | 0x%08X | 0x%08X |\r\n", CONFIG_SECTOR_ADDR, CONFIG_SECTOR_SIZE);
    uart_log("| reserve  | 0x%08X | 0x%08X |\r\n", RESERVE_SECTOR_ADDR, RESERVE_SECTOR_SIZE);
    uart_log("=======================================\r\n");
}

/**
 * @brief  显示等待用户操作提示信息
 * @details 提示用户可选择启动应用程序或进入升级模式
 * @param  None
 * @retval None
 */
void print_wait_message(void)
{
    uart_log("------- Please enter parameter -------\r\n");
    uart_log("[1].Start program\r\n");
    uart_log("[2].Update program\r\n");
    uart_log("--------------------------------------\r\n");
}

/**
 * @brief  主函数
 * @details Bootloader主程序，实现以下功能：
 *          1. 系统初始化和外设配置
 *          2. 等待用户选择启动模式
 *          3. 支持应用程序启动
 *          4. 支持Ymodem协议在线升级
 *          5. 提供CRC校验保证数据完整性
 * @param  None
 * @retval int 程序正常情况下不会返回
 */



int main(void)
{
    RCC_ClocksTypeDef  RCC_Clocks;      // 时钟配置结构体
    process_status  process;            // 当前处理状态
    uint16_t timerout = 0;              // 超时计数器
    uint16_t update_timeout = 0;        // 升级超时计数器
    uint32_t boot_flag;                 // 启动标志
    uint16_t wait_timeout = WAIT_TIMEOUT; // 等待超时时间

    /* 系统时钟配置：外部8MHz晶振 * 4 = 32MHz系统时钟 */
    HSE_SetSysClock(RCC_PLLMul_4);

    /* 外设初始化配置 */
    ymodem_init();                      // 初始化Ymodem协议处理模块
    RS485_Init(9600);                 // 初始化RS485串口通信，波特率115200
    uart_init(115200);                  // 初始化UART1调试串口，波特率115200
    delay_init();                       // 初始化延时函数

    /* 启动信息显示 */
    print_boot_message();               // 显示Bootloader版本和Flash分区信息
    
    /* 读取启动标志，决定启动模式 */
    boot_flag = read_boot_flag_from_flash();
    uart_log("Boot flag: 0x%08X\r\n", boot_flag);
    
    /* 如果boot flag未初始化(全0xFF)，写入NORMAL标志 */
    if (boot_flag == 0xFFFFFFFF) {
        uart_log("Boot flag uninitialized, writing NORMAL flag\r\n");
        write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
        boot_flag = BOOT_FLAG_NORMAL;
    }
    
    /* 根据启动标志设置不同的启动流程 */
    switch(boot_flag)
			{
        case BOOT_FLAG_NORMAL:
            /* 正常启动：快速跳转模式（1秒超时） */
            uart_log("=== FAST BOOT MODE ===\r\n");
            uart_log("Normal startup, jumping to APP in 1 second\r\n");
            wait_timeout = 1;
            set_ymodem_status(WAIT_START_PROGRAM);
            break;
            
        case BOOT_FLAG_NEED_UPDATE:
            /* 需要升级：直接进入升级模式 */
            uart_log("=== UPDATE MODE ===\r\n");
            uart_log("Update requested, entering update mode directly\r\n");
            set_ymodem_status(UPDATE_PROGRAM);
            break;
            
        case BOOT_FLAG_FIRST_BOOT:
            /* 首次启动：需要完整校验 */
            uart_log("=== FIRST BOOT AFTER UPDATE ===\r\n");
            uart_log("Will verify APP integrity before jump\r\n");
            wait_timeout = 2;
            set_ymodem_status(WAIT_START_PROGRAM);
            break;
            
        default:
            /* 未初始化或异常：强制重置为NORMAL并启动 */
            uart_log("=== SAFE BOOT MODE ===\r\n");
            uart_log("Invalid boot flag detected, force reset to NORMAL\r\n");
            
            /* 擦除整个boot flag区域并重新初始化 */
            FLASH_Unlock();
            FLASH_ErasePage(BOOT_FLAG_SECTOR_ADDR);
            FLASH_Lock();
            uart_log("Boot flag area erased\r\n");
            
            /* 写入NORMAL标志 */
            write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
            uart_log("NORMAL flag written, will start APP in 1 second\r\n");
            
            wait_timeout = 1;
            set_ymodem_status(WAIT_START_PROGRAM);
            break;
    }
    
    /* 主循环：处理不同的系统状态 */
    while(1)
    {
        process = get_ymodem_status();  // 获取当前处理状态
        switch (process)
        {
            /* 等待启动程序状态 */
            case WAIT_START_PROGRAM:
                uart_log("Wait start app...(%ds)\r\n", wait_timeout - timerout);
                delay_ms(1000);
                timerout++;                 // 超时计数器递增

                /* 超时后自动启动应用程序 */
                if(timerout >= wait_timeout)
                {
                    set_ymodem_status(START_PROGRAM);
                }
                update_timeout = 0;         // 重置升级超时计数器
                break;
            /* 启动应用程序状态 */
            case START_PROGRAM:
                uart_log("start app...\r\n");
                Usart2_SendString("start app...\n");
                delay_ms(50);

                /* 尝试跳转到应用程序 */
                if (!jump_app(APP_SECTOR_ADDR))
                {
                    uart_log("start app failed: app corrupted or not programmed\r\n");
                    Usart2_SendString("start app failed: app corrupted or not programmed\n");
                    uart_log("Please update firmware\r\n");
                    Usart2_SendString("Please update firmware\n");
                    delay_ms(2000);
                    
                    /* 跳转失败，写入升级标志并进入升级模式 */
                    uart_log("Entering update mode...\r\n");
                    Usart2_SendString("Entering update mode...\n");
                    write_boot_flag_to_flash(BOOT_FLAG_NEED_UPDATE);
                    set_ymodem_status(UPDATE_PROGRAM);
                    timerout = 0;
                    update_timeout = 0;
                }
                /* 注意：如果jump_app成功，程序已经跳转到APP，不会返回这里 */
                break;
            /* 更新应用程序状态 */
            case UPDATE_PROGRAM:
                if (update_timeout == 0) {
                    /* 刚进入升级模式，立即清除NEED_UPDATE标志，避免掉电重启后重复进入 */
                    uart_log("=== Entering UPDATE mode ===\r\n");
                    uart_log("Clearing NEED_UPDATE flag to prevent re-entry after power loss\r\n");
                    write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
                    boot_flag = BOOT_FLAG_NORMAL;
                    
                    uart_log("=== Waiting for firmware file via Ymodem ===\r\n");
                    uart_log("Please send .bin file using Ymodem protocol\r\n");
                    Usart2_SendString("等待接收升级文件(Ymodem协议)\r\n");
                }
                
                ymodem_c();                 // 发送'C'字符，请求CRC模式传输
                delay_ms(500);
                update_timeout++;           // 升级超时计数器递增

                /* 每10秒提示一次 */
                if (update_timeout % 20 == 0) {
                    uart_log("Still waiting... (%ds)\r\n", update_timeout / 2);
                }

                /* 100秒超时保护，避免永久卡住 */
                if (update_timeout >= 200) {  // 100秒
                    uart_log("Update timeout! Trying to start APP\r\n");
                    Usart2_SendString("升级超时！尝试启动APP\r\n");
                    /* 标志已经是NORMAL，直接尝试启动APP */
                    set_ymodem_status(WAIT_START_PROGRAM);
                    update_timeout = 0;
                    timerout = 0;
                }
                break;

            /* 升级成功状态 */
            case UPDATE_SUCCESS:
                Usart2_SendString("update success\n");
                Usart2_SendString("system reboot...\n");
                uart_log("update success\r\n");
                uart_log("system reboot...\r\n");
                delay_ms(1000);
                system_reboot();            // 系统重启
                break;

            /* 默认状态：包括BUSY等其他状态 */
            default:
                break;
        }
    }
}