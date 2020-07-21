/*
 * ado_time.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include <ado_time.h>
#include <math.h>

juliandayfraction TimeGetCurrentDayOfYear();

static ado_tim_systemtime_t adoSystemTime;

static const int timeMonthOffsetDays[2][13] = {
   {0,   0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},  // non leap year
   {0,   0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}   // leap year
};

static inline juliandayfraction TimeMsToDayFractions(uint32_t ms) {
    return (((double)ms)/86400000.0);
}


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

void TimeSetUtc1(uint16_t year, uint8_t month, uint8_t dayOfMonth, uint8_t hour, uint8_t min, uint8_t sec) {
    adoSystemTime.utcOffset.year = year;

    // First we get the day# depending of leap year
    int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    juliandayfraction day = timeMonthOffsetDays[leap][month] + dayOfMonth;
    // Then we add the time fraction
    day += ((sec / 60.0 + min) / 60.0 + hour) / 24.0;

    // To store the offset to this time we subtract current ms.
    day -= TimeMsToDayFractions(adoSystemTime.msAfterStart);
    adoSystemTime.utcOffset.dayOfYear = day;
}

inline void TimeSetUtc2(RTC_TIME_T *fullTime) {
    TimeSetUtc1(fullTime->time[RTC_TIMETYPE_YEAR],
                fullTime->time[RTC_TIMETYPE_MONTH],
                fullTime->time[RTC_TIMETYPE_DAYOFMONTH],
                fullTime->time[RTC_TIMETYPE_HOUR],
                fullTime->time[RTC_TIMETYPE_MINUTE],
                fullTime->time[RTC_TIMETYPE_SECOND]);
}

void TimeGetCurrentUtcTime(RTC_TIME_T *fullTime) {
    uint16_t year = adoSystemTime.utcOffset.year;
    fullTime->time[RTC_TIMETYPE_YEAR] = year;
    if (year == 0) {
        fullTime->time[RTC_TIMETYPE_MONTH] = 0;
        fullTime->time[RTC_TIMETYPE_DAYOFMONTH] = 0;
        fullTime->time[RTC_TIMETYPE_HOUR] = 0;
        fullTime->time[RTC_TIMETYPE_MINUTE] = 0;
        fullTime->time[RTC_TIMETYPE_SECOND] = 0;
    } else {
        double currentTime =  TimeGetCurrentDayOfYear();
        // Date conversionm
        int day = (int)floor(currentTime);
        int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        for (uint8_t month = 1; month<=12; month++) {
            if (timeMonthOffsetDays[leap][month] > day) {
                fullTime->time[RTC_TIMETYPE_MONTH] = month - 1;
                fullTime->time[RTC_TIMETYPE_DAYOFMONTH] = day - timeMonthOffsetDays[leap][month - 1];
                break;
            }
        }
        // Time conversion
        double temp = (currentTime - day) * 24.0;
        fullTime->time[RTC_TIMETYPE_HOUR]  = (uint32_t)floor(temp);
        temp = (temp - fullTime->time[RTC_TIMETYPE_HOUR]) * 60.0;
        fullTime->time[RTC_TIMETYPE_MINUTE]  = (uint32_t)floor(temp);
        fullTime->time[RTC_TIMETYPE_SECOND]  = (temp - fullTime->time[RTC_TIMETYPE_MINUTE]) * 60.0;
    }
}

juliandayfraction TimeGetCurrentDayOfYear() {
    juliandayfraction currentTime = adoSystemTime.utcOffset.dayOfYear;
    currentTime += TimeMsToDayFractions(adoSystemTime.msAfterStart);
    return currentTime;
}

ado_timestamp TimeGetCurrentTimestamp() {
    return adoSystemTime.msAfterStart;
}
