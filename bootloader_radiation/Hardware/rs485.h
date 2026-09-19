#ifndef __RS485_H
#define __RS485_H
#include "sys.h" 

extern uint8_t RS485_RX_BUF[64]; 		
extern uint8_t RS485_RX_CNT;   		
#define RS485_ENABLE

#define RS485_TX_EN		 GPIO_SetBits(GPIOB,GPIO_Pin_2);
#define RS485_RX_EN		 GPIO_ResetBits(GPIOB,GPIO_Pin_2);

#define RS485_RX_ENABLE   GPIO_ResetBits(GPIOB,GPIO_Pin_2); //接收使能、低电平有效
#define RS485_TX_ENABLE   GPIO_SetBits(GPIOB,GPIO_Pin_2); //发送使能、高电平有效

void RS485_Send_Data(uint8_t *buf, uint8_t len);
void RS485_Receive_Data(uint8_t *buf, uint8_t *len);
void RS485_Init(uint32_t bound);

// 发送一个字节的函数
void Usart2_Send_Byte(uint8_t data);


void Usart2_SendString(char *str);//发送字符串

void RS485_Init(u32 bound);
#endif




	 