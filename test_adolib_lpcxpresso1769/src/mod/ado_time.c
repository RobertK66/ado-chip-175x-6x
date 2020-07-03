/*
 * ado_time.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include "ado_time.h"

static ado_tim_systemtime_t adoSystemTime;

void RIT_IRQHandler(void) {
    LPC_RITIMER->CTRL |= 0x0001;                    // Clear the RITINT flag;
    adoSystemTime.msAfterStart++;
    Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 4);         // Debug IO

}

void TimeInit(uint32_t startOffsetMs, uint32_t epochNumber) {
    // we use RIT IRQ to count ms after reset. It counts PCLK cycles
    // This Repetive Interrupt Timer us activated by default with reset.
    Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_RIT);
    Chip_Clock_SetPCLKDiv(SYSCTL_PCLK_RIT, SYSCTL_CLKDIV_1);

    adoSystemTime.msAfterStart = startOffsetMs;
    adoSystemTime.epochNumber = epochNumber;
    LPC_RITIMER->COMPVAL = (SystemCoreClock/1000) - 1;   // We want an IRQ every 0,001 Seconds.
    LPC_RITIMER->COUNTER = 0;
    LPC_RITIMER->CTRL = RIT_CTRL_ENCLR | RIT_CTRL_ENBR | RIT_CTRL_TEN;
    NVIC_EnableIRQ(RITIMER_IRQn);
}

void TimeGetCurrentSystemTime(ado_tim_systemtime_t *sysTime) {
    sysTime->epochNumber = adoSystemTime.epochNumber;
    sysTime->msAfterStart = adoSystemTime.msAfterStart;
    sysTime->utcOffset.year = adoSystemTime.utcOffset.year;
    sysTime->utcOffset.dayOfYear = adoSystemTime.utcOffset.dayOfYear;
}

static const int days[2][13] = {
   {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
   {0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
};

void TimeSetUtc2(RTC_TIME_T *fullTime) {
    adoSystemTime.utcOffset.year = fullTime->time[RTC_TIMETYPE_YEAR];

    // First we get the day# depending of leap year
    int leap = (fullTime->time[RTC_TIMETYPE_YEAR] % 4 == 0 && fullTime->time[RTC_TIMETYPE_YEAR] % 100 != 0) || (fullTime->time[RTC_TIMETYPE_YEAR] % 400 == 0);
    juliandayfraction day = days[leap][fullTime->time[RTC_TIMETYPE_MONTH]] + fullTime->time[RTC_TIMETYPE_DAYOFMONTH];
    // Then we add the time fraction
    day += ((fullTime->time[RTC_TIMETYPE_SECOND] / 60.0 + fullTime->time[RTC_TIMETYPE_MINUTE]) / 60.0 + fullTime->time[RTC_TIMETYPE_HOUR]) / 24.0;
    adoSystemTime.utcOffset.dayOfYear = day;
}
