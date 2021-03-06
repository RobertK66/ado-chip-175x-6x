/*
 * test_ringbuffer.c
 *
 *  Created on: 24.03.2020
 *      Author: Robert
 */

#include <chip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ado_test.h>
#include <mod/cli.h>
#include <stopwatch.h>

#include "test_ringbuffer.h"
#include "../system.h"


static const int C_TEST_SIZE = 16;

void trb_test_basics(test_result_t *res) {
	char data[C_TEST_SIZE];
	RINGBUFF_T	testbuffer;
	char test[] = "123456789";

	// Check if after init the size is reported as free.
	RingBuffer_Init(&testbuffer, (void*)data, 1, C_TEST_SIZE);
	if (RingBuffer_GetFree(&testbuffer) != C_TEST_SIZE ) {
		testFailed(res, "GetFree wrong.");
		return;
	}
	int i = 5;
	char input = 'x';
	// Lets push 5 'x'es.
	while(i-- > 0) RingBuffer_Insert(&testbuffer,(void*)&input);
	// Now free should be size - 5
	if (RingBuffer_GetFree(&testbuffer) != C_TEST_SIZE - 5) {
		testFailed(res, "GetFree not x-5.");
		return;
	}
	// Lets push a string -> 8 chars
	RingBuffer_InsertMult(&testbuffer, "ABCDEFGH", 8);
	if (RingBuffer_GetFree(&testbuffer) != C_TEST_SIZE - 5 - 8) {
		testFailed(res, "GetFree not x-13.");
		return;
	}

	// now we read/pop the 1st 3 chars -> all should be 'x'es.
	i = 3;
	while(i-- > 0){
		char p;
		RingBuffer_Pop(&testbuffer,(void*)&p);
		if (p != 'x') {
			testFailed(res, "Wrong chars 1-3.");
			return;
		}
	}

	// Read next 5 chars (into our prepared testString at pos 1). Read content 'xxABC'
	RingBuffer_PopMult(&testbuffer,&test[1],5);
	if (strcmp(test, "1xxABC789") != 0) {
		testFailed(res, "Wrong string popped.");
		return;
	}
	// Read next 5 chars (into our prepared testString at pos 1). Read content 'DEFGH'
	RingBuffer_PopMult(&testbuffer,&test[1],5);
	if (strcmp(test, "1DEFGH789") != 0) {
		testFailed(res, "Wrong string popped.");
		return;
	}
	//Now Buffer should be Empty again.
	if (! RingBuffer_IsEmpty(&testbuffer)) {
		testFailed(res, "Not empty.");
		return;
	}
	testPassed(res);
}

void trb_test_wraparound(test_result_t *res) {
	char data[C_TEST_SIZE];
	RINGBUFF_T	testbuffer;
	char test[] = "0123456789ABCDEF";

	RingBuffer_Init(&testbuffer, (void*)data, 1, C_TEST_SIZE);

	// We prefill with 10 chars
	RingBuffer_InsertMult(&testbuffer, "xXxXxXxXxX", 10);

	// Then we read 8 of them
	RingBuffer_PopMult(&testbuffer,&test[0],8);

	// Now lets write another 12 bytes -> should wrap around 6 - 6
	int copied = RingBuffer_InsertMult(&testbuffer, "abcdefghijkl", 12);

	if (copied != 12) {
		testFailed(res, "Wrong output of inserted char count!");
		return;
	}
	// As whitbox test. lets see if content of our buffer is correct.
	// should be "ghijklxXxXabcdef"
	if (memcmp(data, "ghijklxXxXabcdef", 16) != 0) {
		testFailed(res, "Wrong buffer content.");
		return;
	}

	// Now we try to fill it up to its last byte (2 more should fit in here)
	copied = RingBuffer_InsertMult(&testbuffer, "YY", 2);
	if (copied != 2) {
		testFailed(res, "Wrong char count when filling completely.");
		return;
	}

	if (memcmp(data, "ghijklYYxXabcdef", 16) != 0) {
		testFailed(res, "Wrong buffer content.");
		return;
	}

	// Try to overfill now
	copied = RingBuffer_InsertMult(&testbuffer, "ZZZZ", 4);
	if (copied != 0) {
		testFailed(res, "Accepted more bytes as available !?");
		return;
	}
	// Content should not be changed here!
	if (memcmp(data, "ghijklYYxXabcdef", 16) != 0) {
			testFailed(res, "Wrong buffer content.");
			return;
		}

	testPassed(res);
}

