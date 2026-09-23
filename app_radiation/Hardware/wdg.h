#ifndef __WDG_H
#define __WDG_H

#include "main.h"

void IWDG_Init(uint8_t prer, uint16_t rlr);
void IWDG_Feed(void);

#endif /* __WDG_H */



