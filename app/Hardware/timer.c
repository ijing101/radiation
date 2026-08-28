#include "timer.h"
#include "rs485.h"

uint8_t sec_flag = 0;

extern MODBUS modbus;

/* 注意：系统时钟 48MHz 时，psc=47、arr=999 -> 中断周期 1ms。
   若更换系统时钟，须同步调整 main 中的 TIM3_Int_Init 参数。 */
void TIM3_Int_Init(uint16_t arr, uint16_t psc)
{
    timer_parameter_struct timer_initpara;

    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);

    timer_initpara.prescaler         = (uint16_t)psc;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = (uint32_t)arr;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2, &timer_initpara);

    timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);
    timer_enable(TIMER2);

    nvic_irq_enable(TIMER2_IRQn, 2, 3);
}

void TIMER2_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER2, TIMER_INT_FLAG_UP) != RESET) {
        timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);

        if (modbus.timrun != 0U) {
            modbus.timout++;
            /* 约 12ms 无新字节 -> 判定一帧结束，置 reflag 交给主循环处理。
               12ms 对 9600/115200 均足够大于帧尾间隔(3.5字符)。 */
            if (modbus.timout >= 12) {
                modbus.timrun = 0;
                modbus.reflag = 1;
            }
        }

        modbus.Host_Sendtime++;
        if (modbus.Host_Sendtime > 1000) {
            modbus.Host_time_flag = 1;
        }
    }
}