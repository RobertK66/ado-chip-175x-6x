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

typedef struct ado_tim_systemtime_s {
    uint32_t    resetCount;             // 0 meaning unknown/unsynced
    uint32_t    msAfterReset;
    ado_tim_juliandate_t utcOffset;     // year 0 meaning unknown/unsynced
} ado_tim_systemtime_t;



#endif /* MOD_ADO_TIME_H_ */
