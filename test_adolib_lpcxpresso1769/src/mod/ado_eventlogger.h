/*
 * ado_eventlogger.h
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#ifndef MOD_ADO_EVENTLOGGER_H_
#define MOD_ADO_EVENTLOGGER_H_

#include <Chip.h>

#include <ado_time.h>

// Event numbers are 16 bit
typedef uint16_t ado_event_nr;

// Use this macro to avoid warnings without explicit cast of the user event.
#define LogUsrEvent(e) LogEvent((ado_event_t *)e)

typedef enum ado_sys_event_e {
    // Events without Data
    ADO_EVENT_LOGGER_CREATED = 1,
    ADO_EVENT_CLITXBYTECUTOFF,          // -> Move to CLI as User event
    ADO_EVENT_CLIBUFFERERROR,           // -> Move to CLI as User event
    ADO_EVENT_CLICMDTABLEFULLERROR,     // -> Move to CLI as User event
    ADO_EVENT_SSPJOBBUFFERFULLERROR,    // -> Move to SSP as User event

    //
    ADO_EVENT_SYSRESET,                 // With data: resetcount
    ADO_EVENT_SYNCTIME,                 // With data: UTC-Offset
    ADO_EVENT_LOGMESSAGE,               // With data: message
    // ....
    ADO_EVENT_USERBASE,                 // First event Nr free to be used by user modules.
    ADO_EVENT_MAXNR = UINT16_MAX        // Make Event enum size using 2 bytes to fit into ado_event_nr.
} ado_sys_event_t;


// This is only the 'header of each concrete event. if data is available it follows directly as struct after this struct in memory.
typedef struct ado_event_s {
    ado_timestamp timestamp;    // This always represents ms after reset! If the event gets persisted additional info is needed (e.g. ResetCounter and/or UTCOffset)!
                                // If resetCounter is (persisted) accurate, then all events and their exact sequence are uniquely identifiable.
                                // If there exists a valid UTCOffset to a specific Reset Counter all events of this 'Reset-epoch' can be assigned to an exact UTC-timestamp
                                // The valid time between 2 resets without overrun occurring in eventlogger time stamps is approx. 49,7 days. So your system should have some
                                // means to reset at least all 49 days, or you have to manually increase the resetCounter and set a new UTCOffset.
    ado_event_nr  eventNr;
    uint16_t      eventDataSize;
}  __attribute__((packed))  ado_event_t;

extern const struct ado_event_reset_s {
    ado_event_t   baseEvent;
    uint32_t      epochNr;
} ado_event_reset_default;
typedef struct ado_event_reset_s ado_event_reset_t;

extern const struct ado_event_logmessage_s {
    ado_event_t   baseEvent;
    char          message[16];                      // TODO: allow Events with data Pointer and copy the content iso struct content......
} ado_event_logmessage_default;
typedef struct ado_event_logmessage_s ado_event_logmessage_t;


ado_event_nr LogInitEventLogger(void);     // Returns the base EventNr For user events.
void LogMain(void);

void LogEvent(ado_event_t *event);


#endif /* MOD_ADO_EVENTLOGGER_H_ */
