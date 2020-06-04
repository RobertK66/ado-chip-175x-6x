/*
 * ado_sdcard.c
 *
 *  Created on: 16.05.2020
 *      Author: Robert
 */
#include "ado_sdcard.h"

#include <stdio.h>
#include <stdlib.h>
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
	ADO_SDC_CARDSTATUS_READ_SBCMD,
	ADO_SDC_CARDSTATUS_READ_SBWAITDATA,
	ADO_SDC_CARDSTATUS_READ_SBDATA,
	ADO_SDC_CARDSTATUS_ERROR = 999
} ado_sdc_status_t;

typedef enum ado_sdc_cardtype_e
{
	ADO_SDC_CARD_UNKNOWN = 0,
	ADO_SDC_CARD_1XSD,				// V1.xx Card (not accepting CMD8 -> standard capacity
	ADO_SDC_CARD_20SD,				// V2.00 Card standard capacity
	ADO_SDC_CARD_20HCXC,			// V2.00 Card High Capacity HC or ExtendenCapacity XC
} ado_sdc_cardtype_t;


void DMAFinishedIRQ(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize);
void SdcPowerupCmd(int argc, char *argv[]);
void SdcInitCmd(int argc, char *argv[]);
void SdcReadCmd(int argc, char *argv[]);
void SdcInitStep2(void);
void SdcInitStep3(void);
void SdcInitStep4(void);
void SdcInitStep5(void);

static ado_sspid_t 		sdcBusNr = -1;
static ado_sdc_status_t sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
static bool				sdcCmdPending = false;
static uint16_t			sdcWaitLoops = 0;
static uint16_t			sdcDumpLines = 0;
static uint8_t 			sdcCmdData[10];
static uint8_t 			sdcCmdResponse[1024];

static ado_sdc_cardtype_t sdcType = ADO_SDC_CARD_UNKNOWN;


void SdcInit(ado_sspid_t bus) {
	sdcBusNr = bus;

	CliRegisterCommand("sdcPow", SdcPowerupCmd);
	CliRegisterCommand("sdcInit", SdcInitCmd);
	CliRegisterCommand("sdcRead", SdcReadCmd);
}

void SdcMain(void) {

	if (!sdcCmdPending) {
		// This is the sdc state engine reacting to a finished SPI job
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
			break;

		case ADO_SDC_CARDSTATUS_READ_SBCMD:
			if ((sdcCmdResponse[1] == 0x00) || (sdcCmdResponse[2] == 0x00))  {
				// Lets wait for the start data token.
				sdcCmdPending = true;
				ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_READ_SBWAITDATA, sdcBusNr, sdcCmdData, sdcCmdResponse, 0 , 1, DMAFinishedIRQ, 0);
			} else {
				printf("Error with read block command\n");
				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_READ_SBWAITDATA:
			if (sdcCmdResponse[0] == 0xFE) {
				// Now we can read all data bytes
				sdcCmdPending = true;
				ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_READ_SBDATA, sdcBusNr, sdcCmdData, sdcCmdResponse, 0 , 514, DMAFinishedIRQ, 0);
			} else {
				// TODO: Hier ein bisschen Zeit / Hauptschleifen vergeuden und dann erst wieder auf Token abfragen!?
				sdcCmdPending = true;
				ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_READ_SBWAITDATA, sdcBusNr, sdcCmdData, sdcCmdResponse, 0 , 1, DMAFinishedIRQ, 0);
			}
			break;


		case ADO_SDC_CARDSTATUS_READ_SBDATA:
		{
			sdcStatus = ADO_SDC_CARDSTATUS_INITIALIZED;
			sdcDumpLines = 16;
			printf("Data received:\n");
			break;
		}

		// Nothing needed here - only count down the main loop timer
		case ADO_SDC_CARDSTATUS_INITIALIZED:
			if (sdcWaitLoops > 0) {
				sdcWaitLoops--;
			}
		default:
			break;
		}

		// Now lets do other stuff in main loop
		if (sdcWaitLoops == 0) {
			// Do other stuff in Mainloop
			if (sdcDumpLines > 0) {
				uint8_t *ptr = &sdcCmdResponse[32*(16-sdcDumpLines)];
				char hex[100];
				char asci[33];
				for (int i=0;i<32;i++) {
					char c = *(ptr+i);
					sprintf(&hex[i*3],"%02X ", c);
					if (c > 0x20) {
						asci[i] = c;
					} else {
						asci[i] = '.';
					}
				}
				asci[32] = 0;
				hex[96] = 0;
				printf("%s %s\n", hex, asci);
				sdcWaitLoops = 1000;
				sdcDumpLines--;
			}
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

void SdcReadCmd(int argc, char *argv[]){
	uint32_t adr = 0;
	if (argc > 0) {
		adr = atoi(argv[0]);
	}
	adr = adr << 9;


	if (sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
		sdcCmdData[0] = 0x40 | READ_SINGLE_BLOCK;
		sdcCmdData[1] = (uint8_t)(adr>>24);
		sdcCmdData[2] = (uint8_t)(adr>>16);
		sdcCmdData[3] = (uint8_t)(adr>>8);
	    sdcCmdData[4] = (uint8_t)(adr);
		sdcCmdData[5] = 0xFF;

		sdcCmdPending = true;
		ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_READ_SBCMD, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 3, DMAFinishedIRQ, 0);
	} else {
		printf("Card not ready to receive commands....");
	}
}

