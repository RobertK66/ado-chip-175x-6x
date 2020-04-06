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

const uint32_t OscRateIn = 12000000;
const uint32_t RTCOscRateIn = 32768;

void LpcExpresso1769Init(void);

/* Set up and initialize hardware prior to call to main */
void SystemInit(void)
{
	unsigned int *pSCB_VTOR = (unsigned int *) 0xE000ED08;
	extern void *g_pfnVectors;
	*pSCB_VTOR = (unsigned int) &g_pfnVectors;
	/* We use XTAL not the default IRC */
	Chip_SetupXtalClocking();	// Chip_SystemInit();
	//Read clock settings and update SystemCoreClock variable (needed only for iap.c module - Common FLASH support functions ...)
	SystemCoreClockUpdate();

	LpcExpresso1769Init();
}


// Following section is written to use the LPCXpresso1769 Board (aka OM13085)
// --------------------------------------------------------------------------
STATIC const PINMUX_GRP_T pinmuxing[] = {								// ExpConnector Pins
	{0,  2,   IOCON_MODE_INACT | IOCON_FUNC1},	/* LPC_UART0 "Uart D" */		// J2-21
	{0,  3,   IOCON_MODE_INACT | IOCON_FUNC1},	/* LPC_UART0 "Uart D" */		// J2-22
	{2,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* LPC_UART1 "Uart C" */		// J2-42
	{2,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* LPC_UART1 "Uart C" */		// J2-43
	{0,  10,  IOCON_MODE_INACT | IOCON_FUNC1},	/* LPC_UART2 "Uart B" */		// J2-40	We use this as command line interface for cli module
	{0,  11,  IOCON_MODE_INACT | IOCON_FUNC1},	/* LPC_UART2 "Uart B" */		// J2-41
	{0,  0,   IOCON_MODE_INACT | IOCON_FUNC2},	/* LPC_UART3 "Uart A" */		// J2-9
	{0,  1,   IOCON_MODE_INACT | IOCON_FUNC2},	/* LPC_UART3 "Uart A" */		// J2-10

	{0,  22,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led red       */
	{3,  25,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led green     */
	{3,  26,  IOCON_MODE_INACT | IOCON_FUNC0},	/* Led blue      */

	{0, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* I2C0 SDA 	 */		// J2-25
	{0, 28, IOCON_MODE_INACT | IOCON_FUNC1},	/* I2C0 SCL 	 */     // J2-26

	{0, 19, IOCON_MODE_INACT | IOCON_FUNC3},	/* I2C1 SDA      */		// PAD8 	this connects to boards e2prom with address 0x50
	{0, 20, IOCON_MODE_INACT | IOCON_FUNC3},	/* I2C1 SCL      */     // PAD2

	{0, 15, IOCON_MODE_INACT | IOCON_FUNC3},	/* SCK   		 */		// J2-13	we use this to test SPI SD card slot.
	{0, 17, IOCON_MODE_INACT | IOCON_FUNC3},	/* MISO	 		 */		// J2-12
	{0, 18, IOCON_MODE_INACT | IOCON_FUNC3},	/* MOSI  		 */		// J2-11
	{0,  4, IOCON_MODE_INACT | IOCON_FUNC0}		/* P0[4]		 */		// J2-38	we use this as SD-Card Chip Select
};

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
	Chip_IOCON_DisableOD(LPC_IOCON, 0, 17);
	Chip_IOCON_DisableOD(LPC_IOCON, 0, 18);
	// SD card CS output
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 4, true);
	sd_cs_high();
//
//	   Chip_SPI_Init(LPC_SPI); 		// All default values as above, Bitrate is set to 4000000 with this!
//	    Chip_SPI_Int_Enable(LPC_SPI);
//	    //NVIC_SetPriority(SPI_IRQn, 5);
//	    NVIC_EnableIRQ(SPI_IRQn);


}
