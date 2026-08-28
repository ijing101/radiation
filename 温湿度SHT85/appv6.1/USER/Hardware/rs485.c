#include "sys.h"
#include "rs485.h"	  
#include "delay.h"
#include "timer.h"
#include "modbus.h"

//数据发送
u8 USART_Send_Data(u8 *buf,u16 len,USART_TypeDef* USARTx)
{
   u16 t;
	for(t=0;t<len;t++)
	{																		//循环发送数据
		while(USART_GetFlagStatus(USARTx,USART_FLAG_TC)==RESET);			//等待发送结束
		USART_SendData(USARTx,buf[t]);
	}	 
	while(USART_GetFlagStatus(USARTx,USART_FLAG_TC)==RESET);
	return 0;
}

//调用数据发送函数，使能485芯片，实现发送。
void Usart2_Printf(u8 *buf,u16 len)
{
	 RS485_TX_ENABLE;     										//使能发送
	 if(!USART_Send_Data(buf,len,USART2))   					//发送完成转变成接受模式
	 {  
		RS485_RX_ENABLE;                       //失能
	 }
}


// 发送一个字节的函数
void Usart2_Send_Byte(uint8_t data) 
{
    RS485_TX_ENABLE; // 使能RS485发送
	// 等待发送缓冲区为空，即等待上一个数据传输完成
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2,data); //  发送字节
	
	  while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
 
    RS485_RX_ENABLE; // 发送完成后切换回接收模式
}


// 发送字符串（文字）的函数
void Usart2_SendString(char *str) 
{
    u16 length = strlen(str); // 计算字符串长度
    RS485_TX_ENABLE; // 使能RS485发送
    USART_Send_Data((u8*)str, length, USART2); // 发送字符串
    RS485_RX_ENABLE; // 发送完成后切换回接收模式
}
 


//modbus串口发送一个字节数据
void Modbus_Send_Byte(u8 Modbus_byte)
{
  USART_SendData(USART2,Modbus_byte);
	while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
	USART_ClearFlag(USART2, USART_FLAG_TC); 
}



//485串口初始化
//初始化IO 串口2 
//bound:波特率
void Modbus_uart2_init(u32 bound){
    //GPIO端口设置
		GPIO_InitTypeDef   GPIO_InitStructure;//GPIO结构体指针
		USART_InitTypeDef  USART_InitStructure;//串口结构体指针
		NVIC_InitTypeDef NVIC_InitStructure;//中断分组结构体指针
		//1、使能串口时钟，串口引脚时钟 串口2挂载到APB1上
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);	//使能USART2时钟
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB,ENABLE);//使能串口时钟和收发使能时钟

		//485收发控制引脚PB2
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;				 //PB2端口配置
 	  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
 	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
   	GPIO_Init(GPIOB, &GPIO_InitStructure);
		
	  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;	//PA2
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽
    GPIO_Init(GPIOA, &GPIO_InitStructure);
   
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;//PA3
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);  


		RCC_APB1PeriphResetCmd(RCC_APB1Periph_USART2,ENABLE);//复位串口2
  	RCC_APB1PeriphResetCmd(RCC_APB1Periph_USART2,DISABLE);//停止复位


 

   //4、USART 初始化设置
		USART_InitStructure.USART_BaudRate = bound;//一般设置为9600;
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
		USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
		USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
		USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
		USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式

    USART_Init(USART2, &USART_InitStructure); //初始化串口
 
   //5、Usart1 NVIC 配置
		NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2 ;//抢占优先级3
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//子优先级3
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
		NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
   
	  //6、开启接收数据中断
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);//开启中断
		//7、使能串口
    USART_Cmd(USART2, ENABLE);                    //使能串口 
		RS485_RX_ENABLE;//使能接收引脚（常态下处于接收状态）
}

//串口重配置函数 - 重新配置波特率和校验位
void Modbus_uart2_reconfig(u32 bound)
{
    USART_InitTypeDef  USART_InitStructure;//串口结构体变量

    // 禁用串口
    USART_Cmd(USART2, DISABLE);

    // 重新配置串口参数
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;

    // 根据校验位参数设置校验位
//    switch(parity) {
//        case 0:  // 无校验
//            USART_InitStructure.USART_Parity = USART_Parity_No;
//            break;
//        case 1:  // 偶校验
//            USART_InitStructure.USART_Parity = USART_Parity_Even;
//            break;
//        case 2:  // 奇校验
//            USART_InitStructure.USART_Parity = USART_Parity_Odd;
//            break;
//        default: // 默认无校验
//            USART_InitStructure.USART_Parity = USART_Parity_No;
//            break;
//    }
	USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    // 重新初始化串口
    USART_Init(USART2, &USART_InitStructure);

    // 重新使能串口
    USART_Cmd(USART2, ENABLE);

    // 设置为接收模式
    RS485_RX_ENABLE;
}

//modbus串口中断服务程序
void USART2_IRQHandler(void)                
{
  u8 st,Res;
	st = USART_GetITStatus(USART2, USART_IT_RXNE);
	if(st == SET)//接收中断
	{
		Res =USART_ReceiveData(USART2);	//读取接收到的数据
	//USART_SendData(USART1, Res);//接受到数据之后返回给串口1
	 if( modbus.reflag==1)  //有数据包正在处理
	  {
		   return ;
		}
		
		// 地址过滤：第一个字节是地址，立即判断
		if(modbus.recount == 0)  // 这是地址字节
		{
			 // 只接受本机地址或广播地址(0x00)的数据包
			 if(Res != modbus.myadd_cached && Res != 0x00)
			 {
				  // 地址不匹配，丢弃此帧，等待下一帧
				  // 注意：这里会过滤掉其他设备的响应帧
				  return;
			 }
			 // 【调试】记录接收到的地址
			 // if(Res == modbus.myadd_cached) { LED_toggle(); } // 可选：LED指示
		}
		
		// 缓冲区溢出保护
		if(modbus.recount >= 100)
		{
			 modbus.recount = 0;
			 modbus.timrun = 0;
			 return;
		}
		
	  modbus.rcbuf[modbus.recount++] = Res;
		modbus.timout = 0;
		if(modbus.recount == 1)  //已经收到了第二个字符数据
		{
		  modbus.timrun = 1;  //开启modbus定时器计时
		}
	}	
} 


