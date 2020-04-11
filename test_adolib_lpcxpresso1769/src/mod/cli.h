/*
 * cli.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef MOD_CLI_CLI_H_
#define MOD_CLI_CLI_H_

#include <chip.h>

// Defaults used by CliInit() macro;
// Do not change the default values here. Use the 'overloaded' CliInitN() macros.
#define CLI_DEFAULT_UART			LPC_UART0
#define CLI_DEFAULT_BAUD			115200
#define CLI_DEFAULT_TXBUFFER_SIZE	((uint8_t)256)					// default buffer is allocated on heap by init()!!!!

#define CliInit() 				_CliInit(CLI_DEFAULT_UART, CLI_DEFAULT_BAUD, 0, CLI_DEFAULT_TXBUFFER_SIZE)
#define CliInit1(a) 			_CliInit(a, CLI_DEFAULT_BAUD, 0, CLI_DEFAULT_TXBUFFER_SIZE )
#define CliInit2(a,b) 			_CliInit(a, b,  0, CLI_DEFAULT_TXBUFFER_SIZE)
#define CliInit4(a,b, c, d) 	_CliInit(a, b,  c, d)


// Module API Prototypes
void _CliInit(LPC_USART_T *pUart, int baud, char *pTxBuffer, uint8_t txBufferSize);	// module init to be called once prior mainloop. (Use macros for calling with various par lists.)
void CliMain(void);								// module routine should participate in regular mainloop calls.

void RegisterCommand(char* cmdStr, void (*callback)(int argc, char *argv[]));	// Assign Callback for your custom commands.

#endif /* MOD_CLI_CLI_H_ */
