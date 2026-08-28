#ifndef USART_H
#define USART_H
#include "main.h"
//-------------------------------------DEFINES--------------------------------//
#define RX_BUFFER_SIZE0   64U                // 接收缓冲区大小，可根据需要修改
#define TX_BUFFER_SIZE0   86U
//#define Slave_ADD   0xAA
//-------------------------------------DATA-----------------------------------//
extern char rx_buffer0[RX_BUFFER_SIZE0]; //接收原始数据缓冲
extern char tx_buffer0[TX_BUFFER_SIZE0];      //发送数据缓冲
extern char Recv_Buf0[RX_BUFFER_SIZE0];      //接收数据缓冲
extern char Send_Buf0[TX_BUFFER_SIZE0];      //发送数据缓冲
extern volatile unsigned char F_Data_Begin0;        //数据头开始标志
extern volatile unsigned char F_Data_Error0;        //数据比较错误标志
extern volatile unsigned char Recv_Data_Len;       //有效接收数据长读
extern volatile unsigned char Send_Data_Len;       //有效发送数据长读
extern volatile unsigned char UART0_RX_NO;    //串口收到的数据量
extern uint8_t Uart_data_flag;
void gd_eval_com_init(void);
void UART_Write(char *pData, uint32_t dataLen);
//extern unsigned char MODBUS_RX(void);
extern void MODBUS_DEAL(void);
extern unsigned char HEX_ASCII(unsigned char Data);   //HEX TO ASCII
extern unsigned char ASCII_HEX(unsigned char Data);   //ASCII TO HEX
extern unsigned int CalculateCRC16(char *Data_Tmp,unsigned char D_Len);  //累加
extern unsigned int CalculateBCC16(char* pchMsg,unsigned char wDataLen); //CRC16
static void Modbus_Function_3(void);
static void Modbus_Function_5(void);
static void Modbus_Function_16(void);

#endif
