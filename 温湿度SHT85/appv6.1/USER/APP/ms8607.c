#include "ms8607.h"
#include "math.h"
#define   MaxMSec                                  60000
#define   MS8607_ADDR_W_PT                         0x76<<1  //鍥營2C璁惧?囧湴鍧�鍙?鏈?7浣嶏紝鎵�浠ヨ?佸乏绉讳竴浣嶇敤<<1锛屽緱鍒颁竴涓?鏂扮殑7浣嶅湴鍧�銆?
#define   MS8607_ADDR_R_PT                         (0x76<<1)+1
#define   MS8607_ADDR_W_RH                         0x40<<1
#define   MS8607_ADDR_R_RH                         (0x40<<1)+1
#define   MS8607_PT_RESET                          0x1E       //鍘嬪姏娓╁害浼犳劅鍣ㄥ?嶄綅鍛戒护
#define   MS8607_RH_RESET                          0xFE       //婀垮害浼犳劅鍣ㄥ?嶄綅鍛戒护
#define   MS8607_READ_USER_RH                      0XE6       //璇诲彇鐢ㄦ埛瀵勫瓨鍣ㄥ懡浠?
#define   MS8607_WRITE_USER_RH                     0XE7       //鍐欏叆鐢ㄦ埛瀵勫瓨鍣ㄥ懡浠?
#define   MS8607_MEASU_PH                          0XE5       //娴嬮噺婀垮害锛屼富鏈虹瓑寰呮ā寮?
#define   MS8607_D1_8192                           0x4A       //杞?鎹?D1锛屼娇鐢?8192浣滀负鍒嗚鲸鐜?
#define   MS8607_D2_8192                           0x5A       //杞?鎹?D2锛屼娇鐢?8192浣滀负鍒嗚鲸鐜?
#define   MS8607_R_ADC_PT                          0x00       //璇诲彇ADC缁撴灉瀵勫瓨鍣?
#define   MS6807_R_ADC_RH                          0xe5       //璇诲彇婀垮害ADC缁撴灉瀵勫瓨鍣?


extern unsigned short  MMsec;
//typedef struct
//{
//  unsigned short MS_PROM[8];
//	unsigned int  Temp_D2;
//	unsigned int  Pressure_D1;
//	unsigned short  Tumi_D3;
//  float  temp;
//	float  humi;
//	float  pressure;
//}MS;
//MS MS8607;
MS MS8607;

//IIC配置函数
void MS8607_IIC_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE );  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10|GPIO_Pin_11; 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; 
	GPIO_Init(GPIOB, &GPIO_InitStructure); 
	MS8607_SCL_H;
	MS8607_SDA_H;
  
}
//实现微秒级延时，用于I2C通信中的时钟控制
void delay1us(unsigned char x)//精确延时,iic_40K
{
	unsigned char i=20;
	x=i*x;
	while(x--);
}

//实现毫秒级延时，用于传感器复位等待
void delay1ms(unsigned short x)
{
  unsigned short y;
	while(x--)
	for(y=0;y<1100;y++){}
}

//IIC开始信号
/*
IIC开始:在SCL处于高电平期间，SDA由高电平变成低电平产生一个下降沿，然后SCL拉低
*/
unsigned char MS8607_I2C_Start(void)
{
		MS8607_SDA_H; 
		delay1us(5);	//延时保证时钟频率低于40K，以便从机识别
		MS8607_SCL_H;
		delay1us(5);//延时保证时钟频率低于40K，以便从机识别
		if(!MS8607_SDA_read) return 0;//SDA为低电平，总线忙,退出
		MS8607_SDA_L;   //SCL处于高电平时，SDA拉低
		delay1us(5);
	  if(MS8607_SDA_read) return 0;//SDA为高电平，启动出错,退出
		MS8607_SCL_L;
	  delay1us(5);
	  return 1;
}


