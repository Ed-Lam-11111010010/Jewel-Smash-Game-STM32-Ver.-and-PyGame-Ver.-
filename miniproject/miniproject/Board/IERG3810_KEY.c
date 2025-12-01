#include "stm32f10x.h"
#include "IERG3810_KEY.h"

// put your procedure and code here

void IERG3810_KEY_Init(void)
{

    RCC ->APB2ENR |= 1 << 2;	
	RCC ->APB2ENR |= 1 << 3;
	RCC ->APB2ENR |= 1 << 6;

    GPIOB->CRL &= 0xFF0FFFFF;
	GPIOB->CRL |= 0x00300000;

    GPIOE->CRL &= 0xFF0F00FF;
	GPIOE->CRL |= 0x00308800;
	
	GPIOE->ODR &= 0xFFFFFFF0;
	GPIOE->ODR |= 0x0000000C;

    GPIOA->CRL &= 0xFFFFFFF0;
	GPIOA->CRL |= 0x00000008;


}
