#include "iic.h"

unsigned int Param_Radi[2];
unsigned int Saved_Time;
unsigned int Param_Radi_ALL[2];
unsigned int Saved_NO;                //已存储数据大小,清除数据就是清这个
unsigned int Param_Adj_Flow[2];

#define AT24C02_DEV_ADDR 0xA0    //设备地址

#define AT24C02_PAGE_SIZE 8      // AT24C02页大小为8字节,32页
#define AT24C02_TOTAL_SIZE 256   // AT24C02总容量256字节
void delay_ms(uint32_t count);

void i2c1_gpio_config(void)
{
    rcu_periph_clock_enable(I2C_RCU);
    rcu_periph_clock_enable(RCU_I2C1);
    
    /* 将PA0复用为I2C1_SCL, 将PA1复用为I2C1_SDA */
    gpio_af_set(I2C_Port, GPIO_AF_4, I2C_SCL_Pin); // SCL
    gpio_af_set(I2C_Port, GPIO_AF_4, I2C_SDA_Pin); // SDA
	//PA4 WP
    gpio_mode_set(I2C_Port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, I2C_WP_Pin);
    gpio_output_options_set(I2C_Port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, I2C_WP_Pin);
	gpio_bit_reset(I2C_Port,I2C_WP_Pin);
    /* 配置GPIO模式：复用开漏输出，上拉，50MHz */
    gpio_mode_set(I2C_Port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, I2C_SCL_Pin);
    gpio_output_options_set(I2C_Port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, I2C_SCL_Pin);
    
    gpio_mode_set(I2C_Port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, I2C_SDA_Pin);
    gpio_output_options_set(I2C_Port, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, I2C_SDA_Pin);
	
	gpio_bit_reset(I2C_Port,I2C_WP_Pin);
}

void i2c1_config(void)
{
    /* 配置I2C时钟：标准模式，100kHz */
    i2c_clock_config(I2C1, 100000, I2C_DTCY_2);
    /* I2C地址配置 */
    i2c_mode_addr_config(I2C1, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x00); 
    /* 使能I2C1 */
    i2c_enable(I2C1);
    /* 使能应答 */
    i2c_ack_config(I2C1, I2C_ACK_ENABLE);
}

/**
 * @brief 单字节写入 (保留原有函数)
 */
uint8_t at24c02_byte_write(uint8_t mem_addr, uint8_t data)
{
    /* 等待I2C总线空闲 */
    while(i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));
    
    /* 发送起始条件 */
    i2c_start_on_bus(I2C1);
    while(!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));
    gpio_bit_reset(I2C_Port,I2C_WP_Pin);
    /* 发送设备地址 + 写命令 */
    i2c_master_addressing(I2C1, AT24C02_DEV_ADDR, I2C_TRANSMITTER);
    while(!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C1, I2C_FLAG_ADDSEND);
    
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 发送存储器地址 */
    i2c_data_transmit(I2C1, mem_addr);
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 发送数据 */
    i2c_data_transmit(I2C1, data);
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 发送停止条件 */
    i2c_stop_on_bus(I2C1);
    while(I2C_CTL0(I2C1) & I2C_CTL0_STOP);
    
    return 0;
}

/**
 * @brief 单字节读取 (保留原有函数)
 */
uint8_t at24c02_byte_read(uint8_t mem_addr, uint8_t *pdata)
{
    if(pdata == NULL) return 1;
    
    /* 等待I2C总线空闲 */
    while(i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));
    
    /* 阶段1：发送设备地址和要读取的存储器地址 */
    i2c_start_on_bus(I2C1);
    while(!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));
    
    i2c_master_addressing(I2C1, AT24C02_DEV_ADDR, I2C_TRANSMITTER);
    while(!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C1, I2C_FLAG_ADDSEND);
    
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    i2c_data_transmit(I2C1, mem_addr);
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 阶段2：重新起始条件，开始读取数据 */
    i2c_start_on_bus(I2C1);
    while(!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));
    
    i2c_master_addressing(I2C1, AT24C02_DEV_ADDR, I2C_RECEIVER);
    while(!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C1, I2C_FLAG_ADDSEND);
    
    /* 在接收最后一个字节前，需要禁用ACK并发送停止条件 */
    i2c_ack_config(I2C1, I2C_ACK_DISABLE);
    
    /* 等待接收缓冲区非空 */
    while(!i2c_flag_get(I2C1, I2C_FLAG_RBNE));
    
    /* 读取数据 */
    *pdata = i2c_data_receive(I2C1);
    
    /* 发送停止条件 */
    i2c_stop_on_bus(I2C1);
    while(I2C_CTL0(I2C1) & I2C_CTL0_STOP);
    
    /* 重新使能ACK以备后续通信 */
    i2c_ack_config(I2C1, I2C_ACK_ENABLE);
    
    return 0;
}

