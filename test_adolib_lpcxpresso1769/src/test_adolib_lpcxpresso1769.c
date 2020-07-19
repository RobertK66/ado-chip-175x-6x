/*
 ===============================================================================
 Name        : test_adolib_lpcxpresso1769.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
 ===============================================================================
 */

#include <chip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ado_libmain.h>
#include <ado_time.h>
#include <ado_test.h>
#include <ado_sspdma.h>
#include <stopwatch.h>
#include <mod/cli.h>
#include <ado_crc.h>

#include "system.h"
#include "tests/test_cli.h"

#include "tests/test_ringbuffer.h"
#include "tests/test_systemconfigs.h"
#include "tests/test_crc.h"
#include "tests/test_sspdma.h"

#include "mod/ado_sdcard.h"
#include "mod/ado_eventlogger.h"



// collect all module tests together into one test suite.
static const test_t moduleTests[] = {
   trbTestSuite,
   sysTestSuite,
   tcrcTestSuite,
   sspTestSuite
};
static const test_t moduleTestSuite = TEST_SUITE("main", moduleTests);

// Function prototypes
void print_failures(test_failure_t *fp);
void main_testCmd(int argc, char *argv[]);
void main_showVersionCmd(int argc, char *argv[]);
void main_showSysTimeCmd(int argc, char *argv[]);
void main_setUtcTimeCmd(int argc, char *argv[]);

uint32_t GetResetCntFromRTC(void);
uint32_t GetResetCountFromPersistence(void);
void IncrementResetCount(void);

int main(void) {
    // Start the systemTime running on RIT IRQ. The offset to start from is dependent on code runtime from reset up to here.
    uint32_t resetCount = GetResetCntFromRTC();
    uint32_t startOffsetMs = 10;
    if (resetCount == 0) {
        resetCount = GetResetCountFromPersistence();
        startOffsetMs = 100;     //?? TODO: Measure this value....
    }
    TimeInit(startOffsetMs, resetCount);
    IncrementResetCount();

	// 'special test module'
	TestCliInit(); // This routine will block, if there was an error. No Cli would be available then....
				   // To check on the test result and error message hit Debugger-'Pause' ('Suspend Debug Session') and check result structure....

	// Select UART to be used for command line interface.
	CliInit1(LPC_UART2);
	// or use SWO Trace mode if your probe supports this. (not avail on LPXXpresso1769 board)
	//CliInitSWO();			// This configures SWO ITM Console as CLI in/output

	uint32_t userBaseEventNr = LogInitEventLogger();
	userBaseEventNr++;      // dummy usage to avoid warning....

	ado_event_reset_t event = ado_event_reset_default;
	event.epochNr = resetCount;
	LogUsrEvent(&event);

	StopWatch_Init1(LPC_TIMER0);
	ADO_SSP_Init(ADO_SSP0, 24000000, SSP_CLOCK_MODE3);			// With sys clck 96MHz: Possible steps are: 12MHz, 16Mhz, 24Mhz, 48Mhz (does not work with my external sd card socket)
	                                                            // My SD cards all work with clock mode mode3 or mode0. Mode3 is 10% faster as no SSL de-actiavtion between bytes is done.
	SdcInit(ADO_SSP0);

	// register (test) command(s) ...
	CliRegisterCommand("test", main_testCmd);
	CliRegisterCommand("ver", main_showVersionCmd);
	CliRegisterCommand("time", main_showSysTimeCmd);
	CliRegisterCommand("setTime", main_setUtcTimeCmd);

	// ... and auto start it for running all test.
	main_showVersionCmd(0,0);
	//main_testCmd(0,0);

	while(1) {
		CliMain();
		SdcMain();
	}

}

void print_failures(test_failure_t *fp){
	printf("Error in %s/%s() at %d:\n",fp->fileName, fp->testName, fp->lineNr);
	printf("  %s\n\n", fp->message);
	if (fp->nextFailure != test_ok) {
		print_failures(fp->nextFailure);
	}
}

void listElement(char* name, uint8_t count, uint8_t intend) {
	for(int i = 0; i<intend; i++) printf(" ");
	printf("%s [%d]\n",name, count);
}


unsigned param_table[5];
unsigned result_table[5];
void read_serial_number(char* str)    //read serial via IAP
{
   param_table[0] = 58; //IAP command
   iap_entry(param_table,result_table);

   if(result_table[0] == 0) //return: CODE SUCCESS
   {
      sprintf(str, "%08X-%08X-%08X-%08X",result_table[1],result_table[2],result_table[3],result_table[4]);
   }
   else
   {
       sprintf(str, "Read Error",result_table[1],result_table[2],result_table[3],result_table[4]);
   // printf("Serial Number Read Error\n\r");
   }
}

static char devnr[128];
void main_showVersionCmd(int argc, char *argv[]) {
	printf("Lib Version:  %s\n", adoGetVersion());
	printf("Lib Build:    %s\n",  adoGetBuild());
	printf("Lib CompConf: %s\n",  adoGetCompileConf());
	read_serial_number(devnr);
	printf("LPC1769SerNr: %s\n", devnr);
}

