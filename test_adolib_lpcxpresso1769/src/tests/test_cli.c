/*
 * test_cli.c
 *
 *  Created on: 11.04.2020
 *      Author: Robert
 */
#include <stdio.h>

#include <Chip.h>

#include "test_cli.h"
#include "../mod/cli.h"
#include "../system.h"

// reference to the module variable of cli module.
extern RINGBUFF_T cliTxRingbuffer;

// references needed to get heap pointers for checks.
register void * stack_ptr asm ("sp");
extern char _pvHeapLimit __attribute__((weak));
extern char _pvHeapStart __attribute__((weak));
extern unsigned int __end_of_heap;


void cli_testInitDefault(test_result_t* res){
	//
	char msg[32];
	LPC_USART_T *pUart = CLI_DEFAULT_UART;

	CliInit();		// should initialize all default values: CLI_DEFAULT_UART, 115200 baud,

	// Check the baudrate.
	Chip_UART_EnableDivisorAccess(pUart);
	uint32_t dll = pUart->DLL;
	uint32_t dlm = pUart->DLM;
	Chip_UART_DisableDivisorAccess(pUart);

	if (dll != 13) {
		sprintf(msg, "DLL was %d iso %d", dll, 13);		// This is the value expected with XTAL 12 Mhz on LPCXpresso board and baud=115200
		testFailed(res, msg);
		return;
	}

	if (dlm != 0) {
		sprintf(msg, "DLM was %d iso %d", dlm, 0);		// This is the value expected with XTAL 12 Mhz on LPCXpresso board and baud=115200
		testFailed(res, msg);
		return;
	}

	// Check the Tx buffer (size)
	if (cliTxRingbuffer.count != 128) {
		sprintf(msg, "Tx buffer size was %d iso %d", cliTxRingbuffer.count, 128);
		testFailed(res, msg);
		return;
	}

	// Data pointer should point to heap.
	void *start_of_heap = (void* )& _pvHeapStart;
	void *end_of_heap = (void* )__end_of_heap;
	if (!((cliTxRingbuffer.data < end_of_heap) & (cliTxRingbuffer.data > start_of_heap))) {
		testFailed(res, "Data pointer not pointing to heap.");
		return;
	}

	testPassed(res);
}


void cli_testInit2(test_result_t* res){
	//
	char myTxBuffer[20];
	char msg[32];


	LPC_USART_T *pUart = LPC_UART2;

	CliInit4(pUart, 9600, myTxBuffer, 20);				// should initialize the asked UART to 9600 and use lokal Txbuffer variable.

	// Check the baudrate.
	Chip_UART_EnableDivisorAccess(pUart);
	uint32_t dll = pUart->DLL;
	uint32_t dlm = pUart->DLM;
	Chip_UART_DisableDivisorAccess(pUart);

	if (dll != 156) {
		sprintf(msg, "DLL was %d iso %d", dll, 156);		// This is the value expected with XTAL 12 Mhz on LPCXpresso board and baud=9600
		testFailed(res, msg);
		return;
	}

	if (dlm != 0) {
		sprintf(msg, "DLM was %d iso %d", dlm, 0);		// This is the value expected with XTAL 12 Mhz on LPCXpresso board and baud=9600
		testFailed(res, msg);
		return;
	}

	// Check the Tx buffer (size). 20 is not allowed and cut down to next powOfTwo -> 16.
	if (cliTxRingbuffer.count != 16) {
		sprintf(msg, "Tx buffer size was %d iso %d", cliTxRingbuffer.count, 16);
		testFailed(res, msg);
		return;
	}

	// Data pointer should NOT point to heap.
	void *start_of_heap = (void* )& _pvHeapStart;
	void *end_of_heap = (void* )__end_of_heap;
	if (((cliTxRingbuffer.data < end_of_heap) & (cliTxRingbuffer.data > start_of_heap))) {
		testFailed(res, "Data pointer pointing to heap!");
		return;
	}

	testPassed(res);
}



void cli_testAll(test_result_t* result) {
	cli_testInitDefault(result);
	cli_testInit2(result);
}

test_result_t TestCliInit() {
	test_result_t result;
	result.run = 0;
	result.failed = 0;
	result.failures = test_ok;

	cli_testAll(&result);

	if (result.failed != 0) {
		// indicate failure without using CLI!!!
		red_on();
		blue_off();
		green_off();
		while(true);	// Do not proceed with normal execution.....
	}
	return result;
}
