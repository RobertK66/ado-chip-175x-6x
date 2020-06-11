/*
 * test_sspdma.h
 *
 *  Created on: 11.06.2020
 *      Author: Robert
 */

#ifndef TESTS_TEST_SSPDMA_H_
#define TESTS_TEST_SSPDMA_H_

#include <ado_test.h>

// Test Functions prototypes
void ssp_test1(test_result_t* result);

// Array of test functions.
static const test_t sspTests[] = {
   TEST_CASE(ssp_test1)
};

// Module Test Suite
static const test_t sspTestSuite = TEST_SUITE("SSP", sspTests);

#endif /* TESTS_TEST_SSPDMA_H_ */
