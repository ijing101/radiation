/**
 ******************************************************************************
 * @file    system_clock_config_gd32f130.c
 * @brief   GD32F130 通用系统时钟配置函数（GD32标准外设库版本）
 *
 * 使用方法：
 *   1. 根据你的板子（是否外接晶振、晶振频率），修改
 *      system_clock_config_gd32f130.h 中的宏定义。
 *   2. 在 main() 最开始调用一次 SystemClock_Config()，再进行其他外设初始化。
 *   3. 如果返回值不是 CLOCK_CONFIG_OK，说明时钟没有配置成功，
 *      请自行决定是死循环报错、点灯提示，还是回退到默认IRC8M运行。
 *
 * 移植注意事项：
 *   - 本文件依赖GD32标准外设库的 gd32f1x0_rcu.h / gd32f1x0_fmc.h，
 *     不依赖具体项目的其他模块，可直接复制到新工程使用。
 *   - 【重要】GD32F130系列官方规格最高主频是48MHz（已核对Datasheet
 *     Rev3.9原文确认），不是72MHz更不是108MHz，本文件默认目标就是
 *     48MHz满速，不要照搬F103的参数。
 *   - 不同批次/版本的GD32F1x0固件库，个别函数名可能有细微差异，
 *     如果编译报错找不到符号，去 gd32f1x0_rcu.h / gd32f1x0_fmc.h 里
 *     搜一下对应关键字，改成你库里实际的名字即可，逻辑不用动。
 ******************************************************************************
 */

#include "system_clock_config_gd32f130.h"

/**
 * @brief  配置系统时钟：时钟源(HXTAL或IRC8M) -> PLL -> SYSCLK，并设置总线分频
 * @param  无
 * @retval ClockConfigStatus_t 配置结果状态
 */
ClockConfigStatus_t SystemClock_Config(void)
{
    __IO uint32_t PLLStartUpCounter = 0;
    ErrStatus OsciStartUpStatus;

    /* 1. 复位RCU相关寄存器到默认值，避免之前(如bootloader)的配置残留干扰 */
    rcu_deinit();

#if CLOCK_SOURCE_USE_HXTAL
    /* 2a. 开启外部高速晶振HXTAL，并等待其稳定就绪
     *     rcu_osci_stab_wait() 内部有超时机制，超时会返回ERROR */
    rcu_osci_on(RCU_HXTAL);
    OsciStartUpStatus = rcu_osci_stab_wait(RCU_HXTAL);
#else
    /* 2b. 使用内部8MHz RC振荡器IRC8M，无需外接晶振
     *     IRC8M上电即工作，这里等待其稳定标志置位即可 */
    rcu_osci_on(RCU_IRC8M);
    OsciStartUpStatus = rcu_osci_stab_wait(RCU_IRC8M);
#endif

    if (OsciStartUpStatus != SUCCESS)
    {
        /* 时钟源起振失败：多半是晶振没焊好/没接/负载电容不对
         * 交由调用者处理（比如报错提示，或者用默认时钟继续跑） */
        return CLOCK_CONFIG_OSCI_FAIL;
    }

    /* 3. 配置Flash：预取使能、等待周期
     *    GD32F130在48MHz下官方规格是零等待状态，这里配置为WS_WSCNT_0 */
    fmc_prefetch_enable();
    fmc_wscnt_set(FLASH_WAIT_STATE);

    /* 4. 配置AHB/APB1/APB2总线分频系数（先配置分频再切换SYSCLK更安全） */
    rcu_ahb_clock_config(AHB_PRESCALER);
    rcu_apb1_clock_config(APB1_PRESCALER);
    rcu_apb2_clock_config(APB2_PRESCALER);

    /* 5. 配置主PLL参数，时钟源按上面宏切换的结果 */
    rcu_pll_config(PLL_SOURCE, PLL_MUL);

    /* 6. 使能PLL，并等待PLL锁定，带超时保护 */
    rcu_osci_on(RCU_PLL_CK);
    while (RESET == rcu_flag_get(RCU_FLAG_PLLSTB))
    {
        if (++PLLStartUpCounter > 0x5000)
        {
            return CLOCK_CONFIG_PLL_FAIL;
        }
    }

    /* 7. 将系统时钟源切换为PLL输出，并等待切换完成 */
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLL);
    while (RCU_SCSS_PLL != rcu_system_clock_source_get())
    {
    }

    /* 8. 刷新CMSIS全局变量SystemCoreClock，使其反映真实主频
     *    SysTick_Config()、部分delay函数等都依赖这个变量，
     *    不刷新的话它们拿到的还是默认值，会导致定时不准 */
    SystemCoreClockUpdate();

    return CLOCK_CONFIG_OK;
}

/**
 * @brief  获取当前实际的系统主频（Hz），便于调试确认配置是否生效
 * @param  无
 * @retval uint32_t 当前SYSCLK频率
 */
uint32_t SystemClock_GetSysClockFreq(void)
{
    return rcu_clock_freq_get(CK_SYS);
}
