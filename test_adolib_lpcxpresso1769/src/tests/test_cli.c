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

void cli_testInit(test_result_t* res){
	//
	char msg[32];
	LPC_USART_T *pUart = CLI_DEFAULT_UART;

	CliInit();		// should initialize all default values: ....

	// Check set baudrate.
	Chip_UART_EnableDivisorAccess(pUart);
	uint32_t dll = pUart->DLL;
	uint32_t dlm = pUart->DLM;
	Chip_UART_DisableDivisorAccess(pUart);

	if (dll != 13) {
		sprintf(msg, "DLL was %d iso %d", dll, 13);
		testFailed(res, msg);
		return;
	}

	if (dlm != 0) {
		sprintf(msg, "DLM was %d iso %d", dlm, 0);
		testFailed(res, msg);
		return;
	}

}

void cli_testAll(test_result_t* result) {
	cli_testInit(result);
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
