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

#include <ado_test.h>

#include "system.h"

#include "mod/cli.h"
#include "tests/test_cli.h"
#include "tests/test_ringbuffer.h"
#include "tests/test_systemconfigs.h"

// collect all module tests to one test suite.
static const test_t moduleTests[] = {
   trbTestSuite,
   sysTestSuite
};

static const test_t moduleTestSuite = TEST_SUITE("main", moduleTests);
//{	moduleTests,
//	"main",
//	(uint8_t)(sizeof(moduleTests) / sizeof(test_t)),
//	testRunAll };


void print_failures(test_failure_t *fp);
void main_testCmd(int argc, char *argv[]);

int main(void) {
	// 'special test module'
	TestCliInit(); // This routine will block, if there was an error. No Cli would be available then....
				   // To check on the test result and error message hit Debugger-'Pause' ('Suspend Debug Session') and check result structure....

	// Select UART to be used for command line interface.
	CliInit1(LPC_UART2);
	// or use SWO Trace mode if your probe supports this. (not avail on LPXXpresso1769 board)
	//CliInitSWO();			// This configures SWO ITM Console as CLI in/output

	// register test command ...
	CliRegisterCommand("test", main_testCmd);
	// ... and autostart it for running all test.
	main_testCmd(0,0);

	while(1) {
		CliMain();
	}

}

void print_failures(test_failure_t *fp){
	printf("Error in %s/%s() at %d:\n",fp->fileName, fp->testName, fp->lineNr);
	printf("  %s\n\n", fp->message);
	if (fp->nextFailure != test_ok) {
		print_failures(fp->nextFailure);
	}
}

void main_testCmd(int argc, char *argv[]) {
	test_result_t result;
	result.run = 0;
	result.failed = 0;
	result.failures = test_ok;
	blue_on();
	red_off();
	green_off();


	testRunAll(&result, &moduleTestSuite );

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
