#ifndef __DELAY_H
#define __DELAY_H

#include "gd32f1x0.h"

/* ========================================================================
 * 基于SysTick实现的裸机延时函数（GD32F130版）
 *
 * 使用方法：
 *   1. 在时钟配置完成之后（即 SystemClock_Config() 调用之后）调用一次
 *      delay_init()，之后就可以在任意地方使用 delay_us() / delay_ms()。
 *   2. 本文件不依赖具体主频数值，delay_init() 内部会根据实际的
 *      SystemCoreClock 自动计算，换晶振、换主频都不需要改这个文件。
 *
 * 移植说明：
 *   SysTick是ARM Cortex-M3内核自带的标准外设，寄存器定义（CTRL/LOAD/VAL）
 *   在STM32和GD32上完全一致，本文件直接操作SysTick寄存器，不依赖任何
 *   厂商封装函数，因此和GD32F103版本逻辑完全一样，仅头文件包含不同。
 *
 * 注意事项：
 *   - SysTick的重载寄存器（LOAD）是24位的，单次delay_ms()能延时的最大
 *     时长和实际主频有关。本文件已处理这个问题：nms超过单次上限时会
 *     自动拆成多次调用，可以放心传入任意大小的nms。
 * ======================================================================== */

void delay_init(void);
void delay_us(uint32_t nus);
void delay_ms(uint16_t nms);
void delay_1ms(uint32_t count);    /* 简单毫秒延时（等价 delay_ms，兼容旧代码命名） */
void Delay_NOP(unsigned int Del);  /* NOP 空指令延时（用于 HX711 等 GPIO 位控短延时） */

#endif /* __DELAY_H */
