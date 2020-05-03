/*
 * test_ringbuffer.h
 *
 *  Created on: 24.03.2020
 *      Author: Robert
 */

#ifndef TESTS_TEST_RINGBUFFER_H_
#define TESTS_TEST_RINGBUFFER_H_

// Module API
const test_t * TestRbInit(bool autoStart);

// Test Functions prototypes
void trb_test_basics(test_result_t* result);
void trb_test_wraparound(test_result_t* result);
void trb_head_tail_wraparound(test_result_t* result);

// Array of test functions. Cast the function pointers to test suite pointers having one entry and no name
// to fit into oo like test structure. TestSuite --> TestCase
static const test_t trbTests[] = {
   TEST_CASE(trb_test_basics),
   TEST_CASE(trb_test_wraparound),
   TEST_CASE(trb_head_tail_wraparound)
};


static const test_t trbTestSuite = TEST_SUITE("My tests", trbTests );
//static const test_t trbTestSuite = {
//	trbTests,
//	"Ringbuffer Tests",
//	(uint8_t)(sizeof(trbTests) / sizeof(test_t)),
//	trbTests[0].runIt
//};
//	//TEST_SUITE("My Tests", trbTests);
//#define TEST_SUITE(name, tests) {tests, name, (uint8_t)(sizeof(tests) / sizeof(test_t)), test[0].runIt }


#endif /* TESTS_TEST_RINGBUFFER_H_ */
