#include "Usart.h"
char rx_buffer0[RX_BUFFER_SIZE0];     //接收原始数据缓冲
char tx_buffer0[TX_BUFFER_SIZE0];     //发送数据缓冲
char Recv_Buf0[RX_BUFFER_SIZE0];      //接收数据缓冲
char Send_Buf0[TX_BUFFER_SIZE0];      //发送数据缓冲
char Slave_ADD;//仪器地址位
volatile unsigned char F_Data_Begin0;        //数据头开始标志
volatile unsigned char F_Data_Error0;        //数据比较错误标志
volatile unsigned char Recv_Data_Len;       //有效接收数据长读
volatile unsigned char Send_Data_Len;       //有效发送数据长读
volatile unsigned char UART0_RX_NO;         //串口收到的数据量
uint8_t Uart_data_flag;

const unsigned int wCRCTalbeAbs[16] =
{
    0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400,
};
unsigned char HEX_ASCII(unsigned char Data)
{
    if(Data>9)
    {
        Data = (Data-10+'A');
    }
    else
    {
        Data = Data+'0';
    }
    return Data;
}
unsigned char ASCII_HEX(unsigned char Data)
{
    if(Data >= 'A')
    {
        Data = Data - 'A' + 10;
    }
    else
    {
        Data = Data-'0';
    }
    return Data;
}
unsigned char BcdToBin_Char(unsigned char val) //将BCD码转换为10进制数
{
    return((val & 0x000000F0)>>4)*10+(val&0x0f);
}
unsigned char BinToBcd_Char(unsigned char val) //将10码转换为BCD进制数
{
    return (((val%100)/10)*0x10 + val%10);
}
unsigned int CalculateBCC16(char* pchMsg,unsigned char wDataLen)
{
    unsigned int wBCC = 0x0000;
    unsigned char i = 0;
    for (i = 0; i < wDataLen; i++)
    {
        wBCC +=  ASCII_HEX(*pchMsg++);
    }
    wBCC = wBCC & 0xFFFF;
    return wBCC;
}
unsigned int CalculateCRC16(char *Data_Tmp,unsigned char D_Len)
{
    unsigned int wCRC = 0xFFFF;
    //unsigned int CRCValue = 0x0000;
    unsigned char i = 0;
    unsigned char chChar = 0;
    for (i = 0; i < D_Len; i++)
    {
        chChar = *(Data_Tmp+i);
        wCRC = wCRCTalbeAbs[(chChar ^ wCRC) & 15] ^ (wCRC >> 4);
        wCRC = wCRCTalbeAbs[((chChar >> 4) ^ wCRC) & 15] ^ (wCRC >> 4);
    }
    return wCRC;
}

void gd_eval_com_init(void)
{
	rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);
	gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
    	
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 9600U);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART0, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART0, USART_CTS_DISABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);
}
void UART_Write(char *pData, uint32_t dataLen)
{
	uint8_t i;	
    for(i = 0; i < dataLen; i++)
	{
		usart_data_transmit(USART0, pData[i]);                  // 发送一个字节数据
        while(RESET == usart_flag_get(USART0, USART_FLAG_TBE)); // 发送完成判断
	}
}
int fputc(int ch, FILE *f)
{
    usart_data_transmit(USART0, (uint8_t)ch);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    return ch;
}
int fgetc(FILE *f)
{
	uint8_t ch = 0;
	ch = usart_data_receive(USART0);
    return ch;
}
unsigned char MODBUS_RX(void)
{
	char Slave_ADD= Param_Adj_Flow[0];
	if(RESET != usart_flag_get(USART0, USART_FLAG_RBNE)) // 等待接收缓冲区非空
    {	
		usart_flag_clear(USART0, USART_FLAG_RBNE);
		uint8_t data = usart_data_receive(USART0);
		if(UART0_RX_NO < RX_BUFFER_SIZE0)
		{
			rx_buffer0[UART0_RX_NO++] = data;	
		}
	}
	if(RESET != usart_flag_get(USART0, USART_FLAG_IDLE)) //串口空闲中断标志 USART_FLAG_IDLE
		{
			unsigned char Data_L = 0;
			unsigned char i = 0;
			usart_data_receive(USART0); // 先读数据寄存器
			usart_flag_clear(USART0, USART_FLAG_IDLE); // 再清除标志位
			while((Data_L < UART0_RX_NO) && (UART0_RX_NO <= 64))
			{
				if((rx_buffer0[Data_L] == 0x00)|(rx_buffer0[Data_L] == Slave_ADD))//一直扫描到有本机地址
				{
					for(i = 0; (i + Data_L) < UART0_RX_NO; i++)
					{
						Recv_Buf0[i] = rx_buffer0[Data_L+i]; //接收数据缓冲
					}
					return  i;
				}
				Data_L++;
			}
			for(i = 0; i < RX_BUFFER_SIZE0; i++)
			{
				rx_buffer0[i] = 0x00;
			}
		}
	return 0;
}

