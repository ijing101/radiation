/**
 * @file ymodem.h
 * @brief USART0（PA9/PA10，9600 baud）上的可靠 YMODEM 接收器。
 */
#ifndef __YMODEM_H
#define __YMODEM_H

#include "gd32f1x0.h"
#include "bootloader.h"
#include "crc_check.h"

#define YMODEM_SOH 0x01U
#define YMODEM_STX 0x02U
#define YMODEM_EOT 0x04U
#define YMODEM_ACK 0x06U
#define YMODEM_NAK 0x15U
#define YMODEM_CA  0x18U
#define YMODEM_C   0x43U

#define MAX_QUEUE_SIZE 1200U

typedef void (*ymodem_callback)(process_status);

typedef struct
{
    process_status process;
    uint8_t status;
    uint8_t id;
    uint32_t addr;
    uint32_t image_size;
    uint32_t received_size;
    uint8_t expected_packet;
    uint8_t sectors_size;
    ymodem_callback cb;
} ymodem_t;

typedef struct
{
    uint8_t queue[MAX_QUEUE_SIZE];
    int rear;
    int front;
    int count;
} seq_queue_t;

typedef struct
{
    uint8_t data[1100];
    uint16_t len;
} download_buf_t;

extern ymodem_t ymodem;
extern seq_queue_t rx_queue;

void ymodem_ack(void);
void ymodem_nack(void);
void ymodem_c(void);
void queue_initiate(seq_queue_t *queue);
int queue_not_empty(seq_queue_t *queue);
int queue_append(seq_queue_t *queue, uint8_t value);
int queue_delete(seq_queue_t *queue, uint8_t *value);
void set_ymodem_status(process_status process);
process_status get_ymodem_status(void);
void ymodem_reset_transfer(void);
/* 只放弃未完成传输；不修改升级 metadata 或旧 APP 备份。 */
void ymodem_abort_transfer(void);
/* 返回自上次调用后是否收到过 YMODEM 数据。 */
uint8_t ymodem_take_rx_activity(void);
void ymodem_recv(download_buf_t *packet);
void ymodem_init(void);
/* 避免与 GD32 标准库 timer_init(uint32_t, ...) 同名。 */
void ymodem_timer_init(void);

#endif /* __YMODEM_H */
