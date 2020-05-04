/*
 * test_systemconfigs.h
 *
 *  Created on: 18.04.2020
 *      Author: Robert
 */

#ifndef TESTS_TEST_SYSTEMCONFIGS_H_
#define TESTS_TEST_SYSTEMCONFIGS_H_

#include <Chip.h>
#include <ado_test.h>

// Test Functions prototypes
void sys_testPrintFNotusingHeap(test_result_t* result);

// Array of test functions.
static const test_t sysTests[] = {
   TEST_CASE(sys_testPrintFNotusingHeap)
};

// Module Test Suite
static const test_t sysTestSuite = TEST_SUITE("SysConfig", sysTests);

#endif /* TESTS_TEST_SYSTEMCONFIGS_H_ */
