/*
 *  obc_ssp0.c
 *
 *  Created on: 07.10.2012 ...
 *      Author: Andi, Robert
 *
 *  Copied over from pegasus flight software on 2019-12-14
 */

#include <stdio.h>
#include <string.h>
#include <chip.h>

#include "ado_sspdma.h"

#define ADO_SSP_DUMP_RX(device) { uint32_t temp; (void)temp; while ((device->SR & SSP_STAT_RNE) != 0) { temp = device->DR; } }

const LPC_SSP_T *ADO_SSP_RegBase[] = {
	LPC_SSP0,					
	LPC_SSP1					
};

const CHIP_SYSCTL_CLOCK_T ADO_SSP_SysCtlClock[] = {
	SYSCTL_CLOCK_SSP0,
	SYSCTL_CLOCK_SSP1
};

const uint8_t ADO_SSP_RxDmaChannel[] = {
	ADO_SSP_RXDMACHANNEL0,
	ADO_SSP_RXDMACHANNEL1
};

const uint8_t ADO_SSP_TxDmaChannel[] = {
	ADO_SSP_TXDMACHANNEL0,
	ADO_SSP_TXDMACHANNEL1
};

const uint8_t ADO_SSP_IdForChannel[] = {
	ADO_SSP0,					// DMA Channel 0 -> SSPx		// TODO: configure in accordance with channel defines!!!!
	ADO_SSP0,					// DMA Channel 1 -> SSPx
	ADO_SSP0,					// DMA Channel 2 -> SSPx
	ADO_SSP0,					// DMA Channel 3 -> SSPx
	ADO_SSP0,					// DMA Channel 4 -> SSPx
	ADO_SSP0,					// DMA Channel 5 -> SSPx
	ADO_SSP0,					// DMA Channel 6 -> SSPx
	ADO_SSP0					// DMA Channel 7 -> SSPx
};


const uint32_t ADO_SSP_GPDMA_CONN_RX[] = {
	GPDMA_CONN_SSP0_Rx,
	GPDMA_CONN_SSP1_Rx
};

const uint32_t ADO_SSP_GPDMA_CONN_TX[] = {
	GPDMA_CONN_SSP0_Tx,
	GPDMA_CONN_SSP1_Tx
};


typedef struct ado_sspjob_s
{
	uint8_t *txData;
	uint8_t *rxData;
	uint16_t txSize;
	uint16_t rxSize;
	void(*ADO_SSP_JobFinished_IRQCallback)(ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize);
} ado_sspjob_t;

typedef struct ado_sspjobs_s
{
	ado_sspjob_t job[ADO_SSP_MAXJOBS];
	uint8_t current_job;
	uint8_t jobs_pending;
	uint8_t txDmaChannel;
	uint8_t rxDmaChannel;
	uint16_t ssp_job_error_counter;
} ado_sspjobs_t;

// local/module variables
ado_sspjobs_t ado_sspjobs[2];		// One structure per Ssp device.

// Prototypes
void ADO_SSP_InitiateDMA(ado_sspid_t sspId, ado_sspjob_t *newJob);


