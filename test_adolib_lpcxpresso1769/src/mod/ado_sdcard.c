/*
 * ado_sdcard.c
 *
 *  Created on: 16.05.2020
 *      Author: Robert
 */
#include "ado_sdcard.h"

#include <stdio.h>
#include <string.h>
#include <mod/cli.h>


//SD commands, many of these are not used here
#define GO_IDLE_STATE            0
#define SEND_OP_COND             1
#define SEND_IF_COND			 8
#define SEND_CSD                 9
#define STOP_TRANSMISSION        12
#define SEND_STATUS              13
#define SET_BLOCK_LEN            16
#define READ_SINGLE_BLOCK        17
#define READ_MULTIPLE_BLOCKS     18
#define WRITE_SINGLE_BLOCK       24
#define WRITE_MULTIPLE_BLOCKS    25
#define ERASE_BLOCK_START_ADDR   32
#define ERASE_BLOCK_END_ADDR     33
#define ERASE_SELECTED_BLOCKS    38
#define SD_SEND_OP_COND			 41   //ACMD
#define APP_CMD					 55
#define READ_OCR				 58
#define CRC_ON_OFF               59


#define SDC_CS_PORT				0
#define SDC_CS_PIN				4



void SdcMyFirstCmd(int argc, char *argv[]);
static ssp_busnr_t sdcBusNr = -1;
//volatile bool card_busy;


void SdcInit(ssp_busnr_t bus) {
	sdcBusNr = bus;

//	Chip_IOCON_PinMuxSet(LPC_IOCON, SDC_CS_PORT, SDC_CS_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
//	Chip_IOCON_DisableOD(LPC_IOCON, SDC_CS_PORT, SDC_CS_PIN);
//	Chip_GPIO_SetPinState(LPC_GPIO, SDC_CS_PORT, SDC_CS_PIN, true);

	CliRegisterCommand("sdc", SdcMyFirstCmd);
}

//
// Be careful here! This Callbacks are called from IRQ !!!
// Do not do any complicated logic here!!!
// Transfer all things to be done to next mainloop....
bool SdcChipSelect(bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, SDC_CS_PORT, SDC_CS_PIN, !select);
//	if (!select) {
//		card_busy = false;
//	}
	return true;
}


void SdcMyFirstCmd(int argc, char *argv[]) {
	printf("SDCard on Bus %d\n",sdcBusNr);
	//DumpSspJobs(sdcBusNr);

	uint8_t tx[6];
	uint8_t rx[2];
	uint8_t *job_status = NULL;
	volatile uint32_t helper;

	if((argc > 0) && strcmp(argv[0],"po") == 0) {
		/* prepare (after power up with 80 clock without CS!!!! */
		for (int i = 0; i<10; i++){
			LPC_SSP0->DR = 0xFF;
		}
		printf("Power On.\n");
		return;
	}


	/* Read flash ID register */
	tx[0] = 0x40 | GO_IDLE_STATE;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 0;
	tx[4] = 0;
	tx[5] = 0x95;		// dummy CRC"!?

	rx[0] = 0x33;
	rx[1] = 0x44;
	rx[2] = 0x44;


	if (ssp_add_job2(sdcBusNr , tx, 6, rx, 3, &job_status, SdcChipSelect))
	{
		printf("Error1");
	}

	helper = 0;
	while ((*job_status != SSP_JOB_STATE_DONE) && (helper < 1000000))
	{
		/* Wait for job to finish */
		helper++;
	}

	if (rx[0] != 0x01)
	{
		/* Error - Flash could not be accessed */
		printf("Error2 %02X %02X %02X", rx[0], rx[1], rx[2]);
	}

}
