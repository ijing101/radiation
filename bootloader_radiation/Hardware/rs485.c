#include "rs485.h"
uint8_t RS485_RX_BUF[64];  
uint8_t RS485_RX_CNT = 0;   

void RS485_Init(u32 bound)
{
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
 



void RS485_Send_Data(uint8_t *buf, uint8_t len)
{
    uint8_t t;
#ifdef RS485_ENABLE
    RS485_TX_ENABLE;    
#endif        
    for(t=0;t<len;t++)        
    {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);     
        USART_SendData(USART2, buf[t]);
    }     
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);    
    RS485_RX_CNT=0;      
#ifdef RS485_ENABLE
    RS485_RX_ENABLE;
#endif
}


void RS485_Receive_Data(uint8_t *buf, uint8_t *len)
{
    uint8_t rxlen = RS485_RX_CNT;
    uint8_t i = 0;
    *len = 0;                
    // delay_ms(10);        
    if(rxlen == RS485_RX_CNT&&rxlen)
    {
        for(i = 0; i < rxlen; i++)
        {
            buf[i] = RS485_RX_BUF[i];    
        }        
        *len = RS485_RX_CNT;    
        RS485_RX_CNT=0;
    }
}









