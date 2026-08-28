/**
 * @file    ymodem.c
 * @brief   Ymodem 升级协议实现
 * @details 基于 USART0（PA9/PA10）与 TIMER2 实现，
 *          TIMER2 作为帧间超时定时器（1ms 无字节则认为一帧结束）
 */
#include "ymodem.h"
#include "usart.h"

/* 全局变量 */
static ymodem_t ymodem;         /* Ymodem 控制结构 */
static download_buf_t recvBuf;  /* 帧接收缓冲 */
seq_queue_t rx_queue;           /* 接收环形队列 */

/**
 * @brief  初始化队列
 */
void queue_initiate(seq_queue_t *Q)
{
    Q->rear = 0;
    Q->front = 0;
    Q->count = 0;
}

/**
 * @brief  判断队列是否非空
 * @retval 1=非空, 0=空
 */
int queue_not_empty(seq_queue_t *Q)
{
    return (Q->count != 0) ? 1 : 0;
}

/**
 * @brief  入队
 * @retval 1=成功, 0=队列已满
 */
int queue_append(seq_queue_t *Q, uint8_t x)
{
    if (Q->count >= MAX_QUEUE_SIZE) {
        return 0;
    }

    Q->queue[Q->rear] = x;
    Q->rear = (Q->rear + 1) % MAX_QUEUE_SIZE;
    Q->count++;
    return 1;
}

/**
 * @brief  出队
 * @retval 1=成功, 0=队列已空
 */
int queue_delete(seq_queue_t *Q, uint8_t *d)
{
    if (Q->count == 0) {
        return 0;
    }

    *d = Q->queue[Q->front];
    Q->front = (Q->front + 1) % MAX_QUEUE_SIZE;
    Q->count--;
    return 1;
}

/**
 * @brief  发送 ACK
 */
void ymodem_ack(void)
{
    usart0_send_byte(YMODEM_ACK);
}

/**
 * @brief  发送 NAK
 */
void ymodem_nack(void)
{
    usart0_send_byte(YMODEM_NAK);
}

/**
 * @brief  发送 'C'（请求 CRC 模式传输）
 */
void ymodem_c(void)
{
    usart0_send_byte(YMODEM_C);
}

/**
 * @brief  设置 Ymodem 主状态
 */
void set_ymodem_status(process_status process)
{
    ymodem.process = process;
}

/**
 * @brief  获取 Ymodem 主状态
 */
process_status get_ymodem_status(void)
{
    return ymodem.process;
}

/**
 * @brief  Ymodem 接收状态机（处理一帧完整数据）
 * @param  p: 帧数据缓冲
 */
void ymodem_recv(download_buf_t *p)
{
    uint8_t type = p->data[0];

    switch (ymodem.status)
    {
    case 0:
        /* 等待文件信息帧（SOH，128 字节包） */
        if (type == YMODEM_SOH) {
            uint8_t pkt_num = p->data[1];
            uint8_t pkt_num_inv = p->data[2];

            /* 校验包序号：序号与取反应互补 */
            if ((pkt_num ^ pkt_num_inv) != 0xFF) {
                break;   /* 非法帧，忽略 */
            }

            /* 擦除 APP 区域 */
            ymodem.process = BUSY;
            ymodem.addr = APP_SECTOR_ADDR;
            mcu_flash_erase(ymodem.addr, APP_ERASE_SECTORS);

            /* ACK 后再次发送 'C'，请求数据包 */
            ymodem_ack();
            ymodem_c();
            ymodem.status++;
        }
        break;

    case 1:
        if (type == YMODEM_SOH || type == YMODEM_STX) {
            uint8_t packet_num = p->data[1];
            uint16_t data_len;
            uint8_t crc_ok;

            /* 跳过包序号 0（文件信息帧，已处理） */
            if (packet_num == 0) {
                ymodem_ack();
                break;
            }

            data_len = (type == YMODEM_SOH) ? 128 : 1024;
            crc_ok = verify_ymodem_packet(p->data, data_len);

            if (crc_ok) {
                /* 边界保护：防止写入越界到 CONFIG 区域 */
                if ((ymodem.addr + data_len) > APP_WRITE_MAX_ADDR) {
                    ymodem.status = 0;
                    ymodem.process = UPDATE_SUCCESS;   /* 触发错误处理 */
                    ymodem_nack();
                    break;
                }

                mcu_flash_write(ymodem.addr, &p->data[3], data_len);
                ymodem.addr += data_len;
                ymodem_ack();
            } else {
                /* CRC 错误，NAK 请求重传 */
                ymodem_nack();
            }
        }
        else if (type == YMODEM_EOT) {
            /* 第一个 EOT：NAK，等待第二个 EOT */
            ymodem_nack();
            ymodem.status++;
        }
        else {
            ymodem.status = 0;
        }
        break;

    case 2:
        /* 等待第二个 EOT */
        if (type == YMODEM_EOT) {
            ymodem_ack();
            ymodem.status++;
        }
        break;

    case 3:
        /* 等待结束批次帧（空文件名的 SOH 包） */
        if (type == YMODEM_SOH) {
            ymodem_ack();
            ymodem.status = 0;

            /* 升级完成：存储 APP CRC 并设置首次启动标志 */
            store_app_crc();
            write_boot_flag_to_flash(BOOT_FLAG_FIRST_BOOT);

            ymodem.process = UPDATE_SUCCESS;
        }
        break;
    }

    p->len = 0;
}

