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

#define WRITEFUNC __sys_write
#define READFUNC __sys_readc

int WRITEFUNC(int iFileHandle, char *pcBuffer, int iLength)
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
int READFUNC(void)
{
	char c = CliGetChar();
	return (int) c;
}
