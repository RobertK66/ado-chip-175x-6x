/*
 * cli.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef MOD_CLI_CLI_H_
#define MOD_CLI_CLI_H_

#include <chip.h>

// Module API Prototypes
//void CliInitUart(LPC_USART_T *pUART, LPC175X_6X_IRQn_Type irqType);		//  Choose the UART to be used for CLI
void CliUartIRQHandler(LPC_USART_T *pUART);								//  Wrap the right Interrupt to this method (done in board.c)!
void CliInit();										// Module Init called once prior mainloop
void CliMain();										// Module routine participating each mainloop.
void SetCliUart(LPC_USART_T *pUart);
//This 2 methods are needed by retarget.h to make printf and read??? work. Read not tested yet!
void CliPutChar(char ch);
int  CliGetChar();

void RegisterCommand(char* cmdStr, void (*callback)(int argc, char *argv[]));



#endif /* MOD_CLI_CLI_H_ */
