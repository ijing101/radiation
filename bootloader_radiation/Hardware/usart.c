/**
 * @file    usart.c
 * @brief   USART0 串口驱动实现（单串口架构）
 * @details USART0 映射：PA9 = TX，PA10 = RX，
 *          同时承担 Ymodem 升级协议通信与调试日志输出
 */
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>

#define CONSOLE_BUF_SIZE    128     /* 格式化输出缓冲大小 */

/**
 * @brief  初始化 USART0（PA9=TX，PA10=RX，8N1，接收中断）
 * @param  baud: 波特率
 */
void usart0_init(uint32_t baud)
{
    /* 使能 GPIOA 与 USART0 时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    /* 配置 PA9(TX)/PA10(RX) 为复用功能 AF1 */
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_9 | GPIO_PIN_10);
    /* 发送引脚 PA9 配置为推挽输出 */
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

    /* 复位 USART0 并配置参数（8 数据位 / 无校验 / 1 停止位 / 无流控） */
    usart_deinit(USART0);
    usart_baudrate_set(USART0, baud);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART0, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART0, USART_CTS_DISABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);

    /* 使能接收中断（RBNE），抢占优先级 1 / 子优先级 0 */
    usart_interrupt_enable(USART0, USART_INT_RBNE);
    nvic_irq_enable(USART0_IRQn, 1, 0);

    /* 使能 USART0 */
    usart_enable(USART0);
}

/**
 * @brief  阻塞发送单字节
 * @param  data: 待发送字节
 */
void usart0_send_byte(uint8_t data)
{
    /* 等待发送缓冲区为空（TBE） */
    while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET) {
    }
    usart_data_transmit(USART0, data);
}

/**
 * @brief  阻塞发送数据块
 * @param  buf: 数据指针
 * @param  len: 数据长度
 */
void usart0_send_data(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        usart0_send_byte(buf[i]);
    }
}

/**
 * @brief  发送字符串（以 '\0' 结尾）
 * @param  str: 字符串指针
 */
void console_send_string(const char *str)
{
    while (*str) {
        usart0_send_byte((uint8_t)*str++);
    }
}

/**
 * @brief  格式化输出（类似 printf，输出到 USART0）
 * @param  fmt: 格式字符串
 */
void console_printf(const char *fmt, ...)
{
    static char buf[CONSOLE_BUF_SIZE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    console_send_string(buf);
}
