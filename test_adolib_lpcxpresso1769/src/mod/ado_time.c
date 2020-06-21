/*
 * ado_time.c
 *
 *  Created on: 19.06.2020
 *      Author: Robert
 */

#include "ado_time.h"

static ado_tim_systemtime_t adoSystemTime;

void RIT_IRQHandler(void) {
    LPC_RITIMER->COUNTER = 0;
    //LPC_RITIMER->COMPVAL = LPC_RITIMER->COUNTER + ((uint32_t)11999);
    LPC_RITIMER->CTRL |= 0x0001;                    // Clear the RITINT flag;
    adoSystemTime.msAfterReset++;
    if (adoSystemTime.msAfterReset%1000 == 0) {
        Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 4);         // Debug IO
    }
}


void TimeInit() {
    // Check and read valid RTC Time / ResetNr available ....

    // we use RIT IRQ to count ms after reset. It counts PCLK cycles
    // This Repetive Interrupt Timer us activated by default with reset.
    Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_RIT);
    Chip_Clock_SetPCLKDiv(SYSCTL_PCLK_RIT, SYSCTL_CLKDIV_8);

    uint32_t startOffset = LPC_RITIMER->COUNTER;
    LPC_RITIMER->COUNTER = 0;
    adoSystemTime.msAfterReset = 0; //startOffset/1000;  //Assuming we run all the time with PCLK on crystal frequency already. TODO: Check correct value to be set here.....
    LPC_RITIMER->COMPVAL = (SystemCoreClock/1000)-1;   // We want an IRQ every 0,001 Seconds.
    NVIC_EnableIRQ (RITIMER_IRQn);
}