void trb_head_tail_wraparound(test_result_t *res) {
	char data[C_TEST_SIZE];
	RINGBUFF_T	testbuffer;
	char test[] = "0123456789012345";

	// prefill the data
    memset(data,'$', C_TEST_SIZE);

	RingBuffer_Init(&testbuffer, (void*)data, 1, C_TEST_SIZE);

	// White box testing: We assume long life time and head and tail (internal pointers) are
	// counted up almost to its max_val of uint32
	testbuffer.head = UINT32_MAX - 7;
	testbuffer.tail = UINT32_MAX - 7;

	// now we put 10 bytes into buffer
	RingBuffer_InsertMult(&testbuffer, "ABCDEFGHIJ", 10);

	if (testbuffer.head != 2) {
		testFailed(res, "Wrong head wraparound.");
		return;
	}

	if (RingBuffer_GetCount(&testbuffer) != 10){
		testFailed(res, "Wrong entry Count.");
		return;
	}

	if (RingBuffer_GetFree(&testbuffer) != 6){
		testFailed(res, "Wrong free entry Count.");
		return;
	}

	RingBuffer_PopMult(&testbuffer,&test[0],5);
	if (strcmp(test, "ABCDE56789012345") != 0) {
		testFailed(res, "Wrong part1 popped.");
		return;
	}
	RingBuffer_PopMult(&testbuffer,&test[0],5);
	if (strcmp(test, "FGHIJ56789012345") != 0) {
		testFailed(res, "Wrong part2 popped.");
		return;
	}

	testPassed(res);
}

void trb_performance_char(test_result_t *res) {
	char data[C_TEST_SIZE];
	RINGBUFF_T	testbuffer;
	char test[] = "0123456789012345";

	RingBuffer_Init(&testbuffer, (void*)data, 1, C_TEST_SIZE);

	// Disable all irq to make measurements reproducible.
	__asm volatile ("cpsid i");
	uint32_t startTim =  StopWatch_Start();

	for (int i=0;i<1000;i++) {
		// now we put 10 bytes into buffer
		RingBuffer_InsertMult(&testbuffer, "ABCDEFGHIJ", 10);

		// and read it out again
		RingBuffer_PopMult(&testbuffer,&test[0],10);
		//RingBuffer_PopMult(&testbuffer,&test[0],2);
	}

	uint32_t ticks1 = StopWatch_Elapsed(startTim);

	for (int i=0;i<10000;i++) {
		// now we put 10 bytes into buffer
		RingBuffer_Insert(&testbuffer, "A");

		// and read it out again
		RingBuffer_Pop(&testbuffer,&test[0]);
	}

	uint32_t ticks2 = StopWatch_Elapsed(startTim) - ticks1;
	// Enable all irq again.
	__asm volatile ("cpsie i");

	uint32_t time1us = StopWatch_TicksToUs(ticks1);
	uint32_t time2us = StopWatch_TicksToUs(ticks2);

	printf("Push/pop test (char): %d/%d us.\n", time1us, time2us);

	if (time2us > 30300) {
		testFailed(res, "Ringbuffer using char (single pushPop) is to slow!");
		return;
	}
	if (time1us > 8500) {
		testFailed(res, "Ringbuffer using char (multi pushPop) is to slow!");
		return;
	}

	testPassed(res);
}


void trb_performance_ptrs(test_result_t *res) {
	void *data[C_TEST_SIZE];
	RINGBUFF_T	testbuffer;
	void *test[C_TEST_SIZE];

	for (int i=0;i<C_TEST_SIZE;i++) {
		test[i] = (void*)trb_performance_ptrs;
	}

	RingBuffer_Init(&testbuffer, (void*)data, sizeof(void*), C_TEST_SIZE);

	// Disable all irq to make measurements reproducible.
	__asm volatile ("cpsid i");
	uint32_t startTim =  StopWatch_Start();

	for (int i=0;i<1000;i++) {
		// now we put 10 bytes into buffer
		RingBuffer_InsertMult(&testbuffer, &test[0], 10);

		// and read it out again
		RingBuffer_PopMult(&testbuffer,&test[0],8);
	}

	uint32_t ticks1 = StopWatch_Elapsed(startTim);

	for (int i=0;i<10000;i++) {
		// now we put 10 bytes into buffer
		RingBuffer_Insert(&testbuffer, trb_performance_ptrs);

		// and read it out again
		RingBuffer_Pop(&testbuffer,&test[0]);
	}

	uint32_t ticks2 = StopWatch_Elapsed(startTim) - ticks1;
	// Enable all irq again.
	__asm volatile ("cpsie i");

	uint32_t time1us = StopWatch_TicksToUs(ticks1);
	uint32_t time2us = StopWatch_TicksToUs(ticks2);

	printf("Push/pop test (ptr): %d/%d us.\n", time1us, time2us);

	if (time2us > 32900) {
		testFailed(res, "Ringbuffer using ptr (single pushPop) is to slow!");
		return;
	}
	if (time1us > 8900) {
		testFailed(res, "Ringbuffer using ptr (multi pushPop) is to slow!");
		return;
	}

	testPassed(res);
}
