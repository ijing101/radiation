/**
 * @file    main.h
 * @brief   主程序头文件
 * @note    单串口架构：USART0（PA9=TX，PA10=RX）经 RS485 收发器同时承担
 *          Modbus 从机通信与调试日志输出。
 */
#ifndef __MAIN_H
#define __MAIN_H

/* 公共头文件聚合（原 gd32_common.h 的内容整合到此处，工程内统一包含 main.h 即可） */
#include "gd32f1x0.h"
#include "gd32f1x0_libopt.h"
#include "delay.h"
#include <string.h>
#include <stdint.h>

/* ============================================================================
 * Modbus 保持寄存器地址映射表（从机视角，上位机用 0x03 读、0x06/0x10 写）
 * ----------------------------------------------------------------------------
 * Reg[0]  辐射整型值        （只读）标定并限幅后的辐射计数，范围 0~1268
 * Reg[1]  辐射浮点值 高16位 （只读）IEEE754，与 Reg[2] 组成一个 float
 * Reg[2]  辐射浮点值 低16位 （只读）IEEE754，高字在前（大端）
 * Reg[3]  辐射原始定标值    （只读）调试用，量程标定前的中间值低 16 位
 * Reg[8]  波特率            （可写）0=9600，1=115200，写后保存并复位生效
 * Reg[16] 从机地址          （可写）1~99，写后保存并复位生效
 * Reg[17] IAP 升级触发      （可写）写入 0x1234 触发进入 Bootloader 升级
 * Reg[18] 辐射灵敏度        （可写）Param_Adj_Radi[0]，写后存 EEPROM，03 可读
 * Reg[19] 修正系数          （可写）Param_Adj_Dir[0]，写后存 EEPROM，03 可读
 * ==========================================================================*/

/* 辐射浮点输出的换算系数：浮点值 = 整型值 / 该系数。
 * 例如 100.0f 表示浮点保留 2 位小数；按实际量程/单位修改即可。 */
#define RADIATION_FLOAT_SCALE   100.0f

/* Modbus 寄存器地址定义（标定参数，与上表对应） */
#define REG_PARAM_RADI   18   /* 辐射灵敏度 Param_Adj_Radi[0] */
#define REG_PARAM_DIR    19   /* 修正系数 Param_Adj_Dir[0] */

#endif /* __MAIN_H */
