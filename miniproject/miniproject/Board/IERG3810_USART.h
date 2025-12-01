#ifndef __IERG3810_USART_H
#define __IERG3810_USART_H
#include "stm32f10x.h"

// put procedure header here
void IERG3810_usart1_init(u32 pclk2, u32 baud);
void IERG3810_usart2_init(u32 pclkl, u32 baud);


#endif
