/**
 * @file    bootloader.c
 * @brief   Bootloader底层功能实现
 * @details 提供应用跳转、系统复位、Flash擦写与读取等功能
 */

#include "bootloader.h"
#include "crc_check.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "usart.h"
#include "delay.h"


/**
 * @brief  清理Bootloader运行环境
 * @details 关闭所有外设、定时器、中断，为APP启动做准备
 */
static void cleanup_bootloader_environment(void)
{
    uint8_t i;
    
    uart_log("Cleaning up bootloader environment...\r\n");
    
    // 1. 关闭所有中断
    __disable_irq();
    
    // 2. 关闭TIM3定时器（ymodem使用）
    TIM_Cmd(TIM3, DISABLE);
    TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE);
    
    // 3. 关闭USART2（RS485）
    USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
    USART_Cmd(USART2, DISABLE);
    
    // 4. 关闭USART1（调试串口）
    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
    
    // 5. 关闭SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    // 6. 重置所有NVIC中断
    for(i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  // 禁用所有中断
        NVIC->ICPR[i] = 0xFFFFFFFF;  // 清除所有挂起中断
    }
    
    // 7. 重置中断向量表到默认位置（让APP自己设置）
    SCB->VTOR = 0;
    
    uart_log("Cleanup complete\r\n");
}

/**
 * @brief  跳转到APP应用程序
 * @param  app_addr: APP起始地址
 * @retval 1=跳转成功, 0=跳转失败
 */
uint8_t jump_app(uint32_t app_addr)
{
    uint32_t jump_addr;
    jump_callback cb;
    uint32_t boot_flag;
    uint8_t need_verify;

    /* 检查堆栈指针有效性 */
    if (((*(__IO uint32_t*)app_addr) & 0x2FFE0000 ) != 0x20000000) {
        uart_log("Invalid stack pointer: 0x%08X\r\n", *(__IO uint32_t*)app_addr);
        uart_log("APP may be corrupted or not programmed\r\n");
        Usart2_SendString("Invalid stack pointer\n");
        return 0;
    }

    Usart2_SendString("Stack pointer valid\n");

    /* 读取启动标志，决定是否需要校验 */
    boot_flag = read_boot_flag_from_flash();
    need_verify = (boot_flag == BOOT_FLAG_FIRST_BOOT) || 
                  (boot_flag == 0xFFFFFFFF);
    
    /* 根据标志决定是否进行CRC校验 */
    if (need_verify) {
        uart_log("Verifying app integrity (first boot or uninitialized)...\r\n");
        Usart2_SendString("Verifying app integrity...\n");
        if (!verify_app_integrity()) {
            uart_log("App integrity check FAILED!\r\n");
            uart_log("Please re-flash or update firmware\r\n");
            Usart2_SendString("App integrity check FAILED!\n");
            return 0;
        }
        uart_log("App integrity OK\r\n");
        Usart2_SendString("App integrity OK\n");
        
        /* 校验通过，标记为正常启动 */
        write_boot_flag_to_flash(BOOT_FLAG_NORMAL);
    } else {
        uart_log("Fast boot mode (flag: 0x%08X), skipping CRC check\r\n", boot_flag);
        Usart2_SendString("Fast boot mode\n");
    }
    
    /* 清理Bootloader环境 */
    cleanup_bootloader_environment();
    
    /* 准备跳转 */
    jump_addr = *(__IO uint32_t*) (app_addr + 4);
    cb = (jump_callback)jump_addr;
    
    uart_log("Jumping to APP at 0x%08X...\r\n", jump_addr);
    delay_ms(10);  // 等待串口发送完成
    
    /* 设置主堆栈指针并跳转 */
    __set_MSP(*(__IO uint32_t*)app_addr);
    cb();
    
    /* 正常情况不会执行到这里 */
    return 1;
}

void system_reboot(void)
{
    __set_FAULTMASK(1); // 屏蔽中断
    NVIC_SystemReset(); // 系统复位
}



uint8_t mcu_flash_erase(uint32_t addr, uint8_t count)
{
	
	uint8_t i;
	
	/* 安全检查：防止擦除超出APP区域 */
	#define APP_ERASE_SECTORS_MAX  42  // 硬编码最大值，对应42KB
	if (count > APP_ERASE_SECTORS_MAX) {
		uart_log("WARNING: Erase count %d exceeds max %d, limiting to max\r\n", 
		         count, APP_ERASE_SECTORS_MAX);
		count = APP_ERASE_SECTORS_MAX;
	}
	
	/* 边界检查：确保不会擦除到CONFIG/RESERVE区域 */
	if (addr + (count * 1024) > APP_WRITE_MAX_ADDR) {
		uart_log("ERROR: Erase range 0x%08X ~ 0x%08X exceeds safe boundary 0x%08X!\r\n",
		         addr, addr + (count * 1024), APP_WRITE_MAX_ADDR);
		return 0;
	}
	
	FLASH_Unlock();
	for (i = 0; i < count; ++i)
	{
		if (FLASH_ErasePage(addr + i * 1024) != FLASH_COMPLETE)
		{
			FLASH_Lock();
			uart_log("ERROR: Failed to erase page at 0x%08X\r\n", addr + i * 1024);
			return 0;
		}
	}
	FLASH_Lock();
	return 1;
}



// 另一种擦除Flash（示例保留）
//uint8_t mcu_flash_erase(uint32_t addr, uint8_t sector_num)
//{
//	uint8_t i;
//    uint32_t sector_addr;
//	FLASH_Unlock();
////	FLASH_Sector_1
//	sector_addr = get_mcu_flash_sector(addr);
//	for (i = 0; i < sector_num; ++i)
//	{
//
//		FLASH_ErasePage(sector_addr);//?????
//	//FLASH_EraseSector(sector_addr, VoltageRange_3);
//		if(sector_addr < FLASH_Sector_10)
//			sector_addr += 0x08;
//	}
//	FLASH_Lock();
//	return 1;
//}

uint8_t mcu_flash_write(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    FLASH_Status result;
    uint16_t i, data = 0;
    FLASH_Unlock();
    for (i = 0; i < length; i += 2)
    {
        data = (*(buffer + i + 1) << 8) + (*(buffer + i));
        result = FLASH_ProgramHalfWord((uint32_t)(addr + i), data);
    }
    FLASH_Lock();

    if(result != FLASH_COMPLETE)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void mcu_flash_read(uint32_t addr, uint8_t *buffer, uint32_t length)
{
    memcpy(buffer, (void *)addr, length);
}
