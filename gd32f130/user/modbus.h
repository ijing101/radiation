#ifndef _MODBUS_H
#define _MODBUS_H

#include "gd32_common.h"
#include "modbus_crc.h"
#include "rs485.h"
#include "eeprom.h"

typedef struct {
    /* 从机通信 */
    uint8_t  myadd;          /* 本机从机地址 */
    uint8_t  myadd_cached;   /* 缓存地址，供中断地址过滤用，避免频繁取本机地址 */
    uint8_t  rcbuf[100];     /* 接收缓存 */
    uint8_t  timout;         /* 帧超时计数 */
    uint8_t  recount;        /* 已收字节数 */
    uint8_t  timrun;         /* 超时计时运行标志 */
    uint8_t  reflag;         /* 一帧接收完成标志，主循环处理 */
    uint8_t  sendbuf[100];   /* 发送缓存 */
    uint32_t bound;          /* 当前波特率 */

    /* 主机模式预留字段（本移植未使用，计时器仍在更新 Host_Sendtime） */
    uint8_t  Host_Txbuf[8];
    uint8_t  slave_add;
    uint8_t  Host_send_flag;
    int Host_Sendtime;
    uint8_t  Host_time_flag;
    uint8_t  Host_End;
} MODBUS;

extern MODBUS modbus;
extern uint16_t Reg[];

void Modbus_Init(void);
void Modbus_Func3(void);
void Modbus_Func6(void);
void Modbus_Func6_Broadcast(void);
void Modbus_Func16(void);
void Modbus_Func16_Broadcast(void);
void Modbus_Event(void);

#endif /* _MODBUS_H */