/**
 * @brief 页写入 - 在同一页内连续写入多个字节
 * @param mem_addr: 起始存储器地址
 * @param pdata: 要写入的数据指针
 * @param len: 要写入的字节数 (不能超过页边界)
 * @return 0: 成功, 其他: 失败
 * @note 此函数假设所有数据都在同一页内，不会跨页写入
 */
uint8_t at24c02_page_write(uint8_t mem_addr, uint8_t *pdata, uint8_t len)
{
    uint8_t i;
    
    if(pdata == NULL) return 1;
    if(len == 0) return 1;
    
    /* 检查是否跨页边界 */
    uint8_t page_start = mem_addr / AT24C02_PAGE_SIZE;
    uint8_t page_end = (mem_addr + len - 1) / AT24C02_PAGE_SIZE;
    
    if(page_start != page_end) {
        /* 跨页写入，返回错误或使用连续写入函数 */
        return 2;
    }
    
    /* 等待I2C总线空闲 */
    while(i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));
    
    /* 发送起始条件 */
    i2c_start_on_bus(I2C1);
    while(!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));
    
    /* 发送设备地址 + 写命令 */
    i2c_master_addressing(I2C1, AT24C02_DEV_ADDR, I2C_TRANSMITTER);
    while(!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C1, I2C_FLAG_ADDSEND);
    
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 发送起始存储器地址 */
    i2c_data_transmit(I2C1, mem_addr);
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 连续发送数据 */
    for(i = 0; i < len; i++) {
        i2c_data_transmit(I2C1, pdata[i]);
        while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    }
    
    /* 发送停止条件 */
    i2c_stop_on_bus(I2C1);
    while(I2C_CTL0(I2C1) & I2C_CTL0_STOP);
    
    return 0;
}

/**
 * @brief 连续写入 - 自动处理页边界限制
 * @param mem_addr: 起始存储器地址
 * @param pdata: 要写入的数据指针
 * @param len: 要写入的总字节数
 * @return 0: 成功, 其他: 失败
 * @note 此函数会自动处理跨页写入，将长数据分割为多个页写入操作
 */
uint8_t at24c02_sequential_write(uint8_t mem_addr, uint8_t *pdata, uint16_t len)
{
    uint16_t bytes_written = 0;
    uint16_t bytes_remaining = len;
    uint8_t current_addr = mem_addr;
    uint8_t write_size;
    
    if(pdata == NULL) return 1;
    if(len == 0) return 1;
   
    while(bytes_remaining > 0) {
        /* 计算当前页剩余空间 */
        uint8_t page_remaining = AT24C02_PAGE_SIZE - (current_addr % AT24C02_PAGE_SIZE);
        
        /* 确定本次写入的字节数 */
        write_size = (bytes_remaining < page_remaining) ? bytes_remaining : page_remaining;
        
        /* 执行页写入 */
        if(at24c02_page_write(current_addr, &pdata[bytes_written], write_size) != 0) {
            return 2; // 页写入失败
        }
        
        /* 更新计数器和地址 */
        bytes_written += write_size;
        bytes_remaining -= write_size;
        current_addr += write_size;
        
        /* 等待EEPROM内部写操作完成 */
        delay_ms(5);
    }
    return 0;
}

/**
 * @brief 连续读取多个字节
 * @param mem_addr: 起始存储器地址
 * @param pdata: 存储读取数据的指针
 * @param len: 要读取的字节数
 * @return 0: 成功, 其他: 失败
 * @note AT24C02支持连续读取，不需要考虑页边界
 */
uint8_t at24c02_sequential_read(uint8_t mem_addr, uint8_t *pdata, uint16_t len)
{
    uint16_t i;
    
    if(pdata == NULL) return 1;
    if(len == 0) return 1;
    
    /* 等待I2C总线空闲 */
    while(i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));
    
    /* 阶段1：发送设备地址和要读取的起始存储器地址 */
    i2c_start_on_bus(I2C1);
    while(!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));
    
    i2c_master_addressing(I2C1, AT24C02_DEV_ADDR, I2C_TRANSMITTER);
    while(!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C1, I2C_FLAG_ADDSEND);
    
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    i2c_data_transmit(I2C1, mem_addr);
    while(!i2c_flag_get(I2C1, I2C_FLAG_TBE));
    
    /* 阶段2：重新起始条件，开始连续读取数据 */
    i2c_start_on_bus(I2C1);
    while(!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));
    
    i2c_master_addressing(I2C1, AT24C02_DEV_ADDR, I2C_RECEIVER);
    while(!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
    i2c_flag_clear(I2C1, I2C_FLAG_ADDSEND);
    
    /* 连续读取数据 */
    for(i = 0; i < len; i++) {
        if(i == len - 1) {
            /* 最后一个字节：发送NACK和停止条件 */
            i2c_ack_config(I2C1, I2C_ACK_DISABLE);
        } else {
            /* 非最后一个字节：保持ACK使能 */
            i2c_ack_config(I2C1, I2C_ACK_ENABLE);
        }
        
        /* 等待接收缓冲区非空 */
        while(!i2c_flag_get(I2C1, I2C_FLAG_RBNE));
        
        /* 读取数据 */
        pdata[i] = i2c_data_receive(I2C1);
    }
    
    /* 发送停止条件 */
    i2c_stop_on_bus(I2C1);
    while(I2C_CTL0(I2C1) & I2C_CTL0_STOP);
    
    /* 重新使能ACK以备后续通信 */
    i2c_ack_config(I2C1, I2C_ACK_ENABLE);
    
    return 0;
}

