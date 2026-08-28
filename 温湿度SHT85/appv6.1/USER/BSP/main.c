#include "stm32f10x.h"
#include "usart.h"
#include "rs485.h"
#include "timer.h"
#include "modbus.h"
#include "eeprom.h"
#include "bsp_clkconfig.h"
#include "wdg.h"
#include "iap_trigger.h"
#include "sht85.h"
#include "stdio.h"

int main(void)
 {
	SCB->VTOR = FLASH_BASE | 0x5000;
	__enable_irq(); 

	HSE_SetSysClock(RCC_PLLMul_4);  //璁剧疆绯荤粺鏃堕挓涓猴細8MHZ * 4 = 32MHZ
	delay_init();//寤舵椂鍒濆鍖?
	SHT85_IIC_Config();//SHT85初始化
	uart_init(115200);//usart1涓插彛鍒濆鍖?
	
	Flash_Storage_Init();
	IWDG_Init(6,2344); //婧㈠嚭鏃堕棿15s
	delay_ms(500);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//涓柇鍒嗙粍

	Modbus_uart2_init(9600);//485鍒濆鍖栵紙涓存椂浣跨敤9600锛?
	TIM3_Int_Init(1000-1,32-1);//modbus瀹氭椂
	TIM4_Int_Init(1000-1,32-1);//MMsec 1ms鍩哄噯
	Usart2_SendString("app V6.1\r\n");
	delay_ms(100);
	
	Modbus_Init();//鍒濆鍖栦粠鏈哄湴鍧€
	
	while(1)
	{
		IWDG_Feed();
		//
		SHT85_ReadData();
		// 銆愪慨澶嶃€戝鐞哅odbus浜嬩欢鍓嶅啀娆″杺鐙?
		IWDG_Feed();
		Modbus_Event();
		delay_ms(50);
	}
 }

 

