#ifndef Hx711_H
#define Hx711_H
#include "main.h"

#define 	Hx711_SCK    GPIO_PIN_7
#define 	Hx711_DOUT   GPIO_PIN_6
#define	 	Hx711_Port	 GPIOA 
#define	 	Hx711_RCU	 RCU_GPIOA

extern unsigned int Param_Adj_Radi[2];
extern unsigned int Radi_ALL[2];
extern unsigned int Param_Radi_1[2];
extern unsigned int Param_Radi_ALL_1[2];
extern unsigned int Param_Adj_Dir[5]; //校准系数

unsigned int Hx711_Data(void);
void Hx711_init(void);
void Read_Hx711(void);

#endif 

