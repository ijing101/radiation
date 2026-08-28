#ifndef __RS485_H
#define __RS485_H

#include "main.h"

#define RS485_RX_ENABLE     gpio_bit_reset(GPIOB, GPIO_PIN_2)
#define RS485_TX_ENABLE     gpio_bit_set(GPIOB, GPIO_PIN_2)

void Usart2_Send_Byte(uint8_t data);
void Modbus_uart2_init(uint32_t bound);
void Modbus_Send_Byte(uint8_t byte);
void Usart2_SendString(char *str);

#endif /* __RS485_H */