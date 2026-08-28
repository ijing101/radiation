#ifndef _MS8607_h
#define _MS8607_h
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#define MS8607_Rest_H      1   //
#define MS8607_Rest_L      0   //复位

#define MS8607_SCL_H         GPIOB->BSRR = GPIO_Pin_10
#define MS8607_SCL_L         GPIOB->BRR  = GPIO_Pin_10
   
#define MS8607_SDA_H         GPIOB->BSRR = GPIO_Pin_11
#define MS8607_SDA_L         GPIOB->BRR  = GPIO_Pin_11

#define MS8607_SCL_read      GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_10)
#define MS8607_SDA_read      GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11)
typedef struct
{
	unsigned short MS_PROM[8];   //存储压力传感器的PROM数据
	unsigned int  Temp_D2;       //存储温度相关的D2数据
	unsigned int  Pressure_D1;   //存储压力相关的D1数据
	unsigned short  Tumi_D3;     //存储湿度相关的D3数据
	float  temp;                 //存储最终计算的温度值
	float  humi;                 //存储最终计算的湿度值
	float  pressure;             //存储最终计算的压力值
}MS;
extern MS MS8607;

void MS8607_IIC_Config(void);
int MS8607_ReadDate(void);


#endif


