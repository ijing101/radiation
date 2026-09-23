/**
 * @file ymodem.c
 * @brief 有精确镜像长度、序号和 CRC 检查的 YMODEM 接收状态机。
 */
#include "ymodem.h"
#include "usart.h"

ymodem_t ymodem = { WAIT_START_PROGRAM, 0U, 0U, APP_SECTOR_ADDR,
                    0U, 0U, 1U, 0U, 0 };
static download_buf_t recv_buffer;
static uint16_t rx_expected_len;
static volatile uint8_t rx_idle_ticks;
static volatile uint8_t rx_activity_since_check;
seq_queue_t rx_queue;

void queue_initiate(seq_queue_t *queue)
{
    queue->rear = 0;
    queue->front = 0;
    queue->count = 0;
}

int queue_not_empty(seq_queue_t *queue)
{
    return queue->count != 0 ? 1 : 0;
}

int queue_append(seq_queue_t *queue, uint8_t value)
{
    if (queue->count >= (int)MAX_QUEUE_SIZE)
    {
        return 0;
    }
    queue->queue[queue->rear] = value;
    queue->rear = (queue->rear + 1) % (int)MAX_QUEUE_SIZE;
    queue->count++;
    return 1;
}

int queue_delete(seq_queue_t *queue, uint8_t *value)
{
    if (queue->count == 0)
    {
        return 0;
    }
    *value = queue->queue[queue->front];
    queue->front = (queue->front + 1) % (int)MAX_QUEUE_SIZE;
    queue->count--;
    return 1;
}

void ymodem_ack(void)
{
    usart0_send_byte(YMODEM_ACK);
}

void ymodem_nack(void)
{
    usart0_send_byte(YMODEM_NAK);
}

void ymodem_c(void)
{
    usart0_send_byte(YMODEM_C);
}

void set_ymodem_status(process_status process)
{
    ymodem.process = process;
}

process_status get_ymodem_status(void)
{
    return ymodem.process;
}

void ymodem_reset_transfer(void)
{
    ymodem.status = 0U;
    ymodem.id = 0U;
    ymodem.addr = APP_SECTOR_ADDR;
    ymodem.image_size = 0U;
    ymodem.received_size = 0U;
    ymodem.expected_packet = 1U;
    ymodem.sectors_size = 0U;
    recv_buffer.len = 0U;
    rx_expected_len = 0U;
    rx_idle_ticks = 0U;
}

