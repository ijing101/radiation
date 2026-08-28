#include "I2C.h"
//unsigned int Param_Radi[2];
//unsigned int Saved_Time;
//unsigned int Param_Radi_ALL[2];
//unsigned int Saved_NO;                //已存储数据大小,清除数据就是清这个
//unsigned int Param_Adj[2];
//void I2C_Init(void)
//{ 
//	rcu_periph_clock_enable(RCU_GPIOA);
//	rcu_periph_clock_enable(RCU_I2C1);	
//	//WP
//	gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_2);
//    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
//	gpio_bit_set(GPIOA,GPIO_PIN_2);
//	//SCL_PA0   SDA_PA1
//	gpio_af_set(GPIOA, GPIO_AF_4, GPIO_PIN_0 | GPIO_PIN_1);	
//	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_0 | GPIO_PIN_1);
//    gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_0 | GPIO_PIN_1);
//	
//	i2c_clock_config(I2C1, 100000, I2C_DTCY_2);//100k，占空比2
//	i2c_mode_addr_config(I2C1,I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0xA0);//7位地址格式
//	i2c_enable(I2C1);
//	i2c_ack_config(I2C1,I2C_ACK_ENABLE);
//}

//unsigned char *I2C_Write(unsigned char *byte,unsigned int address,unsigned int count)
//{
//	unsigned char n;
//    //unsigned int i;
//	i2c_start_on_bus(I2C1);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_SBSEND));

//	i2c_master_addressing(I2C1,0xA0,I2C_TRANSMITTER);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_ADDSEND));
//	i2c_flag_clear(I2C1,I2C_FLAG_ADDSEND);

//	while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));

//	if(AT24C_VALUE > 16)
//    {
//		 i2c_data_transmit(I2C1,(address>>8)&0XFF);
//         while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));
//    }
//	i2c_data_transmit(I2C1,address);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));
//	for(n=0; n<count; n++)
//    {
//        //send data
//		i2c_data_transmit(I2C1,*(byte+n));
//		while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));
//    }
//	i2c_stop_on_bus(I2C1);
//	return (byte+n);
//}
//void I2C_Write_Byte(unsigned char *byte,unsigned int address,unsigned int count)
//{
//	volatile static unsigned int N,A;
//    unsigned int n;
//    unsigned int i;
//	n = address/PAGE_SIZE;     //确定地址与块地址的差
//    if(n)
//    {
//        n = PAGE_SIZE*(n+1) - address;
//    }
//    else
//    {
//        n = PAGE_SIZE-address;
//    }
//	gpio_bit_reset(GPIOA,GPIO_PIN_2);
//	 for(i=0; i<600; i++)
//    {
//        ;
//    }
//    if(n >= count)     //如果address所在的数据块的末尾地址 >= address + count, 就直接写入count个数据
//    {
//        I2C_Write(byte,address,count);
//        Delay_NOP(600);
//    }
//	else           //如果address所在的数据块末尾地址 < address + count, 就先写入address所在的数据块末尾地址与 address 之差个数据
//    {
//        N = count;
//        A = address;
//        byte = I2C_Write(byte,address,n);
//        Delay_NOP(600);
//        count = N;
//        address = A;
//        count -= n;     //更新剩下数据个数
//        address += n; //更新剩下数据的起始地址
//        //把剩下数据写入器件
//        while(count >= PAGE_SIZE) //先按PAGE_SIZE为长度一页一页的写入
//        {
//            N = count;
//            A = address;
//            byte = I2C_Write(byte,address,PAGE_SIZE);
//            Delay_NOP(600);
//            count = N;
//            address = A;
//            count -= PAGE_SIZE;  //更新剩余数据个数
//            address += PAGE_SIZE;    //更新剩下数据的起始地址
//        }
//        if(count)         //把最后剩下的小于一个PAGE_SIZE长度的数据写入器件
//        {
//            I2C_Write(byte,address,count);
//        }
//        Delay_NOP(600);
//    }
//	gpio_bit_set(GPIOA,GPIO_PIN_2);
//}
//void I2C_Write_Int(unsigned int *byte,unsigned int address,unsigned int count)
//{
//    unsigned char Data_Tmp[32];
//    for(unsigned char i = 0; i < count; i++)
//    {
//        Data_Tmp[2*i] = *(byte+i) >> 8;
//        Data_Tmp[2*i + 1] = *(byte+i)&0x00FF;
//    }
//    count *=2;
//    I2C_Write_Byte(Data_Tmp,address,count);
//}
//void I2C_Read_Byte(unsigned char *byte,unsigned int address,unsigned int count)
//{
//	uint8_t n;
//	i2c_start_on_bus(I2C1);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_SBSEND));
//	i2c_master_addressing(I2C1,0xA0,I2C_TRANSMITTER);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_ADDSEND));
//	i2c_flag_clear(I2C1,I2C_FLAG_ADDSEND);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));
//	if(AT24C_VALUE > 16)
//    {
//		 i2c_data_transmit(I2C1,(address>>8)&0XFF);
//         while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));
//    }
//	i2c_data_transmit(I2C1,address);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_TBE));
//	i2c_start_on_bus(I2C1);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_SBSEND));
//	i2c_master_addressing(I2C1,0xA1,I2C_RECEIVER);
//	while(!i2c_flag_get(I2C1,I2C_FLAG_ADDSEND));
//	i2c_ack_config(I2C1,I2C_ACK_DISABLE);
//	i2c_flag_clear(I2C1,I2C_FLAG_ADDSEND);
////	i2c_stop_on_bus(I2C1);
//	for(n=0; n < (count-1); n++)
//    {
//		while(!i2c_flag_get(I2C1,I2C_FLAG_RBNE));
//        *(byte+n)= i2c_data_receive(I2C1);         //读取数据
//		i2c_ack_config(I2C1,I2C_ACK_ENABLE);
//		
//    }
//}
//void I2C_Read_Int(unsigned int *byte,unsigned int address,unsigned int count)
//{
//    unsigned char Data_Tmp[32];
//    unsigned int Tmp;
//    count*=2;
//    I2C_Read_Byte(Data_Tmp,address,count);
//    for(unsigned char i = 0; i < count; i+=2)
//    {
//        Tmp = Data_Tmp[i];
//        Tmp = Tmp*256 + Data_Tmp[i+1];
//        *(byte+i/2) = Tmp;
//    }
//}
//void Read_EEPROM(void)//读出设定参数
//{
//    I2C_Read_Int(&Saved_NO,Addr_EEP_Saved_NO,1);
////    I2C_Read_Int(&Saved_Time,Addr_EEP_Saved_Time,1);
//    I2C_Read_Int(Param_Adj_Radi,Addr_EEP_Adj_Radi,2);
//}
//void Init_EEPROM(void) //初始化数据
//{
//    Saved_NO = 0;
////    I2C_Write_Int(&Saved_NO,Addr_EEP_Saved_NO,1);
//    Saved_Time = 1;
////    I2C_Write_Int(&Saved_Time,Addr_EEP_Saved_Time,1);
//    Radi_ALL[0] =0;
//    Radi_ALL[1] =0;
//    for(unsigned char i = 0; i < 2; i++)
//    {
//        Param_Adj_Radi[i] = 10000;
//    }
//    I2C_Write_Int(Param_Adj_Radi,Addr_EEP_Adj_Radi,2);
//}

