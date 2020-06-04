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
#include <string.h>
#include <stdlib.h>

#include <ado_libmain.h>
#include <ado_test.h>
#include <stopwatch.h>
#include <mod/cli.h>

#include "system.h"
#include "tests/test_cli.h"

#include "tests/test_ringbuffer.h"
#include "tests/test_systemconfigs.h"

#include "mod/ado_sdcard.h"
#include "mod/ado_sspdma.h"


// collect all module tests together into one test suite.
static const test_t moduleTests[] = {
   trbTestSuite,
   sysTestSuite
};
static const test_t moduleTestSuite = TEST_SUITE("main", moduleTests);

// Function prototypes
//void testListSuites(const test_t *tests, int intend);
//const test_t* findTestSuite(const test_t* tests, char *name);
void print_failures(test_failure_t *fp);
void main_testCmd(int argc, char *argv[]);
void main_showVersionCmd(int argc, char *argv[]);


int main(void) {
	// 'special test module'
	TestCliInit(); // This routine will block, if there was an error. No Cli would be available then....
				   // To check on the test result and error message hit Debugger-'Pause' ('Suspend Debug Session') and check result structure....

	// Select UART to be used for command line interface.
	CliInit1(LPC_UART2);
	// or use SWO Trace mode if your probe supports this. (not avail on LPXXpresso1769 board)
	//CliInitSWO();			// This configures SWO ITM Console as CLI in/output

	StopWatch_Init1(LPC_TIMER0);
	ADO_SSP_Init(ADO_SSP0, 24000000);			// With sys clck 96MHz: Possible steps are: 12MHz, 16Mhz, 24Mhz, 48Mhz (does not work with my external sd card socket)
	SdcInit(ADO_SSP0);

	// register test command ...
	CliRegisterCommand("test", main_testCmd);
	CliRegisterCommand("ver", main_showVersionCmd);

	// ... and auto start it for running all test.
	main_showVersionCmd(0,0);
	main_testCmd(0,0);

	while(1) {
		CliMain();
		SdcMain();
	}

}

void print_failures(test_failure_t *fp){
	printf("Error in %s/%s() at %d:\n",fp->fileName, fp->testName, fp->lineNr);
	printf("  %s\n\n", fp->message);
	if (fp->nextFailure != test_ok) {
		print_failures(fp->nextFailure);
	}
}

void listElement(char* name, uint8_t count, uint8_t intend) {
	for(int i = 0; i<intend; i++) printf(" ");
	printf("%s [%d]\n",name, count);
}

void main_showVersionCmd(int argc, char *argv[]) {
	printf("Lib Version:  %s\n", adoGetVersion());
	printf("Lib Build:    %s\n",  adoGetBuild());
	printf("Lib CompConf: %s\n",  adoGetCompileConf());
}

void main_testCmd(int argc, char *argv[]) {
	const test_t *root = &moduleTestSuite;
	const test_t *toRun = &moduleTestSuite;

	if ((argc == 1) && (strcmp(argv[0],"?") == 0) ){
		testListSuites(root, 0, listElement);
		return;
	}
	if (argc > 0) {
		toRun = testFindSuite(root, argv[0]);
		if (toRun == 0) {
			printf("Test '%s' not available.\n", argv[0]);
			printf("Available:\n");
			testListSuites(root, 2, listElement);
			return;
		}
	}
    if ((argc > 1) && IS_TESTSUITE(toRun)) {
    	int idx = atoi(argv[1]);
    	if (idx < toRun->testCnt) {
    		toRun = &toRun->testBase[idx];
    	}
    }

	test_result_t result;
	result.run = 0;
	result.failed = 0;
	result.failures = test_ok;
	blue_on();
	red_off();
	green_off();

	testRunAll(&result, toRun );

	printf("%d/%d Tests ok\n", result.run - result.failed, result.run);
	blue_off();
	if (result.failed > 0) {
		print_failures(result.failures);
		testClearFailures();
		red_on();
		green_off();
	} else {
		red_off();
		green_on();
	}
}
