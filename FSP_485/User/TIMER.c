#include "TIMER.h"

unsigned int timer_flag = 0;
unsigned int Save_Count = 0;
unsigned int Radi_Count = 0;
void timer2_init(void)
{
    rcu_periph_clock_enable(RCU_TIMER2);   
    timer_deinit(TIMER2);
    
    timer_parameter_struct timer_initpara;
    timer_struct_para_init(&timer_initpara);
    
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;  	 // 不分频
    timer_initpara.counterdirection = TIMER_COUNTER_UP;  
    timer_initpara.period = 999;  			// 自动重装载值
    timer_initpara.prescaler = 7199;  		// 预分频值
    timer_initpara.repetitioncounter = 0;   // 重复计数器值
    
    timer_init(TIMER2, &timer_initpara);
    timer_interrupt_flag_clear(TIMER2, TIMER_INT_UP);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);

    nvic_irq_enable(TIMER2_IRQn, 1, 1);  // 抢占优先级1，子优先级1    
    timer_enable(TIMER2);
}

void Save_Record(void)//定时保存数据
{
	if(timer_flag >= 60)
	{
		Save_Count = 0;
		for(unsigned char i = 0; i < 2; i++)
        {
            Param_Radi[i] = Param_Radi_ALL_1[i]/Radi_Count;
        }
		for(unsigned char i = 0; i < 2; i++)
        {
           Param_Radi_ALL_1[i] = 0;
        }
		for(unsigned char i = 0; i < 1; i++)
        {
            Radi_ALL[i] += Param_Radi[i]*60;
            
            Param_Radi_ALL[i] = Radi_ALL[i]/1000;	//累计值
        }
//		STOP_Save_Data();
		Radi_Count = 0;
		timer_flag = 0;
	}
}

void Test_Time(void)
{
		Save_Record();		//定时保存数据				
		for(unsigned char i = 0; i < 2; i++)
		{
			Param_Radi_ALL_1[i] += Param_Radi_1[i];
		}
		Radi_Count++;
}

/*
计算说明：
GD32F130默认系统时钟为72MHz

定时器时钟频率 = 72MHz / (预分频值 + 1) = 72MHz / (7199 + 1) = 10kHz
定时时间 = (自动重装载值 + 1) / 定时器时钟频率 = (9999 + 1) / 10kHz = 10000 / 10000 = 1秒

*/
//void TIMER2_IRQHandler(void)
//{
//    if(timer_interrupt_flag_get(TIMER2, TIMER_INT_UP) != RESET)
//    {
//		Test_Time();
//		timer_flag++;
//		if(timer_flag > 60)
//		{
////			Save_Count = 1;
//			timer_flag = 0;
//		}
//        timer_interrupt_flag_clear(TIMER2, TIMER_INT_UP);
//    }
//}
