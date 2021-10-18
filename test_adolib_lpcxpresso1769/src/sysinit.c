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
void ClimbObcEm2Init(void);

// Set up and initialize hardware prior to call to main
void SystemInit(void) {
    // Set the ARM VectorTableOffsetRegister to our Vector table (at 0x00000000 in flash)
	unsigned int *pSCB_VTOR = (unsigned int*) 0xE000ED08;
	extern void *g_pfnVectors;
	*pSCB_VTOR = (unsigned int) &g_pfnVectors;

	// Read and keep the reset source.
	resetSource = LPC_SYSCTL->RSID;

    // Switch to XTAL wait for PLL To be locked and set the CPU Core Frequency.
    Chip_SetupXtalClocking();
    //Read clock settings and update SystemCoreClock variable (needed only for iap.c module - Common FLASH support functions ...)
    SystemCoreClockUpdate();

#ifdef BOARD_CLIMB_EM2
	ClimbObcEm2Init();
#else
    LpcExpresso1769Init();
#endif
	
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

	// SSP0 Version on Pin 60-63
//	{ 0, 15, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SCK   		 */ // J2-13	we use this to test SSP SD card slot.
//	//{ 0, 16, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SSL - CS  	 */ // J2-13    the 'recommended' SSL/CS for SSP0
//	{ 0, 16, IOCON_MODE_INACT | IOCON_FUNC0 }, /* SSL - CS       */ // J2-13    the 'recommended' SSL/CS for SSP0 used as GPIO - CS has to be controlled by SW
//	{ 0, 17, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MISO	 		 */	// J2-12
//	{ 0, 18, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MOSI  		 */	// J2-11

	// SPI Version on Pin 60-63
	{ 0, 15, IOCON_MODE_INACT | IOCON_FUNC3 }, /* SCK            */ // J2-13    we use this to test SPI SD Card
    { 0, 16, IOCON_MODE_INACT | IOCON_FUNC0 }, /* SSL - CS       */ // J2-13    SPI only uses the SSL pin when in slave mode. To use as CS for Master this is normal IO
    { 0, 17, IOCON_MODE_INACT | IOCON_FUNC3 }, /* MISO           */ // J2-12
    { 0, 18, IOCON_MODE_INACT | IOCON_FUNC3 }, /* MOSI           */ // J2-11


	{ 0, 7, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SCK   		 	*/  // J2-7	SSP1
	//{ 0, 6, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SSL   		 	*/  // J2-8 SSP1    SSL/CS controlled by SSP-HW function
    { 0, 6, IOCON_MODE_INACT | IOCON_FUNC0 }, /* SSL            */  // J2-8 SSP1    SSL/CS controlled by SW.
	{ 0, 8, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MISO	 	 	*/	// J2-6	SSP1
	{ 0, 9, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MOSI  		 	*/	// J2-5	SSP1

	{ 0, 23, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP1-CS1 	*/ // J2-15
	{ 0, 24, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP1-CS2		*/ // J2-16
	{ 0, 25, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP1-CS3		*/ // J2-17
	{ 0, 26, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP0-CS1		*/ // J2-18
	{ 1, 30, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP0-CS2		*/ // J2-19
	{ 1, 31, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP0-CS3		*/ // J2-20



	{ 0, 4, IOCON_MODE_INACT | IOCON_FUNC0 } /* P0[4]		     */ // J2-38
};

// This is the Pinmux for the OBC board - EM2
STATIC const PINMUX_GRP_T pinmuxingEM2[] = {
    // Side Panel UARTS
	{ 0, 2, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART0 Tx "Uart C" !!!*/
	{ 0, 3, IOCON_MODE_INACT | IOCON_FUNC1 }, /* LPC_UART0 Rx "Uart C"    */
	{ 2, 0, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 Tx   "Uart D" !!!*/
	{ 2, 1, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 Rx   "Uart D"    */
	{ 2, 5, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART1 DTR1 "Uart D" the RS485 Direction PIN -> "Output Enable" FUNC 2*/
	{ 2, 8, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART2 Tx "Uart B" */	// This prog uses this UART as CLI !!!
	{ 2, 9, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART2 Rx "Uart B" */
	{ 0, 0, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART3 Tx "Uart A" */
	{ 0, 1, IOCON_MODE_INACT | IOCON_FUNC2 }, /* LPC_UART3 Rx "Uart A" */


#if 1
	// "I2C C/D" Side panel bus
	{ 0, 27, IOCON_MODE_INACT | IOCON_FUNC1 }, /* I2C0 SDA0 	    */
	{ 0, 28, IOCON_MODE_INACT | IOCON_FUNC1 }, /* I2C0 SCL0 	    */

	// "ONBOARD I2C"
	{ 0, 19, IOCON_MODE_INACT | IOCON_FUNC3 }, /* I2C1 SDA1         */
	{ 0, 20, IOCON_MODE_INACT | IOCON_FUNC3 }, /* I2C1 SCL1         */

	// "I2C A/B" Side panel bus
	{ 0, 10, IOCON_MODE_INACT | IOCON_FUNC2 }, /* I2C2 SDA2         */
	{ 0, 11, IOCON_MODE_INACT | IOCON_FUNC2 }, /* I2C2 SCL2         */
#else
	// "I2C C/D" Side panel bus
		{ 0, 27, IOCON_MODE_INACT | IOCON_FUNC0 }, /* I2C0 SDA0 	    */
		{ 0, 28, IOCON_MODE_INACT | IOCON_FUNC0 }, /* I2C0 SCL0 	    */

		// "ONBOARD I2C"
		{ 0, 19, IOCON_MODE_INACT | IOCON_FUNC0 }, /* I2C1 SDA1         */
		{ 0, 20, IOCON_MODE_INACT | IOCON_FUNC0 }, /* I2C1 SCL1         */

		// "I2C A/B" Side panel bus
		{ 0, 10, IOCON_MODE_INACT | IOCON_FUNC0 }, /* I2C2 SDA2         */
		{ 0, 11, IOCON_MODE_INACT | IOCON_FUNC0 }, /* I2C2 SCL2         */
#endif

	// SPI connected to SD Card slot
	{ 0, 15, IOCON_MODE_INACT | IOCON_FUNC3 }, /* SCK   		 */
	{ 0, 16, IOCON_MODE_INACT | IOCON_FUNC0 }, /* (SSL) P2[5]	 */ // SPI only uses the SSL pin when in slave mode. To use as CS for Master this is normal IO
	{ 0, 17, IOCON_MODE_INACT | IOCON_FUNC3 }, /* MISO	 		 */
	{ 0, 18, IOCON_MODE_INACT | IOCON_FUNC3 }, /* MOSI  		 */

	// SSP0 connected to MRAM01/02/03 (1/2/3)
	{ 1, 20, IOCON_MODE_INACT | IOCON_FUNC3 }, /* SCK0           */
	{ 1, 23, IOCON_MODE_INACT | IOCON_FUNC3 }, /* MISO0          */
	{ 1, 24, IOCON_MODE_INACT | IOCON_FUNC3 }, /* MOSI0          */
	// CS for MRAM 01/02/03
    { 0, 22, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP0-CS1 */
    { 2, 11, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP0-CS2 */
    { 2, 12, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP0-CS3 */

	// SSP1 connected to MRAM11/12/13 (4/5/6)
    { 0,  7, IOCON_MODE_INACT | IOCON_FUNC2 }, /* SCK1           */
    { 0,  8, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MISO1          */
    { 0,  9, IOCON_MODE_INACT | IOCON_FUNC2 }, /* MOSI1          */
    // CS for MRAMs 11/12/13
    { 2,  2, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP1-CS1     */
    { 0,  4, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP1-CS2     */
    { 1, 10, IOCON_MODE_INACT | IOCON_FUNC0 }, /* MRAM CS  SSP1-CS3     */

	// GPIOs
	{ 1, 18, IOCON_MODE_INACT | IOCON_FUNC0 }, /* Watchdog Feed    */
	{ 2, 6, IOCON_MODE_INACT | IOCON_FUNC0 },  /* OBC LED    */
	{ 2, 4, IOCON_MODE_INACT | IOCON_FUNC0 },  /* SP-VCC-FAULT (Input)  */
	{ 3, 25, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Supply Rail Indicator (Input)  */
	{ 3, 26, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Boot-loader selection (Input)  */
	{ 0, 21, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Remove before flight (Input)  */
	{ 4, 29, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Ext. WDT triggered latch (Input)  */
	{ 1, 9, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Clear ext. WDT latch  */
	{ 1, 26, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Thruster latchup (Input)  */
	{ 1, 19, IOCON_MODE_INACT | IOCON_FUNC0 },  /* Thruster chip select  */

	// Power supply
	{ 1, 28, IOCON_MODE_INACT | IOCON_FUNC0 },  /* SP-A Supply Enable   */
	{ 2, 7, IOCON_MODE_INACT | IOCON_FUNC0 },   /* SP-B Supply Enable   */
	{ 1, 15, IOCON_MODE_INACT | IOCON_FUNC0 },  /* SP-D Supply Enable   */
	{ 1, 22, IOCON_MODE_INACT | IOCON_FUNC0 },  /* SP-D Supply Enable   */
	{ 4, 28, IOCON_MODE_INACT | IOCON_FUNC0 },  /* SD Supply Enable     */
	{ 0, 29, IOCON_MODE_INACT | IOCON_FUNC0 },  /* OBC Supply disable (not connected in EM2) */

	// I2C Routing
	{ 0, 30, IOCON_MODE_INACT | IOCON_FUNC0 },  /* I2C-A EN    */
	{ 2, 3, IOCON_MODE_INACT | IOCON_FUNC0 },  /* I2C-B EN    */
	{ 1, 4, IOCON_MODE_INACT | IOCON_FUNC0 },  /* I2C-C EN    */
	{ 1, 1, IOCON_MODE_INACT | IOCON_FUNC0 },  /* I2C-D EN    */

	// ADC
	{ 0,23 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Main supply current (Analog Input 0)  */
	{ 0,24 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Sidpanel supply current (Analog Input 1)  */
	{ 0,25 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Temperature (Analog Input 2)  */
	{ 0,26 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Supply Voltage (Analog Input 3)  */

	{ 1, 27, IOCON_MODE_INACT | IOCON_FUNC1},	/* CLKOUT */


};

void ClimbObcEm2Init(void) {
    // Pin Config
    Chip_IOCON_SetPinMuxing(LPC_IOCON, pinmuxingEM2, sizeof(pinmuxingEM2) / sizeof(PINMUX_GRP_T));
	Chip_GPIO_Init(LPC_GPIO);

	/* --- SPI pins no open drain --- */
		Chip_IOCON_DisableOD(LPC_IOCON, 0, 15);
		Chip_IOCON_DisableOD(LPC_IOCON, 0, 16);
		Chip_IOCON_DisableOD(LPC_IOCON, 0, 17);
		Chip_IOCON_DisableOD(LPC_IOCON, 0, 18);

	// SD CS
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 16, true);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 16);

	// Watchdog feed
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 18, true);
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 18);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 18);

	/* OBC LED    */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 6, true);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 6);


// GPIOs
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 4, false);   /* SP-VCC-FAULT (Input)  */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 25, false);  /* Supply Rail Indicator (Input)  */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 3, 26, false);   /* Boot-loader selection (Input)  */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 21, false);   /* Remove before flight (Input)  */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 4, 29, false);   /* Ext. WDT triggered latch (Input)  */

	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 9, true);   	/* Clear ext. WDT latch  */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 9);   		/* WDT latch set to high   */

	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 26, false);   /* Thruster latchup (Input)  */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 19, true);   /* Thruster chip select  */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 19);   	/* Unselect thruster   */

	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 30, true); /* I2C-A EN    */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 3, true);  /* I2C-B EN    */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 4, true);  /* I2C-C EN    */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 1, true);  /* I2C-D EN    */

	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 30); /* I2C-A EN    */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 3);  /* I2C-B EN    */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 4);  /* I2C-C EN    */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 1);  /* I2C-D EN    */


	// Power supply
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 28, true);   /* SP-A Supply Enable   */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 7, true);    /* SP-B Supply Enable   */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 15, true);   /* SP-D Supply Enable   */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 22, true);   /* SP-D Supply Enable   */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 4, 28, true);   /* SD Supply Enable     */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 29, true);   /* OBC Supply disable  */

	//Enable supply
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 28);   /* SP-A Supply Enable   */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 2, 7);    /* SP-B Supply Enable   */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 15);   /* SP-D Supply Enable   */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 1, 22);   /* SP-D Supply Enable   */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 4, 28);   /* SD Supply Enable     */
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 29);   /* OBC Supply disable  */


	// CS for MRAM 01/02/03
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 22, true); /* MRAM CS  SSP0-CS1 */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 11, true); /* MRAM CS  SSP0-CS2 */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2, 12, true); /* MRAM CS  SSP0-CS3 */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 22); /* MRAM CS  SSP0-CS1 */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 11); /* MRAM CS  SSP0-CS2 */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2, 12); /* MRAM CS  SSP0-CS3 */

	    // CS for MRAMs 11/12/13
	Chip_GPIO_WriteDirBit(LPC_GPIO, 2,  2, true); /* MRAM CS  SSP1-CS1     */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 0,  4, true); /* MRAM CS  SSP1-CS2     */
	Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 10, true); /* MRAM CS  SSP1-CS3     */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 2,  2); /* MRAM CS  SSP1-CS1     */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0,  4); /* MRAM CS  SSP1-CS2     */
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 10); /* MRAM CS  SSP1-CS3     */



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

	Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 6, true);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 6);

    Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 23, true);
    Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 23);
    Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 24, true);
    Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 24);
    Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 25, true);
    Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 25);
    Chip_GPIO_WriteDirBit(LPC_GPIO, 0, 26, true);
    Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 26);
    Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 30, true);
     Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 30);
     Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 31, true);
      Chip_GPIO_SetPinOutHigh(LPC_GPIO, 1, 31);


	
}