void ymodem_abort_transfer(void)
{
    uint32_t primask = __get_PRIMASK();

    /* 保留 BOOT_STATE_RECEIVING 和 Backup，仅丢弃断线留下的半包。 */
    __disable_irq();
    queue_initiate(&rx_queue);
    ymodem_reset_transfer();
    rx_activity_since_check = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

uint8_t ymodem_take_rx_activity(void)
{
    uint8_t activity;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    activity = rx_activity_since_check;
    rx_activity_since_check = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
    return activity;
}

static uint8_t header_packet_valid(download_buf_t *packet)
{
    return (packet != 0 && packet->len == 133U &&
            packet->data[0] == YMODEM_SOH && packet->data[1] == 0U &&
            ((uint8_t)(packet->data[1] ^ packet->data[2])) == 0xFFU &&
            verify_ymodem_packet(packet->data, 128U)) ? 1U : 0U;
}

static uint8_t parse_image_size(const uint8_t *data, uint32_t *image_size)
{
    uint16_t i = 0U;
    uint32_t size = 0U;
    uint8_t digit_found = 0U;

    if (data == 0 || image_size == 0)
    {
        return 0U;
    }
    while (i < 128U && data[i] != 0U)
    {
        i++;
    }
    if (i >= 128U)
    {
        return 0U;
    }
    i++;
    while (i < 128U && (data[i] == ' ' || data[i] == '\t'))
    {
        i++;
    }
    while (i < 128U && data[i] >= '0' && data[i] <= '9')
    {
        digit_found = 1U;
        if (size > 0xFFFFFFFFU / 10U)
        {
            return 0U;
        }
        size = size * 10U + (uint32_t)(data[i] - '0');
        i++;
    }
    if (!digit_found || size == 0U || size > APP_IMAGE_MAX_SIZE)
    {
        return 0U;
    }
    *image_size = size;
    return 1U;
}

static uint8_t begin_transfer(download_buf_t *packet)
{
    boot_metadata_t metadata;
    uint32_t image_size;

    if (!header_packet_valid(packet) ||
        !parse_image_size(&packet->data[3], &image_size) ||
        !boot_metadata_read(&metadata) ||
        metadata.state != BOOT_STATE_RECEIVING ||
        (metadata.backup_size != 0U &&
         !boot_backup_is_valid(metadata.backup_size, metadata.backup_crc32)))
    {
        ymodem_nack();
        return 0U;
    }

    /* 此处才擦 Active；进入本函数前旧程序已被安全备份。 */
    if (!mcu_flash_erase(APP_SECTOR_ADDR, APP_ERASE_SECTORS))
    {
        ymodem_nack();
        return 0U;
    }
    ymodem.addr = APP_SECTOR_ADDR;
    ymodem.image_size = image_size;
    ymodem.received_size = 0U;
    ymodem.expected_packet = 1U;
    ymodem.status = 1U;
    ymodem.process = UPDATE_PROGRAM;
    ymodem_ack();
    ymodem_c();
    return 1U;
}

static uint8_t is_empty_end_header(download_buf_t *packet)
{
    uint16_t i;

    for (i = 3U; i < 131U; i++)
    {
        if (packet->data[i] != 0U)
        {
            return 0U;
        }
    }
    return 1U;
}

void ymodem_recv(download_buf_t *packet)
{
    uint8_t type;
    uint8_t packet_num;
    uint8_t packet_inv;
    uint8_t previous_packet;
    uint16_t data_len;
    uint32_t logical_length;
    uint32_t active_crc;
    boot_metadata_t metadata;

    if (packet == 0 || packet->len == 0U)
    {
        return;
    }

    /* 主机取消时立即回到 block 0 等待状态。 */
    if (packet->len == 1U && packet->data[0] == YMODEM_CA &&
        ymodem.status != 0U)
    {
        ymodem_reset_transfer();
        packet->len = 0U;
        return;
    }

    /* 新 block 0 表示主机重试；仍由已确认 Backup 保护旧 APP。 */
    if (ymodem.status != 3U && header_packet_valid(packet))
    {
        begin_transfer(packet);
        packet->len = 0U;
        return;
    }

    type = packet->data[0];
    packet_num = packet->len > 1U ? packet->data[1] : 0U;
    packet_inv = packet->len > 2U ? packet->data[2] : 0U;

    switch (ymodem.status)
    {
    case 0U:
        break;

    case 1U:
        if (type == YMODEM_SOH || type == YMODEM_STX)
        {
            data_len = type == YMODEM_SOH ? 128U : 1024U;
            if (packet->len != data_len + 5U ||
                ((uint8_t)(packet_num ^ packet_inv)) != 0xFFU ||
                !verify_ymodem_packet(packet->data, data_len))
            {
                ymodem_nack();
                break;
            }

            previous_packet = ymodem.expected_packet == 1U ? 255U :
                              (uint8_t)(ymodem.expected_packet - 1U);
            if (packet_num == previous_packet)
            {
                ymodem_ack(); /* 主机没收到 ACK 时允许安全重发。 */
                break;
            }
            if (packet_num != ymodem.expected_packet ||
                ymodem.received_size >= ymodem.image_size ||
                ymodem.addr + data_len > APP_SECTOR_ADDR + APP_SLOT_SIZE)
            {
                ymodem_nack();
                break;
            }

            logical_length = ymodem.image_size - ymodem.received_size;
            if (logical_length > data_len)
            {
                logical_length = data_len;
            }
            /* 尾包按协议可带填充字节；CRC32 只覆盖 logical_length。 */
            if (!mcu_flash_write(ymodem.addr, &packet->data[3], data_len))
            {
                ymodem_nack();
                break;
            }
            ymodem.addr += data_len;
            ymodem.received_size += logical_length;
            ymodem.expected_packet = ymodem.expected_packet == 255U ? 1U :
                                     (uint8_t)(ymodem.expected_packet + 1U);
            ymodem_ack();
        }
        else if (type == YMODEM_EOT)
        {
            if (ymodem.received_size != ymodem.image_size)
            {
                ymodem_nack();
                break;
            }
            ymodem_nack();
            ymodem.status = 2U;
        }
        else
        {
            ymodem_nack();
        }
        break;

    case 2U:
        if (type == YMODEM_EOT)
        {
            ymodem_ack();
            ymodem_c();
            ymodem.status = 3U;
        }
        else
        {
            ymodem_nack();
        }
        break;

    case 3U:
        if (type == YMODEM_SOH && packet->len == 133U && packet_num == 0U &&
            ((uint8_t)(packet_num ^ packet_inv)) == 0xFFU &&
            verify_ymodem_packet(packet->data, 128U) &&
            is_empty_end_header(packet))
        {
            active_crc = boot_image_crc32(APP_SECTOR_ADDR, ymodem.image_size);
            if (!boot_image_is_valid(APP_SECTOR_ADDR, ymodem.image_size, active_crc) ||
                !boot_metadata_read(&metadata) ||
                metadata.state != BOOT_STATE_RECEIVING)
            {
                ymodem_nack();
                break;
            }

            metadata.active_size = ymodem.image_size;
            metadata.active_crc32 = active_crc;
            metadata.trial_attempts = 0U;
            metadata.state = BOOT_STATE_TRIAL;
            if (!boot_metadata_write(&metadata))
            {
                ymodem_nack();
                break;
            }
            ymodem_ack();
            ymodem.status = 0U;
            ymodem.process = UPDATE_SUCCESS;
        }
        else
        {
            ymodem_nack();
        }
        break;

    default:
        ymodem_reset_transfer();
        break;
    }
    packet->len = 0U;
}

void ymodem_init(void)
{
    /* GD32 端统一使用 USART0 PA9/PA10，波特率由公共配置固定为 9600。 */
    usart0_init(BOOT_USART_BAUD);
    ymodem_timer_init();
    queue_initiate(&rx_queue);
    rx_activity_since_check = 0U;
    ymodem_reset_transfer();
    ymodem.process = WAIT_START_PROGRAM;
}

static uint16_t expected_frame_length(uint8_t first_byte)
{
    if (ymodem.status == 0U || ymodem.status == 3U)
    {
        if (ymodem.status == 3U && first_byte == YMODEM_CA)
        {
            return 1U;
        }
        return first_byte == YMODEM_SOH ? 133U : 0U;
    }
    if (ymodem.status == 1U)
    {
        if (first_byte == YMODEM_SOH) return 133U;
        if (first_byte == YMODEM_STX) return 1029U;
        if (first_byte == YMODEM_EOT || first_byte == YMODEM_CA) return 1U;
        return 0U;
    }
    if (ymodem.status == 2U)
    {
        if (first_byte == YMODEM_EOT) return 1U;
        if (first_byte == YMODEM_CA) return 1U;
        return first_byte == YMODEM_SOH ? 133U : 0U;
    }
    return 0U;
}

void USART0_IRQHandler(void)
{
    uint8_t value;

    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET)
    {
        value = (uint8_t)usart_data_receive(USART0);
        queue_append(&rx_queue, value);
        /* 与 STM32 原版一致：每字节都清除帧间空闲累计并重新计时。 */
        rx_idle_ticks = 0U;
        rx_activity_since_check = 1U;
        timer_disable(TIMER2);
        timer_flag_clear(TIMER2, TIMER_FLAG_UP);
        timer_counter_value_config(TIMER2, 0U);
        timer_enable(TIMER2);
    }
    if (usart_flag_get(USART0, USART_FLAG_ORERR) != RESET)
    {
        usart_flag_clear(USART0, USART_FLAG_ORERR);
    }
}

