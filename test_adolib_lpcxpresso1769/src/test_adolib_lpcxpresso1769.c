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


void print_failures(test_failure_t *fp);
void main_testCmd(int argc, char *argv[]);

int main(void) {
	// 'special test module'
	TestCliInit(); // This routine will block, if there was an error. No Cli would be available then....
				   // To check on the test result and error message hit Debugger-'Pause' ('Suspend Debug Session') and check result structure....

	// Select UART to be used for command line interface.
	//CliInit1(LPC_UART2);
	// or use SWO Trace mode if your probe supports this. (not avail on LPXXpresso1769 board)
	CliInitSWO();			// This configures SWO ITM Console as CLI in/output

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

void listTests(const test_t *tests, int intend);
const test_t* findTest(const test_t* tests, char *name);

const test_t* findTest(const test_t* tests, char *name) {
 	const test_t* found = 0;
	if (IS_TESTSUITE(tests)) {
		if (strcmp(tests->name, name) == 0) {
			return tests;
		}
		for (int i = 0; i< tests->testCnt; i++) {
			found = findTest(&tests->testBase[i], name);
			if (found != 0) {
				break;
			}
		}
	}
	return found;
}

void listTests(const test_t *tests, int intend) {
	if (IS_TESTSUITE(tests)) {
		for(int i = 0; i<intend; i++) printf(" ");
		printf("%s [%d]\n",tests->name, tests->testCnt);
		intend = intend + 2;
		for(int i = 0; i<tests->testCnt; i++) {
			listTests(&tests->testBase[i], intend);
		}
	}
}

void main_testCmd(int argc, char *argv[]) {
	const test_t *root = &moduleTestSuite;
	const test_t *toRun = &moduleTestSuite;

	if ((argc == 1) && (strcmp(argv[0],"?") == 0) ){
		listTests(root, 0);
		return;
	}
	if (argc > 0) {
		toRun = findTest(root, argv[0]);
		if (toRun == 0) {
			printf("Test '%s' not available.\n", argv[0]);
			printf("Available:\n", argv[0]);
			listTests(root, 0);
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