// Common init routine used for both SSP buses
void ADO_SSP_Init(ado_sspid_t sspId, uint32_t bitRate) {
	LPC_SSP_T *pSSP = ADO_SSP_RegBase[sspId];
	
	Chip_Clock_EnablePeriphClock(ADO_SSP_SysCtlClock[sspId]);		
	Chip_Clock_SetPCLKDiv(ADO_SSP_SysCtlClock[sspId], SYSCTL_CLKDIV_1);						// No additional prescaler used.
	Chip_SSP_Set_Mode(pSSP, SSP_MODE_MASTER);												// SSP is Master
	Chip_SSP_SetFormat(pSSP, SSP_BITS_8, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA0_CPOL0);		// SPI Mode with 8 bit and 'mode1' SPI: this makes CS pulses per Byte (on native SSP SSL Port).
																							// 		( SSP_CLOCK_CPHA0_CPOL0 'mode3' would keep SSL active for a whole job transfer. )
	
//	Chip_SSP_SetBitRate(pSSP, bitRate);				// This calculates settings with near fit. Actual bitrate is calculated as ....
	
//	Chip_SSP_SetClockRate(pSSP, 5, 2);
//	Chip_SSP_SetClockRate(pSSP, 4, 2);				// SSPClk: 9.6Mhz   	( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (4+1) )
	Chip_SSP_SetClockRate(pSSP, 3, 2);				// SSPClk: 12Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (3+1) )	This config makes problems with IRQ Version!!!!
//	Chip_SSP_SetClockRate(pSSP, 2, 2);				// SSPClk: 16Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (2+1) )
//	Chip_SSP_SetClockRate(pSSP, 1, 2);				// SSPClk: 24Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (1+1) )
//	Chip_SSP_SetClockRate(pSSP, 0, 2);				// SSPClk: 48Mhz			- not working with my external wired socket !? ...

	Chip_SSP_DisableLoopBack(pSSP);
	Chip_SSP_Enable(pSSP);
	ADO_SSP_DUMP_RX(pSSP);

	// Disable all IRQs 
	Chip_SSP_Int_Disable(pSSP, SSP_INT_BITMASK);

	/* Reset buffers to default values */
	ado_sspjobs[sspId].current_job = 0;
	ado_sspjobs[sspId].jobs_pending = 0;
	ado_sspjobs[sspId].ssp_job_error_counter = 0;
	
	ado_sspjobs[sspId].rxDmaChannel = ADO_SSP_RxDmaChannel[sspId];
	ado_sspjobs[sspId].txDmaChannel = ADO_SSP_TxDmaChannel[sspId];
}

void ADO_SSP_AddJobScattered(ado_sspid_t sspId, uint8_t *txData, uint8_t *rxData, uint16_t bytes_to_send, uint16_t bytes_to_read, void(*finished)(ado_sspstatus_t s, uint8_t *rx, uint16_t cnt)){
	ado_sspjobs_t *jobs = &ado_sspjobs[sspId];

	if (jobs->jobs_pending >= ADO_SSP_MAXJOBS) {
		/* Maximum amount of jobs stored, job can't be added! */
		jobs->ssp_job_error_counter++;
		finished(SSP_JOB_BUFFER_OVERFLOW, 0, 0);		// Use the IRQ callback to Signal this error.
		return;
	}

	uint8_t newJobIdx = (jobs->current_job + jobs->jobs_pending) % ADO_SSP_MAXJOBS;
	ado_sspjob_t *newJob = &(jobs->job[newJobIdx]);

	newJob->txData = txData;
	newJob->rxData = rxData;
	newJob->txSize = bytes_to_send;
	newJob->rxSize = bytes_to_read;
	newJob->ADO_SSP_JobFinished_IRQCallback = finished;

	if (jobs->jobs_pending == 0) {
		/* if no jobs pending, start the Communication.*/
		/* Select device TODO: Add Multi CS Callback!!!*/
//			if (chipSelectHandler != 0) {
//				chipSelectHandler(true);
//			}
		ADO_SSP_InitiateDMA(sspId, newJob);
	}

	jobs->jobs_pending++;
}


DMA_TransferDescriptor_t llirx1;
DMA_TransferDescriptor_t llirx2;
DMA_TransferDescriptor_t llitx1;
DMA_TransferDescriptor_t llitx2;
uint8_t rxDummy;
uint8_t txDummy = 0xFF;
uint8_t dmaStat;

