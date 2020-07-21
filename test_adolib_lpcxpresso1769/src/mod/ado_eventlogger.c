/*
 * ado_eventlogger.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include "ado_eventlogger.h"

#include <stdio.h>
#include <string.h>

#include <mod\cli.h>
#include <ado_time.h>

#define ADO_EVENT_BUFFER_COUNT 32
#define ADO_EVENT_BUFFER_BYTES (ADO_EVENT_BUFFER_COUNT * (sizeof(ado_event_t) + 4))      // Lets asume 4 byte data per event 'average'

// Default struct with initializer for correct event nr and datasize - located in ROM area.
// Usage: make a variable of type ado_event_reset_t and assign this struct.
//        ado_event_reset_t myEvent = ado_event_reset_default;
const struct ado_event_reset_s ado_event_reset_default = { {0, ADO_EVENT_SYSRESET, (sizeof(ado_event_reset_t) - sizeof(ado_event_t))}, 0 };

// internal / local  module events
struct ado_event_loggercreated_s {
    ado_event_t   baseEvent;
};
typedef struct ado_event_loggercreated_s ado_event_loggercreated_t;
const struct ado_event_loggercreated_s ado_event_loggercreated_default = { {0, ADO_EVENT_LOGGER_CREATED,sizeof(ado_event_loggercreated_t) - sizeof(ado_event_t) }};

static ado_event_t *AdoBufferedEvent[ADO_EVENT_BUFFER_COUNT];
static uint8_t AdoEventMemory[ADO_EVENT_BUFFER_BYTES];
static uint16_t AdoEventsBuffered = 0;
static uint8_t *AdoBufferPtr = AdoEventMemory;

void LogEvent(ado_event_t *event) {
    event->timestamp = TimeGetCurrentTimestamp();

    uint16_t size = sizeof(ado_event_reset_t) + event->eventDataSize;

    if ((AdoEventsBuffered < ADO_EVENT_BUFFER_COUNT) &&
        (AdoBufferPtr + size - AdoEventMemory) <  ADO_EVENT_BUFFER_BYTES ) {

        // copy the event including user data to Buffer RAM & remember start position.
        memcpy(AdoBufferPtr, event, size);
        AdoBufferedEvent[AdoEventsBuffered++] = (ado_event_t *)AdoBufferPtr;
        AdoBufferPtr += size;

        printf("Event %d stored");
    } else {
        // Buffer Full :-(

        printf("Event buffer full!");
    }

}

void LogListEvents(int argc, char *argv[]) {
    for (int i = 0; i < AdoEventsBuffered; i++) {
        ado_event_t *event = AdoBufferedEvent[i];
        printf("Event[%d]: %d at: %d data: %d\n", i,  event->eventNr, event->timestamp, event->eventDataSize);
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
}


ado_event_nr LogInitEventLogger() {
    CliRegisterCommand("log", LogListEvents);

    ado_event_loggercreated_t createdEvent = ado_event_loggercreated_default;
    LogUsrEvent(&createdEvent);
    return ADO_EVENT_USERBASE;
}

