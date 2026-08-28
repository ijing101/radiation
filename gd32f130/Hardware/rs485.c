#include "gd32_common.h"
#include "rs485.h"
#include "timer.h"
#include "modbus.h"
#include <string.h>

static uint8_t usart_send_data(uint8_t *buf, uint16_t len, uint32_t usart)
{
    uint16_t t;
    for (t = 0; t < len; t++) {
        while (usart_flag_get(usart, USART_FLAG_TBE) == RESET) {
        }
        usart_data_transmit(usart, buf[t]);
    }
    while (usart_flag_get(usart, USART_FLAG_TC) == RESET) {
    }
    return 0;
}

void Usart2_Send_Byte(uint8_t data)
{
    RS485_TX_ENABLE;
    while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET) {
    }
    usart_data_transmit(USART0, data);
    while (usart_flag_get(USART0, USART_FLAG_TC) == RESET) {
    }
    RS485_RX_ENABLE;
}

void Usart2_SendString(char *str)
{
    uint16_t length = (uint16_t)strlen(str);
    RS485_TX_ENABLE;
    usart_send_data((uint8_t *)str, length, USART0);
    RS485_RX_ENABLE;
}

void Modbus_Send_Byte(uint8_t byte)
{
    usart_data_transmit(USART0, byte);
    while (usart_flag_get(USART0, USART_FLAG_TC) == RESET) {
    }
    while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET) {
    }
    usart_flag_clear(USART0, USART_FLAG_TC);
}

void Modbus_uart2_init(uint32_t bound)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_USART0);

    /* 注意：RS485 方向控制 PB2（高=发送，低=接收）。
       若封装/原理图改用其他引脚，仅需改 rs485.h 的宏与这里。 */
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    gpio_bit_reset(GPIOB, GPIO_PIN_2);

    /* USART0 默认复用 AF0：PA9=TX，PA10=RX（对应原理图“出口1 PA10/PA9”）。
       注意：USART0 若需改到其他复用脚，须同步改 gpio_af_set 与引脚。 */
    gpio_af_set(GPIOA, GPIO_AF_0, GPIO_PIN_9);
    gpio_af_set(GPIOA, GPIO_AF_0, GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);

    rcu_periph_reset_enable(RCU_USART0RST);
    rcu_periph_reset_disable(RCU_USART0RST);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, bound);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);

    nvic_irq_enable(USART0_IRQn, 2, 1);
    usart_interrupt_enable(USART0, USART_INT_RBNE);

    RS485_RX_ENABLE;
}

void USART0_IRQHandler(void)
{
    uint8_t Res;

    if (usart_flag_get(USART0, USART_FLAG_RBNE) != RESET) {
        Res = (uint8_t)usart_data_receive(USART0);

        /* 注意：上一帧尚未被主循环 Modbus_Event 处理时丢弃新数据，
           避免破坏 rcbuf。单主轮询下应答后再来下一帧，不受影响。 */
        if (modbus.reflag == 1U) {
            return;
        }

        /* 首字节地址过滤：仅接收本机地址或广播地址 0x00。 */
        if (modbus.recount == 0U) {
            if ((Res != modbus.myadd_cached) && (Res != 0x00U)) {
                return;
            }
        }

        /* 接收缓冲 rcbuf[100] 上限保护 */
        if (modbus.recount >= 100) {
            modbus.recount = 0;
            modbus.timrun  = 0;
            return;
        }

        modbus.rcbuf[modbus.recount++] = Res;
        modbus.timout = 0;
        if (modbus.recount == 1U) {
            modbus.timrun = 1;
        }
    }
}