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

typedef enum ado_sdc_cmd_e
{
 ADO_SDC_CMD0_GO_IDLE_STATE   		  =  0,
 ADO_SDC_CMD1_SEND_OP_COND            =  1,
 ADO_SDC_CMD8_SEND_IF_COND			  =  8,
 ADO_SDC_CMD9_SEND_CSD                =  9,
 ADO_SDC_CMD12_STOP_TRANSMISSION      =  12,
 ADO_SDC_CMD13_SEND_STATUS            =  13,
 ADO_SDC_CMD16_SET_BLOCK_LEN          =  16,
 ADO_SDC_CMD17_READ_SINGLE_BLOCK      =  17,
 ADO_SDC_CMD18_READ_MULTIPLE_BLOCKS   =  18,
 ADO_SDC_CMD24_WRITE_SINGLE_BLOCK     =  24,
 ADO_SDC_CMD25_WRITE_MULTIPLE_BLOCKS  =  25,
 ADO_SDC_CMD32_ERASE_BLOCK_START_ADDR =  32,
 ADO_SDC_CMD33_ERASE_BLOCK_END_ADDR   =  33,
 ADO_SDC_CMD38_ERASE_SELECTED_BLOCKS  =  38,
 ADO_SDC_CMD41_SD_SEND_OP_COND		  =  41,   //ACMD41
 ADO_SDC_CMD55_APP_CMD				  =  55,
 ADO_SDC_CMD58_READ_OCR				  =  58,
 ADO_SDC_CMD59_CRC_ON_OFF 			  =  59
} ado_sdc_cmd_t;


//#define SDC_CS_PORT				0
//#define SDC_CS_PIN				4

