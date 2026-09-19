#ifndef _TIMER_H
#define _TIMER_H

#include "main.h"
#include "modbus.h"

extern uint8_t sec_flag;

void TIM3_Int_Init(uint16_t arr, uint16_t psc);

#endif /* _TIMER_H */