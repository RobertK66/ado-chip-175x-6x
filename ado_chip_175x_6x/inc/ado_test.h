/*
 * ado_test.h
 *
 *  Created on: 06.04.2020
 *      Author: Robert
 */

#ifndef ADO_TEST_H_
#define ADO_TEST_H_

// structure prototype. (needed because of self reference!)
typedef struct test_failure_s test_failure_t;

// structure definition
typedef struct test_failure_s {
	test_failure_t* nextFailure;	// prototype can be used here!
	const char* 	fileName;
	const char* 	testName;
	int 			lineNr;
	char* 			message;
} test_failure_t;


typedef struct test_result_s {
	int				run;
	int				failed;
	test_failure_t* failures;
} test_result_t;


#if TEST_CFG_MAXFAILURES
	#define TEST_MAX_FAILURES	TEST_CFG_MAXFAILURES
#else
	#define TEST_MAX_FAILURES	10
#endif


// private prototype. Use with defined testFailed(a,b) macro.
void _testFailed(const char* file, const char* func, int ln, test_result_t* res, char* message);
void testClearFailures();

#define testFailed(pRes, message) _testFailed(__FILE__, __func__, __LINE__, pRes, message)
#define testPassed(a)	 a->run++;

#define test_ok ((test_failure_t *)0)



#endif /* ADO_TEST_H_ */
