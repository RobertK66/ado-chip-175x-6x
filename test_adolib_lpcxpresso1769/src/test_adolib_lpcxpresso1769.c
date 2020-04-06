/*
===============================================================================
 Name        : test_adolib_lpcxpresso1769.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#include <chip.h>
#include <stdio.h>

#include "mod/cli.h"
#include "tests/test_ringbuffer.h"

int main(void) {
	// Select UART to be used for command line interface and init it.
	CliInit(LPC_UART2);
	TestRbInit(true);

	while(1) {
		CliMain();
	}

}

