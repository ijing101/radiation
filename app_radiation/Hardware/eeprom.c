#include "main.h"
#include "eeprom.h"

#define SLAVE_ADDR_ADDR   0x0010
#define BOUND_ADDR_ADDR   0x0020


/* 说明：从机地址/波特率存储地址与 Bootloader 约定一致，改动需同步两边。
   Modbus Reg16(地址) / Reg8(波特率) 经此存储，掉电保持。 */

void slave_add_write(uint16_t add)
{
    AT24CXX_WriteOneByte(SLAVE_ADDR_ADDR, (uint8_t)add);
}

uint8_t slave_add_read(void)
{
    uint8_t a = AT24CXX_ReadOneByte(SLAVE_ADDR_ADDR);
    if (a == 0 || a >= 100) {
        return 1;
    }
    return a;
}

void bound_add_write(uint32_t add)
{
    if (add > 2U) { add = 0U; }
    AT24CXX_WriteOneByte(BOUND_ADDR_ADDR, (uint8_t)add);
}

uint32_t bound_add_read(void)
{
    uint8_t bound = AT24CXX_ReadOneByte(BOUND_ADDR_ADDR);
    if (bound > 2U) {
        return 0;
    }
    return bound;
}

static void sda_input(void)
{
    /* 使用内部上拉，配合总线外部上拉使 SDA 读/应答可靠 */
    gpio_mode_set(IIC_SDA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, IIC_SDA_PIN);
}

static void sda_output(void)
{
    gpio_mode_set(IIC_SDA_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, IIC_SDA_PIN);
    gpio_output_options_set(IIC_SDA_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SDA_PIN);
}

#define IIC_SCL_H()     gpio_bit_set(IIC_SCL_PORT, IIC_SCL_PIN)
#define IIC_SCL_L()     gpio_bit_reset(IIC_SCL_PORT, IIC_SCL_PIN)
#define IIC_SDA_H()     gpio_bit_set(IIC_SDA_PORT, IIC_SDA_PIN)
#define IIC_SDA_L()     gpio_bit_reset(IIC_SDA_PORT, IIC_SDA_PIN)
#define READ_SDA()      gpio_input_bit_get(IIC_SDA_PORT, IIC_SDA_PIN)

void IIC_Init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_mode_set(IIC_SCL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, IIC_SCL_PIN | IIC_SDA_PIN);
    gpio_output_options_set(IIC_SCL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_SCL_PIN | IIC_SDA_PIN);
    gpio_bit_set(IIC_SCL_PORT, IIC_SCL_PIN | IIC_SDA_PIN);
    gpio_mode_set(IIC_WP_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, IIC_WP_PIN);
    gpio_output_options_set(IIC_WP_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, IIC_WP_PIN);
    gpio_bit_reset(IIC_WP_PORT, IIC_WP_PIN);
}

void IIC_Start(void)
{
    sda_output();
    IIC_SDA_H();
    IIC_SCL_H();
    delay_us(4);
    IIC_SDA_L();
    delay_us(4);
    IIC_SCL_L();
}

void IIC_Stop(void)
{
    sda_output();
    IIC_SCL_L();
    IIC_SDA_L();
    delay_us(4);
    IIC_SCL_H();
    IIC_SDA_H();
    delay_us(4);
}

uint8_t IIC_Wait_Ack(void)
{
    uint8_t err = 0;
    sda_input();
    IIC_SDA_H();
    delay_us(1);
    IIC_SCL_H();
    delay_us(1);
    while (READ_SDA()) {
        err++;
        if (err > 250) {
            IIC_Stop();
            return 1;
        }
    }
    IIC_SCL_L();
    return 0;
}

void IIC_Ack(void)
{
    IIC_SCL_L();
    sda_output();
    IIC_SDA_L();
    delay_us(2);
    IIC_SCL_H();
    delay_us(2);
    IIC_SCL_L();
}

void IIC_NAck(void)
{
    IIC_SCL_L();
    sda_output();
    IIC_SDA_H();
    delay_us(2);
    IIC_SCL_H();
    delay_us(2);
    IIC_SCL_L();
}