/**
 * @brief  初始化 Ymodem 模块（队列 + 定时器）
 */
void ymodem_init(void)
{
    ymodem.process = WAIT_START_PROGRAM;
    ymodem.status = 0;
    ymodem.addr = APP_SECTOR_ADDR;

    Timer_init();
    queue_initiate(&rx_queue);
}

/**
 * @brief  USART0 接收中断服务函数
 * @details 每收到一个字节就入队，并重触发帧间超时定时器
 */
void USART0_IRQHandler(void)
{
    /* 接收中断（读数据缓冲非空） */
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET) {
        uint8_t res = (uint8_t)usart_data_receive(USART0);
        queue_append(&rx_queue, res);

        /* 仅在升级过程中启动帧间超时定时器，避免与按键检测冲突 */
        if (get_ymodem_status() == UPDATE_PROGRAM) {
            timer_disable(TIMER2);
            timer_flag_clear(TIMER2, TIMER_FLAG_UP);
            timer_counter_value_config(TIMER2, 0);
            timer_enable(TIMER2);
        }
    }

    /* 清除溢出错误标志（如有） */
    if (usart_flag_get(USART0, USART_FLAG_ORERR) != RESET) {
        usart_flag_clear(USART0, USART_FLAG_ORERR);
    }
}

/**
 * @brief  初始化 TIMER2（1ms 帧间超时定时器）
 */
void Timer_init(void)
{
    timer_parameter_struct timer_initpara;

    /* 使能 TIMER2 时钟并复位 */
    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);

    /* 1ms 定时：48MHz / (47 + 1) = 1MHz，计 1000 次 = 1ms */
    timer_initpara.prescaler = 47;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = 999;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2, &timer_initpara);

    /* 使能更新事件与更新中断 */
    timer_update_event_enable(TIMER2);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);
    nvic_irq_enable(TIMER2_IRQn, 0, 1);
}

/**
 * @brief  TIMER2 更新中断服务函数
 * @details 帧间超时：说明一帧数据接收完毕，取出整帧处理
 */
void TIMER2_IRQHandler(void)
{
    if (timer_flag_get(TIMER2, TIMER_FLAG_UP) != RESET) {
        timer_flag_clear(TIMER2, TIMER_FLAG_UP);
        timer_disable(TIMER2);

        /* 队列非空：取出完整帧并处理 */
        if (queue_not_empty(&rx_queue)) {
            recvBuf.len = 0;
            while ((recvBuf.len < sizeof(recvBuf.data)) &&
                   queue_delete(&rx_queue, &recvBuf.data[recvBuf.len])) {
                recvBuf.len++;
            }
            ymodem_recv(&recvBuf);
        }
    }
}