//void STOP_Save_Data(void)
//{
//    unsigned int Radi_Tmp[4];
//    for(unsigned char i = 0; i<2; i++)
//    {
//        Radi_Tmp[i*2 + 0] = Radi_ALL[i] >> 16;
//        Radi_Tmp[i*2 + 1] = Radi_ALL[i] & 0xFFFF;
//    }
//    I2C_Write_Int(Radi_Tmp,Addr_EEP_Radi_Accum,4);
//}
//void Clear_Accum_Data(void)
//{
//    unsigned int Radi_Tmp[4];
//    for(unsigned char i = 0; i<2; i++)
//    {
//        Radi_ALL[i] = 0;
//        Radi_Tmp[i*2 + 0] = 0;//Radi_ALL[i] >> 16;
//        Radi_Tmp[i*2 + 1] = 0;//Radi_ALL[i] & 0xFFFF;
//        Param_Radi_ALL[i] = 0;//
//    }
//    I2C_Write_Int(Radi_Tmp,Addr_EEP_Radi_Accum,4);
//}

//void Save_Record(void)//定时保存数据
//{
//	if(++Save_Count >= 1)//分钟,时间到
//	{
//		Save_Count = 0;
//		for(unsigned char i = 0; i < 2; i++)
//        {
//            Param_Radi[i] = Param_Radi_ALL_1[i]/Radi_Count;
//        }
//		for(unsigned char i = 0; i < 2; i++)
//        {
//           Param_Radi_ALL_1[i] = 0;
//        }
//		for(unsigned char i = 0; i < 1; i++)
//        {
//            Radi_ALL[i] += Param_Radi[i]*60;
//            //累计值
//            Param_Radi_ALL[i] = Radi_ALL[i]/1000;
//        }
////		STOP_Save_Data();
//		Radi_Count = 0;
//	}
//}

//void Test_Time(void)
//{
//    if(State_ALL == POWER_ON)  // ==0x04  State_ALL == STANDBY
//    {
//        if(DS1302_Data[0] != DS1302_Data_Back)
//        {
//            DS1302_Data_Back = DS1302_Data[0];
//            if(++Test_Sec > 59)
//            {
//                Test_Sec = 0x00;
//                if(++Test_Min > 59)
//                {
//                    Test_Min = 0x00;
//                    if(++Test_Hour > 99)
//                    {
//                        Test_Hour = 0x00;
//                    }
//                }
//                Save_Record();//定时保存数据
//				usart_data_transmit(UART4, 0x77);
//            }
//            for(unsigned char i = 0; i < 2; i++)
//            {
//                Param_Radi_ALL_1[i] += Param_Radi_1[i];
//            }
//            Radi_Count++;
//     }
//    }
//    if((F_AUTO_Clear_Data == 0) & (DS1302_Data[Hour_B] == 0x00) & (DS1302_Data[Min_B] == 0x00) & (DS1302_Data[Sec_B] == 0x00))
//    {
//        F_AUTO_Clear_Data = 1;
//        Clear_Accum_Data();
//    }
//    else
//    {
//        F_AUTO_Clear_Data = 0;
//    }
//}
