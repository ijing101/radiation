#ifndef __YMODEM_H
#define __YMODEM_H

#include "string.h"
#include "stm32f10x.h"
#include "usart.h"
#include "bootloader.h"
#include "main.h"
#include "delay.h"
#include "crc_check.h"

#define YMODEM_SOH		0x01
#define YMODEM_STX		0x02
#define YMODEM_EOT		0x04
#define YMODEM_ACK		0x06
#define YMODEM_NAK		0x15
#define YMODEM_CA		  0x18
#define YMODEM_C		  0x43

#define MAX_QUEUE_SIZE  1200

typedef void (*ymodem_callback)(process_status);

typedef struct 
{
	process_status process;
	uint8_t status;
	uint8_t id;
	uint32_t addr;
	uint8_t sectors_size;
	ymodem_callback cb;
} ymodem_t;

// 环形队列（串口接收缓冲）
typedef struct
{
	uint8_t queue[MAX_QUEUE_SIZE];
	int rear;  // 尾指针
	int front;  // 头指针
	int count;  // 元素个数
} seq_queue_t; 

typedef struct 
{
	uint8_t data[1100];
	uint16_t len;
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
void ymodem_start(ymodem_callback cb);
void ymodem_recv(download_buf_t *p);
void ymodem_init(void);
void ymodem_handle(void);
void timer_init(void);

#endif
