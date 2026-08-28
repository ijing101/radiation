/**
 ******************************************************************************
 * @file    delay.c
 * @brief   GD32F130 通用SysTick延时函数（裸机版，标准外设库）
 *
 * 说明：
 *   SysTick时钟源固定为 HCLK/8，通过配置LOAD重载值+轮询COUNTFLAG标志
 *   来实现精确的us/ms级延时，不占用额外的定时器资源。逻辑和STM32版
 *   完全一致，因为SysTick是ARM内核外设而不是厂商私有外设。
 *
 * 移植注意事项：
 *   - 直接操作SysTick寄存器（CTRL/LOAD/VAL），不依赖任何GD32/STM32
 *     厂商封装函数，避免因固件库版本差异导致的编译问题。
 *   - fac_us/fac_ms 是根据 SystemCoreClock 实时算出来的，只要你在
 *     delay_init() 之前已经正确调用过时钟配置函数，换主频不需要改这个文件。
 ******************************************************************************
 */

#include "delay.h"

static uint8_t  fac_us = 0;    /* us延时倍乘数 */
static uint16_t fac_ms = 0;    /* ms延时倍乘数 */

/* SysTick->LOAD是24位寄存器，单次能装载的最大计数值 */
#define SYSTICK_MAX_LOAD    0x00FFFFFFU

/* SysTick->CTRL寄存器CLKSOURCE位：0=HCLK/8，1=HCLK（ARM内核标准定义，
 * STM32/GD32/所有Cortex-M3芯片通用，不依赖任何厂商头文件） */
#define SYSTICK_CLKSOURCE_HCLK_DIV8_BIT   (0U << 2)

/**
 * @brief  初始化延时函数
 *         SysTick的时钟固定为 HCLK 的 1/8
 * @param  无
 * @retval 无
 */
void delay_init(void)
{
    /* 清除CLKSOURCE位，选择HCLK/8作为SysTick时钟源 */
    SysTick->CTRL &= ~SysTick_CTRL_CLKSOURCE_Msk;

    fac_us = (uint8_t)(SystemCoreClock / 8000000U);   /* 每us需要的SysTick时钟数 */
    fac_ms = (uint16_t)fac_us * 1000U;                /* 每ms需要的SysTick时钟数 */
}

/**
 * @brief  延时nus（微秒级）
 * @param  nus  要延时的us数
 * @retval 无
 */
void delay_us(uint32_t nus)
{
    uint32_t temp;

    SysTick->LOAD = nus * fac_us;              /* 时间加载 */
    SysTick->VAL = 0x00;                       /* 清空计数器 */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;  /* 开始倒数 */
    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));  /* 等待时间到达 */

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; /* 关闭计数器 */
    SysTick->VAL = 0x00;                       /* 清空计数器 */
}

/**
 * @brief  延时nms（毫秒级）
 *         nms没有上限限制：内部会根据实际主频自动算出SysTick单次能承受的
 *         最大ms数，超过部分自动拆分成多次调用，避免LOAD寄存器溢出。
 * @param  nms  要延时的ms数
 * @retval 无
 */
void delay_ms(uint16_t nms)
{
    /* 单次delay_ms能安全延时的最大ms数（由24位LOAD寄存器和当前主频决定） */
    uint32_t max_ms_per_call = SYSTICK_MAX_LOAD / fac_ms;

    while (nms > max_ms_per_call)
    {
        delay_us((uint32_t)max_ms_per_call * 1000U);
        nms -= (uint16_t)max_ms_per_call;
    }

    if (nms > 0U)
    {
        delay_us((uint32_t)nms * 1000U);
    }
}
