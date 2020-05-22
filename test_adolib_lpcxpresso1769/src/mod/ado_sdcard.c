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

static ssp_busnr_t sdcBusNr = -1;
//volatile bool card_busy;


void SdcInit(ssp_busnr_t bus) {
	sdcBusNr = bus;

	Chip_GPDMA_Init(LPC_GPDMA);
	NVIC_EnableIRQ (DMA_IRQn);


	CliRegisterCommand("sdc", SdcMyFirstCmd);
	CliRegisterCommand("dma", SdcWithDMACmd);
}

//
// Be careful here! This Callbacks are called from IRQ !!!
// Do not do any complicated logic here!!!
// Transfer all things to be done to next mainloop....
//void SdcChipSelect(bool select) {
//	//Chip_GPIO_SetPinState(LPC_GPIO, SDC_CS_PORT, SDC_CS_PIN, !select);
////	if (!select) {
////		card_busy = false;
////	}
////	return true;
//}
//


volatile uint8_t dmaStat = 0x00;
uint8_t dmaTxDataBuffer[512];
uint8_t dmaRxDataBuffer[512];



void DMA_IRQHandler(void) {
	IntStatus s;
	s = Chip_GPDMA_IntGetStatus(LPC_GPDMA, GPDMA_STAT_INTTC , 1);
	if (s != RESET) {
		Chip_GPDMA_ClearIntPending(LPC_GPDMA, GPDMA_STATCLR_INTTC , 1);
		dmaStat &= ~0x01;
	}
	s = Chip_GPDMA_IntGetStatus(LPC_GPDMA, GPDMA_STAT_INTTC , 2);
	if (s != RESET) {
		Chip_GPDMA_ClearIntPending(LPC_GPDMA, GPDMA_STATCLR_INTTC , 2);
		dmaStat &= ~0x02;
	}

}



void SdcWithDMACmd(int argc, char *argv[]) {
	uint32_t helper = 0;
	uint8_t dataCntToSend = 6;
	uint8_t dataCntToReceive = 64;
	uint8_t bytesToProcess = dataCntToSend + dataCntToReceive;

	for (int i = 0; i<bytesToProcess; i++) {
		dmaTxDataBuffer[i] = 0xFF;
		dmaRxDataBuffer[i] = 0;
	}
	dmaTxDataBuffer[0] = 0x40 | GO_IDLE_STATE;		// CMD consists of 6 bytes to send.sdc
	dmaTxDataBuffer[1] = 0;
	dmaTxDataBuffer[2] = 0;
	dmaTxDataBuffer[3] = 0;
	dmaTxDataBuffer[4] = 0;
	dmaTxDataBuffer[5] = 0x95;

	dmaStat = 0x03;

	Chip_SSP_DMA_Enable(LPC_SSP0);
	// Receiver Channel
	Chip_GPDMA_Transfer(LPC_GPDMA, 1, GPDMA_CONN_SSP0_Rx, (uint32_t)dmaRxDataBuffer, GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA, bytesToProcess);
	// Transfer Channel
	Chip_GPDMA_Transfer(LPC_GPDMA, 2, (uint32_t)dmaTxDataBuffer, GPDMA_CONN_SSP0_Tx, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, bytesToProcess);



	while ((dmaStat != 0) && (helper < 1000000))
		{
			/* Wait for both dma channels to finish */
			helper++;
		}

	if (helper >= 1000000) {
	    	printf("Error DMA not finished.");
	}

	if (dmaRxDataBuffer[7] != 0x01)
		{
			/* Error - Flash could not be accessed */
			printf("Error2 %02X %02X %02X", dmaRxDataBuffer[6], dmaRxDataBuffer[7], dmaRxDataBuffer[8]);
		} else {
			printf("DMA Reset ok!");
		}

	//Chip_SSP_DMA_Disable(LPC_SSP0);
}


void SdcMyFirstCmd(int argc, char *argv[]) {
	Chip_SSP_DMA_Disable(LPC_SSP0);
	printf("SDCard on Bus %d\n",sdcBusNr);
	//DumpSspJobs(sdcBusNr);

	uint8_t tx[6];
	uint8_t rx[512];
	uint8_t *job_status = NULL;
	uint8_t *job_status2 = NULL;

	uint32_t helper;
	uint32_t helper2;


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
	rx[2] = 0x45;


	if (ssp_add_job2(sdcBusNr , tx, 6, rx, 3, &job_status, 0))
	{
		printf("Error1");
	}
//	if (ssp_add_job2(sdcBusNr , tx, 6, rx, 128, &job_status2, 0))
//	{
//		printf("Error1a");
//	}

	helper = 0;
	while ((*job_status != SSP_JOB_STATE_DONE) && (helper < 1000000))
	{
		/* Wait for job to finish */
		helper++;
	}

	helper2 = 0;
	while ((*job_status2 != SSP_JOB_STATE_DONE) && (helper2 < 1000000))
	{
		/* Wait for job to finish */
		helper2++;
	}

    if (helper >= 1000000) {
    	printf("Error3 Job A not finished.");
    }
    if (helper2 >= 1000000) {
     	printf("Error3 Job B not finished.");
     }
	if (rx[1] != 0x01)
	{
		/* Error - Flash could not be accessed */
		printf("Error2 %02X %02X %02X", rx[0], rx[1], rx[2]);
	} else {
		printf("Reset ok!");
	}
}