void IIC_Send_Byte(uint8_t txd)
{
    uint8_t t;
    sda_output();
    IIC_SCL_L();
    for (t = 0; t < 8; t++) {
        if (txd & 0x80U) { IIC_SDA_H(); } else { IIC_SDA_L(); }
        txd <<= 1;
        delay_us(2);
        IIC_SCL_H();
        delay_us(2);
        IIC_SCL_L();
        delay_us(2);
    }
}

uint8_t IIC_Read_Byte(unsigned char ack)
{
    unsigned char i, receive = 0;
    sda_input();
    for (i = 0; i < 8; i++) {
        IIC_SCL_L();
        delay_us(2);
        IIC_SCL_H();
        receive <<= 1;
        if (READ_SDA()) { receive++; }
        delay_us(1);
    }
    if (!ack) { IIC_NAck(); } else { IIC_Ack(); }
    return receive;
}

void AT24CXX_Init(void)
{
    IIC_Init();
}

uint8_t AT24CXX_ReadOneByte(uint16_t ReadAddr)
{
    uint8_t temp = 0;
    IIC_Start();
    if (EE_TYPE > AT24C16) {
        IIC_Send_Byte(0xA0);
        IIC_Wait_Ack();
        IIC_Send_Byte((uint8_t)(ReadAddr >> 8));
        IIC_Wait_Ack();
    } else {
        IIC_Send_Byte(0xA0 + (((uint8_t)(ReadAddr / 256)) << 1));
    }
    IIC_Wait_Ack();
    IIC_Send_Byte((uint8_t)(ReadAddr % 256));
    IIC_Wait_Ack();
    IIC_Start();
    IIC_Send_Byte(0xA1);
    IIC_Wait_Ack();
    temp = IIC_Read_Byte(0);
    IIC_Stop();
    return temp;
}

void AT24CXX_WriteOneByte(uint16_t WriteAddr, uint8_t DataToWrite)
{
    IIC_Start();
    if (EE_TYPE > AT24C16) {
        IIC_Send_Byte(0xA0);
        IIC_Wait_Ack();
        IIC_Send_Byte((uint8_t)(WriteAddr >> 8));
    } else {
        IIC_Send_Byte(0xA0 + (((uint8_t)(WriteAddr / 256)) << 1));
    }
    IIC_Wait_Ack();
    IIC_Send_Byte((uint8_t)(WriteAddr % 256));
    IIC_Wait_Ack();
    IIC_Send_Byte(DataToWrite);
    IIC_Wait_Ack();
    IIC_Stop();
    delay_ms(10);
}

void AT24CXX_WriteLenByte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len)
{
    uint8_t t;
    for (t = 0; t < Len; t++) {
        AT24CXX_WriteOneByte(WriteAddr + t, (uint8_t)((DataToWrite >> (8 * t)) & 0xff));
    }
}

uint32_t AT24CXX_ReadLenByte(uint16_t ReadAddr, uint8_t Len)
{
    uint8_t t;
    uint32_t temp = 0;
    for (t = 0; t < Len; t++) {
        temp <<= 8;
        temp += AT24CXX_ReadOneByte(ReadAddr + Len - t - 1);
    }
    return temp;
}

uint8_t AT24CXX_Check(void)
{
    uint8_t temp;
    temp = AT24CXX_ReadOneByte(255);
    if (temp == 0x55) { return 0; }
    AT24CXX_WriteOneByte(255, 0x55);
    temp = AT24CXX_ReadOneByte(255);
    if (temp == 0x55) { return 0; }
    return 1;
}

void AT24CXX_Read(uint16_t ReadAddr, uint8_t *pBuffer, uint16_t NumToRead)
{
    while (NumToRead) {
        *pBuffer++ = AT24CXX_ReadOneByte(ReadAddr++);
        NumToRead--;
    }
}

void AT24CXX_Write(uint16_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite)
{
    while (NumToWrite--) {
        AT24CXX_WriteOneByte(WriteAddr, *pBuffer);
        WriteAddr++;
        pBuffer++;
    }
}

