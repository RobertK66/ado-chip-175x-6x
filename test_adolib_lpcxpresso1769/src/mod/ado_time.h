/*
 * ado_time.h
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#ifndef MOD_ADO_TIME_H_
#define MOD_ADO_TIME_H_

#include <chip.h>

typedef struct ado_tim_juliandate_s {
    uint16_t    year;
    double      julianday;
} ado_tim_juliandate_t;

typedef uint32_t ado_timestamp;

typedef struct ado_tim_systemtime_s {
    uint32_t                resetCount;     // 0 meaning unknown/unsynced   alias: 'epoch number'
    ado_timestamp           msAfterReset;   // 'after epoch start'
    ado_tim_juliandate_t    utcOffset;      // year 0 meaning unknown/unsynced
} ado_tim_systemtime_t;

void TimeInit();
void TimeSetUtc1(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
void TimeSetUtc2(RTC_TIME_T *fullTime);
ado_timestamp TimeGetCurrentTimestamp();
void TimeGetCurrentSystemTime(ado_tim_systemtime_t *sysTime);
void TimeGetUtcTime(RTC_TIME_T *fullTime);

#endif /* MOD_ADO_TIME_H_ */
