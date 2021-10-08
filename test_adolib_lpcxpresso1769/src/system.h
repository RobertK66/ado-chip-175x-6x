/*
 * system.h
 *
 *  Created on: 06.04.2020
 *      Author: Robert
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#define BOARD_CLIMB_EM2 1    // Temp used here to get a version for hardware commissioning the new prototypes of EM2


#include <chip.h>

#define red_on()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 22)
#define red_off()		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 22)
#define red_toggle() 	Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 22)
#define blue_on()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 18)
#define blue_off()		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 18)
#define blue_toggle()	Chip_GPIO_SetPinToggle(LPC_GPIO, 1, 18)
#define green_on()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 3, 25)
#define green_off()		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 25)
#define green_toggle()	Chip_GPIO_SetPinToggle(LPC_GPIO, 3, 25)

//#define sd_cs_high()	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4)
//#define sd_cs_low()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4)


#if DEBUG_TESTCONFIG_2
	// This test config is used to check on available module configs
	#define CLI_CFG_TXBUFFER_SIZE 512			// use of CLI internal buffer with smaller footprint.
#endif


#endif /* SYSTEM_H_ */
