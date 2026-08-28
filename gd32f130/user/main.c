#include "gd32_common.h"
#include "rs485.h"
#include "timer.h"
#include "modbus.h"
#include "eeprom.h"
#include "wdg.h"

int main(void)
{
    uint32_t bound;
    uint8_t  addr;

    /* 中断向量重定位到 APP 起点 0x08005000（GD32 Flash 基址 0x08000000）。
       必须与 Bootloader 约定一致，否则中断不响应。 */
    SCB->VTOR = FLASH_BASE | 0x5000;
    __enable_irq();

    gd32_clock_init();
    IWDG_Init(6, 2344);
    delay_ms(500);

    Modbus_uart2_init(9600);
    TIM3_Int_Init(1000 - 1, 32 - 1);

    Modbus_Init();
    AT24CXX_Init();

    Usart2_SendString("GD32F130 Modbus Slave V6.0\r\n");
    delay_ms(100);

    while (AT24CXX_Check()) {
        /* 注意：检测不到 EEPROM 时在此循环提示并喂狗（不会跑业务）。
           如希望“缺 EEPROM 也继续运行”，需把本循环改为超时退出。 */
        Usart2_SendString("EEPROM not found\r\n");
        delay_ms(1000);
        IWDG_Feed();
    }

    /* bound 存 0(9600) 或 1(115200) */
    bound = bound_add_read();
    if (bound != 0 && bound != 1) {
        bound = 0;
    }
    modbus.bound = (bound == 1) ? 115200 : 9600;
    Reg[8] = (uint16_t)bound;

    addr = slave_add_read();
    modbus.myadd = addr;
    modbus.myadd_cached = addr;
    Reg[16] = addr;

    Modbus_uart2_init(modbus.bound);
    delay_ms(100);

    for (;;) {
        IWDG_Feed();
        Modbus_Event();
    }
}