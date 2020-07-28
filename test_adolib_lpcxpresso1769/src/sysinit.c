/*
 * @brief Common SystemInit function for LPC17xx/40xx chips
 *
 * @note
 * Copyright 2013-2014, 2019 NXP
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include <chip.h>
#include "system.h"

const uint32_t  OscRateIn = 12000000;       // Here you define the XTAL-freq used on your Board/System. Its expected External by chip.h!
const uint32_t  RTCOscRateIn = 32768;       // Here you define the RTC-freq used on  your Board/System. Its expected External by chip.h!
extern uint32_t SystemCoreClock;            // This is also expected external by chip.h and defined in chip_17xx_40xx.c

static uint32_t resetSource = 0x0;
void LpcExpresso1769Init(void);
void PegasusObcInit(void);

// Set up and initialize hardware prior to call to main
void SystemInit(void) {
    // Set the ARM VectorTableOffsetRegister to our Vector table (at 0x00000000 in flash)
	unsigned int *pSCB_VTOR = (unsigned int*) 0xE000ED08;
	extern void *g_pfnVectors;
	*pSCB_VTOR = (unsigned int) &g_pfnVectors;

	// Read and keep the reset source.
	resetSource = LPC_SYSCTL->RSID;

    LpcExpresso1769Init();

	// Switch to XTAL wait for PLL To be locked and set the CPU Core Frequency.
	Chip_SetupXtalClocking();
	//Read clock settings and update SystemCoreClock variable (needed only for iap.c module - Common FLASH support functions ...)
	SystemCoreClockUpdate();
	
	// Choose your board here
	//LpcExpresso1769Init();
	//PegasusObcInit();
}

// Following section is written to use the LPCXpresso1769 Board (aka OM13085)
// --------------------------------------------------------------------------
STATIC const PINMUX_GRP_T pinmuxing[] = {									// ExpConnector Pins
	{ 0, 2, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART0 "Uart D" */		// J2-21
	{ 0, 3, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART0 "Uart D" */		// J2-22
	{ 2, 0, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 "Uart C" */		// J2-42
	{ 2, 1, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 "Uart C" */		// J2-43
	{ 0, 10, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART2 "Uart B" */		// J2-40	We use this as command line interface for cli module
	{ 0, 11, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART2 "Uart B" */		// J2-41
	{ 0, 0, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART3 "Uart A" */		// J2-9
	{ 0, 1, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART3 "Uart A" */		// J2-10
	
	{ 0, 22, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Led red       */
	{ 3, 25, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Led green     */
	{ 3, 26, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Led blue      */

	{ 0, 27, IOCON_MODE_INACT | IOCON_FUNC1 }, /* I2C0 SDA 	    */	// J2-25
	{ 0, 28, IOCON_MODE_INACT | IOCON_FUNC1 }, /* I2C0 SCL 	    */  // J2-26

	{ 0, 19, IOCON_MODE_INACT | IOCON_FUNC3 }, /* I2C1 SDA      */ // PAD8 	this connects to boards e2prom with address 0x50
	{ 0, 20, IOCON_MODE_INACT | IOCON_FUNC3 }, /* I2C1 SCL      */  // PAD2

	{ 0, 15, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SCK   		 */ // J2-13	we use this to test SPI SD card slot.
	{ 0, 16, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SSL   		 */ // J2-13    the 'recommended' SSL for SSP0
	{ 0, 17, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MISO	 		 */	// J2-12
	{ 0, 18, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MOSI  		 */	// J2-11

	{ 0, 7, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SCK   		 	*/  // J2-7	SSP1
	{ 0, 6, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SSL   		 	*/  // J2-8 SSP1
	{ 0, 8, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MISO	 	 	*/	// J2-6	SSP1
	{ 0, 9, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MOSI  		 	*/	// J2-5	SSP1

	{ 0, 23, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MRAM CS  SSP1-CS1 	*/ // J2-15
	{ 0, 24, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MRAM CS  SSP1-CS2		*/ // J2-16
	{ 0, 25, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MRAM CS  SSP1-CS3		*/ // J2-17
	{ 0, 26, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MRAM CS  SSP0-CS1		*/ // J2-18
	{ 1, 30, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MRAM CS  SSP0-CS2		*/ // J2-19
	{ 1, 31, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MRAM CS  SSP0-CS3		*/ // J2-20

	{ 0, 4, IOCON_MODE_INACT | IOCON_FUNC0 } /* P0[4]		     */ // J2-38
};



// This is the Pinmux for the OBC board - Only Uarts used and checked yet ....
STATIC const PINMUX_GRP_T pinmuxing2[] = {
	{ 0, 2, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART0 "Uart D" */
	{ 0, 3, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART0 "Uart D" */
	{ 2, 0, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 "Uart C" */
	{ 2, 1, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 "Uart C" */
	{ 2, 8, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART2 "Uart B" */		// We use this as command line interface for cli module
	{ 2, 9, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART2 "Uart B" */		//
	{ 0, 0, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART3 "Uart A" */
	{ 0, 1, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART3 "Uart A" */

	{ 0, 22, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Led red       */
	{ 3, 25, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Led green     */
	{ 3, 26, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Led blue      */

	{ 0, 27, IOCON_MODE_INACT | IOCON_FUNC1 }, /* I2C0 SDA 	    */	// J2-25
	{ 0, 28, IOCON_MODE_INACT | IOCON_FUNC1 }, /* I2C0 SCL 	    */  // J2-26

	{ 0, 19, IOCON_MODE_INACT | IOCON_FUNC3 }, /* I2C1 SDA      */ // PAD8 	this connects to boards e2prom with address 0x50
	{ 0, 20, IOCON_MODE_INACT | IOCON_FUNC3 }, /* I2C1 SCL      */  // PAD2

	{ 0, 15, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SCK   		 */ // J2-13	we use this to test SPI SD card slot.
	{ 0, 16, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SSL   		 */ // J2-13    the 'recommended' SSL for SSP0
	{ 0, 17, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MISO	 		 */	// J2-12
	{ 0, 18, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MOSI  		 */	// J2-11

	{ 0, 4, IOCON_MODE_INACT | IOCON_FUNC0 } /* P0[4]		     */ // J2-38
};


void PegasusObcInit(void) {
	// All IOS are not checked yet. Only Uart B (has other MUX Pins!!!) is used at this moment.
	Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing2, sizeof(pinmuxing2) / sizeof(PINMUX_GRP_T));
	Chip_GPIO_Init(LPC_GPIO);


}

void LpcExpresso1769Init(void) {
	Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxing, sizeof(pinmuxing) / sizeof(PINMUX_GRP_T));
	Chip_GPIO_Init(LPC_GPIO);
	
	// LEDs are output and switch off
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 22, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 25, true);
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 26, true);
	red_off();
	green_off();
	blue_off();
	
	/* --- SPI pins no open drain --- */
	Chip_IOCON_DisableOD(LPC_IOCON, 0, 15);
	Chip_IOCON_DisableOD(LPC_IOCON, 0, 16);
	Chip_IOCON_DisableOD(LPC_IOCON, 0, 17);
	Chip_IOCON_DisableOD(LPC_IOCON, 0, 18);
	// Test GPIO
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 4, true);
	//Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
	// SD card CS output
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 16, true);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 16);


//    Chip_SPI_Init(LPC_SPI); 		// All default values as above, Bitrate is set to 4000000 with this!
//	Chip_SPI_Int_Enable(LPC_SPI);
//	//NVIC_SetPriority(SPI_IRQn, 5);
//	NVIC_EnableIRQ(SPI_IRQn);
	
}