void ADO_SSP_InitiateDMA(ado_sspid_t sspId, ado_sspjob_t *newJob) {
	dmaStat = 0x03;
	Chip_SSP_DMA_Enable(ADO_SSP_RegBase[sspId]);

	// Receiver Channel - Setup as Scatter Transfer First Block has a dummy destination without incrementing the pointer, second block is real RX.
	// first bytes are TX only we write all Rx to Dummy Byte
	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llirx1, ADO_SSP_GPDMA_CONN_RX[sspId], (uint32_t)&rxDummy, newJob->txSize, GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA, &llirx2);
	// Then the real receive starts to write in rxData
	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llirx2, ADO_SSP_GPDMA_CONN_RX[sspId], (uint32_t)newJob->rxData, newJob->rxSize, GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA, 0);

	// Some corrections needed here !!!!!
	llirx1.ctrl &= (~GPDMA_DMACCxControl_DI);	// First block should not increment destination (All Rx write to dummy byte)!
	llirx1.src = ADO_SSP_GPDMA_CONN_RX[sspId];	// Source of First Block must be readjusted (from real Peripherial Address back to Periphery-ID)
												// in order to make the SGTransfer prepare work!
	Chip_GPDMA_SGTransfer(LPC_GPDMA, 1, &llirx1, GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA);

	// Transmit Channel
	//Chip_GPDMA_Transfer(LPC_GPDMA, 2, (uint32_t)txData, GPDMA_CONN_SSP0_Tx, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, bytesToProcess);
	// first 6 byte are the real TX
	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llitx1, (uint32_t)newJob->txData, ADO_SSP_GPDMA_CONN_TX[sspId],  newJob->txSize, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, &llitx2);
	// Then the tx channel only sends dummy 0xFF bytes in order to receive all rx bytes
	Chip_GPDMA_PrepareDescriptor(LPC_GPDMA, &llitx2, (uint32_t)&txDummy,  ADO_SSP_GPDMA_CONN_TX[sspId], newJob->rxSize, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA, 0);

	// Some corrections needed here !!!!!
	llitx2.ctrl &= (~GPDMA_DMACCxControl_SI);	// 2nd block should not increment source (All Tx write from dummy byte)!
	llitx1.dst =  ADO_SSP_GPDMA_CONN_TX[sspId];			// Destination of First Block must be readjusted (from real Peripherial Address back to Periphery-ID)
												// in order to make the SGTransfer prepare work!
	Chip_GPDMA_SGTransfer(LPC_GPDMA, 2, &llitx1, GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA);
}


void DMA_IRQHandler(void) {
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4);
	IntStatus s;

	s = Chip_GPDMA_IntGetStatus(LPC_GPDMA, GPDMA_STAT_INTTC , 1);
	if (s != RESET) {
		Chip_GPDMA_ClearIntPending(LPC_GPDMA, GPDMA_STATCLR_INTTC , 1);
		dmaStat &= ~0x01;
		if ((dmaStat & 0x03) == 0) {
			ado_sspid_t id = ADO_SSP_IdForChannel[1];
			ado_sspjob_t *job = &ado_sspjobs[id].job[ado_sspjobs[id].current_job];
			job->ADO_SSP_JobFinished_IRQCallback(SSP_JOB_STATE_DONE, job->rxData, job->rxSize);
			ado_sspjobs[id].jobs_pending--;
		}
	}
	s = Chip_GPDMA_IntGetStatus(LPC_GPDMA, GPDMA_STAT_INTTC , 2);
	if (s != RESET) {
		Chip_GPDMA_ClearIntPending(LPC_GPDMA, GPDMA_STATCLR_INTTC , 2);
		dmaStat &= ~0x02;
//		if ((dmaStat & 0x03) == 0) {
//			ado_sspid_t id = ADO_SSP_IdForChannel[1];
//			ado_sspjob_t *job = &ado_sspjobs[id].job[ado_sspjobs[id].current_job];
//			job->ADO_SSP_JobFinished_IRQCallback(SSP_JOB_STATE_DONE, job->rxData, job->rxSize);
//			ado_sspjobs[id].jobs_pending--;
//		}
	}
	s = Chip_GPDMA_IntGetStatus(LPC_GPDMA, GPDMA_STAT_INTTC , 3);
	if (s != RESET) {
		Chip_GPDMA_ClearIntPending(LPC_GPDMA, GPDMA_STATCLR_INTTC , 3);
		dmaStat &= ~0x04;
	}
	s = Chip_GPDMA_IntGetStatus(LPC_GPDMA, GPDMA_STAT_INTTC , 4);
	if (s != RESET) {
		Chip_GPDMA_ClearIntPending(LPC_GPDMA, GPDMA_STATCLR_INTTC , 4);
		dmaStat &= ~0x08;
	}
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
}
