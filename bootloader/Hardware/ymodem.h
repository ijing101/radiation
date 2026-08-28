/**
 * @file    ymodem.h
 * @brief   Ymodem 升级协议处理接口
 * @details 基于 USART0 与 TIMER2 实现，TIMER2 作为帧间超时定时器
 */
#ifndef __YMODEM_H
#define __YMODEM_H

#include "gd32f1x0.h"
#include <string.h>
#include "bootloader.h"
#include "crc_check.h"

/* Ymodem 协议控制字符 */
#define YMODEM_SOH      0x01    /* 128 字节数据帧起始 */
#define YMODEM_STX      0x02    /* 1024 字节数据帧起始 */
#define YMODEM_EOT      0x04    /* 传输结束 */
#define YMODEM_ACK      0x06    /* 应答 ACK */
#define YMODEM_NAK      0x15    /* 否定应答 NAK */
#define YMODEM_CA       0x18    /* 取消传输 */
#define YMODEM_C        0x43    /* 字符 'C'，请求 CRC 模式传输 */

/* 接收队列大小（一个 STX 帧最大 1024 + 3 头 + 2 CRC = 1029 字节） */
#define MAX_QUEUE_SIZE  1200

/* Ymodem 回调函数类型 */
typedef void (*ymodem_callback)(process_status);

/* Ymodem 控制结构体 */
typedef struct
{
    process_status process;      /* 主流程状态（与 main 共享） */
    uint8_t status;              /* Ymodem 内部子状态 0~3 */
    uint8_t id;                  /* 包序号 */
    uint32_t addr;               /* 当前写入地址 */
    uint8_t sectors_size;        /* 需要擦除的扇区数 */
    ymodem_callback cb;          /* 回调函数 */
} ymodem_t;

/* 环形队列（接收缓冲） */
typedef struct
{
    uint8_t queue[MAX_QUEUE_SIZE];
    int rear;                    /* 队尾指针 */
    int front;                   /* 队头指针 */
    int count;                   /* 元素个数 */
} seq_queue_t;

/* 接收数据缓冲 */
typedef struct
{
    uint8_t data[1100];          /* 数据缓存 */
    uint16_t len;                /* 数据长度 */
} download_buf_t;

extern seq_queue_t rx_queue;

void ymodem_ack(void);
void ymodem_nack(void);
void ymodem_c(void);

void queue_initiate(seq_queue_t *Q);
int queue_not_empty(seq_queue_t *Q);
int queue_delete(seq_queue_t *Q, uint8_t *d);

void set_ymodem_status(process_status process);
process_status get_ymodem_status(void);
void ymodem_recv(download_buf_t *p);
void ymodem_init(void);
void Timer_init(void);

#endif /* __YMODEM_H */
