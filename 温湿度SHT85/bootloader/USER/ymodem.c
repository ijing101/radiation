#include "ymodem.h"
#include "rs485.h"
#include <stdio.h>

ymodem_t ymodem = {WAIT_START_PROGRAM, 0, 0, APP_SECTOR_ADDR, 0, {0}};
download_buf_t recvBuf;
seq_queue_t rx_queue;

// 初始化队列 queue_initiate(Q)
void queue_initiate(seq_queue_t *Q)
{
    Q->rear = 0;        
    Q->front = 0;
    Q->count = 0;
}

// 判断队列是否非空 queue_not_empty(Q)
// 若队列Q非空返回1，否则返回0
int queue_not_empty(seq_queue_t *Q)
{
    if(Q->count != 0)
        return 1;
    else 
        return 0;
}

// 入队操作 queue_append(Q, x)
// 将元素x写入环形队列Q，成功返回1，失败返回0
int queue_append(seq_queue_t *Q, uint8_t x)
{
    if(Q->count >= MAX_QUEUE_SIZE)
    {
        uart_log("queue is full! count=%d\r\n", Q->count);
        return 0;
    }
    else
    {
        Q->queue[Q->rear] = x;
        Q->rear = (Q->rear + 1) % MAX_QUEUE_SIZE;
        Q->count ++;

        /* 队列使用率监控 */
        if (Q->count > (MAX_QUEUE_SIZE * 3 / 4)) {
            uart_log("queue usage high: %d/%d\r\n", Q->count, MAX_QUEUE_SIZE);
        }
        return 1;
    }
}

// 出队操作 queue_delete(Q, d)
// 从队列Q读取一个字节到d，成功返回1，失败返回0
int queue_delete(seq_queue_t *Q, uint8_t *d)
{
    if(Q->count == 0)
    {    
        // uart_log("queue is empty! \n");
        return 0;
    }
    else
    {    *d = Q->queue[Q->front];
        Q->front = (Q->front + 1) % MAX_QUEUE_SIZE;
        Q->count--;
        return 1;
    }
}

// 读取队头但不出队 queue_get(Q, d)
int queue_get(seq_queue_t Q, uint8_t *d)
{
    if(Q.count == 0)
    {
         uart_log("queue is empty! \n");
        return 0;
    }
    else
    {
        *d = Q.queue[Q.front];
        return 1;
    }
}

void ymodem_ack(void) 
{
    uint8_t buf;
    buf = YMODEM_ACK;
    RS485_Send_Data(&buf, 1);
}

void ymodem_nack(void) 
{
    uint8_t buf;
    buf = YMODEM_NAK;
    RS485_Send_Data(&buf, 1);
}

void ymodem_c(void) 
{
    uint8_t buf;
    buf = YMODEM_C;
    RS485_Send_Data(&buf, 1);
}

void set_ymodem_status(process_status process) 
{
    ymodem.process = process;
}

process_status get_ymodem_status(void) 
{
    process_status process = ymodem.process;
    return process;
}

void ymodem_start(ymodem_callback cb) 
{
    if (ymodem.status == 0) 
    {
        ymodem.cb = cb;
    }
}






