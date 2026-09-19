#ifndef __SYSTEM_CLOCK_CONFIG_GD32F130_H
#define __SYSTEM_CLOCK_CONFIG_GD32F130_H

#include "gd32f1x0.h"

/* ========================================================================
 *                      用户可修改配置区（移植时只改这里）
 * ========================================================================
 * 重要说明（已核对GD32F130xx官方Datasheet Rev3.9原文）：
 * 1. GD32F130系列官方规格：CPU核心/AHB/APB1/APB2最高频率均为 48MHz，
 *    不是72MHz、更不是108MHz（108MHz是GD32F103系列的规格，72MHz网上
 *    部分资料把该系列和别的系列混淆了，一律以本文件为准：48MHz）。
 * 2. Flash在48MHz下是"零等待状态"（这点和GD32F103不同，F103需要2个等待
 *    周期，F130官方直接做到0等待周期），所以下面Flash等待周期配置为0。
 *
 * 3. GD32F130F8P6是TSSOP20小封装，引脚很少。有些方案会省略外部晶振电路，
 *    直接用内部8MHz RC振荡器(IRC8M)+PLL；如果你的板子确实外接了HXTAL晶振，
 *    则用HXTAL精度更高。用下面的 CLOCK_SOURCE_SELECT 宏切换这两种情况，
 *    其余代码不用动。
 *
 * 4. HXTAL_VALUE_HZ 必须和板载晶振实际频率一致，同时要和 gd32f1x0.h 里的
 *    HXTAL_VALUE 宏保持一致。
 *
 * PLL计算公式：
 *      PLLCLK = (时钟源 / PLL_SOURCE_DIV) * PLL_MUL      最大不超过48MHz
 *
 * 常见配置对照（目标48MHz满速）：
 *      HXTAL=8MHz用作PLL源  : PLL_MUL=RCU_PLL_MUL6   (8*6=48)
 *      IRC8M(内部,固定8MHz) : PLL_MUL=RCU_PLL_MUL12  (IRC8M/2=4MHz先2分频，
 *                              再*12=48，即RCU_PLLSRC_IRC8M_DIV2 + MUL12)
 * ======================================================================== */

/* 时钟源选择：1=使用外部HXTAL晶振，0=使用内部IRC8M（无需外接晶振） */
#define CLOCK_SOURCE_USE_HXTAL   1

#define HXTAL_VALUE_HZ           8000000U

#if CLOCK_SOURCE_USE_HXTAL
    #define PLL_SOURCE           RCU_PLLSRC_HXTAL
    #define PLL_MUL              RCU_PLL_MUL6        /* 8MHz*6=48MHz */
#else
    #define PLL_SOURCE           RCU_PLLSRC_IRC8M_DIV2   /* IRC8M固定8MHz，先2分频得4MHz */
    #define PLL_MUL              RCU_PLL_MUL12           /* 4MHz*12=48MHz */
#endif

/* AHB/APB 分频系数：GD32F130的AHB/APB1/APB2上限都是48MHz（不像F103那样
 * APB1要求减半），所以正常情况下三条总线都可以配DIV1直接跑满48MHz */
#define AHB_PRESCALER            RCU_AHB_CKSYS_DIV1  /* AHB  = SYSCLK  = 48MHz */
#define APB1_PRESCALER           RCU_APB1_CKAHB_DIV1 /* APB1 = AHB     = 48MHz */
#define APB2_PRESCALER           RCU_APB2_CKAHB_DIV1 /* APB2 = AHB     = 48MHz */

/* Flash等待周期：官方规格48MHz下为零等待状态 */
#define FLASH_WAIT_STATE         WS_WSCNT_0

/* ======================================================================== */

typedef enum
{
    CLOCK_CONFIG_OK = 0,         /* 配置成功                          */
    CLOCK_CONFIG_OSCI_FAIL,      /* 时钟源（HXTAL或IRC8M）起振/稳定失败 */
    CLOCK_CONFIG_PLL_FAIL        /* PLL锁定超时                       */
} ClockConfigStatus_t;

ClockConfigStatus_t SystemClock_Config(void);
uint32_t SystemClock_GetSysClockFreq(void);

#endif /* __SYSTEM_CLOCK_CONFIG_GD32F130_H */
