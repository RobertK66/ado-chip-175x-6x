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

#include "ado_sspdma.h"

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

typedef enum ado_sdc_status_e
{
	ADO_SDC_CARDSTATUS_UNDEFINED = 0,
	ADO_SDC_CARDSTATUS_INIT_RESET,
	ADO_SDC_CARDSTATUS_INIT_CMD8,
	ADO_SDC_CARDSTATUS_INIT_ACMD41_1,
	ADO_SDC_CARDSTATUS_INIT_ACMD41_2,
	ADO_SDC_CARDSTATUS_INIT_ACMD41_3,
	ADO_SDC_CARDSTATUS_INIT_READ_OCR,
	ADO_SDC_CARDSTATUS_INITIALIZED,
	ADO_SDC_CARDSTATUS_ERROR = 999
} ado_sdc_status_t;

typedef enum ado_sdc_cardtype_e
{
	ADO_SDC_CARD_UNKNOWN = 0,
	ADO_SDC_CARD_1XSD,				// V1.xx Card (not accepting CMD8 -> standard capacity
	ADO_SDC_CARD_20SD,				// V2.00 Card standard capacity
	ADO_SDC_CARD_20HCXC,			// V2.00 Card High Capacity HC or ExtendenCapacity XC
} ado_sdc_cardtype_t;



void SdcInitCmd(int argc, char *argv[]);
void SdcPowerupCmd(int argc, char *argv[]);
void SdcInitStep2(void);
void SdcInitStep3(void);
void SdcInitStep4(void);
void SdcInitStep5(void);

static ado_sspid_t 		sdcBusNr = -1;
static ado_sdc_status_t sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
static bool				sdcCmdPending = false;
static uint16_t			sdcWaitLoops = 0;
static uint8_t 			sdcCmdData[10];
static uint8_t 			sdcCmdResponse[10];

static ado_sdc_cardtype_t sdcType = ADO_SDC_CARD_UNKNOWN;


void SdcInit(ado_sspid_t bus) {
	sdcBusNr = bus;

	CliRegisterCommand("sdcPow", SdcPowerupCmd);
	CliRegisterCommand("sdcInit", SdcInitCmd);
}

