#ifndef _EEPROM_H
#define _EEPROM_H

#include "main.h"   /* 使本头文件自包含：uint8_t/uint16_t 等类型可用 */

#define IIC_SCL_PORT     	GPIOA
#define IIC_SCL_PIN      	GPIO_PIN_0
#define IIC_SDA_PORT     	GPIOA
#define IIC_SDA_PIN      	GPIO_PIN_1
#define IIC_WP_PORT     	GPIOA
#define IIC_WP_PIN      	GPIO_PIN_4	//GPIO_PIN_2//FSP3-PA2

void IIC_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_Send_Byte(uint8_t txd);
uint8_t   IIC_Read_Byte(unsigned char ack);
uint8_t   IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);
void IIC_Write_One_Byte(uint8_t daddr, uint8_t addr, uint8_t data);
uint8_t   IIC_Read_One_Byte(uint8_t daddr, uint8_t addr);

#define AT24C01     127
#define AT24C02     255
#define AT24C04     511
#define AT24C08     1023
#define AT24C16     2047
#define AT24C32     4095
#define AT24C64     8191
#define AT24C128    16383
#define AT24C256    32767
#define EE_TYPE     AT24C02

void     slave_add_write(uint16_t add);
uint8_t  slave_add_read(void);
void     bound_add_write(uint32_t add);
uint32_t bound_add_read(void);

void AT24CXX_Init(void);
uint8_t   AT24CXX_ReadOneByte(uint16_t ReadAddr);
void AT24CXX_WriteOneByte(uint16_t WriteAddr, uint8_t DataToWrite);
void AT24CXX_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len);
uint32_t  AT24CXX_ReadLenByte(uint16_t ReadAddr, uint8_t Len);
uint8_t   AT24CXX_Check(void);
void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead);
void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite);

#endif /* _EEPROM_H */