typedef enum ado_sdc_status_e
{
	ADO_SDC_CARDSTATUS_UNDEFINED = 0,
	ADO_SDC_CARDSTATUS_INIT_RESET,
	ADO_SDC_CARDSTATUS_INIT_RESET2,
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
void SdcSendCommand(ado_sdc_cmd_t cmd, uint32_t jobContext, uint32_t arg);

void SdcPowerupCmd(int argc, char *argv[]);
void SdcInitCmd(int argc, char *argv[]);
void SdcReadCmd(int argc, char *argv[]);
//void SdcInitStep2(void);
//void SdcInitStep3(void);
//void SdcInitStep4(void);
//void SdcInitStep5(void);

static ado_sspid_t 		sdcBusNr = -1;
static ado_sdc_status_t sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
static bool				sdcCmdPending = false;
static uint16_t			sdcWaitLoops = 0;
static uint16_t			sdcDumpLines = 0;
static uint8_t 			sdcCmdData[10];
static uint8_t 			sdcCmdResponse[1024];
static uint32_t 		curBlockNr;

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
				// Send CMD8
				SdcSendCommand(ADO_SDC_CMD8_SEND_IF_COND, ADO_SDC_CARDSTATUS_INIT_CMD8, 0x000001AA);
			} else {
				// Try a 2nd time
				// All my SDHD cards switch to SPI mode with this repeat - regardless of power up sequence!
				SdcSendCommand(ADO_SDC_CMD0_GO_IDLE_STATE, ADO_SDC_CARDSTATUS_INIT_RESET2, 0);
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_RESET2:
				if (sdcCmdResponse[1] == 0x01) {
					// Send CMD8
					SdcSendCommand(ADO_SDC_CMD8_SEND_IF_COND, ADO_SDC_CARDSTATUS_INIT_CMD8, 0x000001AA);
				} else {
					printf("CMD GO_IDLE error.\n");
					sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
				}
				break;

		case ADO_SDC_CARDSTATUS_INIT_CMD8:
			if (sdcCmdResponse[1] == 0x01) {
				sdcType = ADO_SDC_CARD_20SD;
				// Send CMD55 (-> ACMD41)
				SdcSendCommand(ADO_SDC_CMD55_APP_CMD, ADO_SDC_CARDSTATUS_INIT_ACMD41_1, 0);
			} else {
				sdcType = ADO_SDC_CARD_1XSD;
				// TODO: init not ready here ....
				printf("d Card version not implemented yet.\n");
				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_1:
			if (sdcCmdResponse[1] == 0x01) {
				// Send (CMD55 - ) ACMD41
				if (sdcType == ADO_SDC_CARD_1XSD) {
					SdcSendCommand(ADO_SDC_CMD41_SD_SEND_OP_COND, ADO_SDC_CARDSTATUS_INIT_ACMD41_2, 0);
				} else {
					SdcSendCommand(ADO_SDC_CMD41_SD_SEND_OP_COND, ADO_SDC_CARDSTATUS_INIT_ACMD41_2, 0x40000000);
				}
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
				// Send CMD58 to get R3 - OCR Response
				SdcSendCommand(ADO_SDC_CMD58_READ_OCR, ADO_SDC_CARDSTATUS_INIT_READ_OCR, 0);
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
			} else {
				printf("Errors %02X reading OCR.\n", sdcCmdResponse[1]);
				sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_3:
			if (sdcWaitLoops > 0) {
				sdcWaitLoops--;
				if (sdcWaitLoops == 0) {
					// Retry the ACMD41
					// Send CMD55 (-> ACMD41)
					SdcSendCommand(ADO_SDC_CMD55_APP_CMD, ADO_SDC_CARDSTATUS_INIT_ACMD41_1, 0);
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
				ADO_SSP_AddJob(ADO_SDC_CARDSTATUS_READ_SBDATA, sdcBusNr, sdcCmdData, sdcCmdResponse, 0 , 515, DMAFinishedIRQ, 0);
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
			printf("\nData received Block %d (0x%08X):\n", curBlockNr, curBlockNr);
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
				sdcWaitLoops = 3000;
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

void SdcSendCommand(ado_sdc_cmd_t cmd, uint32_t jobContext, uint32_t arg) {
	int responseSize = 3;

	sdcCmdData[0] = 0x40 | cmd;
	sdcCmdData[1] = (uint8_t)(arg>>24);
	sdcCmdData[2] = (uint8_t)(arg>>16);
	sdcCmdData[3] = (uint8_t)(arg>>8);
	sdcCmdData[4] = (uint8_t)(arg);
	sdcCmdData[5] = 0xFF;

	// This 2 cmds need correct crc
	if (cmd == ADO_SDC_CMD0_GO_IDLE_STATE) {
		sdcCmdData[5] = 0x95;
	} else if (cmd == ADO_SDC_CMD8_SEND_IF_COND) {
		sdcCmdData[5] = 0x87;
		responseSize = 7;
	} else if (cmd ==  ADO_SDC_CMD58_READ_OCR) {
		responseSize = 7;
	}

	sdcCmdPending = true;
	ADO_SSP_AddJob(jobContext, sdcBusNr, sdcCmdData, sdcCmdResponse, 6 , responseSize, DMAFinishedIRQ, 0);
}

void SdcInitCmd(int argc, char *argv[]) {
	LPC_SSP_T *sspBase = 0;
	if (sdcBusNr == ADO_SSP0) {
		sspBase = LPC_SSP0;
	} else if  (sdcBusNr == ADO_SSP0) {
		sspBase = LPC_SSP1;
	}
	if (sspBase != 0) {
		sdcType = ADO_SDC_CARD_UNKNOWN;
		sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
		SdcSendCommand(ADO_SDC_CMD0_GO_IDLE_STATE, ADO_SDC_CARDSTATUS_INIT_RESET, 0);
	}
}



void SdcReadCmd(int argc, char *argv[]){
	uint32_t adr = 0;
	if (argc > 0) {
		//adr = atoi(argv[0]);
		curBlockNr = strtol(argv[0], NULL, 0);
	}
	adr = curBlockNr << 9;


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
		printf("Card not ready to receive commands....\n");
	}
}

