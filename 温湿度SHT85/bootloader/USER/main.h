#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h" 

// BUG修复：禁用UART调试输出，避免干扰Ymodem协议
// Ymodem协议使用UART1通信，uart_log也使用UART1，会导致数据混乱
// #define  UART_DEBUG

#ifdef UART_DEBUG
#define uart_log   printf
#else
#define uart_log(...)
#endif

#endif
