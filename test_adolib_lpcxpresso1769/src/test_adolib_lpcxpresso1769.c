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
#include <math.h>

#include <ado_libmain.h>
//#include <ado_time.h>
#include <ado_test.h>
#include <ado_sspdma.h>
#include <ado_spi.h>
#include <stopwatch.h>
#include <mod/cli.h>
#include <ado_crc.h>

#include "system.h"
#include "tests/test_cli.h"

#include "tests/test_ringbuffer.h"
#include "tests/test_systemconfigs.h"
#include "tests/test_crc.h"
#include "tests/test_sspdma.h"

#include <mod/ado_sdcard.h>
#include "mod/ado_sdcard_cli.h"
#include "mod/ado_eventlogger.h"
#include "mod/ado_mram.h"
#include "mod/ado_mram_cli.h"
#include "ado_adc.h"


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

#ifdef BOARD_CLIMB_EM2
// SSP0 Chips
    #define MRAM0_CS_PORT   0
    #define MRAM0_CS_PIN   22       // EM2: "SSP0-MRAM-CS1"
    #define MRAM1_CS_PORT   2
    #define MRAM1_CS_PIN   11       // EM2: "SSP0-MRAM-CS2"
    #define MRAM2_CS_PORT   2
    #define MRAM2_CS_PIN   12       // EM2: "SSP0-MRAM-CS3"

// SSP1 Chips
    #define MRAM3_CS_PORT   2
    #define MRAM3_CS_PIN    2       // EM2: "SSP1-MRAM-CS1"
    #define MRAM4_CS_PORT   0
    #define MRAM4_CS_PIN    4       // EM2: "SSP1-MRAM-CS2"
    #define MRAM5_CS_PORT   1
    #define MRAM5_CS_PIN   10       // EM2: "SSP1-MRAM-CS3"

#else
    // SSP0 Chips
    #define MRAM0_CS_PORT   0
    #define MRAM0_CS_PIN   26
    #define MRAM1_CS_PORT   1
    #define MRAM1_CS_PIN   30
    #define MRAM2_CS_PORT   1
    #define MRAM2_CS_PIN   31

    // SSP1 Chips
    #define MRAM3_CS_PORT   0
    #define MRAM3_CS_PIN   23
    #define MRAM4_CS_PORT   0
    #define MRAM4_CS_PIN   24
    #define MRAM5_CS_PORT   0
    #define MRAM5_CS_PIN   25
#endif

void CsMram0(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, MRAM0_CS_PORT, MRAM0_CS_PIN, !select);

}

void CsMram1(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, MRAM1_CS_PORT, MRAM1_CS_PIN, !select);

}

void CsMram2(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, MRAM2_CS_PORT, MRAM2_CS_PIN, !select);

}

void CsMram3(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, MRAM3_CS_PORT, MRAM3_CS_PIN, !select);

}

void CsMram4(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, MRAM4_CS_PORT, MRAM4_CS_PIN, !select);

}

void CsMram5(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, MRAM5_CS_PORT, MRAM5_CS_PIN, !select);

}


void CsSdCard0(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, 0, 16, !select);
}

#ifndef BOARD_CLIMB_EM2
void CsSdCard1(bool select) {
    Chip_GPIO_SetPinState(LPC_GPIO, 0, 6, !select);
}
#endif

void ledOn()
{
	Chip_GPIO_SetPinToggle(LPC_GPIO, 2, 6);
}


//{ 0,23 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Main supply current (Analog Input 0)  */
    //{ 0,24 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Sidpanel supply current (Analog Input 1)  */
    //{ 0,25 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Temperature (Analog Input 2)  */
    //{ 0,26 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Supply Voltage (Analog Input 3)  */


float convertCurrent_c0(uint16_t value) {
    return ((value * (ADC_VREF / 4095) - 0.01) / 100 / 0.075);
}