//**************************************
//IIC停止信号
/*
IIC停止:在SCL处于高电平期间，SDA由低电平变成高电平产生一个上升沿
*/
//**************************************
void MS8607_I2C_Stop(void)
{
    MS8607_SDA_L;

	  MS8607_SCL_L;
		delay1us(5);
		MS8607_SCL_H;
		delay1us(5);
		MS8607_SDA_H;//在SCL处于高电平期间，SDA由低电平变高       //延时
}
//**************************************
//IIC发送应答信号
//输入参数:ack (0:ACK 1:NAK)
/*
应答：当从机接收到数据后，向主机发送一个低电平信号
先准备好SDA电平状态，在SCL高电平时，从机读取SDA
*/
//**************************************
void MS8607_I2C_SendACK(unsigned char i)
{
    if(1==i)
		MS8607_SDA_H;	             //准备好SDA电平状态，非应答
    else 
		MS8607_SDA_L;  						//准备好SDA电平状态，应答 	
	  MS8607_SCL_H;                    //拉高时钟线
    delay1us(5);                 //延时
    MS8607_SCL_L ;                  //拉低时钟线
    delay1us(5);    
} 
//等待从机应答
/*
主机(发送)发送完一个数据后，等待从机应答
释放SDA给从机使用，然后采集SDA状态
*/
/////////////////
unsigned char MS8607_I2C_WaitAck(void) 	 //返回值为:=1有ACK,=0无ACK
{
	uint16_t i=0;
	MS8607_SDA_H;	        //释放SDA
	MS8607_SCL_H;         //SCL拉高进行采样
	while(MS8607_SDA_read)//等待SDA拉低
	{
		i++;      //等待计数
		if(i==500)//超时退出循环
		break;
	}
	if(MS8607_SDA_read)//再次判断SDA是否拉低
	{
		MS8607_SCL_L; 
		return 0;//从机应答失败，返回0
	}
  delay1us(5);//延时保证时钟频率低于40K
	MS8607_SCL_L;
	delay1us(5); //延时保证时钟频率低于40K
	return 1;//从机应答成功，返回1
}
//**************************************
//向IIC总线发送一个字节数据
/*
一个字节8bit,在SCL低电平时准备好SDA，SCL高电平时从机读取SDA
*/
//**************************************
void MS8607_I2C_SendByte(unsigned char dat)
{
  unsigned char i;
	MS8607_SCL_L;//SCL拉低，让SDA准备
  for (i=0; i<8; i++)         //8位数据发送
  {
		if(dat&0x80)//SDA准备
		MS8607_SDA_H;  
		else 
		MS8607_SDA_L; 
    MS8607_SCL_H;                //拉高时钟，让从机读取
    delay1us(5);        //延时保证IIC时钟频率，也给从机充足的采样时间
    MS8607_SCL_L;                //拉低时钟，让SDA准备
    delay1us(5);
		dat<<= 1;          //移位到下一位数据 	
  }					 
}
//**************************************
//从IIC总线接收一个字节数据
//**************************************
unsigned char MS8607_I2C_RecvByte()
{
    unsigned char i;
    unsigned char dat = 0;
    MS8607_SDA_H;//释放SDA给从机使用
    delay1us(1);   //延时让从机准备SDA时间            
    for (i=0; i<8; i++)         //8位数据接收
    { 
		  dat <<= 1;			
      MS8607_SCL_H;                //拉高时钟，读取从机SDA    
		  if(MS8607_SDA_read) //读取数据    
		     dat |=0x01;      
      delay1us(5);     //延时保证IIC时钟频率		
      MS8607_SCL_L;           //拉低时钟，准备下一位数据
      delay1us(5);   //延时让从机准备SDA时间
    } 
    return dat;
}

unsigned char crc4_PT(unsigned short n_prom[]) // n_prom定义为8x16位数组(n_prom[8])
{
int cnt;                                       //简单计数器           
unsigned int n_rem=0;                          //crc余数
unsigned char n_bit;
n_prom[0]=((n_prom[0]) & 0x0FFF);
n_prom[7]=0;
for (cnt = 0; cnt < 16; cnt++)
{
     if (cnt%2==1) n_rem ^= (unsigned short) ((n_prom[cnt>>1]) & 0x00FF);
     else   n_rem ^= (unsigned short) (n_prom[cnt>>1]>>8);
     for (n_bit = 8; n_bit > 0; n_bit--)
     {
         if (n_rem & (0x8000)) n_rem = (n_rem << 1) ^ 0x3000;
         else n_rem = (n_rem << 1);
     }
}
n_rem= ((n_rem >> 12) & 0x000F);
return (n_rem ^ 0x00);
}


//向传感器写入指定命令
int Write_MS8607Cmd(unsigned char addr,unsigned char cmd)
{
  if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();return 0;}
	MS8607_I2C_SendByte(addr);
	if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
	MS8607_I2C_SendByte(cmd);
	if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
	MS8607_I2C_Stop();
	return 1;
}

