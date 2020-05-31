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
	ADO_SDC_CARDSTATUS_ERROR = 99
} ado_sdc_status_t;



void SdcInitCmd(int argc, char *argv[]);
void SdcPowerupCmd(int argc, char *argv[]);
void SdcInitStep2(void);
void SdcInitStep3(void);
void SdcInitStep4(void);


static ado_sspid_t 		sdcBusNr = -1;
static ado_sdc_status_t sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
static bool				sdcCmdPending = false;
static uint16_t			sdcWaitLoops = 0;
static uint8_t 			sdcCmdData[60];
static uint8_t 			sdcCmdResponse[30];



void SdcInit(ado_sspid_t bus) {
	sdcBusNr = bus;

	CliRegisterCommand("sdcInit", SdcInitCmd);
	CliRegisterCommand("sdcPow", SdcPowerupCmd);

}

void SdcMain(void) {
	if (!sdcCmdPending) {
		switch (sdcStatus) {
		case ADO_SDC_CARDSTATUS_INIT_RESET:
			if (sdcCmdResponse[1] == 0x01) {
				SdcInitStep2();
			} else {
				printf("init reset Fehler\n");
				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;
		case ADO_SDC_CARDSTATUS_INIT_CMD8:
			if (sdcCmdResponse[1] == 0x01) {
				SdcInitStep3();
//				printf("CMD8 accepted.\n");
//				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			} else {
				printf("CMD8 rejected.\n");
				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
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
					//Init sequence is not finished yet.
					// Lets wait some main.loops and then retry the ACMD41
					sdcWaitLoops = 100;
					sdcStatus = ADO_SDC_CARDSTATUS_INIT_ACMD41_3;
					printf(".");
				} else if (sdcCmdResponse[1] == 0x00) {
					printf("Init Step finished\n");
					sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
				} else {
					printf("No Valid Answer to ACMD41\n");
					sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
				}
				break;
		case ADO_SDC_CARDSTATUS_INIT_ACMD41_3:
			if (sdcWaitLoops > 0) {
				sdcWaitLoops--;
				if (sdcWaitLoops == 0) {
					// Rerty the ACMD41
					SdcInitStep3();
				}
			}

		default:
			break;
		}
	}
//	for (int i=0; i<16; i++) {
//		if (cmd2finished[i] == true) {
//			cmd2finished[i] = false;
//			changed = true;
//			if (cmd2result[i] == true) {
//				res[i] = 'O';
//			} else {
//				res[i] = 'x';
//			}
//		}
//	}
//	if (changed) {
//		changed = false;
//		printf("R: %s\n", res);
//	}
}


void DMAFinishedIRQ(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize) {
	if (jobStatus == ADO_SSP_JOBDONE) {
		sdcStatus = context;
	} else {
		sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
	}
	sdcCmdPending = false;

//	if (context<16) {
//		cmd2finished[context] = true;
//		if (rxData[1] == 0x01) {
//			cmd2result[context] = true;
//		} else {
//			cmd2result[context] = false;
//		}
//	}
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
		sdcCmdData[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
		sdcCmdData[1] = 0;
		sdcCmdData[2] = 0;
		sdcCmdData[3] = 0;
		sdcCmdData[4] = 0;
		sdcCmdData[5] = 0x95;

		sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
		sdcCmdPending = true;
		ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_RESET, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 3, DMAFinishedIRQ, 0);
	}
}

void SdcInitStep2(void) {
	// Send CMD8
	sdcCmdData[0] = 0x40 | SEND_IF_COND;		// CMD consists of 6 bytes to send.sdc
	sdcCmdData[1] = 0x00;
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x01;
	sdcCmdData[4] = 0xAA;
	sdcCmdData[5] = 0x87;

	sdcCmdPending = true;
	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_CMD8, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 7, DMAFinishedIRQ, 0);
}

void SdcInitStep3(void) {
	// Send CMD55 - ACMD41
	sdcCmdData[0] = 0x40 | APP_CMD;
	sdcCmdData[1] = 0x00;
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x00;
	sdcCmdData[4] = 0x00;
	sdcCmdData[5] = 0xFF;
//	sdcCmdData[6] = 0x40 | SD_SEND_OP_COND;
//	sdcCmdData[7] = 0x00;
//	sdcCmdData[8] = 0x00;
//	sdcCmdData[9] = 0x01;
//	sdcCmdData[10] = 0xAA;
//	sdcCmdData[11] = 0xFF;

	sdcCmdPending = true;
	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_ACMD41_1, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 3, DMAFinishedIRQ, 0);
}

void SdcInitStep4(void) {
	// Send CMD55 - ACMD41
	sdcCmdData[0] = 0x40 | SD_SEND_OP_COND;
	sdcCmdData[1] = 0x40;
	sdcCmdData[2] = 0x00;
	sdcCmdData[3] = 0x00;
	sdcCmdData[4] = 0x00;
	sdcCmdData[5] = 0xFF;

	sdcCmdPending = true;
	ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_INIT_ACMD41_2, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , 7, DMAFinishedIRQ, 0);
}


//
//static bool cmd2finished[16];
//static bool cmd2result[16];
//static char res[] = "................";
//static bool changed = false;
//uint8_t rxData[16][64];
//
//void SdcWithDMACmd2(int argc, char *argv[]) {
//	uint8_t dataCntToSend = 6;
//	uint8_t dataCntToReceive = 10;
//
//	sdcCmdData[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
//	sdcCmdData[1] = 0;
//	sdcCmdData[2] = 0;
//	sdcCmdData[3] = 0;
//	sdcCmdData[4] = 0;
//	sdcCmdData[5] = 0x95;
//
//
//	for (int i=0;i<16;i++) {	// test fill up the queue
//		cmd2finished[i] = false;
//		cmd2result[i] = false;
//		res[i] = '.';
//		for (int j =0; j< 64; j++) {
//			rxData[i][j] = j;
//		}
//	}
//	for (int i=0;i<16;i++) {	// test fill up the queue
//		ADO_SSP_AddJob(i, sdcBusNr, sdcCmdData, &(rxData[i][0]), dataCntToSend, dataCntToReceive, DMAFinishedIRQ, 0);
//	}
//
//
//}


