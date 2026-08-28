#ifndef _SHT85_H
#define _SHT85_H

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

// I2C引脚定义 (使用PB10=SCL, PB11=SDA)
#define SHT85_SCL_H         GPIOB->BSRR = GPIO_Pin_10
#define SHT85_SCL_L         GPIOB->BRR  = GPIO_Pin_10
   
#define SHT85_SDA_H         GPIOB->BSRR = GPIO_Pin_11
#define SHT85_SDA_L         GPIOB->BRR  = GPIO_Pin_11

#define SHT85_SCL_read      GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10)
#define SHT85_SDA_read      GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11)

// SHT85 I2C地址 (7位地址左移1位)
#define SHT85_ADDR_W        (0x44 << 1)       // 写地址 0x88
#define SHT85_ADDR_R        ((0x44 << 1) + 1) // 读地址 0x89

// SHT85命令定义 (单次测量模式)
#define SHT85_CMD_MEAS_HIGH_MSB     0x24  // 高重复性测量
#define SHT85_CMD_MEAS_HIGH_LSB     0x00
#define SHT85_CMD_MEAS_MED_MSB      0x24  // 中重复性测量
#define SHT85_CMD_MEAS_MED_LSB      0x0B
#define SHT85_CMD_MEAS_LOW_MSB      0x24  // 低重复性测量
#define SHT85_CMD_MEAS_LOW_LSB      0x16

#define SHT85_CMD_SOFT_RESET_MSB    0x30  // 软复位
#define SHT85_CMD_SOFT_RESET_LSB    0xA2

#define SHT85_CMD_HEATER_EN_MSB     0x30  // 使能加热器
#define SHT85_CMD_HEATER_EN_LSB     0x6D
#define SHT85_CMD_HEATER_DIS_MSB    0x30  // 禁用加热器
#define SHT85_CMD_HEATER_DIS_LSB    0x66

#define SHT85_CMD_READ_STATUS_MSB   0xF3  // 读状态寄存器
#define SHT85_CMD_READ_STATUS_LSB   0x2D
#define SHT85_CMD_CLEAR_STATUS_MSB  0x30  // 清除状态寄存器
#define SHT85_CMD_CLEAR_STATUS_LSB  0x41

// 数据结构
typedef struct
{
    unsigned short temp_raw;    // 温度原始ADC值
    unsigned short humi_raw;    // 湿度原始ADC值
    float temp;                 // 温度值 (°C)
    float humi;                 // 湿度值 (%)
} SHT85_Data;

extern SHT85_Data SHT85;

// 函数声明
void SHT85_IIC_Config(void);
int SHT85_ReadData(void);
int SHT85_SoftReset(void);

#endif
