/**
 * @file    Hx711.h
 * @brief   HX711 24 位 ADC 驱动接口
 * @details HX711 是一款 24 位高精度 A/D 芯片，常用于称重传感器/辐射探头信号采集。
 *          本驱动通过 GPIO 软件时序（位控）读取 24 位转换结果，不占用硬件 SPI。
 *          引脚定义：SCK = PA7，DOUT = PA6。
 */
#ifndef __HX711_H
#define __HX711_H

#include "main.h"

/* ---- 引脚定义 ---- */
#define Hx711_SCK    GPIO_PIN_7    /* 时钟输出，PA7 */
#define Hx711_DOUT   GPIO_PIN_6    /* 数据输入，PA6（HX711 数据就绪/输出） */
#define Hx711_Port   GPIOA         /* 所在端口 */
#define Hx711_RCU    RCU_GPIOA     /* 端口时钟 */

/* ---- 标定参数与测量结果（全局变量，供 main/Modbus 读取） ---- */
extern unsigned int Param_Adj_Radi[2];     /* 辐射量程标定系数 */
extern unsigned int Param_Adj_Dir[5];      /* 辐射方向/校准系数 */
extern unsigned int Radi_Back[2];          /* 原始定标后的整型测量值 */
extern unsigned int Param_Radi_1[2];       /* 标定后的最终整型测量值（限幅 0~1268） */
extern unsigned int Param_Radi_ALL_1[2];   /* （保留） */
extern unsigned int Radi_ALL[2];           /* （保留） */

/* ---- 接口函数 ---- */
unsigned int Hx711_Data(void);             /* 读 24 位原始数据（一次完整转换） */
void         Hx711_init(void);             /* 初始化 GPIO 并预热 */
void         Read_Hx711(void);             /* 读取并处理一次测量（非阻塞） */
void         Hx711_Load_Calibration(void); /* 从 EEPROM 装载标定参数（含默认值兜底） */
void         Hx711_Save_Calibration(void); /* 标定参数写回 EEPROM（Modbus 写入时调用） */

#endif /* __HX711_H */
