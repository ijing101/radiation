/**
 * @file    usart.h
 * @brief   USART0 串口驱动接口（单串口架构）
 * @details USART0 映射：PA9 = TX，PA10 = RX，
 *          同时承担 Ymodem 升级协议通信与调试日志输出
 */
#ifndef __USART_H
#define __USART_H

#include "gd32f1x0.h"

void usart0_init(uint32_t baud);                              /* 初始化 USART0 */
void usart0_send_byte(uint8_t data);                          /* 阻塞发送单字节 */
void usart0_send_data(const uint8_t *buf, uint16_t len);      /* 阻塞发送数据块 */
void console_send_string(const char *str);                    /* 发送字符串 */
void console_printf(const char *fmt, ...);                    /* 格式化发送 */

#endif /* __USART_H */
