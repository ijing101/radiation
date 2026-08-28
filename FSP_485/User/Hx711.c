#include "Hx711.h"
unsigned char HX711_Stable,HX711_UnStable;
unsigned char HX711_Count;
unsigned int Radi_Back[2];//备份值
unsigned int Param_Adj_Radi[2];
unsigned int Param_Adj_Dir[5]; //校准系数
unsigned int Param_Radi_1[2];
unsigned int Param_Radi_ALL_1[2];
unsigned int Radi_ALL[2];


unsigned int Hx711_Data(void)
{
	unsigned long Data = 0x0000;
    for(unsigned char i = 0; i < 24; i++)//A通道，27位，64增益，10HZ
    {
        gpio_bit_set(Hx711_Port,Hx711_SCK);
        Delay_NOP(10);
        gpio_bit_reset(Hx711_Port,Hx711_SCK);
        Delay_NOP(10);
        Data = Data<<1;
        if((gpio_input_bit_get(Hx711_Port,Hx711_DOUT)) == 1)
        {
            Data++;
        }
    }
    gpio_bit_set(Hx711_Port,Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_reset(Hx711_Port,Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_set(Hx711_Port,Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_reset(Hx711_Port,Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_set(Hx711_Port,Hx711_SCK);
    Delay_NOP(10);	
    gpio_bit_reset(Hx711_Port,Hx711_SCK);
    Delay_NOP(10);
    Data = Data^0x800000; 
    return Data;
}

void Hx711_init(void)
{
	rcu_periph_clock_enable(Hx711_RCU);
	//SCK
	gpio_mode_set(Hx711_Port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, Hx711_SCK);
    gpio_output_options_set(Hx711_Port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, Hx711_SCK);
	gpio_bit_reset(Hx711_Port,Hx711_SCK);
	//DOUT
	gpio_mode_set(Hx711_Port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, Hx711_DOUT);
	gpio_bit_set(Hx711_Port,Hx711_DOUT);
	delay_1ms(10);
	Hx711_Data();
	delay_1ms(10);
	Hx711_Data();

 
}

void Read_Hx711(void)
{
    signed int Tmp;
    if((gpio_input_bit_get(Hx711_Port,Hx711_DOUT)) == 0)//HX711准备好了
    {
        if(HX711_Count != 0)
        {
            Tmp = Hx711_Data() >> 10; 	
            if(Tmp >= 0x2000)//0x1FFF
            {
                HX711_UnStable = 0x00;
                if(++HX711_Stable > 1)
                {
                    //Tmp = (Tmp - 0x1FFF)*1000/2650; //10uV的标准值
					Tmp = (Tmp - 0x2000)*1000/Param_Adj_Dir[0]; 
					
                    Radi_Back[0] = Tmp;
                }
                else
                {
                    Tmp = Radi_Back[0];
                }
            }
            else
            {
				Tmp = 0;
				if(++HX711_UnStable > 1)
				{
						Tmp = 0x00;
						Radi_Back[0] = Tmp;
				}
				else
				{
						Tmp = Radi_Back[0];
				}
            }
            Param_Radi_1[0]= Tmp*10000/Param_Adj_Radi[0];
			if(Param_Radi_1[0]>1368)
			{
				Param_Radi_1[0]= 1268;//0x04F4
			}
			else
			{
				Param_Radi_1[0]=Param_Radi_1[0];
			}
//----------------------------------------------------------------------------//
            if(++HX711_Count > 3)
            {
                HX711_Count = 0;
                HX711_Stable = 0;
                HX711_UnStable = 0;
            }
        }
        else//彻换后的第一次不能要
        {
            ++HX711_Count;
            Tmp = Hx711_Data();
        }
    }
}


