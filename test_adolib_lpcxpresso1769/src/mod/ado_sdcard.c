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
void SdcWithDMACmd(int argc, char *argv[]);
void SdcWithDMACmd2(int argc, char *argv[]);


static ado_sspid_t sdcBusNr = -1;
//volatile bool card_busy;


void SdcInit(ado_sspid_t bus) {
	sdcBusNr = bus;

	Chip_GPDMA_Init(LPC_GPDMA);
	NVIC_EnableIRQ (DMA_IRQn);


	CliRegisterCommand("sdc", SdcMyFirstCmd);
	CliRegisterCommand("dma", SdcWithDMACmd);
	CliRegisterCommand("dma2", SdcWithDMACmd2);

}

void DMACmd2Finished(ado_sspstatus_t status, uint8_t *rxData, uint16_t cnt) {
	if (rxData[1] != 0x01) {
		/* Error - Flash could not be accessed */
		printf("Error Sc %02X %02X %02X", rxData[0], rxData[1], rxData[2]);
	} else {
		printf("Scattered DMA Reset ok!");
	}
}

void SdcWithDMACmd2(int argc, char *argv[]) {
	uint8_t dataCntToSend = 6;
	uint8_t dataCntToReceive = 64;
	uint8_t txData[6];
	uint8_t rxData[64];

	txData[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
	txData[1] = 0;
	txData[2] = 0;
	txData[3] = 0;
	txData[4] = 0;
	txData[5] = 0x95;

	ADO_SSP_AddJobScattered(sspId, txData, rxData, bytes_to_send, bytes_to_read, finished)sspId, txData, rxData, bytes_to_send, bytes_to_read, finished)sdcBusNr, txData, rxData, dataCntToSend, dataCntToReceive, DMACmd2Finished);

//	dmaStat = 0x03;
//	Chip_SSP_DMA_Enable(LPC_SSP0);
//
//	// Receiver Channel - Setup as Scatter Transfer First Block has a dummy destination without incrementing the pointer, second block is real RX.
//	// first 6 byte are TX only we write all Rx to Dummy Byte
//	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llirx1, GPDMA_CONN_SSP0_Rx, (uint32_t)&rxDummy,dataCntToSend,GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA, &llirx2);
//	// Then the real receive starts to write in rxData
//	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llirx2, GPDMA_CONN_SSP0_Rx, (uint32_t)rxData,dataCntToReceive,GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA, 0);
//
//	// Some corrections needed here !!!!!
//	llirx1.ctrl &= (~GPDMA_DMACCxControl_DI);	// First block should not increment destination (All Rx write to dummy byte)!
//	llirx1.src = GPDMA_CONN_SSP0_Rx;			// Source of First Block must be readjusted (from real Peripherial Address back to Periphery-ID)
//												// in order to make the SGTransfer prepare work!
//	Chip_GPDMA_SGTransfer(LPC_GPDMA, 1, &llirx1, GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA);
//
//	// Transmit Channel
//	//Chip_GPDMA_Transfer(LPC_GPDMA, 2, (uint32_t)txData, GPDMA_CONN_SSP0_Tx, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, bytesToProcess);
//	// first 6 byte are the real TX
//	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llitx1, (uint32_t)txData, GPDMA_CONN_SSP0_Tx , dataCntToSend ,GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, &llitx2);
//	// Then the tx channel only sends dummy 0xFF bytes in order to receive all rx bytes
//	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llitx2, (uint32_t)&txDummy, GPDMA_CONN_SSP0_Tx, dataCntToReceive, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, 0);
//
//	// Some corrections needed here !!!!!
//	llitx2.ctrl &= (~GPDMA_DMACCxControl_SI);	// 2nd block should not increment source (All Tx write from dummy byte)!
//	llitx1.dst = GPDMA_CONN_SSP0_Tx;			// Destination of First Block must be readjusted (from real Peripherial Address back to Periphery-ID)
//												// in order to make the SGTransfer prepare work!
//	Chip_GPDMA_SGTransfer(LPC_GPDMA, 2, &llitx1, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA);
//
//	while ((dmaStat != 0) && (helper < 1000000))
//		{
//			/* Wait for both dma channels to finish */
//			helper++;
//		}
//
//	if (helper >= 1000000) {
//	    	printf("Error DMA not finished.");
//	}
//
//	if (rxData[1] != 0x01)
//		{
//			/* Error - Flash could not be accessed */
//			printf("Error Sc %02X %02X %02X", rxData[0], rxData[1], rxData[2]);
//		} else {
//			printf("Scattered DMA Reset ok!");
//		}
//
//	Chip_SSP_DMA_Disable(LPC_SSP0);
}

