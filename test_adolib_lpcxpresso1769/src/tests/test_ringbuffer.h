/*
 * test_ringbuffer.h
 *
 *  Created on: 24.03.2020
 *      Author: Robert
 */

#ifndef TESTS_TEST_RINGBUFFER_H_
#define TESTS_TEST_RINGBUFFER_H_

// Test Functions prototypes
void trb_test_basics(test_result_t* result);
void trb_test_wraparound(test_result_t* result);
void trb_head_tail_wraparound(test_result_t* result);
void trb_performance_char(test_result_t* result);
void trb_performance_ptrs(test_result_t* result);

// Array of test functions.
static const test_t trbTests[] = {
   TEST_CASE(trb_test_basics),
   TEST_CASE(trb_test_wraparound),
   TEST_CASE(trb_head_tail_wraparound),
   TEST_CASE(trb_performance_char),
   TEST_CASE(trb_performance_ptrs)
};

// Module Test Suite
static const test_t trbTestSuite = TEST_SUITE("Ringbuffer", trbTests);

#endif /* TESTS_TEST_RINGBUFFER_H_ */
