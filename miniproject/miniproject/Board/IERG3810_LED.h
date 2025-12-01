#ifndef __IERG3810_LED_H
#define __IERG3810_LED_H
#include "stm32f10x.h"

// put procedure header here

#define DS0_on  (GPIOB->BRR = 1<<5)
#define DS0_off (GPIOB->BSRR = 1<<5)

#define DS1_on  (GPIOE->BRR = 1<<5)
#define DS1_off (GPIOE->BSRR = 1<<5)


void IERG3810_LED_Init(void);


#endif
