#include "gd32_common.h"
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
    AT24CXX_WriteOneByte(BOUND_ADDR_ADDR, (uint8_t)add);
}

uint32_t bound_add_read(void)
{
    uint8_t bound = AT24CXX_ReadOneByte(BOUND_ADDR_ADDR);
    if (bound != 0 && bound != 1) {
        return 0;
    }
    return bound;
}

static void sda_input(void)
{
    /* 使用内部上拉，配合总线外部上拉使 SDA 读/应答可靠 */
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_11);
}

static void sda_output(void)
{
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_11);
}

#define IIC_SCL_H()     gpio_bit_set(GPIOB, GPIO_PIN_10)
#define IIC_SCL_L()     gpio_bit_reset(GPIOB, GPIO_PIN_10)
#define IIC_SDA_H()     gpio_bit_set(GPIOB, GPIO_PIN_11)
#define IIC_SDA_L()     gpio_bit_reset(GPIOB, GPIO_PIN_11)
#define READ_SDA()      gpio_input_bit_get(GPIOB, GPIO_PIN_11)

void IIC_Init(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_10 | GPIO_PIN_11);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10 | GPIO_PIN_11);
    gpio_bit_set(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
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
    gpio_bit_set(GPIOB, GPIO_PIN_11);
    delay_us(1);
    gpio_bit_set(GPIOB, GPIO_PIN_10);
    delay_us(1);
    while (READ_SDA()) {
        err++;
        if (err > 250) {
            IIC_Stop();
            return 1;
        }
    }
    gpio_bit_reset(GPIOB, GPIO_PIN_10);
    return 0;
}

void IIC_Ack(void)
{
    gpio_bit_reset(GPIOB, GPIO_PIN_10);
    sda_output();
    gpio_bit_reset(GPIOB, GPIO_PIN_11);
    delay_us(2);
    gpio_bit_set(GPIOB, GPIO_PIN_10);
    delay_us(2);
    gpio_bit_reset(GPIOB, GPIO_PIN_10);
}

void IIC_NAck(void)
{
    gpio_bit_reset(GPIOB, GPIO_PIN_10);
    sda_output();
    gpio_bit_set(GPIOB, GPIO_PIN_11);
    delay_us(2);
    gpio_bit_set(GPIOB, GPIO_PIN_10);
    delay_us(2);
    gpio_bit_reset(GPIOB, GPIO_PIN_10);
}

void IIC_Send_Byte(uint8_t txd)
{
    uint8_t t;
    sda_output();
    gpio_bit_reset(GPIOB, GPIO_PIN_10);
    for (t = 0; t < 8; t++) {
        if (txd & 0x80U) { IIC_SDA_H(); } else { IIC_SDA_L(); }
        txd <<= 1;
        delay_us(2);
        gpio_bit_set(GPIOB, GPIO_PIN_10);
        delay_us(2);
        gpio_bit_reset(GPIOB, GPIO_PIN_10);
        delay_us(2);
    }
}

uint8_t IIC_Read_Byte(unsigned char ack)
{
    unsigned char i, receive = 0;
    sda_input();
    for (i = 0; i < 8; i++) {
        gpio_bit_reset(GPIOB, GPIO_PIN_10);
        delay_us(2);
        gpio_bit_set(GPIOB, GPIO_PIN_10);
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