float convertCurrentSp_c1(uint16_t value) {
    return ((value * (ADC_VREF / 4095) - 0.01) / 100 / 0.075 - 0.0003);
}

float convertTemperature_c2(uint16_t value) {
    float temp = value * (4.9 / 3.9 * ADC_VREF / 4095);
    return (-1481.96 + sqrt(2196200 + (1.8639 - temp) * 257730));
}

float convertSupplyVoltage_c3(uint16_t value) {
    return (value * (2.0 * ADC_VREF / 4095));
}

static const adc_channel_t AdcChannels [] = {
   { convertCurrent_c0 },
   { convertCurrentSp_c1 },
   { convertTemperature_c2 },
   { convertSupplyVoltage_c3 }
};
static const adc_channel_array_t Channels = {
    (sizeof(AdcChannels)/sizeof(adc_channel_t)), AdcChannels
};

void read_transmit_sensors() {
    float cur = AdcReadChannelResult(0);
    float cur_sp = AdcReadChannelResult(1);
    printf("Currents1: %.2f mA %.2f mA\n", 1000 * cur, 1000 * cur_sp);

    float temp = AdcReadChannelResult(2);
    printf("AN-Temp3: %.3f °C\n", temp);

    float volt = AdcReadChannelResult(3);
    printf("Supply: %.3f V\n", volt);
    return;
}


static const sdcard_init_t SdCards[] = {
        {ADO_SBUS_SPI, CsSdCard0},
};

static sdcard_init_array_t Cards = {
    (sizeof(SdCards)/sizeof(sdcard_init_t)), SdCards
};