void ymodem_timer_init(void)
{
    timer_parameter_struct timer_initpara;

    rcu_periph_clock_enable(RCU_TIMER2);
    timer_deinit(TIMER2);
    /* 48MHz / (47 + 1) = 1MHz，999 + 1 个周期为 1ms。 */
    timer_initpara.prescaler = 47U;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = 999U;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0U;
    timer_init(TIMER2, &timer_initpara);
    timer_update_event_enable(TIMER2);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);
    nvic_irq_enable(TIMER2_IRQn, 0U, 1U);
    /* 与 STM32 TIM3 初始化一致，定时器从启动起即可处理接收队列。 */
    timer_enable(TIMER2);
}

void TIMER2_IRQHandler(void)
{
    uint8_t value;

    if (timer_flag_get(TIMER2, TIMER_FLAG_UP) == RESET)
    {
        return;
    }
    timer_flag_clear(TIMER2, TIMER_FLAG_UP);
    timer_disable(TIMER2);

    while (queue_delete(&rx_queue, &value))
    {
        if (recv_buffer.len == 0U)
        {
            rx_expected_len = expected_frame_length(value);
            if (rx_expected_len == 0U)
            {
                continue;
            }
        }
        if (recv_buffer.len >= sizeof(recv_buffer.data))
        {
            ymodem_nack();
            recv_buffer.len = 0U;
            rx_expected_len = 0U;
            rx_idle_ticks = 0U;
            continue;
        }
        recv_buffer.data[recv_buffer.len++] = value;
        if (recv_buffer.len == rx_expected_len)
        {
            ymodem_recv(&recv_buffer);
            rx_expected_len = 0U;
            rx_idle_ticks = 0U;
        }
    }

    if (recv_buffer.len > 0U && rx_expected_len > recv_buffer.len)
    {
        if (++rx_idle_ticks >= 10U)
        {
            ymodem_nack();
            recv_buffer.len = 0U;
            rx_expected_len = 0U;
            rx_idle_ticks = 0U;
        }
        else
        {
            timer_counter_value_config(TIMER2, 0U);
            timer_enable(TIMER2);
        }
    }
}
