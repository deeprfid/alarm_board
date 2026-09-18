#ifndef _HWPORT_H_
#define _HWPORT_H_
#include "type.h"
#include "hc32_ll.h"
#include "uart.h"

void RCC_Configuration(void);
void GPIO_Configuration(void);
void time_ini(void);
void USART3_IT_DISABLE(void);
void USART3_IT_ENABLE(void);

#endif