int main(void) {
    // Start the systemTime running on RIT IRQ. The offset to start from is dependent on code runtime from reset up to here.
    uint32_t resetCount = GetResetCntFromRTC();
    uint32_t startOffsetMs = 10;
    if (resetCount == 0) {
        resetCount = GetResetCountFromPersistence();
        startOffsetMs = 100;     //?? TODO: Measure this value....
    }
    //TimeInit(startOffsetMs, resetCount);
    IncrementResetCount();

	// 'special test module'
	TestCliInit(); // This routine will block, if there was an error. No Cli would be available then....
				   // To check on the test result and error message hit Debugger-'Pause' ('Suspend Debug Session') and check result structure....

	// Select UART to be used for command line interface.
	//
	CliInit2(LPC_UART2,115200);
	// or use SWO Trace mode if your probe supports this. (not avail on LPXXpresso1769 board)
	//CliInitSWO();			// This configures SWO ITM Console as CLI in/output

	//uint32_t userBaseEventNr = LogInitEventLogger();
	//userBaseEventNr++;      // dummy usage to avoid warning....

//	ado_event_reset_t event = ado_event_reset_default;
//	event.epochNr = resetCount;
//	LogUsrEvent(&event);

	StopWatch_Init1(LPC_TIMER0);





	AdcInit((adc_channel_array_t *)&Channels);
	CliRegisterCommand("adcRead", read_transmit_sensors);

//	void *cards[2];

	// SSPx init:
	// With sys clck 96MHz: Possible steps are: 12MHz, 16Mhz, 24Mhz, 48Mhz (does not work with my external sd card socket)
	// My SD cards all work with clock mode mode3 or mode0. Mode3 is 10% faster as no SSL de-actiavtion between bytes is done.
#ifdef BOARD_CLIMB_EM2
	ADO_SSP_Init(ADO_SSP0, 24000000, SSP_CLOCK_MODE3);
    ADO_SSP_Init(ADO_SSP1, 24000000, SSP_CLOCK_MODE3);
	ADO_SPI_Init(0x08, SPI_CLOCK_MODE3);                                   // Clock Divider 0x08 -> fastest, must be even: can be up to 0xFE for slower SPI Clocking


	_SdcInitAll(&Cards);
	AdoSdcardCliInit(1);
#else
	//ADO_SSP_Init(ADO_SSP0, 24000000, SSP_CLOCK_MODE3);
    ADO_SSP_Init(ADO_SSP1, 24000000, SSP_CLOCK_MODE3);
	ADO_SPI_Init(0x08, SPI_CLOCK_MODE3);                                   // Clock Divider 0x08 -> fastest, must be even: can be up to 0xFE for slower SPI Clocking

	//cards[0]= SdcInit(ADO_SSP0, CsSdCard0);
	cards[0] = SdcInitSPI(CsSdCard0);
	cards[1] = SdcInit(ADO_SSP1, CsSdCard1);
	AdoSdcardCliInit(2, cards);
#endif

	// 6 MRAM chip inits
#ifdef BOARD_CLIMB_EM2
    MramInit(0, ADO_SSP0, CsMram0);
    MramInit(1, ADO_SSP0, CsMram1);
    MramInit(2, ADO_SSP0, CsMram2);
#else
    //  MramInit(0, ADO_SSP0, CsMram0);
    //  MramInit(1, ADO_SSP0, CsMram1);
    //  MramInit(2, ADO_SSP0, CsMram2);
    MramInitSPI(0, CsMram0);
    MramInitSPI(1, CsMram1);
    MramInitSPI(2, CsMram2);
#endif

    MramInit(3, ADO_SSP1, CsMram3);
    MramInit(4, ADO_SSP1, CsMram4);
    MramInit(5, ADO_SSP1, CsMram5);

    // CLI commands registering
    AdoMramCliInit();

	// register (test) command(s) ...
	CliRegisterCommand("test", main_testCmd);
	CliRegisterCommand("ver", main_showVersionCmd);
	CliRegisterCommand("time", main_showSysTimeCmd);
	CliRegisterCommand("setTime", main_setUtcTimeCmd);

	// ... and auto start it for running all test.
	main_showVersionCmd(0,0);
	//main_testCmd(0,0);

	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 18);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 18);

	 /* Inbetriebnahme */




	uint32_t fault = Chip_GPIO_GetPinState(LPC_GPIO, 2, 4);   /* SP-VCC-FAULT (Input)  */
	uint32_t rail = Chip_GPIO_GetPinState(LPC_GPIO, 3, 25);  /* Supply Rail Indicator (Input)  */
	uint32_t boot = Chip_GPIO_GetPinState(LPC_GPIO, 3, 26);   /* Boot-loader selection (Input)  */
	uint32_t rbf = Chip_GPIO_GetPinState(LPC_GPIO, 0, 21);   /* Remove before flight (Input)  */
	uint32_t wdt_trig = Chip_GPIO_GetPinState(LPC_GPIO, 4, 29);   /* Ext. WDT triggered latch (Input)  */

	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 9);   		/* Reset WDT latch   */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 9);   		/* WDT latch reset set to high   */

	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 19);   		/* Select thruster   */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 19);   	/* Unselect thruster   */

	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 26, true);   /* Thruster latchup (Test Output)  */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 26);   		/* Select thruster   */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 26);   	/* Unselect thruster   */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 26, false);   /* Thruster latchup (Input)  */

	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 5, true);   /* RS485 RX/TX sel  */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 2, 5);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 5);


	// Disable I2C
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 30); /* I2C-A EN    */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 2, 3);  /* I2C-B EN    */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 4);  /* I2C-C EN    */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 1);  /* I2C-D EN    */

 	// Enable I2C
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 30); /* I2C-A EN    */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 3);  /* I2C-B EN    */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 4);  /* I2C-C EN    */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 1);  /* I2C-D EN    */

 	//Disable supply
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 28);   /* SP-A Supply Enable   */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 7);    /* SP-B Supply Enable   */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 15);   /* SP-D Supply Enable   */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 22);   /* SP-D Supply Enable   */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 4, 28);   /* SD Supply Enable     */
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 29);   /* OBC Supply disable  */

 	//Enable supply
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 28);   /* SP-A Supply Enable   */
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 2, 7);    /* SP-B Supply Enable   */
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 15);   /* SP-D Supply Enable   */
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 22);   /* SP-D Supply Enable   */
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 4, 28);   /* SD Supply Enable     */
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 29);   /* SD Supply Enable     */

 	Chip_Clock_DisableCLKOUT();
 	//Chip_Clock_SetCLKOUTSource(SYSCTL_CLKOUTSRC_MAINOSC, 1);
 	//Chip_Clock_SetCLKOUTSource(SYSCTL_CLKOUTSRC_IRC, 1);
 	Chip_Clock_SetCLKOUTSource(SYSCTL_CLKOUTSRC_RTC, 1);
 	Chip_Clock_EnableCLKOUT();

 	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 27, 1); /* I2C0 SDA0 	    */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 28, 1); /* I2C0 SCL0 	    */

 	// "ONBOARD I2C"
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 19, 1); /* I2C1 SDA1         */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 20, 1); /* I2C1 SCL1         */

 	// "I2C A/B" Side panel bus
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 10, 1); /* I2C2 SDA2         */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 11, 1); /* I2C2 SCL2         */




 	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 5, true);
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 5);
 	Chip_GPIO_SetPinOutLow(LPC_GPIO, 2, 5);
 	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 5);



 	CliRegisterCommand("ledOn", ledOn);


 	/* Ende Inbetriebnahme*/

 	//ado_timestamp ts = TimeGetCurrentTimestamp();
 	uint32_t count = 0;


	while(1) {

//		 if (ts < TimeGetCurrentTimestamp())
//		 {
//			 ts = TimeGetCurrentTimestamp() + 500;
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 1, 18);
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 2, 6);
//
//#if 0
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 27); /* I2C0 SDA0 	    */
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 28); /* I2C0 SCL0 	    */
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 19); /* I2C1 SDA1         */
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 20); /* I2C1 SCL1         */
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 10); /* I2C2 SDA2         */
//			 Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 11); /* I2C2 SCL2         */
//#endif
//
//
//			 //printf("Fault: %d\n", Chip_GPIO_GetPinState(LPC_GPIO,2,4)); // Fault pin
//
//			 			 //printf("Tick %i\n", count);
//			 //Chip_GPIO_SetPinOutLow(LPC_GPIO, 2, 6);
//			 //printf("ledOn\r\n"); // Loopback test
//			 //Chip_GPIO_SetPinToggle(LPC_GPIO, 2, 6);
//			 count++;
//		 }


		CliMain();
		SdcMain();

