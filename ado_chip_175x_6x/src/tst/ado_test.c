/*
 * ado_test.c
 *
 *  Created on: 06.04.2020
 *      Author: Robert
 */
#include "ado_test.h"

static int				TestUsedFailures	=	0;
static test_failure_t	TestFailures[TEST_MAX_FAILURES];

void _testFailed(const char* file, const char* func, int ln, test_result_t* res, char* message) {
	if (TestUsedFailures < TEST_MAX_FAILURES) {
		test_failure_t *tf = &TestFailures[TestUsedFailures++];
		tf->fileName = file;
		tf->testName = func;
		tf->lineNr = ln;
		if (TestUsedFailures == TEST_MAX_FAILURES) {
			tf->message = "Failure Buffer filled! Increase TEST_CFG_MAXFAILURES.";
		} else {
			tf->message = message;
		}
		tf->nextFailure = res->failures;
		res->failures = tf;
	}
	res->failed++;
	res->run++;
}

void testClearFailures(void) {
	TestUsedFailures = 0;
}

void testRunAll(test_result_t* result, const test_t *test) {
	if (test->testCnt == 1) {
		// This is a single test function not a test suite, cast the call pointer to its original function pointer.
		void (*callit)(test_result_t* result) = (void (*)(test_result_t* result))(test->runIt);
		callit(result);
		return;
	}
	// This is a Test Suite. test points to an array of test_t structures
	int i = 0;
	test_t *looptest = (test_t *)&test->testBase[0];
	while (i < test->testCnt) {
		// in case this is again a test Suite we have reenter this runner method.
		testRunAll(result, looptest);
		//test->runIt(result, test, test->testCnt);
		i++;
		looptest++;
	}
}