void DMACmdFinished(ado_sspstatus_t status, uint8_t *rxData, uint16_t cnt) {
	if (rxData[1] != 0x01) {
		/* Error - Flash could not be accessed */
		printf("Error %02X %02X %02X", rxData[0], rxData[1], rxData[2]);
	} else {
		printf("DMA Reset ok!");
	}
}

void SdcWithDMACmd(int argc, char *argv[]) {
	uint8_t dataCntToSend = 6;
	uint8_t dataCntToReceive = 64;
	uint8_t bytesToProcess = dataCntToSend + dataCntToReceive;
	uint8_t dmaTxDataBuffer[512];

	for (int i = 0; i<bytesToProcess; i++) {
		dmaTxDataBuffer[i] = 0xFF;
	}
	dmaTxDataBuffer[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
	dmaTxDataBuffer[1] = 0;
	dmaTxDataBuffer[2] = 0;
	dmaTxDataBuffer[3] = 0;
	dmaTxDataBuffer[4] = 0;
	dmaTxDataBuffer[5] = 0x95;


	//ADO_SSP_AddJobSharedBuffer(sdcBusNr, dmaTxDataBuffer, dataCntToSend, dataCntToReceive, DMACmdFinished);

//
//	Chip_SSP_DMA_Enable(LPC_SSP0);
//	// Receiver Channel - As the receiver channel is always behind the transmitter, we can use the same buffer here (original tx data gets overwritten by RX!)
//	Chip_GPDMA_Transfer(LPC_GPDMA, 2, GPDMA_CONN_SSP0_Rx, (uint32_t)dmaTxDataBuffer, GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA, bytesToProcess);
//	// Transmiter Channel
//	Chip_GPDMA_Transfer(LPC_GPDMA, 1, (uint32_t)dmaTxDataBuffer, GPDMA_CONN_SSP0_Tx, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, bytesToProcess);
//
//	while ((dmaStat != 0) && (helper < 1000000))
//		{
//			/* Wait for both dma channels to finish */
//			helper++;
//		}
//
//	if (helper >= 1000000) {
//	    	printf("Error DMA not finished.");
//	}
//
//	if (dmaTxDataBuffer[7] != 0x01)
//		{
//			/* Error - Flash could not be accessed */
//			printf("Error2 %02X %02X %02X", dmaTxDataBuffer[6], dmaTxDataBuffer[7], dmaTxDataBuffer[8]);
//		} else {
//			printf("DMA Reset ok!");
//		}
//
//	Chip_SSP_DMA_Disable(LPC_SSP0);
}


void SdcMyFirstCmd(int argc, char *argv[]) {
	printf("SDCard on Bus %d\n",sdcBusNr);
	//DumpSspJobs(sdcBusNr);
//
//	uint8_t tx[6];
//	uint8_t rx[512];
//	uint8_t *job_status = NULL;
//	uint8_t *job_status2 = NULL;
//
//	uint32_t helper;
//	uint32_t helper2;
//
//
//	if((argc > 0) && strcmp(argv[0],"po") == 0) {
//		/* prepare (after power up with 80 clock without CS!!!! */
//		for (int i = 0; i<10; i++){
//			LPC_SSP0->DR = 0xFF;
//		}
//		printf("Power On.\n");
//		return;
//	}
//
//
//	/* Read flash ID register */
//	tx[0] = 0x40 | GO_IDLE_STATE;
//	tx[1] = 0;
//	tx[2] = 0;
//	tx[3] = 0;
//	tx[4] = 0;
//	tx[5] = 0x95;		// dummy CRC"!?
//
//	rx[0] = 0x33;
//	rx[1] = 0x44;
//	rx[2] = 0x45;
//
//
//	if (ssp_add_job2(sdcBusNr , tx, 6, rx, 64, &job_status, 0))
//	{
//		printf("Error1");
//	}
////	if (ssp_add_job2(sdcBusNr , tx, 6, rx, 128, &job_status2, 0))
////	{
////		printf("Error1a");
////	}
//
//	helper = 0;
//	while ((*job_status != SSP_JOB_STATE_DONE) && (helper < 1000000))
//	{
//		/* Wait for job to finish */
//		helper++;
//	}
//
//	helper2 = 0;
//	while ((*job_status2 != SSP_JOB_STATE_DONE) && (helper2 < 1000000))
//	{
//		/* Wait for job to finish */
//		helper2++;
//	}
//
//    if (helper >= 1000000) {
//    	printf("Error3 Job A not finished.");
//    }
//    if (helper2 >= 1000000) {
//     	printf("Error3 Job B not finished.");
//     }
//	if (rx[1] != 0x01)
//	{
//		/* Error - Flash could not be accessed */
//		printf("Error2 %02X %02X %02X", rx[0], rx[1], rx[2]);
//	} else {
//		printf("Reset ok!");
//	}
}