void delay_ms(uint32_t count)
{
    for(uint32_t i = 0; i < count; i++) {
        for(uint32_t j = 0; j < 10000; j++) {
            __NOP();
        }
    }
}

void IIC_Write_Int(unsigned int *byte, unsigned int address, unsigned int count)
{
    unsigned char Data_Tmp[32];
    for(unsigned char i = 0; i < count; i++)
    {
        Data_Tmp[2*i] = *(byte+i) >> 8;
        Data_Tmp[2*i + 1] = *(byte+i)&0x00FF;
    }
    count *=2;
    at24c02_sequential_write(address,Data_Tmp,count);
}

void IIC_Read_Int(unsigned int *byte, unsigned int address, unsigned int count)
{
    unsigned char Data_Tmp[32];
    unsigned int Tmp;
    count*=2;
    at24c02_sequential_read(address, Data_Tmp, count);
    for(unsigned char i = 0; i < count; i+=2)
    {
        Tmp = Data_Tmp[i];
        Tmp = Tmp*256 + Data_Tmp[i+1];
        *(byte+i/2) = Tmp;
    }
}
void STOP_Save_Data(void)
{
    unsigned int Radi_Tmp[4];
    for(unsigned char i = 0; i<2; i++)
    {
        Radi_Tmp[i*2 + 0] = Radi_ALL[i] >> 16;
        Radi_Tmp[i*2 + 1] = Radi_ALL[i] & 0xFFFF;
    }
	IIC_Write_Int(Radi_Tmp, Addr_EEP_Radi_Accum, 4);   
}
void Clear_Accum_Data(void)
{
    unsigned int Radi_Tmp[4];
    for(unsigned char i = 0; i<2; i++)
    {
        Radi_ALL[i] = 0;
        Radi_Tmp[i*2 + 0] = 0;
        Radi_Tmp[i*2 + 1] = 0;
        Param_Radi_ALL[i] = 0;
    }
	IIC_Write_Int(Radi_Tmp, Addr_EEP_Radi_Accum, 4);  
}
void EEP_Read_Flow(void)
{
    unsigned int Flow_Tmp[4];
    unsigned long Tmp;
    IIC_Read_Int(Flow_Tmp, Addr_EEP_Adj_Flow, 2);
    for(unsigned char i = 0; i<2; i++)
    {
        Tmp = Flow_Tmp[i*2+0];
        Tmp = (Tmp << 16) + Flow_Tmp[i*2+1];
        Param_Adj_Flow[i] = Tmp;
    }
}
void EEP_Write_Flow(void)
{
    unsigned int Flow_Tmp[4];
    for(unsigned char i = 0; i<2; i++)
    {
        Flow_Tmp[i*2 + 0] = Param_Adj_Flow[i] >> 16;
        Flow_Tmp[i*2 + 1] = Param_Adj_Flow[i] & 0xFFFF;
    }
    IIC_Write_Int(Flow_Tmp, Addr_EEP_Adj_Flow, 2);
}
void Read_EEPROM(void)//读出设定参数
{
	EEP_Read_Flow();
	IIC_Read_Int(Param_Adj_Radi, Addr_EEP_Adj_Radi, 2);
	IIC_Read_Int(Param_Adj_Dir, Addr_EEP_Adj_Dir, 1);
	
}
void Init_EEPROM(void) //初始化数据
{
    Radi_ALL[0] =0;
    Radi_ALL[1] =0;
    for(unsigned char i = 0; i < 2; i++)
    {
        Param_Adj_Radi[i] = 10000;
    }
    IIC_Write_Int(Param_Adj_Radi, Addr_EEP_Adj_Radi, 2);
	IIC_Write_Int(Param_Adj_Radi, Addr_EEP_Adj_Radi, 2);
	
    Param_Adj_Flow[0] = 0X01;
    EEP_Write_Flow();
	
	Param_Adj_Dir[0] = 3180;
    IIC_Write_Int(Param_Adj_Dir,Addr_EEP_Adj_Dir,1);
}



//测试
//	uint8_t array[30],byte[30];
//	for(uint8_t i = 0;i < 30;i++)			
//	{
//		array[i] = i;
//	}
//	at24c02_sequential_write(0x05,array,30);
//	at24c02_sequential_read(0x05,byte,30);