//读压力温度传感器PROM校验数据，并存储到MS_PROM数组
int Read_PTPROM(unsigned short date[])
{
	unsigned char i=0;
	unsigned char FirstProm=0xA0;
  unsigned char DateH,DateL;
  for(i=0;i<7;i++)           //读7个PROM值
	{
    if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();return 0;}
	  MS8607_I2C_SendByte(MS8607_ADDR_W_PT);
	  if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
	  MS8607_I2C_SendByte(FirstProm);
	  if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
		
		if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();return 0;}
		MS8607_I2C_SendByte(MS8607_ADDR_R_PT);
	  if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
		DateH=MS8607_I2C_RecvByte();
		MS8607_I2C_SendACK(0);
		DateL=MS8607_I2C_RecvByte();
		MS8607_I2C_SendACK(1);
		date[i]=(unsigned short)(DateH<<8)+(unsigned short)DateL;
		FirstProm+=2;		
	}
	  MS8607_I2C_Stop();	
	  return 1;
}
//设置湿度传感器分辨率，不同分辨率对应不同模式，有不同分辨率
int Write_UserRegister(unsigned mode)
{
   if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();return 0;}
	 MS8607_I2C_SendByte(MS8607_ADDR_W_RH);
	 if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
	 MS8607_I2C_SendByte(MS8607_WRITE_USER_RH);
	 if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
	 switch (mode)
	 {
	   case 0 :    MS8607_I2C_SendByte(0X02); break;
		 case 1 :    MS8607_I2C_SendByte(0X03); break;
		 case 2 :    MS8607_I2C_SendByte(0X82); break;
		 case 3 :    MS8607_I2C_SendByte(0X83); break;	
     default :   return 0;		 
   }
	 if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();return 0;}
	 MS8607_I2C_Stop();	
	 return 1;
}
//鏁版嵁璇诲彇璁＄畻鍑芥暟
//璇诲彇娓╂箍搴︼紝鍘嬪姏鏁版嵁
int MS8607_ReadDate()
{
	static unsigned char error;
	unsigned char i;
	unsigned int PT24;
  static unsigned char MS8607Step,channl;
	static unsigned int oldtime;
	int count_time;
	long pressure;	
	//long long pressure_temp;
	//long long var2;
	long long var1=0,Sens=0,Off=0,Dt=0;
	double T2,Off2,Sens2;
	//double temp_diff;

	if(error>=50)//閿欒??璁℃暟杈惧埌50娆?,澶嶄綅涓�娆′紶鎰熷櫒,闃叉?㈢郴缁熸?婚攣		
	{
	  MS8607Step=0;
	  error=0;  // 閲嶇疆閿欒??璁℃暟鍣?
	}
	switch  (MS8607Step)
	{
	  case 0 :  //复位
			        if((Write_MS8607Cmd(MS8607_ADDR_W_PT,MS8607_PT_RESET))&&(Write_MS8607Cmd(MS8607_ADDR_W_RH,MS8607_RH_RESET)))
		          {
		             MS8607Step=1;
								 oldtime=MMsec;
								 break;
		          }
              else
							{
								error++;
							  MS8607Step=0;
								return 0;							
	            }
		case 1 :  //延时5ms  等待传感器复位完成
			         count_time=MMsec-oldtime;
		           if((count_time)<0)   count_time=MMsec+MaxMSec-oldtime;
	             else                 count_time=MMsec-oldtime;
		           if(count_time>=5)
							 {
							    MS8607Step=2;
								  break;
							 }
							 else  break;
		case  2 : //读取PT的PROM存储器  压力温度传感器PROM读取校准数据
			         if(Read_PTPROM(MS8607.MS_PROM))
							 {
								  i=(MS8607.MS_PROM[0]>>12);
								  if(i==(crc4_PT(MS8607.MS_PROM)))//校验
							    {MS8607Step=3;break;}
                  else
									{MS8607Step=2;error++;break;}
							 }
							 else  
							 {
								 error++;
								 break;
							 }
		case  3 : //启动D1和D2转换  启动温度和压力AD转换
			         if(channl==0)
							 {
			            if(Write_MS8607Cmd(MS8607_ADDR_W_PT,MS8607_D2_8192)) {MS8607Step=4;}
							    else                                                 {error++;break;}		  
						   }
							 if(channl==1)
							 {
			            if(Write_MS8607Cmd(MS8607_ADDR_W_PT,MS8607_D1_8192)) {MS8607Step=4;}
							    else                                                 {error++;break;}		
						   }
							 
		case 4  : //发送读取ADC命令 读取转换后ADC结果
				        if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();error++;return 0;}
								MS8607_I2C_SendByte(MS8607_ADDR_W_PT);
								if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();error++;return 0;}
								MS8607_I2C_SendByte(MS8607_R_ADC_PT);
								if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();error++;return 0;}
								oldtime=MMsec;
								MS8607Step=5;
								break;
	  case 5  : //延时20ms等待转换完成
				       count_time=MMsec-oldtime;
		           if((count_time)<0)   count_time=MMsec+MaxMSec-oldtime;
	             else                 count_time=MMsec-oldtime;
		           if(count_time>=20)
							 {
							    MS8607Step=6;
								  break;
							 }
							 else  break;
		case 6  : //读取温压adc结果
			          PT24=0;
				        if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();error++;return 0;}
		            MS8607_I2C_SendByte(MS8607_ADDR_R_PT);
				        if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();error++;return 0;}
								for(i=0;i<2;i++)
								{
									PT24+=MS8607_I2C_RecvByte();
									MS8607_I2C_SendACK(0);
									PT24<<=8;
								}
								PT24+=MS8607_I2C_RecvByte();
								MS8607_I2C_SendACK(1);
							  MS8607_I2C_Stop();
								if(channl==0)
								{
								   channl=1;
									 MS8607.Temp_D2=PT24;
									 MS8607Step=3;
									 break;
								}
								if(channl==1)
								{
								   channl=0;
									 MS8607.Pressure_D1=PT24;
									 MS8607Step=7;
									 break;
								}
		case 7 :  //湿度传感器发送测量指令  
			        if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();error++;return 0;}
							MS8607_I2C_SendByte(MS8607_ADDR_W_RH);
							if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();error++;return 0;}
							MS8607_I2C_SendByte(MS6807_R_ADC_RH);
							if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();error++;return 0;}
							oldtime=MMsec;
							MS8607Step=8;
							break;
		case 8 :  //延时20ms
			        count_time=MMsec-oldtime;
		          if((count_time)<0)   count_time=MMsec+MaxMSec-oldtime;
	            else                 count_time=MMsec-oldtime;
		          if(count_time>=20)
							{
							   MS8607Step=9;
								 break;
							}
							else  break;
		case 9 :  //读取湿度adc结果
			        PT24=0;
				      if(MS8607_I2C_Start()==0)  {MS8607_I2C_Stop();error++;return 0;}
		          MS8607_I2C_SendByte(MS8607_ADDR_R_RH);
				      if(MS8607_I2C_WaitAck()==0) {MS8607_I2C_Stop();error++;return 0;}						
						  PT24+=MS8607_I2C_RecvByte();
							MS8607_I2C_SendACK(0);
							PT24<<=8;
							PT24+=MS8607_I2C_RecvByte();
							MS8607_I2C_SendACK(1);
							MS8607_I2C_Stop();
							MS8607.Tumi_D3=PT24;
							MS8607Step=10;
		case 10 :  //温度压力计算
			        // 基本计算（按照MS8607数据手册）
							Dt = (long long)MS8607.Temp_D2 - ((long long)MS8607.MS_PROM[5] << 8);
							
							// 一次温度计算（单位：0.01°C）
							var1 = 2000 + ((Dt * (long long)MS8607.MS_PROM[6]) >> 23);
							
							// 一次压力计算
							Off = ((long long)MS8607.MS_PROM[2] << 17) + (((long long)MS8607.MS_PROM[4] * Dt) >> 6);
							Sens = ((long long)MS8607.MS_PROM[1] << 16) + (((long long)MS8607.MS_PROM[3] * Dt) >> 7);
							
							// 温度补偿
							if(var1 >= 2000) // 高温补偿（≥20°C）
							{
							   T2 = (5 * Dt * Dt) >> 38;
								 Off2 = 0;
								 Sens2 = 0;
							}
							else // 低温补偿（<20°C）
							{
							   long long temp_diff = var1 - 2000;
							   T2 = (3 * Dt * Dt) >> 33;
								 Off2 = (61 * temp_diff * temp_diff) >> 4;
								 Sens2 = (29 * temp_diff * temp_diff) >> 4;
							   
							   if(var1 < -1500) // 极低温补偿（<-15°C）
								 {
								    long long very_low_diff = var1 + 1500;
								    Off2 = Off2 + (17 * very_low_diff * very_low_diff);
									  Sens2 = Sens2 + (9 * very_low_diff * very_low_diff);
								 }
							}
							
							// 应用温度补偿
							var1 = var1 - T2;
							Off = Off - Off2;
							Sens = Sens - Sens2;
							
							// 计算最终压力（单位：Pa）
							pressure = (((MS8607.Pressure_D1 * Sens) >> 21) - Off) >> 15;
							
							// 转换为最终单位
							MS8607.temp = var1 / 100.0;        // 转换为°C
  						MS8607.pressure = pressure / 100.0; // 转换为hPa
						  MS8607.humi = 125.0 * (float)MS8607.Tumi_D3 / 65536.0 - 6.0; // 湿度计算
							
							// 湿度范围限制
							if(MS8607.humi < 0) MS8607.humi = 0;
							if(MS8607.humi > 100) MS8607.humi = 100;
							
							MS8607Step=3;
              error=0;	
              break;			
	}
	return 1;
}