#ifndef BOARD_CLIMB_EM2
		SdcMain(cards[1]);
#endif
		AdoCliMain();
		//LogMain();
		MramMain();
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
//    ado_tim_systemtime_t time;
//    TimeGetCurrentSystemTime(&time);
//
//    printf("Epoch #: %d ", time.epochNumber);
//    if (time.utcOffset.year == 0) {
//            printf("(no UTC sync)\n");
//        } else {
//            printf("(started %d - ", time.utcOffset.year);
//            printf("%d.%05d)\n", (int)time.utcOffset.dayOfYear,(int)((time.utcOffset.dayOfYear - (int)time.utcOffset.dayOfYear) * 100000));
//        }
//    printf("Epoch ms: %d\n", time.msAfterStart);
//
//    RTC_TIME_T t;
//    TimeGetCurrentUtcTime(&t);
//    printf("Utc: %02d.%02d.%04d %02d:%02d:%02d\n",
//            t.time[RTC_TIMETYPE_DAYOFMONTH],
//            t.time[RTC_TIMETYPE_MONTH],
//            t.time[RTC_TIMETYPE_YEAR],
//            t.time[RTC_TIMETYPE_HOUR],
//            t.time[RTC_TIMETYPE_MINUTE],
//            t.time[RTC_TIMETYPE_SECOND]);
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

    //TimeSetUtc2(&t);
}
