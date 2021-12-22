/*
 * ado_eventlogger.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include "ado_eventlogger.h"

#include <stdio.h>
#include <string.h>
//#include <stdarg.h>

#include <mod\cli.h>
//#include <ado_time.h>

#define ADO_EVENT_BUFFER_COUNT 32
#define ADO_EVENT_BUFFER_BYTES (ADO_EVENT_BUFFER_COUNT * (sizeof(ado_event_t) + 4))     // Lets asume 4 byte data per event 'average'
#define ADO_EVENT_SAVE_INTERVALL_MS 60000                                               // Persist Event log at least every Minute,

//// Default struct with initializer for correct event nr and datasize - located in ROM area.
//// Usage: make a variable of type ado_event_reset_t and assign this struct.
////        ado_event_reset_t myEvent = ado_event_reset_default;
//const struct ado_event_reset_s ado_event_reset_default = { {0, ADO_EVENT_SYSRESET, (sizeof(ado_event_reset_t) - sizeof(ado_event_t))}, 0 };
//const struct ado_event_logmessage_s ado_event_logmessage_default = { {0, ADO_EVENT_LOGMESSAGE, (sizeof(ado_event_logmessage_t) - sizeof(ado_event_t))}, "123456789abcdef" };
//
//// internal / local  module events
//struct ado_event_loggercreated_s {
//    ado_event_t   baseEvent;
//};
//typedef struct ado_event_loggercreated_s ado_event_loggercreated_t;
//const struct ado_event_loggercreated_s ado_event_loggercreated_default = { {0, ADO_EVENT_LOGGER_CREATED,sizeof(ado_event_loggercreated_t) - sizeof(ado_event_t) }};
//
//
//void LogListEventsCmd(int argc, char *argv[]);
//void LogEventCmd(int argc, char *argv[]);
//
//static ado_event_t *AdoBufferedEvent[2][ADO_EVENT_BUFFER_COUNT];
//static uint8_t AdoEventMemory[2][ADO_EVENT_BUFFER_BYTES];
//
//static bool AdoBufferSwapped = false;
//static uint16_t AdoEventsBuffered = 0;
//static uint8_t *AdoBufferPtr = AdoEventMemory[false];
////static ado_timestamp nextSaveTime;
//
//static uint16_t eventsToStore = 0;
//static bool storeActive = false;
//
//
//
//// This allows to give a arbitrary parameter list for initializing any concrete 'subclass' of ado_event.
//// The initializer taking a va_arg* list must be provided in event callback variable.
//void InitializeAndLogEvent(ado_event_t *event, ...) {
//    va_list args;
//
//    va_start(args, event);
//    if (event->initializer != 0) {
//        event->initializer(event, &args);
//    }
//    va_end(args);
//    LogEvent(event);
//}
//
//
//void LogEvent(ado_event_t *event) {
//    event->timestamp = TimeGetCurrentTimestamp();
//
//    uint16_t size = sizeof(ado_event_reset_t) + event->eventDataSize;
//
//    if ((AdoEventsBuffered < ADO_EVENT_BUFFER_COUNT) &&
//        (AdoBufferPtr + size - AdoEventMemory[AdoBufferSwapped]) <  ADO_EVENT_BUFFER_BYTES ) {
//
//        // copy the event including user data to Buffer RAM & remember start position.
//        memcpy(AdoBufferPtr, event, size);
//        AdoBufferedEvent[AdoBufferSwapped][AdoEventsBuffered++] = (ado_event_t *)AdoBufferPtr;
//        AdoBufferPtr += size;
//    } else {
//        // Buffer Full :-(
//        printf("Event buffer full!!!!");
//    }
//
//}
//
//
//ado_event_nr LogInitEventLogger() {
//    CliRegisterCommand("log", LogListEventsCmd);
//    CliRegisterCommand("logEvent", LogEventCmd);
//
//    ado_event_loggercreated_t createdEvent = ado_event_loggercreated_default;
//    LogUsrEvent(&createdEvent);
//
//    nextSaveTime = TimeGetCurrentTimestamp() + ADO_EVENT_SAVE_INTERVALL_MS;
//    return ADO_EVENT_USERBASE;
//}
//
//
//void EventStored(void) {
//    storeActive = false;
//    eventsToStore--;
//}
//
//void SaveEventToMemAsync(ado_event_t *event, void EventStoredCallback(void)){
//
//}
//
//void LogMain(void) {
//    if (nextSaveTime < TimeGetCurrentTimestamp()) {
//        nextSaveTime = TimeGetCurrentTimestamp() + ADO_EVENT_SAVE_INTERVALL_MS;
//        if (eventsToStore > 0) {
//            printf("Buffer Swap prohibited by Storage not being ready!");
//            return;
//        }
//        AdoBufferSwapped = !AdoBufferSwapped;
//        eventsToStore = AdoEventsBuffered;
//        AdoEventsBuffered = 0;
//        AdoBufferPtr = AdoEventMemory[AdoBufferSwapped];
//    }
//    if ((eventsToStore > 0) && !storeActive) {
//        SaveEventToMemAsync(AdoBufferedEvent[!AdoBufferSwapped][eventsToStore], EventStored);
//    }
//}
//
//void LogListEventsCmd(int argc, char *argv[]) {
//    printf("Events stored: %d Memory free: %d bytes.\n", AdoEventsBuffered, ADO_EVENT_BUFFER_BYTES - (AdoBufferPtr - AdoEventMemory[AdoBufferSwapped]));
//    for (int i = 0; i < AdoEventsBuffered; i++) {
//        ado_event_t *event = AdoBufferedEvent[AdoBufferSwapped][i];
//        printf("Event[%02d]: %03d at: %08d data(%02d): ", i,  event->eventNr, event->timestamp, event->eventDataSize);
//        if (event->eventDataSize > 0) {
//            char *ptr = ((char*)event);
//            ptr += sizeof(ado_event_t);
//            for (int i=0; i< event->eventDataSize; i++) {
//                printf(" %02X", *(ptr++));
//            }
//        }
//        printf("\n");
//    }
//}
//
//void LogEventCmd(int argc, char *argv[]) {
//
//    ado_event_logmessage_t msgEvent = ado_event_logmessage_default;
//    if (argc > 0) {
//        strncpy(msgEvent.message, argv[0], 16);
//    }
//    LogUsrEvent(&msgEvent);
//}