void ymodem_recv(download_buf_t *p)
{
    uint8_t type = p->data[0];
    uint8_t pkt_num;
    uint8_t pkt_num_inv;

    switch (ymodem.status)
    {
        case 0:
            if (type == YMODEM_SOH) 
            {
                /* 安全检查：验证这是真正的Ymodem帧，而不是其他协议（如Modbus） */
                /* Ymodem SOH帧最小长度：133字节（1+1+1+128+2） */
                if (p->len < 133) {
                    uart_log("WARNING: Received byte 0x01 but frame too short (%d bytes), ignoring\r\n", p->len);
                    Usart2_SendString("Invalid Ymodem frame\r\n");
                    break;  // 不是有效的Ymodem帧，忽略
                }
                
                /* 进一步验证：检查包序号和包序号取反 */
                pkt_num = p->data[1];
                pkt_num_inv = p->data[2];
                if ((pkt_num ^ pkt_num_inv) != 0xFF) {
                    uart_log("WARNING: Invalid Ymodem packet number (0x%02X, ~0x%02X), ignoring\r\n", 
                             pkt_num, pkt_num_inv);
                    Usart2_SendString("Invalid packet number\r\n");
                    break;  // 包序号不匹配，不是Ymodem帧
                }
                
                uart_log("Valid Ymodem SOH frame detected, starting erase...\r\n");
                ymodem.process = BUSY;
                ymodem.addr = APP_SECTOR_ADDR;
                uart_log("erase flash: 0x%08X\r\n", APP_SECTOR_ADDR);
							  Usart2_SendString("erase flash\r\n");
                mcu_flash_erase(ymodem.addr, APP_ERASE_SECTORS); // 擦除Flash
								Usart2_SendString("erase flash success\r\n");
                uart_log("erase flash success\r\n");
                ymodem_ack();
                ymodem_c();
                ymodem.status++;
            }

            break;
        case 1:
            if (type == YMODEM_SOH || type == YMODEM_STX)
            {
                uint8_t packet_num;
                uint16_t data_len;
                uint8_t crc_ok;
                
                // 跳过包序号为0的包（文件信息包）
                packet_num = p->data[1];
                if (packet_num == 0) {
                    Usart2_SendString("Skip packet 0\n");
                    ymodem_ack();
                    break;
                }
                
                data_len = (type == YMODEM_SOH) ? 128 : 1024;
                crc_ok = 1;  // 默认假设CRC正确

                /* 智能CRC校验：如果数据包长度足够，则进行CRC校验 */
                if (p->len >= (data_len + 5)) {
                    /* 数据包包含CRC，进行校验 */
                    crc_ok = verify_ymodem_packet(p->data, data_len);
                    if (crc_ok) {
                        uart_log("CRC OK, writing %d bytes to 0x%08X\r\n", data_len, ymodem.addr);
                    } else {
                        uart_log("CRC FAILED, requesting retransmission\r\n");
                        Usart2_SendString("CRC FAILED\r\n");
                    }
                } else {
                    /* 数据包不包含CRC，使用兼容模式 */
                    uart_log("No CRC mode, writing %d bytes to 0x%08X\r\n", data_len, ymodem.addr);
                }

                /* 只有CRC校验通过才写入Flash */
                if (crc_ok) {
                    char buf[64];
                    
                    /* 边界保护：检查写入地址是否越界 */
                    if (ymodem.addr + data_len > APP_WRITE_MAX_ADDR) {
                        uart_log("ERROR: Write address 0x%08X + %d exceeds safe boundary 0x%08X!\r\n",
                                 ymodem.addr, data_len, APP_WRITE_MAX_ADDR);
                        Usart2_SendString("ERROR: Address overflow!\r\n");
                        ymodem.status = 0;  // 重置状态
                        ymodem.process = UPDATE_SUCCESS;  // 标记为失败，触发错误处理
                        ymodem_nack();
                        break;
                    }
                    
                    // 输出写入地址信息
                    sprintf(buf, "Write to 0x%08X\n", ymodem.addr);
                    Usart2_SendString(buf);
                    
                    // 调试：如果是第一个数据包，输出前8字节
                    if (ymodem.addr == APP_SECTOR_ADDR) {
                        sprintf(buf, "First data: %02X %02X %02X %02X\n",
                                p->data[3], p->data[4], p->data[5], p->data[6]);
                        Usart2_SendString(buf);
                    }
                    
                    mcu_flash_write(ymodem.addr, &p->data[3], data_len);
                    ymodem.addr += data_len;
                    ymodem_ack();
                } else {
                    /* CRC校验失败，发送NAK重传 */
                    ymodem_nack();
                }
            }
            else if (type == YMODEM_EOT) 
            {
                ymodem_nack();
                ymodem.status++;
            }
            else 
            {
                ymodem.status = 0;
            }
            break;
        case 2:
            if (type == YMODEM_EOT) 
            {
                ymodem_ack();
                ymodem_c();
                ymodem.status++;
            }
            break;
        case 3:
            if (type == YMODEM_SOH)
            {
                ymodem_ack();
                ymodem.status = 0;

                /* 升级完成，计算并存储应用程序CRC */
                uart_log("Update completed, calculating app CRC...\r\n");
                Usart2_SendString("Calculating app CRC...\r\n");
                store_app_crc();

                /* 设置首次启动标志，下次启动时会进行完整校验 */
                uart_log("Setting first boot flag...\r\n");
                write_boot_flag_to_flash(BOOT_FLAG_FIRST_BOOT);

                ymodem.process = UPDATE_SUCCESS;
                uart_log("App CRC stored, update success!\r\n");
                uart_log("System will reboot and verify APP integrity\r\n");
                Usart2_SendString("Update success!\r\n");
            }
    }
    p->len = 0;
}

void ymodem_init(void)
{
    //RS485_Init(9600);
	  RS485_Init(115200);
    timer_init();
    queue_initiate(&rx_queue);
}

void USART2_IRQHandler(void)
{
    uint8_t res;
    if(USART_GetITStatus(USART2,USART_IT_RXNE)!= RESET)
    {
        res = USART_ReceiveData(USART2);
        queue_append(&rx_queue, res);
    }
    TIM3->CNT = 0;
    TIM_Cmd(TIM3, ENABLE);
}

void timer_init(void) 
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    NVIC_InitTypeDef         NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // 使能TIM3时钟
    
    // 定时器TIM3定时配置
    TIM_TimeBaseStructure.TIM_Period = 999; // 自动重装载周期：1000计数=1ms
    TIM_TimeBaseStructure.TIM_Prescaler = 71; // 72MHz/(71+1) = 1MHz, 1us per count
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 时钟分频：TDTS = Tck_tim
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  // TIM向上计数模式
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); // 初始化TIM3基础定时器
 
    TIM_ITConfig(TIM3,TIM_IT_Update, ENABLE); // 允许TIM3更新中断

    // 配置NVIC中断控制器
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  // TIM3中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  // 抢占优先级0
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;  // 子优先级1
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // 使能IRQ通道
    NVIC_Init(&NVIC_InitStructure);  // 初始化NVIC

    TIM_Cmd(TIM3, ENABLE);  // 使能TIM3
}

void TIM3_IRQHandler(void) 
{
    if(TIM_GetITStatus(TIM3, TIM_IT_Update) == SET) 
    {
        int result = 1;
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        TIM_Cmd(TIM3, DISABLE);
        
        result = queue_not_empty(&rx_queue);
        if(result == 1)
        {
            recvBuf.len = 0;
            do
            {
                result = queue_delete(&rx_queue, &recvBuf.data[recvBuf.len]);
                if(result == 1)
                {
                    recvBuf.len ++;
                }
            }
            while(result);
            ymodem_recv(&recvBuf);
        }
    }
}
