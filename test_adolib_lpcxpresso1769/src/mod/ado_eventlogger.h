/*
 * ado_eventlogger.h
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#ifndef MOD_ADO_EVENTLOGGER_H_
#define MOD_ADO_EVENTLOGGER_H_

#include <Chip.h>

typedef enum ado_event_nr_e {
    // Events without Data
    ADO_EVENT_LOGGER_CREATED,
    ADO_EVENT_CLITXBYTECUTOFF,
    ADO_EVENT_CLIBUFFERERROR,
    ADO_EVENT_CLICMDTABLEFULLERROR,
    ADO_EVENT_SSPJOBBUFFERFULLERROR,

    //
    ADO_EVENT_SYSRESET,      // With data: resetcount
    ADO_EVENT_SYNCTIME,      // With data: UTC-Offset
    ADO_EVENT_LOGMESSAGE     // With data: message
    // ....
} ado_event_nr_t ;

void LogEvent(ado_event_nr_t eventNr, uint16_t dataSize, uint8_t *data);

#endif /* MOD_ADO_EVENTLOGGER_H_ */
