/*
 * system.h
 *
 *  Created on: 06.04.2020
 *      Author: Robert
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_


#include <chip.h>

#define red_on()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 22)
#define red_off()		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 22)
#define red_toggle() 	Chip_GPIO_SetPinToggle(LPC_GPIO, 0, 22)
#define blue_on()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 3, 26)
#define blue_off()		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 26)
#define blue_toggle()	Chip_GPIO_SetPinToggle(LPC_GPIO, 3, 26)
#define green_on()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 3, 25)
#define green_off()		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 3, 25)
#define green_toggle()	Chip_GPIO_SetPinToggle(LPC_GPIO, 3, 25)

#define sd_cs_high()	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4)
#define sd_cs_low()		Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4)


#endif /* SYSTEM_H_ */
