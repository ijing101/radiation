#include "main.h"

void Delay_NOP(unsigned int Del)
{
    for(; Del > 0; Del--)
    {
        ;
    }
}

int main(void)
{
	systick_config();
    gd_eval_com_init();    //RS485初始化
	Hx711_init();          //辐射初始化
	timer2_init();
	
	i2c1_gpio_config();
    i2c1_config();
	//Init_EEPROM();//初始化数据
	Read_EEPROM();//读出设定参数
		
	while(1)
	{	
		MODBUS_DEAL();
		delay_1ms(1);//等待100ms
		Read_Hx711();

	}
}

void TIMER2_IRQHandler(void)
{
    if(timer_interrupt_flag_get(TIMER2, TIMER_INT_UP) != RESET)
    {	
        timer_interrupt_flag_clear(TIMER2, TIMER_INT_UP);
		
    }
}

/*
#include "gd32f1x0.h"

void rcu_config(void)
{

	rcu_regs_deinit();//手动复位 RCU 寄存器

    // 1. 使能 HSE（外部 8MHz 晶振）
    rcu_osci_on(RCU_HXTAL);
    if (SUCCESS != rcu_osci_stab_wait(RCU_HXTAL)) {
        while(1);  // HSE 启动失败
    }
    
    // 2. 配置 Flash 等待状态（48MHz 需 1 WS）
    // 手册：0~24MHz = 0 WS, 24~48MHz = 1 WS
    fmc_wscnt_set(FMC_WAIT_STATE_1);
    
    // 3. 配置总线分频（48MHz 下 APB1/APB2 都可以跑满）
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);   // AHB = 48MHz
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV1);  // APB1 = 48MHz 
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV1);  // APB2 = 48MHz 
    
    // 4. 配置 PLL：HSE × 6 = 48MHz
    rcu_pll_config(RCU_PLLSRC_HXTAL, RCU_PLL_MUL6);
    
    // 5. 使能 PLL
    rcu_osci_on(RCU_PLL_CK);
    rcu_osci_stab_wait(RCU_PLL_CK);
    
    // 6. 切换到 PLL
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLL);
    while (rcu_system_clock_source_get() != RCU_SCSS_PLL);
}
*/
// 手动实现 RCU 的复位 (相当于 rcu_deinit) 
//void rcu_regs_deinit(void)
//{
//    /* 1. 开启内部 IRC8M 振荡器 */
//    RCU_CTL0 |= RCU_CTL0_IRC8MEN;
//    
//    /* 2. 清零 RCU_CFG0 寄存器（结合你上一条发出的定义） */
//    RCU_CFG0 &= ~(RCU_CFG0_SCS    | RCU_CFG0_AHBPSC  | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC  |
//                  RCU_CFG0_PLLSEL | RCU_CFG0_PLLPREDV| RCU_CFG0_PLLMF   | RCU_CFG0_CKOUTSEL | 
//                  RCU_CFG0_CKOUTDIV);

//    /* 3. 关闭 HXTAL (外部晶振) 和 PLL */
//    RCU_CTL0 &= ~(RCU_CTL0_HXTALEN | RCU_CTL0_PLLEN);
//    
//    /* 4. 关闭 HXTAL 旁路模式 (Bypass) */
//    RCU_CTL0 &= ~RCU_CTL0_HXTALBPS;
//    
//    /* 5. 禁用所有时钟中断 (RCU_INT 寄存器的低 16 位通常包含使能位，直接整体清零最安全) */
//    RCU_INT = 0x00000000U;
//    
//    /* 6. 清除所有时钟中断标志位 */
//    // 完全对应你发出来的 IC 结尾的清除宏
//    RCU_INT = RCU_INT_IRC8MSTBIC | RCU_INT_HXTALSTBIC | RCU_INT_PLLSTBIC;
//}