void main_testCmd(int argc, char *argv[]) {
	const test_t *root = &moduleTestSuite;
	const test_t *toRun = &moduleTestSuite;

	if ((argc == 1) && (strcmp(argv[0],"?") == 0) ){
		testListSuites(root, 0, listElement);
		return;
	}
	if (argc > 0) {
		toRun = testFindSuite(root, argv[0]);
		if (toRun == 0) {
			printf("Test '%s' not available.\n", argv[0]);
			printf("Available:\n");
			testListSuites(root, 2, listElement);
			return;
		}
	}
    if ((argc > 1) && IS_TESTSUITE(toRun)) {
    	int idx = atoi(argv[1]);
    	if (idx < toRun->testCnt) {
    		toRun = &toRun->testBase[idx];
    	}
    }

	test_result_t result;
	result.run = 0;
	result.failed = 0;
	result.failures = test_ok;
	blue_on();
	red_off();
	green_off();

	testRunAll(&result, toRun );

	printf("%d/%d Tests ok\n", result.run - result.failed, result.run);
	blue_off();
	if (result.failed > 0) {
		print_failures(result.failures);
		testClearFailures();
		red_on();
		green_off();
	} else {
		red_off();
		green_on();
	}
}


uint32_t GetResetCntFromRTC(void) {
    uint32_t retVal = 0;
    //uint16_t crc = CRC16_XMODEM((const uint8_t *)LPC_RTC->GPREG, 20);
    if ((LPC_RTC->GPREG[0] == 0xA5A5A5A5) &&
        (CRC16_XMODEM((const uint8_t *)LPC_RTC->GPREG, 20) == 0)){
        retVal = LPC_RTC->GPREG[1];
    }
    return retVal;
}

void SetResetCntToRTC(uint32_t resetCount) {
    if ((LPC_RTC->GPREG[0] == 0xA5A5A5A5) &&
        (CRC16_XMODEM((const uint8_t *)LPC_RTC->GPREG, 20) == 0)){
        LPC_RTC->GPREG[1] = resetCount;
    } else {
        /// init the RTC RAM
        LPC_RTC->GPREG[0] = 0xA5A5A5A5;
        LPC_RTC->GPREG[1] = resetCount;
        LPC_RTC->GPREG[2] = 0;
        LPC_RTC->GPREG[3] = 0;
        LPC_RTC->GPREG[4] = 0;
    }
    uint16_t crc = CRC16_XMODEM((const uint8_t *)LPC_RTC->GPREG, 18);
    uint8_t p1 = (uint8_t)(crc>>8);
    uint8_t p2 = (uint8_t)crc;
    LPC_RTC->GPREG[4] =  ( (((uint32_t)(p1)) << 16) | (((uint32_t)(p2)) << 24) );

}

uint32_t GetResetCountFromPersistence(void) {
    return 0;
}

void IncrementResetCount(void) {
    uint32_t resetCnt = GetResetCntFromRTC() + 1;
    SetResetCntToRTC(resetCnt);
}

void main_showSysTimeCmd(int argc, char *argv[]) {
    ado_tim_systemtime_t time;
    TimeGetCurrentSystemTime(&time);

    printf("Epoch #: %d ", time.epochNumber);
    if (time.utcOffset.year == 0) {
            printf("(no UTC sync)\n");
        } else {
            printf("(started %d - ", time.utcOffset.year);
            printf("%d.%05d)\n", (int)time.utcOffset.dayOfYear,(int)((time.utcOffset.dayOfYear - (int)time.utcOffset.dayOfYear) * 100000));
        }
    printf("Epoch ms: %d\n", time.msAfterStart);

    RTC_TIME_T t;
    TimeGetCurrentUtcTime(&t);
    printf("Utc: %02d.%02d.%04d %02d:%02d:%02d\n",
            t.time[RTC_TIMETYPE_DAYOFMONTH],
            t.time[RTC_TIMETYPE_MONTH],
            t.time[RTC_TIMETYPE_YEAR],
            t.time[RTC_TIMETYPE_HOUR],
            t.time[RTC_TIMETYPE_MINUTE],
            t.time[RTC_TIMETYPE_SECOND]);
}

void main_setUtcTimeCmd(int argc, char *argv[]) {
    RTC_TIME_T t;
    t.time[RTC_TIMETYPE_YEAR] = 2020;
    t.time[RTC_TIMETYPE_MONTH] = 7;
    t.time[RTC_TIMETYPE_DAYOFMONTH] = 26;
    t.time[RTC_TIMETYPE_DAYOFYEAR] = 31 + 29 + 31 + 30 + 31 + 30 + 26;      // Needed??
    t.time[RTC_TIMETYPE_DAYOFWEEK] = RTC_DAYOFWEEK_MAX;                     // Needed??
    t.time[RTC_TIMETYPE_HOUR] = 12;
    t.time[RTC_TIMETYPE_MINUTE] = 30;
    t.time[RTC_TIMETYPE_SECOND] = 0;
    if (argc > 0 ) { t.time[RTC_TIMETYPE_YEAR] = atoi(argv[0]); }
    if (argc > 1 ) { t.time[RTC_TIMETYPE_MONTH] = atoi(argv[1]); }
    if (argc > 2 ) { t.time[RTC_TIMETYPE_DAYOFMONTH] = atoi(argv[2]);
                     t.time[RTC_TIMETYPE_DAYOFYEAR] = 0;    // TODO??
                     t.time[RTC_TIMETYPE_DAYOFWEEK] = 0;    // TODO??
                    }
    if (argc > 3 ) { t.time[RTC_TIMETYPE_HOUR] = atoi(argv[3]); }
    if (argc > 4 ) { t.time[RTC_TIMETYPE_MINUTE] = atoi(argv[4]); }
    if (argc > 5 ) { t.time[RTC_TIMETYPE_SECOND] = atoi(argv[5]); }

    TimeSetUtc2(&t);
}
