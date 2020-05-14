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
#include <ado_libmain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../system.h"
#include <mod/cli.h>

// references needed to get heap pointers for checks.
register void * stack_ptr asm ("sp");
extern char _pvHeapLimit __attribute__((weak));
extern char _pvHeapStart __attribute__((weak));
extern unsigned int __end_of_heap;

void sys_testusedLibraryConfig(test_result_t* res){
	const char *libCC = adoGetCompileConf();
	const char *expected;
#if DEBUG_TESTCONFIG_1
	// DEBUG - floating point printf - printf with buffer (malloc)
	expected = "DFc";
#elif DEBUG_TESTCONFIG_2
	// DEBUG - no floating point printf - printf char wise (without malloc)
	expected = "DfC";
#else
	testFailed(res,"No valid DEBUG_TESTCONFIG!");
	return;
#endif

	if (strcmp(libCC, expected) != 0) {
		testFailed(res,"Lib not compiled with correct config!");
		return;
	}
	testPassed(res);
}


void sys_testPrintFNotusingHeap(test_result_t* res){
	uint32_t heapBase = (uint32_t)& _pvHeapStart;
	uint32_t usedheap_before;
	uint32_t usedheap_after;

	// If not used at all __end_of_heap contains 0!
	if ((uint32_t)__end_of_heap == 0) {
		usedheap_before = 0;
	} else {
		usedheap_before = (uint32_t)__end_of_heap - heapBase;
	}

	float f = 2.2;
	int i = 55;
	printf("A test String writing something to the output cons f:%.6f i:%d\n", f, i);

	// If not used at all __end_of_heap contains 0!
	if ((uint32_t)__end_of_heap == 0) {
		usedheap_after = 0;
	} else {
		usedheap_after = (uint32_t)__end_of_heap - heapBase;
	}

	printf("Heap diff: %d ( H: %d - %d)\n", usedheap_after - usedheap_before, usedheap_before,  usedheap_after );

#if DEBUG_TESTCONFIG_1
	// This Configuration uses redlib-nf (no floating point printf) without any additional debug flags (-> printf is going to use some heap space)
	if ((usedheap_after == usedheap_before)) {
		testFailed(res,"No Heap used!?");
		return;
	}
	if ((usedheap_after - usedheap_before) < 36) {
		testFailed(res,"Too less Heap used!?");
		return;
	}

#elif DEBUG_TESTCONFIG_2
	// This Configuration uses

	if (usedheap_after != usedheap_before) {
		testFailed(res,"printf used some Heap space!");
		return;
	}

	if (usedheap_before != 0) {
		testFailed(res,"Somebody used some Heap space!");
		return;
	}

#endif


	testPassed(res);
}




