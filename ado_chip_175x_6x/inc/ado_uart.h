/*
 * ado_uart.h
 *
 *  Created on: 06.04.2020
 *      Author: Robert
 */

#ifndef ADO_UART_H_
#define ADO_UART_H_

#include <chip.h>

void InitUart(LPC_USART_T *pUart, int baud, void(*irqHandler)(LPC_USART_T *pUART));

#endif /* ADO_UART_H_ */