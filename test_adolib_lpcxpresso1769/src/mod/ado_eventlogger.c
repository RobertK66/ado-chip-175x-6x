/*
 * ado_eventlogger.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include "ado_eventlogger.h"


typedef struct ado_event_s {
    uint32_t      timestamp;    // This always represents ms after reset! If the event gets persisted additional info is needed (e.g. ResetCounter and/or UTCOffset)!
                                // If resetCounter is (persisted) accurate, then all events and their exact sequence are uniquely identifiable.
                                // If there exists a valid UTCOffset to a specific Reset Counter all events of this 'Reset-epoch' can be assigned to an exact UTC-timestamp
                                // The valid time between 2 resets without overrun occurring in eventlogger time stamps is approx. 49,7 days. So your system should have some
                                // means to reset at least all 49 days, or you have to manually increase the resetCounter and set a new UTCOffset.
    ado_event_nr_t  eventNr;
    uint16_t      eventDataSize;
    uint8_t       *eventData;
}  __attribute__((packed))  ado_event_t;


void LogEvent(ado_event_nr_t eventNr, uint16_t dataSize, uint8_t *data) {



}
