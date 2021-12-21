/*
 * ado_time.h
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#ifndef MOD_ADO_TIME_H_
#define MOD_ADO_TIME_H_

#include <chip.h>

//typedef uint32_t    ado_timestamp;
//typedef double      juliandayfraction;
//
//typedef struct ado_tim_tledatetime_s {
//    uint16_t            year;
//    juliandayfraction   dayOfYear;   // This starts with 1.0 on 00:00:00.0000 on 1st January and counts up to 365/6.xxxxx on 31.Dec 23:59:59.9999
//} ado_tim_tledatetime_t;
//
//typedef struct ado_tim_systemtime_s {
//    uint32_t                epochNumber;
//    ado_timestamp           msAfterStart;
//    ado_tim_tledatetime_t   utcOffset;      // year 0 meaning unknown/unsynced
//} ado_tim_systemtime_t;
//
//void TimeInit(uint32_t startOffsetMs, uint32_t epochNumber);
//void TimeSetUtc1(uint16_t year, uint8_t month, uint8_t dayOfMonth, uint8_t hour, uint8_t min, uint8_t sec);
//void TimeSetUtc2(RTC_TIME_T *fullTime);
//ado_timestamp TimeGetCurrentTimestamp();
//void TimeGetCurrentSystemTime(ado_tim_systemtime_t *sysTime);
//void TimeGetCurrentUtcTime(RTC_TIME_T *fullTime);

#endif /* MOD_ADO_TIME_H_ */