void SdcMain(void) {
	if (!sdcCmdPending) {
		switch (sdcStatus) {
		case ADO_SDC_CARDSTATUS_INIT_RESET:
			if (sdcCmdResponse[1] == 0x01) {
				SdcInitStep2();
			} else {
				printf("CMD GO_IDLE error.\n");
				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;
		case ADO_SDC_CARDSTATUS_INIT_CMD8:
			if (sdcCmdResponse[1] == 0x01) {
				sdcType = ADO_SDC_CARD_20SD;
				SdcInitStep3();
			} else {
				sdcType = ADO_SDC_CARD_1XSD;
			}
			break;
		case ADO_SDC_CARDSTATUS_INIT_ACMD41_1:
				if (sdcCmdResponse[1] == 0x01) {
					SdcInitStep4();
				} else {
					printf("CMD55 rejected\n");
					sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
				}
				break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_2:
				if (sdcCmdResponse[1] == 0x01) {
					// Init sequence is not finished yet(idle mode).
					// Lets wait some main loops and then retry the ACMD41
					sdcWaitLoops = 100;
					sdcStatus = ADO_SDC_CARDSTATUS_INIT_ACMD41_3;
					printf(".");
				} else if (sdcCmdResponse[1] == 0x00) {
					printf("\n");
					SdcInitStep5();
				} else {
					printf("Errors %02X to ACMD41\n", sdcCmdResponse[1]);
					sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
				}
				break;

		case ADO_SDC_CARDSTATUS_INIT_READ_OCR:
				if (sdcCmdResponse[1] == 0x00) {
					if ((sdcType == ADO_SDC_CARD_20SD) &&
						((sdcCmdResponse[2] & 0x40) != 0)) {
						sdcType = ADO_SDC_CARD_20HCXC;
					}
					sdcStatus = ADO_SDC_CARDSTATUS_INITIALIZED;
					printf("Card (type %d) initialized.\n", sdcType);
				}
				break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_3:
			if (sdcWaitLoops > 0) {
				sdcWaitLoops--;
				if (sdcWaitLoops == 0) {
					// Retry the ACMD41
					SdcInitStep3();
				}
			}

		// Nothing needed here
		case ADO_SDC_CARDSTATUS_INITIALIZED:
		default:
			break;
		}
	}
}

void DMAFinishedIRQ(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize) {
	if (jobStatus == ADO_SSP_JOBDONE) {
		sdcStatus = context;
	} else {
		sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
	}
	sdcCmdPending = false;
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
}

void SdcPowerupCmd(int argc, char *argv[]) {
	LPC_SSP_T *sspBase = 0;
	if (sdcBusNr == ADO_SSP0) {
		sspBase = LPC_SSP0;
	} else if  (sdcBusNr == ADO_SSP0) {
		sspBase = LPC_SSP1;
	}
	if (sspBase != 0) {
		uint8_t dummy; (void)dummy;
		// Make 80 Clock without CS
		for (int i=0;i<10;i++) {
			sspBase->DR = 0x55;
			dummy = sspBase->SR;
			dummy = sspBase->DR; // Clear Rx buffer.
		}
		{ uint32_t temp; (void)temp; while ((sspBase->SR & SSP_STAT_RNE) != 0) { temp = sspBase->DR; } }
	}
}


void SdcInitCmd(int argc, char *argv[]) {
	LPC_SSP_T *sspBase = 0;
	if (sdcBusNr == ADO_SSP0) {
		sspBase = LPC_SSP0;
	} else if  (sdcBusNr == ADO_SSP0) {
		sspBase = LPC_SSP1;
	}
	if (sspBase != 0) {

		// Now send Command GO_IDLE_STATE
		sdcCmdData[0] = 0x40 | GO_IDLE_STATE;	// CMD0 consists of 6 bytes to send.sdc
		sdcCmdData[1] = 0;
		sdcCmdData[2] = 0;
		sdcCmdData[3] = 0;
		sdcCmdData[4] = 0;
		sdcCmdData[5] = 0x95;

		sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
		sdcType = ADO_SDC_CARD_UNKNOWN;
		sdcCmdPending = true;
		ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_RESET, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 3, DMAFinishedIRQ, 0);
	}
}

void SdcInitStep2(void) {
	// Send CMD8
	sdcCmdData[0] = 0x40 | SEND_IF_COND;		// CMD8 consists of 6 bytes to send.sdc
	sdcCmdData[1] = 0x00;
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x01;
	sdcCmdData[4] = 0xAA;
	sdcCmdData[5] = 0x87;

	sdcCmdPending = true;
	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_CMD8, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 7, DMAFinishedIRQ, 0);
}

void SdcInitStep3() {
	// Send CMD55 - ACMD41
	sdcCmdData[0] = 0x40 | APP_CMD;
	sdcCmdData[1] = 0x00;
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x00;
	sdcCmdData[4] = 0x00;
	sdcCmdData[5] = 0xFF;

	sdcCmdPending = true;
	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_ACMD41_1, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 3, DMAFinishedIRQ, 0);
}

void SdcInitStep4(void) {
	// Send CMD55 - ACMD41
	sdcCmdData[0] = 0x40 | SD_SEND_OP_COND;
	if (sdcType == ADO_SDC_CARD_1XSD) {
		sdcCmdData[1] = 0x00;		// Its an older card. HCS not to be used.
	} else {
		sdcCmdData[1] = 0x40;		// We can activate Host - HCS mode for cards V2.0 or higher.
	}
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x00;
	sdcCmdData[4] = 0x00;
	sdcCmdData[5] = 0xFF;

    sdcCmdPending = true;
    Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4);

	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_ACMD41_2, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 3, DMAFinishedIRQ, 0);
}


void SdcInitStep5() {
	// Send CMD58 to get R3 - OCR Response
	sdcCmdData[0] = 0x40 | READ_OCR;
	sdcCmdData[1] = 0x00;
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x00;
	sdcCmdData[4] = 0x00;
	sdcCmdData[5] = 0xFF;

	sdcCmdPending = true;
	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_READ_OCR, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 7, DMAFinishedIRQ, 0);
}

