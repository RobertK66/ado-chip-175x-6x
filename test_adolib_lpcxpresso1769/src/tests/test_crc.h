/*
 * test_crc.h
 *
 *  Created on: 08.06.2020
 *      Author: Robert
 */

#ifndef TESTS_TEST_CRC_H_
#define TESTS_TEST_CRC_H_

#include <ado_test.h>

// Test Functions prototypes
void tcrc_test1(test_result_t* result);
void tcrc_test2(test_result_t* result);

// Array of test functions.
static const test_t tcrcTests[] = {
   TEST_CASE(tcrc_test1),
   TEST_CASE(tcrc_test2)

};

// Module Test Suite
static const test_t tcrcTestSuite = TEST_SUITE("Crc", tcrcTests);

#endif /* TESTS_TEST_CRC_H_ */
