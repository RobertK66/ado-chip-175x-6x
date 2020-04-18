/*
 * test_systemconfigs.c
 *
 *  Created on: 18.04.2020
 *      Author: Robert
 *
 *
 *  This test module assumes, that there exist different project configurations represented by Project wide
 *  compiler defines.
 *  ( Set in the Properties - C/C++ Build - Settings | Tool Settings - MCU C-Compiler - Preprocessor - Defined Symbols(-D) )
 *
 *  Currently there are coded following Configuration Settings:
 *
 *  DEBUG_TESTCONFIG_1
 *  	Project is linked to 		Redlib nohost(nf)
 *
 *
 *  DEBUG_TESTCONFIG_2
 *  	Project is linked to		Redlib nohost(nf)
 *  	CR_PRINTF_CHAR				defines that printf does not use a temp stringbuilder with malloc
 *  	CR_INTEGER_PRINTF			printf only handles integer variables.
 *
 */
#include "test_systemconfigs.h"


#include <ado_test.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../system.h"
#include "../mod/cli.h"

// references needed to get heap pointers for checks.
register void * stack_ptr asm ("sp");
extern char _pvHeapLimit __attribute__((weak));
extern char _pvHeapStart __attribute__((weak));
extern unsigned int __end_of_heap;

// prototypes
void tsc_testAllCmd(int argc, char *argv[]);
void tsc_testAll(test_result_t* result);

void print_failures2(test_failure_t *fp){
	printf("Error in %s/%s() at %d:\n",fp->fileName, fp->testName, fp->lineNr);
	printf("  %s\n\n", fp->message);
	if (fp->nextFailure != test_ok) {
		print_failures2(fp->nextFailure);
	}
}

void TestScInit(bool autoStart) {
	RegisterCommand("testAllSys", tsc_testAllCmd);
	if (autoStart) {
		tsc_testAllCmd(0,(char**)0);
	}
}

void tsc_testAllCmd(int argc, char *argv[]){
	test_result_t result;
	result.run = 0;
	result.failed = 0;
	result.failures = test_ok;

	blue_on();			// indicate tests running;
	red_off();
	green_off();
	tsc_testAll(&result);
	printf("%d/%d Tests ok\n", result.run - result.failed, result.run);
	blue_off();
	if (result.failed > 0) {
		print_failures2(result.failures);
		testClearFailures();
		red_on();
		green_off();
	} else {
		red_off();
		green_on();
	}
}


void cli_testPrintFNotusingHeap(test_result_t* res){
	uint32_t heapBase = (uint32_t)& _pvHeapStart;
	uint32_t usedheap_before;
	uint32_t usedheap_after;

	if ((uint32_t)__end_of_heap == 0) {
		// If not used at all __end_of_heap contains 0!
		usedheap_before = 0;
	} else {
		usedheap_before = (uint32_t)__end_of_heap - heapBase;
	}


#if DEBUG_TESTCONFIG_1
	// This Configuration uses redlib-nf (no floating point printf) without any additional debug flags (-> printf is going to use heap space)
	float f = 2.2;
	int i = 55;

	printf("A test String writing something to the output UART f:%.6f i:%d\n", f, i);

	usedheap_after = (uint32_t)__end_of_heap - heapBase;

	printf("Heap diff: %d ( H: %d - %d)\n", usedheap_after - usedheap_before, usedheap_before,  usedheap_after );

	if ((usedheap_after == usedheap_before)) {
		testFailed(res,"No Heap used!?");
		return;
	}
	if ((usedheap_after - usedheap_before) != 52) {
		testFailed(res,"Wrong Heap size used!?");
		return;
	}

#elif DEBUG_TESTCONFIG_2
	float f = 2.2;
	int i = 55;

	printf("A test String writing something to the output UART f:%.6f i:%d\n", f, i);

	if ((uint32_t)__end_of_heap == 0) {
			// If not used at all __end_of_heap contains 0!
		usedheap_after = 0;
	} else {
		usedheap_after = (uint32_t)__end_of_heap - heapBase;
	}

	printf("Heap diff: %d\n", usedheap_after - usedheap_before );


	if (usedheap_after != usedheap_before) {
		testFailed(res,"Printf used some Heap space!");
		return;
	}

#endif


	testPassed(res);
}



void tsc_testAll(test_result_t* result) {
	cli_testPrintFNotusingHeap(result);
}