void MODBUS_DEAL(void)
{
	unsigned char temp[2];
    unsigned int CRC16;
    Recv_Data_Len = MODBUS_RX();
//	Recv_Data_Len =UART0_RX_NO;
	char Slave_ADD= Param_Adj_Flow[0];
	if(Recv_Data_Len > 0)
	{
		UART0_RX_NO = 0;
		CRC16 = CalculateCRC16(Recv_Buf0,Recv_Data_Len-2);
        temp[1] = CRC16 & 0xff;
        temp[0] = (CRC16 >> 8) & 0xff;
        if((Recv_Buf0[Recv_Data_Len-1]==temp[0])&&(Recv_Buf0[Recv_Data_Len-2]==temp[1]))
        {
            switch(Recv_Buf0[1])
            {
            case 0x03:
                Modbus_Function_3(); //读取各种数据(温度,流量,时间和修正值等)
                break;
            case 0x05:
                Modbus_Function_5();//清除数据存储
                break;
            case 0x10:
                Modbus_Function_16();//设置各种参数(修正值,时间,)
                break;
            default:
            {
                Send_Buf0[1]=0xFF;
                Send_Buf0[2]=2;
                Send_Buf0[3]=0x00;
                Send_Buf0[4]=0x06;
                Send_Data_Len=5;
            }
            }
            Send_Buf0[0] = Slave_ADD;
            CRC16 = CalculateCRC16(Send_Buf0,Send_Data_Len);
            Send_Buf0[Send_Data_Len++] = CRC16 & 0xFF;
            Send_Buf0[Send_Data_Len++] = (CRC16 >> 8) &0xFF;
            UART_Write(Send_Buf0,Send_Data_Len);
        }
	}
}
void Modbus_Function_3(void)
{
    unsigned int Add_Temp,Data_NO;
    unsigned char Count_Temp;
    Send_Buf0[1]=0x03;
    Add_Temp = (Recv_Buf0[2] << 8) + Recv_Buf0[3];
    Data_NO  = (Recv_Buf0[4] << 8) + Recv_Buf0[5];
    switch(Add_Temp)
    {
		case 0x0007://辐射值
	   if(Data_NO == 0x01)
        {
		    Send_Buf0[2]=2*Data_NO;
            Send_Data_Len=2*Data_NO + 3;
            Count_Temp = 3;
            for(unsigned char i = 0; i < 1; i++)
            {
                Send_Buf0[Count_Temp+i*2] = (Param_Radi_1[i] >> 8);//辐射
                Send_Buf0[Count_Temp+i*2+1] = (Param_Radi_1[i] & 0xFF);
            }
        }
        else
        {
            //show   error!
            Send_Buf0[1]=0xFF;
            Send_Buf0[2]=2;
            Send_Buf0[3]=0x00;
            Send_Buf0[4]=0x04;
            Send_Data_Len=5;
        }
        break;
	case 0x0090://校准系数
		if(Data_NO <= 5)
		{
			Send_Buf0[2]=2*Data_NO;
			Send_Data_Len=2*Data_NO+3;
			for(unsigned char i = 0; i < 1; i++)
            {
                Send_Buf0[3 + i*2] = (Param_Adj_Dir[i] >> 8);
                Send_Buf0[4 + i*2]=  (Param_Adj_Dir[i] & 0xFF);
            }
		}
		else
		{
			////show   error!
			Send_Buf0[1]=0xFF;
			Send_Buf0[2]=2;
			Send_Buf0[3]=0x00;
			Send_Buf0[4]=0x04;
			Send_Data_Len=5;
		}
		break;
    case 0x00B0:
        if(Data_NO == 0x01)
        {
            Send_Buf0[2]=2*Data_NO;
            Send_Data_Len=2*Data_NO+3;
			EEP_Read_Flow();
            for(unsigned char i = 0; i < 1; i++)
            {
                Send_Buf0[3+i*2]= (Param_Adj_Flow[i] >> 8);
                Send_Buf0[4+i*2]= (Param_Adj_Flow[i] & 0xFF);
            }
        }
        else
        {
            ////show   error!
            Send_Buf0[1]=0xFF;
            Send_Buf0[2]=2;
            Send_Buf0[3]=0x00;
            Send_Buf0[4]=0x04;
            Send_Data_Len=5;
        }
        break;
   case 0x00C0://查看辐射灵敏度
        if(Data_NO <= 5)
        {
            Send_Buf0[2]=2*Data_NO;
            Send_Data_Len=2*Data_NO+3;
            for(unsigned char i = 0; i < 5; i++)
            {
                Send_Buf0[3+i*2]= (Param_Adj_Radi[i] >> 8);
                Send_Buf0[4+i*2]= (Param_Adj_Radi[i] & 0xFF);
            } 
        }
        else
        {
            ////show   error!
            Send_Buf0[1]=0xFF;
            Send_Buf0[2]=2;
            Send_Buf0[3]=0x00;
            Send_Buf0[4]=0x04;
            Send_Data_Len=5;
        }
        break;
	case 0x0000://
        if(Data_NO <= 3)//读出当前的所有数据
        {
            Send_Buf0[2]=2*Data_NO;
            Send_Data_Len=2*Data_NO + 3;
            Count_Temp = 3;
			for(unsigned char i = 0; i < 1; i++)
            {
                Send_Buf0[Count_Temp+i*2] = (Param_Radi_1[i] >> 8);//瞬时
                Send_Buf0[Count_Temp+i*2+1] = (Param_Radi_1[i] & 0xFF);
            }
			Count_Temp += 2;
			for(unsigned char i = 0; i < 1; i++)
            {
                Send_Buf0[Count_Temp + i*2]= (Param_Adj_Dir[i] >> 8);//系数
                Send_Buf0[Count_Temp + i*2 + 1]=  (Param_Adj_Dir[i] & 0xFF);
            }
			Count_Temp += 2;
			for(unsigned char i = 0; i < 1; i++)
            {
                Send_Buf0[Count_Temp+i*2] = (Param_Adj_Radi[i] >> 8);//灵敏度
                Send_Buf0[Count_Temp+i*2+1] = (Param_Adj_Radi[i] & 0xFF);
            }
        }
        else
        {
            ////show   error!
            Send_Buf0[1]=0xFF;
            Send_Buf0[2]=2;
            Send_Buf0[3]=0x00;
            Send_Buf0[4]=0x04;
            Send_Data_Len=5;
        }
        break;
    }
}
void Modbus_Function_5(void)
{
    Send_Buf0[1]=0x05;
    Send_Buf0[2]=Recv_Buf0[2];
    Send_Buf0[3]=Recv_Buf0[3];
    Send_Buf0[4]=Recv_Buf0[4];
    Send_Buf0[5]=Recv_Buf0[5];
    Send_Data_Len=6;
    if((Recv_Buf0[2]==0)&&(Recv_Buf0[3]==0xF2))//清所有数据
    {
        if(Recv_Buf0[5] == 0x00)
        {
            Saved_NO = 0;//
            //IIC_Write_Int(&Saved_NO,Addr_EEP_Saved_NO,1);  
        }
        else
        {
            Send_Buf0[1]=0xFF;
            Send_Buf0[2]=2;
            Send_Buf0[3]=0x00;
            Send_Buf0[4]=0x05;
            Send_Data_Len=5;
        }
    }
    //------------------------------------------------------------------//
    else if((Recv_Buf0[2]==0)&&(Recv_Buf0[3]==0xFA))//清除所有累计值
    {
        if(Recv_Buf0[5] == 0x00)
        {
            Clear_Accum_Data();
        }
        else
        {
            Send_Buf0[1]=0xFF;
            Send_Buf0[2]=2;
            Send_Buf0[3]=0x00;
            Send_Buf0[4]=0x05;
            Send_Data_Len=5;
        }
    }
    //--------------------------------------------------------------------//
    else
    {
        Send_Buf0[1]=0xFF;
        Send_Buf0[2]=2;
        Send_Buf0[3]=0x00;
        Send_Buf0[4]=0x05;
        Send_Data_Len=5;
    }
}
void Modbus_Function_16(void)
{
    unsigned int Add_Temp,Data_NO;
    Add_Temp = (Recv_Buf0[2] << 8) + Recv_Buf0[3];
    Data_NO  =  Recv_Buf0[6]/2;
    Send_Buf0[1]=0x10;
    Send_Buf0[2]=Recv_Buf0[2];
    Send_Buf0[3]=Recv_Buf0[3];
    Send_Buf0[4]=Recv_Buf0[4];
    Send_Buf0[5]=Recv_Buf0[5];
    Send_Data_Len=6;
    switch(Add_Temp)
    {
    case 0x00F0:  //写入系统时间
        if(Data_NO == 6)//*2
        {
			
        }
        break;
    case 0x00B0://写入地址位
        if(Data_NO == 1)
        {

            for(unsigned char i = 0; i < 1; i++)
            {
                Param_Adj_Flow[i] = (Recv_Buf0[7+i*2] << 8) +Recv_Buf0[8+i*2];
            }
//            //I2C_Write_Int(Param_Adj_Flow,Addr_EEP_Adj_Flow,MAX_FLOW);
            EEP_Write_Flow();

        }
        break;
   case 0x00C0://写入辐射灵敏度值
        if(Data_NO <= 5)
        {
            for(unsigned char i = 0; i < 2; i++)
            {
                Param_Adj_Radi[i] = (Recv_Buf0[7+i*2] << 8) +Recv_Buf0[8+i*2];
            }
            IIC_Write_Int(Param_Adj_Radi,Addr_EEP_Adj_Radi,2);
        }
        break;
	 case 0x0090://写入辐射校准系数
        if(Data_NO <= 5)
        {
            for(unsigned char i = 0; i < 1; i++)
            {
                Param_Adj_Dir[i] = (Recv_Buf0[7+i*2] << 8) +Recv_Buf0[8+i*2];
            }
            IIC_Write_Int(Param_Adj_Dir, Addr_EEP_Adj_Dir, 1);
        }
        break;
    default:
        Send_Buf0[1]=0xFF;
        Send_Buf0[2]=Recv_Buf0[2];
        Send_Buf0[3]=Recv_Buf0[3];
        Send_Buf0[4]=0x00;
        Send_Buf0[5]=0x05;
        break;
    }
  
}



