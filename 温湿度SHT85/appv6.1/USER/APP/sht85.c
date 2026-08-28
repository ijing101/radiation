#include "sht85.h"

#define MaxMSec 60000

extern unsigned short MMsec;

// 全局数据结构
SHT85_Data SHT85;

// 微秒级延时
static void delay_us(unsigned char x)
{
    unsigned char i = 20;
    x = i * x;
    while(x--);
}

// 毫秒级延时
static void delay_ms(unsigned short x)
{
    unsigned short y;
    while(x--)
        for(y = 0; y < 1100; y++) {}
}

// I2C GPIO配置
void SHT85_IIC_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;  // 开漏输出
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    SHT85_SCL_H;
    SHT85_SDA_H;
    
    // 传感器上电后延时
    delay_ms(50);
    
    // 软复位
    SHT85_SoftReset();
    delay_ms(10);
}

// I2C起始信号
static unsigned char SHT85_I2C_Start(void)
{
    SHT85_SDA_H;
    delay_us(5);
    SHT85_SCL_H;
    delay_us(5);
    
    if(!SHT85_SDA_read) return 0;  // 总线忙
    
    SHT85_SDA_L;  // SCL高电平时SDA拉低
    delay_us(5);
    
    if(SHT85_SDA_read) return 0;  // 启动失败
    
    SHT85_SCL_L;
    delay_us(5);
    
    return 1;
}

// I2C停止信号
static void SHT85_I2C_Stop(void)
{
    SHT85_SDA_L;
    SHT85_SCL_L;
    delay_us(5);
    SHT85_SCL_H;
    delay_us(5);
    SHT85_SDA_H;
}

// I2C发送应答/非应答
static void SHT85_I2C_SendACK(unsigned char ack)
{
    if(ack)
        SHT85_SDA_H;  // 非应答
    else
        SHT85_SDA_L;  // 应答
    
    SHT85_SCL_H;
    delay_us(5);
    SHT85_SCL_L;
    delay_us(5);
}

// I2C等待应答
static unsigned char SHT85_I2C_WaitAck(void)
{
    unsigned short i = 0;
    
    SHT85_SDA_H;  // 释放SDA
    SHT85_SCL_H;
    
    while(SHT85_SDA_read)
    {
        i++;
        if(i >= 500)
            break;
    }
    
    if(SHT85_SDA_read)
    {
        SHT85_SCL_L;
        return 0;  // 无应答
    }
    
    delay_us(5);
    SHT85_SCL_L;
    delay_us(5);
    
    return 1;  // 有应答
}

// I2C发送一个字节
static void SHT85_I2C_SendByte(unsigned char dat)
{
    unsigned char i;
    
    SHT85_SCL_L;
    for(i = 0; i < 8; i++)
    {
        if(dat & 0x80)
            SHT85_SDA_H;
        else
            SHT85_SDA_L;
        
        SHT85_SCL_H;
        delay_us(5);
        SHT85_SCL_L;
        delay_us(5);
        dat <<= 1;
    }
}

// I2C接收一个字节
static unsigned char SHT85_I2C_RecvByte(void)
{
    unsigned char i;
    unsigned char dat = 0;
    
    SHT85_SDA_H;  // 释放SDA
    delay_us(1);
    
    for(i = 0; i < 8; i++)
    {
        dat <<= 1;
        SHT85_SCL_H;
        if(SHT85_SDA_read)
            dat |= 0x01;
        delay_us(5);
        SHT85_SCL_L;
        delay_us(5);
    }
    
    return dat;
}

// CRC-8校验 (多项式: 0x31, 初值: 0xFF)
static unsigned char SHT85_CRC8(unsigned char *data, unsigned char len)
{
    unsigned char crc = 0xFF;
    unsigned char i, j;
    
    for(i = 0; i < len; i++)
    {
        crc ^= data[i];
        for(j = 0; j < 8; j++)
        {
            if(crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    
    return crc;
}

// 软复位
int SHT85_SoftReset(void)
{
    if(SHT85_I2C_Start() == 0) { SHT85_I2C_Stop(); return 0; }
    
    SHT85_I2C_SendByte(SHT85_ADDR_W);
    if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); return 0; }
    
    SHT85_I2C_SendByte(SHT85_CMD_SOFT_RESET_MSB);
    if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); return 0; }
    
    SHT85_I2C_SendByte(SHT85_CMD_SOFT_RESET_LSB);
    if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); return 0; }
    
    SHT85_I2C_Stop();
    
    return 1;
}

