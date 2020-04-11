/*
 * cli.c
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <ado_uart.h>
#include <ring_buffer.h>

#include "cli.h"
//#include "..\..\globals.h"

// Command Interface
#define C_MAX_CMDSTR_LEN	16
typedef struct cliCommand {
	char	cmdStr[C_MAX_CMDSTR_LEN];
	void (*func)(int argc, char *argv[]);
} cliCommand_t;

#define CLI_PROMPT 				">"
#define CLI_MAX_COMMANDS		100
#define CLI_MAX_PARAMS			16

//
// local module variables
// ----------------------
LPC_USART_T *cliUart;						// pointer to UART used for CLI

// The Rx line buffer - used with polling from mainloop
#define CLI_RXBUFFER_SIZE 128
char cliRxBuffer[CLI_RXBUFFER_SIZE];
int cliRxPtrIdx = 0;

// The tx ringbuffer used for TX with interrupt routine.
RINGBUFF_T  cliTxRingbuffer;



//#define CLI_TXBUFFER_SIZE 1024
bool prtTxInProgress = false;
//int prtBufferRead = 0;
//int prtBufferWrite = 0;
//char prtBuffer[CLI_TXBUFFER_SIZE];

// Command Line parsing and registry
char cmdLine[CLI_RXBUFFER_SIZE+10];
int  cliRegisteredCommands = 0;
cliCommand_t commands[CLI_MAX_COMMANDS];


// module statistics
int ignoredTxChars = 0;
int bufferErrors   = 0;
int linesProcessed = 0;
int cmdsProcessed = 0;

//
// module prototypes
// ----------------------
void processLine();
void CliShowStatistics(int argc, char *argv[]);
void CliUartIRQHandler(LPC_USART_T *pUART);
void CliPutChar(char ch);

//
// Module function implementations
// -------------------------------

// The UART Interrupt handler. We only use 'TX empty' interrupt to get out the next byte from our tx 'ringbuffer'
// This method must be wrapped from the real UART interrupt (there is one for each of the 4 UARTx
// We do this in board.c. There we have to call the CliUartIni and make the correct ISR point to here.
void CliUartIRQHandler(LPC_USART_T *pUART) {
	if (pUART->IER & UART_IER_THREINT) {
		char nextChar;
		if (RingBuffer_Pop(&cliTxRingbuffer, &nextChar) == 1) {
			// We still have bytes to catch up with the written buffer content. Send out next one.
			Chip_UART_SendByte(pUART, nextChar);
		} else {
			// Nothing left to send.  Disable transmit interrupt.
			Chip_UART_IntDisable(pUART, UART_IER_THREINT);
			prtTxInProgress = false;
		}
	}
}

// Sends a character on the UART without wasting to much time.
// This is also used by all redlib io to stdout (printf,....)
// We put the char in our TX ringbuffer and initialize sending if needed.
void CliPutChar(char ch) {
	if (prtTxInProgress) {
		// We just put the char into buffer. Tx is already running and will be re-triggered by tx interrupt.
		if (RingBuffer_Insert(&cliTxRingbuffer, &ch) == 0) {
			// no place left in buffer
			// ... TODO ... log event here !?
			ignoredTxChars++;
		}
	} else {
		if (!RingBuffer_IsEmpty(&cliTxRingbuffer)) {
			// Thats strange. somebody left the buffer 'unsent' -> lets reset
			RingBuffer_Flush(&cliTxRingbuffer);
			// TODO .. log event here
			bufferErrors++;
		}
		// No need to put this char in buffer, as it is sent out to UART immediately.

		// We trigger sending without checking of Line Status here because we trust our own variables and
		// want to avoid clearing the possible Rx Overrun error by reading the status register !?
		// I am not sure the 'non occurence' of the Rx Overrun in my first version (without tx interrupt) was caused by this
		// but its my only explanation i had for what i was seeing then. ( Mainloop was delayed by tx waiting for LineStatus
		// -> output was cut off after 2 lines and skipped to the end of my RX - Teststring containing a lot more characters
		// than 2 64 byte lines. Breakpoint for Rx Overrun was never hit !!???
		prtTxInProgress = true;
		Chip_UART_IntEnable(cliUart, UART_IER_THREINT);
		Chip_UART_SendByte(cliUart, (uint8_t) ch);
	}
}

// Used locally and by redlib for stdio readline... (not tested yet)
int CliGetChar() {
	int32_t stat = Chip_UART_ReadLineStatus(cliUart);
//	if (stat & UART_LSR_OE) {
//		return -2;
//	}
//	if (stat & UART_LSR_RXFE) {
//		return -3;
//	}
	if (stat & UART_LSR_RDR) {
		return (int) Chip_UART_ReadByte(cliUart);
	}
	return -1;
}

// With this function you can register your custom command handler(s)
void RegisterCommand(char* cmdStr, void (*callback)(int argc, char *argv[])) {
	// TODO check if duplicate entry !!!
	if ( cliRegisteredCommands < CLI_MAX_COMMANDS) {
		strncpy(commands[cliRegisteredCommands].cmdStr,cmdStr,C_MAX_CMDSTR_LEN);
		commands[cliRegisteredCommands].func = callback;
		cliRegisteredCommands++;
	} else {
		printf("No Command slot left for registering new command.");
	}
}

// This module init from main module.
void _CliInit(LPC_USART_T *pUart, int baud, char *pTxBuffer, uint16_t txBufferSize) {
	if ((cliUart != 0)) {
		// DeInit uart used until now.
		DeInitUart(cliUart);
		// If we did the tx buffer alloc then free it.
		//...

		// Clear all the registered commands. TODO: is this wise here !?
		for (int i=0; i<CLI_MAX_COMMANDS ; i++) {
			commands[i] = (const struct cliCommand){ 0 };
		}
		cliRegisteredCommands = 0;
	}
	cliUart = pUart;

	// Buffer size must be powerOfTwo! Reduce to next lower fitting value in order to not overuse the allocated buffer.
	while((txBufferSize & (txBufferSize - 1))) {
		txBufferSize--;
	}

	if (pTxBuffer == 0) {
		// No Data area specified. Lets reserve the buffer on the heap.
		pTxBuffer = (char*)malloc(txBufferSize);
	}
	RingBuffer_Init(&cliTxRingbuffer, pTxBuffer, sizeof(char), txBufferSize * sizeof(char));
	InitUart(pUart, baud, CliUartIRQHandler);

	RegisterCommand("cliStat", CliShowStatistics);
	printf(CLI_PROMPT);
}

// This is module main loop entry. Do not use (too much) time here!!!
void CliMain(){
	int ch;

	// The UART has 16 byte Input buffer
	// read all available bytes in this main loop call.
	while ((ch = CliGetChar()) != -1) {
		// make echo
		// CliPutChar((char)(ch));
		if (ch != 0x0a &&
		    ch != 0x0d) {
			cliRxBuffer[cliRxPtrIdx++] = (char)(ch);
		}

		if ((cliRxPtrIdx >= CLI_RXBUFFER_SIZE) ||
			 ch == 0x0a ||
			 ch == 0x0d) 	{
			cliRxBuffer[cliRxPtrIdx] = 0x00;
			processLine();
			cliRxPtrIdx = 0;
			printf(CLI_PROMPT);
		}
	}
}


void processLine() {
	bool processed = false;
	linesProcessed++;
	//printf("\nRe:%s",cliRxBuffer);

	// first lets copy the received line to our command line
	strcpy(cmdLine, cliRxBuffer);

	// Then we split for parameters
	int parCnt = 0;
	char* pars[CLI_MAX_PARAMS];
	for (int i = 0; i < CLI_MAX_PARAMS; i++) {
		pars[i] = NULL;
	}
	for (int i = 0, p=0; cmdLine[i] != 0x00; i++) {
		if ( (cmdLine[i] == ' ') &&
			 (p < CLI_MAX_PARAMS) )	{
			cmdLine[i] = 0x00;
			pars[p++] = (&cmdLine[i]) + 1;
			parCnt++;
		}
	}

	// Check if command can be found in definitions
	for (int cmd = 0; cmd < cliRegisteredCommands; cmd++ ) {
		if (strcmp(commands[cmd].cmdStr, &cmdLine[0]) == 0) {	// Todo: cut whitespaces at begin....
			// Call this command with the found params
			commands[cmd].func(parCnt, pars);
			processed = true;
			cmdsProcessed++;
		}
	}

	if (!processed) {
#ifndef RADIATION_TEST		// No need to inform sbd. - nobody is watching ;-)
		printf("Command '%s' not found. Try one of these:\n",  &cmdLine[0]);
		for (int cmd = 0; cmd < cliRegisteredCommands; cmd++ ) {
			printf("'%s' ", commands[cmd].cmdStr);
		}
		printf("\n");
#endif
	}


}

void CliShowStatistics(int argc, char *argv[]){
	printf("CliShowStatistics called with\n");
	for (int i = 0; i < argc; i++) {
		printf("p-%d %s\n", i, argv[i]);
	}

	printf("\nlinesProcessed: %d\ncmdsProcessed: %d\nignoredTxChars: %d\nbufferErrors: %d\n",
	          linesProcessed, cmdsProcessed,  ignoredTxChars, bufferErrors);
}


//#define WRITEFUNC __sys_write
//#define READFUNC __sys_readc

int __sys_write(int iFileHandle, char *pcBuffer, int iLength)
{
	unsigned int i;
	for (i = 0; i < iLength; i++) {
		CliPutChar(pcBuffer[i]);
	}
	return iLength;
}

/* Called by bottom level of scanf routine within RedLib C library to read
   a character. With the default semihosting stub, this would read the character
   from the debugger console window (which acts as stdin). But this version reads
   the character from the LPC1768/RDB1768 UART. */
int __sys_readc(void)
{
	char c = CliGetChar();
	return (int) c;
}
