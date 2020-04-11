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

#include "system.h"

#include "mod/cli.h"
#include "tests/test_cli.h"
#include "tests/test_ringbuffer.h"


int main(void) {
	blue_on();
	red_off();
	green_off();

	TestCliInit();		// This routine will block, if there was an error. No Cli would be available then....
						// To check on the test result and error message hit Debugger-'Pause' ('Suspend Debug Session') and check result structure....

	// Select UART to be used for command line interface and init it.
	CliInit1(LPC_UART2);

	TestRbInit(true);

	while(1) {
		CliMain();
	}

}

