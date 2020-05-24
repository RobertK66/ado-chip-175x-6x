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

void SdcWithDMACmd(int argc, char *argv[]);
void SdcWithDMACmd2(int argc, char *argv[]);


static ado_sspid_t sdcBusNr = -1;
//volatile bool card_busy;


void SdcInit(ado_sspid_t bus) {
	sdcBusNr = bus;

//	Chip_GPDMA_Init(LPC_GPDMA);
//	NVIC_EnableIRQ (DMA_IRQn);

	CliRegisterCommand("dma", SdcWithDMACmd);
	CliRegisterCommand("dma2", SdcWithDMACmd2);

}

static bool cmd2finished[16];
static bool cmd2result[16];
static char res[] = "................";
static bool changed = false;

uint8_t rxData[16][64];

void SdcMain(void) {

	for (int i=0; i<16; i++) {
		if (cmd2finished[i] == true) {
			cmd2finished[i] = false;
			changed = true;
			if (cmd2result[i] == true) {
				res[i] = 'O';
			} else {
				res[i] = 'x';
			}
		}
	}
	if (changed) {
		changed = false;
		printf("R: %s\n", res);
	}
}


void DMAFinishedIRQ(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize) {
	if (context<16) {
		cmd2finished[context] = true;
		if (rxData[1] == 0x01) {
			cmd2result[context] = true;
		} else {
			cmd2result[context] = false;
		}
	}
}


uint8_t txData[6];


void SdcWithDMACmd2(int argc, char *argv[]) {
	uint8_t dataCntToSend = 6;
	uint8_t dataCntToReceive = 10;

	txData[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
	txData[1] = 0;
	txData[2] = 0;
	txData[3] = 0;
	txData[4] = 0;
	txData[5] = 0x95;

//	for (int i =0; i< 64; i++) {
//		rxData[i] = i;
//	}


	for (int i=0;i<16;i++) {	// test fill up the queue
		cmd2finished[i] = false;
		cmd2result[i] = false;
		res[i] = '.';
		for (int j =0; j< 64; j++) {
			rxData[i][j] = j;
		}
	}
	for (int i=0;i<16;i++) {	// test fill up the queue
		ADO_SSP_AddJob(i, sdcBusNr, txData, &(rxData[i][0]), dataCntToSend, dataCntToReceive, DMAFinishedIRQ, 0);
	}


}

//void DMACmdFinished(ado_sspstatus_t status, uint8_t *rxData, uint16_t cnt) {
//	if (rxData[1] != 0x01) {
//		/* Error - Flash could not be accessed */
//		printf("Error %02X %02X %02X", rxData[0], rxData[1], rxData[2]);
//	} else {
//		printf("DMA Reset ok!");
//	}
//}

void SdcWithDMACmd(int argc, char *argv[]) {
//	uint8_t dataCntToSend = 6;
//	uint8_t dataCntToReceive = 64;
//	uint8_t bytesToProcess = dataCntToSend + dataCntToReceive;
//	uint8_t dmaTxDataBuffer[512];
//
//	for (int i = 0; i<bytesToProcess; i++) {
//		dmaTxDataBuffer[i] = 0xFF;
//	}
//	dmaTxDataBuffer[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
//	dmaTxDataBuffer[1] = 0;
//	dmaTxDataBuffer[2] = 0;
//	dmaTxDataBuffer[3] = 0;
//	dmaTxDataBuffer[4] = 0;
//	dmaTxDataBuffer[5] = 0x95;
//
//	//ADO_SSP_AddJobSharedBuffer(sdcBusNr, dmaTxDataBuffer, dataCntToSend, dataCntToReceive, DMACmdFinished);

}


