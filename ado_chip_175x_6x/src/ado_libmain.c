/*
 * ado_libmain.c
 *
 *  Created on: 12.05.2020
 *      Author: Robert
 */
#include "ado_libmain.h"
#include <chip.h>

#include <string.h>
#include <stdio.h>

// Generate a build date-time string in ROM variable
#define BUILD_MONTH_1 (\
  __DATE__ [2] == 'z' ? '1' \
: __DATE__ [2] == 't' ? '1' \
: __DATE__ [2] == 'v' ? '1' \
: '0')

#define BUILD_MONTH_2 (\
  __DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? '1' : '6') \
: __DATE__ [2] == 'b' ? '2' \
: __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? '3' : '4') \
: __DATE__ [2] == 'y' ? '5' \
: __DATE__ [2] == 'l' ? '7' \
: __DATE__ [2] == 'g' ? '8' \
: __DATE__ [2] == 'p' ? '9' \
: __DATE__ [2] == 't' ? '0' \
: __DATE__ [2] == 'v' ? '1' \
: '2')

const char adoBuild[] =  { __DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], BUILD_MONTH_1, BUILD_MONTH_2 , (__DATE__[4] == ' ' ?  '0' : __DATE__[4]), __DATE__[5],'-' , __TIME__[0], __TIME__[1], __TIME__[3], __TIME__[4], __TIME__[6], __TIME__[7], 0 };


// Check for prcompiler settings and generate Compiler feature string in ROM variable
#ifdef CR_INTEGER_PRINTF
	#define ADO_CC_PRINTF_FLOAT 'f'
#else
	#define ADO_CC_PRINTF_FLOAT 'F'
#endif

#ifdef CR_PRINTF_CHAR
	#define ADO_CC_PRINTF_CHAR 'C'
#else
	#define ADO_CC_PRINTF_CHAR 'c'
#endif

#ifdef DEBUG
	#define ADO_CC_DEBUG 'D'
#else
	#define ADO_CC_DEBUG 'd'
#endif

const char adoCompConf[] =  { ADO_CC_DEBUG, ADO_CC_PRINTF_FLOAT, ADO_CC_PRINTF_CHAR, 0 };

char adoVersion[16];

char* adoGetVersion(void) {
	int used = snprintf(adoVersion, sizeof adoVersion, "%d.%d.%d", ADO_VERSION_MAJOR, ADO_VERSION_MINOR, ADO_VERSION_PATCH);

#ifdef 	ADO_VERSION_RELEASE
	strncpy(&adoVersion[used], "-", 1);
	strncpy(&adoVersion[used+1], ADO_VERSION_RELEASE, sizeof ADO_VERSION_RELEASE);
#endif

	return adoVersion;
}

const char* adoGetBuild(void) {
	return adoBuild;
}

const char* adoGetCompileConf() {
	return adoCompConf;
}

// Weak callback for a "LPCOpen C Project" generated by MCUXpresso.
// The LPC17xx/40xx Base library provides a function which initialized the Chip to use the internal oscillator (IRC)
// Using the ADO Lib you can override this behavior by implementing your own void Chip_SystemInit(void) somewhere else
__attribute__ ((weak)) void Chip_SystemInit(void)
{
	/* Setup Chip clocking */
	Chip_SetupIrcClocking();
}