// 读取温湿度数据 (状态机实现)
int SHT85_ReadData(void)
{
    static unsigned char step = 0;
    static unsigned char error = 0;
    static unsigned int oldtime = 0;
    int count_time;
    unsigned char data[6];
    unsigned char crc_temp, crc_humi;
    
    // 错误计数达到50次，复位状态机
    if(error >= 50)
    {
        step = 0;
        error = 0;
    }
    
    switch(step)
    {
        case 0:  // 发送测量命令 (高重复性)
            if(SHT85_I2C_Start() == 0) { SHT85_I2C_Stop(); error++; return 0; }
            
            SHT85_I2C_SendByte(SHT85_ADDR_W);
            if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); error++; return 0; }
            
            SHT85_I2C_SendByte(SHT85_CMD_MEAS_HIGH_MSB);
            if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); error++; return 0; }
            
            SHT85_I2C_SendByte(SHT85_CMD_MEAS_HIGH_LSB);
            if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); error++; return 0; }
            
            SHT85_I2C_Stop();
            oldtime = MMsec;
            step = 1;
            break;
            
        case 1:  // 等待测量完成 (高重复性约15ms)
            count_time = MMsec - oldtime;
            if(count_time < 0) count_time = MMsec + MaxMSec - oldtime;
            
            if(count_time >= 20)  // 等待20ms确保完成
            {
                step = 2;
            }
            break;
            
        case 2:  // 读取6字节数据 (温度MSB, 温度LSB, CRC, 湿度MSB, 湿度LSB, CRC)
            if(SHT85_I2C_Start() == 0) { SHT85_I2C_Stop(); error++; step = 0; return 0; }
            
            SHT85_I2C_SendByte(SHT85_ADDR_R);
            if(SHT85_I2C_WaitAck() == 0) { SHT85_I2C_Stop(); error++; step = 0; return 0; }
            
            // 读取6字节数据
            data[0] = SHT85_I2C_RecvByte();  // 温度MSB
            SHT85_I2C_SendACK(0);
            data[1] = SHT85_I2C_RecvByte();  // 温度LSB
            SHT85_I2C_SendACK(0);
            data[2] = SHT85_I2C_RecvByte();  // 温度CRC
            SHT85_I2C_SendACK(0);
            data[3] = SHT85_I2C_RecvByte();  // 湿度MSB
            SHT85_I2C_SendACK(0);
            data[4] = SHT85_I2C_RecvByte();  // 湿度LSB
            SHT85_I2C_SendACK(0);
            data[5] = SHT85_I2C_RecvByte();  // 湿度CRC
            SHT85_I2C_SendACK(1);  // 最后一字节NACK
            
            SHT85_I2C_Stop();
            
            // CRC校验
            crc_temp = SHT85_CRC8(&data[0], 2);
            crc_humi = SHT85_CRC8(&data[3], 2);
            
            if(crc_temp != data[2] || crc_humi != data[5])
            {
                error++;
                step = 0;
                return 0;  // CRC校验失败
            }
            
            // 解析原始数据
            SHT85.temp_raw = ((unsigned short)data[0] << 8) | data[1];
            SHT85.humi_raw = ((unsigned short)data[3] << 8) | data[4];
            
            // 计算实际值
            // 温度公式: T = -45 + 175 * rawT / 65535
            SHT85.temp = -45.0f + 175.0f * (float)SHT85.temp_raw / 65535.0f;
            
            // 湿度公式: RH = 100 * rawRH / 65535
            SHT85.humi = 100.0f * (float)SHT85.humi_raw / 65535.0f;
            
            // 湿度范围限制
            if(SHT85.humi < 0) SHT85.humi = 0;
            if(SHT85.humi > 100) SHT85.humi = 100;
            
            error = 0;
            step = 0;  // 回到初始状态，准备下次测量
            break;
            
        default:
            step = 0;
            break;
    }
    
    return 1;
}
