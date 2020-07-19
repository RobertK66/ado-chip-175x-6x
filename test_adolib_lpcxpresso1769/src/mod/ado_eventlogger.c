/*
 * ado_eventlogger.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include "ado_eventlogger.h"

#include <stdio.h>

// Default struct with initializer for correct event nr and datasize - located in ROM area
const struct ado_event_reset_s ado_event_reset_default = { {0, ADO_EVENT_SYSRESET, (sizeof(ado_event_reset_t) - sizeof(ado_event_t))}, 0 };

// internal / local  module events
struct ado_event_loggercreated_s {
    ado_event_t   baseEvent;
};
typedef struct ado_event_loggercreated_s ado_event_loggercreated_t;
const struct ado_event_loggercreated_s ado_event_loggercreated_default = { {0, ADO_EVENT_LOGGER_CREATED,sizeof(ado_event_loggercreated_t) - sizeof(ado_event_t) }};

void LogEvent(ado_event_t *event) {
    printf("Event %d: time: %d data size: %d\n", event->eventNr, event->timestamp, event->eventDataSize);
    if (event->eventDataSize > 0) {
        char *ptr = ((char*)event);
        ptr += sizeof(ado_event_t);
        printf("     Data:");
        for (int i=0; i< event->eventDataSize; i++) {
            printf(" %02X", *(ptr++));
        }
        printf("\n");
    }
}

ado_event_nr LogInitEventLogger() {
    ado_event_loggercreated_t createdEvent = ado_event_loggercreated_default;
    LogUsrEvent(&createdEvent);
    return ADO_EVENT_USERBASE;
}

