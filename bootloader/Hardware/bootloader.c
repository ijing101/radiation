/**
 * @file    bootloader.c
 * @brief   Bootloader 底层功能实现
 * @details 提供应用跳转、系统复位、Flash 擦写与读取等功能
 */
#include "bootloader.h"
#include "crc_check.h"
#include "ymodem.h"
#include "usart.h"

/**
 * @brief  清理 Bootloader 运行环境
 * @details 关闭升级过程使用的定时器与串口，为跳转到 APP 做准备
 */
static void cleanup_bootloader_environment(void)
{
    /* 关闭 TIMER2 及其更新中断（Ymodem 帧间超时定时器） */
    timer_interrupt_disable(TIMER2, TIMER_INT_UP);
    timer_disable(TIMER2);

    /* 关闭 USART0 接收中断及串口本身 */
    usart_interrupt_disable(USART0, USART_INT_RBNE);
    usart_disable(USART0);
}

/**
 * @brief  跳转到 APP 应用程序
 * @param  app_addr: APP 起始地址
 * @retval 1=跳转（正常不会返回）, 0=跳转失败（栈顶非法）
 */
uint8_t jump_app(uint32_t app_addr)
{
    uint32_t stack_top;
    uint32_t boot_flag;
    jump_callback cb;

    /* 读取 APP 栈顶指针，校验其位于合法 SRAM 范围（0x20000000 ~ 0x20002000） */
    stack_top = *(volatile uint32_t *)app_addr;
    if ((stack_top & 0xFFF00000U) != 0x20000000U) {
        return 0;   /* 非法栈顶：APP 可能损坏或未烧录 */
    }

    /* 首次启动或标志未初始化时，跳转前校验 APP 完整性 */
    boot_flag = read_boot_flag_from_flash();
    if ((boot_flag == BOOT_FLAG_FIRST_BOOT) || (boot_flag == 0xFFFFFFFF)) {
        if (!verify_app_integrity()) {
            return 0;   /* 完整性校验失败：留在 Bootloader，等待升级 */
        }
        write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
    }

    /* 读取 APP 复位向量（入口地址） */
    cb = (jump_callback)(*(volatile uint32_t *)(app_addr + 4));

    /* 清理 Bootloader 使用的外设 */
    cleanup_bootloader_environment();

    /* 关中断 -> 设置向量表偏移 -> 设置主栈指针 -> 跳转到 APP */
    __disable_irq();
    SCB->VTOR = app_addr;
    __set_MSP(stack_top);
    cb();

    return 1;   /* 正常情况不会执行到这里 */
}

/**
 * @brief  系统复位
 */
void system_reboot(void)
{
    __set_FAULTMASK(1);     /* 屏蔽中断 */
    NVIC_SystemReset();     /* 触发系统复位 */
}

/**
 * @brief  擦除 Flash 指定页
 * @param  addr: 起始地址（页对齐）
 * @param  count: 擦除页数
 * @retval 1=成功, 0=失败
 */
uint8_t mcu_flash_erase(uint32_t addr, uint8_t count)
{
    uint8_t i;

    /* 擦除页数上限保护 */
    if (count > APP_ERASE_SECTORS_MAX) {
        count = APP_ERASE_SECTORS_MAX;
    }

    /* 边界检查：确保不会擦除到 CONFIG/RESERVE 区域 */
    if ((addr + (uint32_t)count * FLASH_SECTOR_SIZE) > APP_WRITE_MAX_ADDR) {
        return 0;
    }

    /* 解锁 FMC，逐页擦除 */
    fmc_unlock();
    for (i = 0; i < count; i++) {
        if (fmc_page_erase(addr + i * FLASH_SECTOR_SIZE) != FMC_READY) {
            fmc_lock();
            return 0;
        }
    }
    fmc_lock();
    return 1;
}

/**
 * @brief  写 Flash（半字写入）
 * @param  addr: 目标地址
 * @param  buffer: 数据指针
 * @param  length: 数据长度（须为偶数）
 * @retval 1=成功, 0=失败
 */
uint8_t mcu_flash_write(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    uint32_t i;
    uint16_t data;

    fmc_unlock();
    for (i = 0; i < length; i += 2) {
        /* 组装半字（小端：低字节在前） */
        data = (uint16_t)((buffer[i + 1] << 8) | buffer[i]);
        if (fmc_halfword_program(addr + i, data) != FMC_READY) {
            fmc_lock();
            return 0;
        }
    }
    fmc_lock();
    return 1;
}

/**
 * @brief  读 Flash（直接内存映射读取）
 * @param  addr: 源地址
 * @param  buffer: 目标缓冲
 * @param  length: 数据长度
 */
void mcu_flash_read(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    memcpy(buffer, (const void *)addr, length);
}
