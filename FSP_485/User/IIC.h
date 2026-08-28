#ifndef IIC_H
#define IIC_H

#include "main.h"

#define  Addr_EEP_Adj_Radi          0x00
#define  Addr_EEP_Radi_Accum        Addr_EEP_Adj_Radi + 2
#define  Addr_EEP_Adj_Dir    	    Addr_EEP_Radi_Accum + 2
#define  Addr_EEP_Adj_Flow  		Addr_EEP_Adj_Dir + 2  
//-----------------------------------------------------------------------------//
#define  Addr_EEP_Saved_NO          Addr_EEP_Adj_Flow + 20  //是否已初始化地址,清除数据就是清这个
#define  Addr_EEP_Saved_Time        Addr_EEP_Saved_NO + 2
#define   AT24C_VALUE        2//256//1024
#if AT24C_VALUE ==1
#define PAGE_SIZE 8
#define SIZE 0x007f
#elif AT24C_VALUE==2
#define PAGE_SIZE 8
#define SIZE 0x00ff
#elif AT24C_VALUE==4
#define PAGE_SIZE 16
#define SIZE 0x01ff
#elif AT24C_VALUE==8
#define PAGE_SIZE 16
#define SIZE 0x03ff
#elif AT24C_VALUE==16
#define PAGE_SIZE 16
#define SIZE 0x07ff
#elif AT24C_VALUE==32
#define PAGE_SIZE 32
#define SIZE 0x0fff
#elif AT24C_VALUE==64
#define PAGE_SIZE 32
#define SIZE 0x1fff
#elif AT24C_VALUE==128
#define PAGE_SIZE 64
#define SIZE 0x3fff
#elif AT24C_VALUE==256
#define PAGE_SIZE 64
#define SIZE 0x7fff
#elif AT24C_VALUE==512
#define PAGE_SIZE 128
#define SIZE 0xffff
#elif AT24C_VALUE==1024
#define PAGE_SIZE 256
#define SIZE 0xffff
#endif
extern unsigned int Param_Radi[2];      	//平均
extern unsigned int Param_Radi_ALL[2];		//累计
extern unsigned int Saved_Time;             //存储数据时间间隔
extern unsigned int Saved_NO;               //已存储数据大小,清除数据就是清这个
extern unsigned int Param_Adj_Flow[2];      //从机地址

#define I2C_WP_Pin			GPIO_PIN_2
#define I2C_SCL_Pin			GPIO_PIN_0
#define I2C_SDA_Pin			GPIO_PIN_1
#define I2C_Port			GPIOA
#define I2C_RCU  			RCU_GPIOA

void IIC_Write_Int(unsigned int *byte,unsigned int address,unsigned int count);
void IIC_Read_Int(unsigned int *byte,unsigned int address,unsigned int count);
void STOP_Save_Data(void);
void Clear_Accum_Data(void);
void EEP_Read_Flow(void);
void EEP_Write_Flow(void);
void Read_EEPROM(void);//读出设定参数
void Init_EEPROM(void);//初始化数据

void delay_ms(uint32_t count);
void i2c1_gpio_config(void);
void i2c1_config(void);
uint8_t at24c02_byte_write(uint8_t mem_addr, uint8_t data);
uint8_t at24c02_byte_read(uint8_t mem_addr, uint8_t *pdata);
uint8_t at24c02_page_write(uint8_t mem_addr, uint8_t *pdata, uint8_t len);
uint8_t at24c02_sequential_write(uint8_t mem_addr, uint8_t *pdata, uint16_t len);
uint8_t at24c02_sequential_read(uint8_t mem_addr, uint8_t *pdata, uint16_t len);

#endif
