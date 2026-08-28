#ifndef _TIMER_H
#define _TIMER_H

#include "sys.h"
#include "modbus.h"

//我们需要识别的是一个报文
//报头，报尾
//按照包与包之间的时间间隔来来判断没有接收到新的数据，我们认为数据接收完成

extern u8 sec_flag;
extern int time;
extern unsigned short MMsec;

		 				   
void TIM3_Int_Init(u16 arr,u16 psc);
void TIM4_Int_Init(u16 arr,u16 psc);



#endif
