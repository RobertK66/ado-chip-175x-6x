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
