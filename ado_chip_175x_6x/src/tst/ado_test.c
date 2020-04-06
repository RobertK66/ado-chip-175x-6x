/*
 * ado_test.c
 *
 *  Created on: 06.04.2020
 *      Author: Robert
 */
#include <stdlib.h>
#include "ado_test.h"

void _testFailed(const char* file, const char* func, int ln, test_result_t* res, char* message) {
	test_failure_t *tf = malloc(sizeof(test_failure_t));
	tf->fileName = file;
	tf->testName = func;
	tf->lineNr = ln;
	tf->message = message;
	tf->nextFailure = res->failures;
	res->failures = tf;

	res->failed++;
	res->run++;
}
