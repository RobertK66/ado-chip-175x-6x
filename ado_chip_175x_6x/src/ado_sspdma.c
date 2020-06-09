/*
 *  obc_ssp0.c
 *
 *  Created on: 07.10.2012 ...
 *      Author: Andi, Robert
 *
 *  Copied over from pegasus flight software on 2019-12-14
 */
#include "ado_sspdma.h"

#include <stdio.h>
#include <string.h>
#include <chip.h>

//#define ADO_SSPDMA_BITFLIPSAFE  // Use this define to reconfigure constant part of DMA-Ctrl structures with every DMA Request.
								// This costs ca. 3,6% performance with massive multi-job read/writes.

// Inline Block to clear SSP Rx Buffer.
#define ADO_SSP_DUMP_RX(device) { uint32_t temp; (void)temp; while ((device->SR & SSP_STAT_RNE) != 0) { temp = device->DR; } }


// Following constants hold the Configuration data for both SSPx interfaces. idx=0 -> SSP0 idx=1 ->SSP1
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
	uint32_t context;											// Any data can be stored here by client. Its fed back to the callbacks when job processes.
	AdoSSP_ActivateHandler(ADO_SSP_JobActivated_IRQCallback);	// Use this for activating chip Select, if SSP SSL not used.
	AdoSSP_FinishedHandler(ADO_SSP_JobFinished_IRQCallback);	// Do not process received data with this callback. Only signal data available to your main routines.
} ado_sspjob_t;

typedef struct ado_sspjobs_s
{
	ado_sspjob_t job[ADO_SSP_MAXJOBS];
	uint8_t current_job;
	uint8_t jobs_pending;
	uint8_t txDmaChannel;
	uint8_t rxDmaChannel;
	DMA_TransferDescriptor_t dmaTd[4];
	uint16_t ssp_job_error_counter;
} ado_sspjobs_t;

// local/module variables
ado_sspjobs_t 	ado_sspjobs[2];		// One structure per SSP Master [0]:SSP0, [1]:SSP1
uint8_t 		rxDummy;
uint8_t 		txDummy = 0xFF;

// Prototypes
void ADO_SSP_InitiateDMA(ado_sspid_t sspId, ado_sspjob_t *newJob);

// Init for one SSP bus.
void ADO_SSP_Init(ado_sspid_t sspId, uint32_t bitRate) {
	LPC_SSP_T *pSSP = (LPC_SSP_T *)ADO_SSP_RegBase[sspId];
	
	Chip_Clock_EnablePeriphClock(ADO_SSP_SysCtlClock[sspId]);		
	Chip_Clock_SetPCLKDiv(ADO_SSP_SysCtlClock[sspId], SYSCTL_CLKDIV_1);					// No additional prescaler used.
	Chip_SSP_Set_Mode(pSSP, SSP_MODE_MASTER);											// SSP is Master
	Chip_SSP_SetFormat(pSSP, SSP_BITS_8, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA0_CPOL0);	// SPI Mode with 8 bit and 'mode1' SPI: this makes CS pulses per Byte (on native SSP SSL Port).
																						// 		( SSP_CLOCK_CPHA0_CPOL0 'mode3' would keep SSL active for a whole job transfer. )
	Chip_SSP_SetBitRate(pSSP, bitRate);					// This calculates settings with a near fit. Actual bitrate is calculated as ....
	
//	Chip_SSP_SetClockRate(pSSP, 5, 2);
//	Chip_SSP_SetClockRate(pSSP, 4, 2);				// SSPClk: 9.6Mhz   	( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (4+1) )
//	Chip_SSP_SetClockRate(pSSP, 3, 2);				// SSPClk: 12Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (3+1) )
//	Chip_SSP_SetClockRate(pSSP, 2, 2);				// SSPClk: 16Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (2+1) )
//	Chip_SSP_SetClockRate(pSSP, 1, 2);				// SSPClk: 24Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (1+1) )
//	Chip_SSP_SetClockRate(pSSP, 0, 2);				// SSPClk: 48Mhz			- not working with my external wired socket !? ...

	Chip_SSP_DisableLoopBack(pSSP);
	Chip_SSP_Enable(pSSP);
	Chip_SSP_Int_Disable(pSSP, SSP_INT_BITMASK);
	Chip_SSP_DMA_Enable(pSSP);

	ADO_SSP_DUMP_RX(pSSP);

	/* Reset buffers to default values */
	ado_sspjobs[sspId].current_job = 0;
	ado_sspjobs[sspId].jobs_pending = 0;
	ado_sspjobs[sspId].ssp_job_error_counter = 0;
	ado_sspjobs[sspId].rxDmaChannel = ADO_SSP_RxDmaChannel[sspId];
	ado_sspjobs[sspId].txDmaChannel = ADO_SSP_TxDmaChannel[sspId];

	// Initialize the DMA Ctrl structures with empty src/dest addresses.
	// Receiver Channel - Setup as Scatter Transfer with 2 blocks.
	// First bytes are TX only. We write all Rx to a dummy Byte.
	ado_sspjobs[sspId].dmaTd[0].src = 1;
	ado_sspjobs[sspId].dmaTd[0].dst = (uint32_t)&rxDummy;
	ado_sspjobs[sspId].dmaTd[0].lli = (uint32_t)(&ado_sspjobs[sspId].dmaTd[1]);
	ado_sspjobs[sspId].dmaTd[0].ctrl = 0x00009000;

	// Then the real receive starts to write into rxData
	ado_sspjobs[sspId].dmaTd[1].src = (uint32_t)&(pSSP->DR);
	ado_sspjobs[sspId].dmaTd[1].dst = 0;					// This will be filled by job entry later.
	ado_sspjobs[sspId].dmaTd[1].lli = 0;
	ado_sspjobs[sspId].dmaTd[1].ctrl = 0x88009000;

	// Transmit Channel - 2 Blocks
	// first n byte are the real TX
	ado_sspjobs[sspId].dmaTd[2].src = 0;					// This will be filled by job entry later.
	ado_sspjobs[sspId].dmaTd[2].dst = 0;
	ado_sspjobs[sspId].dmaTd[2].lli = (uint32_t)(&ado_sspjobs[sspId].dmaTd[3]);
	ado_sspjobs[sspId].dmaTd[2].ctrl = 0x04009000;

	// Then the tx channel only sends dummy 0xFF bytes in order to receive all rx bytes
	ado_sspjobs[sspId].dmaTd[3].src = (uint32_t)&txDummy;
	ado_sspjobs[sspId].dmaTd[3].dst = (uint32_t)&(pSSP->DR);
	ado_sspjobs[sspId].dmaTd[3].lli = 0;
	ado_sspjobs[sspId].dmaTd[3].ctrl = 0x00009000;

	Chip_GPDMA_Init(LPC_GPDMA);
	NVIC_EnableIRQ (DMA_IRQn);
}


void ADO_SSP_AddJob(uint32_t context, ado_sspid_t sspId, uint8_t *txData, uint8_t *rxData, uint16_t bytes_to_send, uint16_t bytes_to_read, AdoSSP_FinishedHandler(finish), AdoSSP_ActivateHandler(activate)){
	bool startIt = false;
	ado_sspjobs_t *jobs = &ado_sspjobs[sspId];

	if (jobs->jobs_pending >= ADO_SSP_MAXJOBS) {
		/* Maximum amount of jobs stored, job can't be added! */
		jobs->ssp_job_error_counter++;
		if (finish != 0) {
			finish(context, SSP_JOB_BUFFER_OVERFLOW, 0, 0);		// Use the IRQ callback to Signal this error. Maybe a direct return value would be better here !?
		}
		return;
	}

	// Be sure the following operations will not be mixed up with an DMA TC-IRQ ending and changing the current/pending job idx.
	// *****
	NVIC_DisableIRQ (DMA_IRQn);

	uint8_t newJobIdx = (jobs->current_job + jobs->jobs_pending) % ADO_SSP_MAXJOBS;
	ado_sspjob_t *newJob = &(jobs->job[newJobIdx]);
	newJob->txData = txData;
	newJob->rxData = rxData;
	newJob->txSize = bytes_to_send;
	newJob->rxSize = bytes_to_read;
	newJob->context = context;
	newJob->ADO_SSP_JobFinished_IRQCallback = finish;
	newJob->ADO_SSP_JobActivated_IRQCallback = activate;

	if (jobs->jobs_pending == 0) {
		startIt = true;
	}
	jobs->jobs_pending++;

	NVIC_EnableIRQ (DMA_IRQn);
	// *****
	// Now we allow DMA IRQs to be executed from here on.

	if (startIt) {
		// If this was the first job entered into an empty queue, we start the communication now.
		// If a DMA was running before this Disable/Enable IRQ block,
		// we should never get here because jobs_pending must have been already > 0 then!
		ADO_SSP_DUMP_RX(ADO_SSP_RegBase[sspId]);
		ADO_SSP_InitiateDMA(sspId, newJob);
	}
}


void DMA_IRQHandler(void) {
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4);
	uint32_t tcs = LPC_GPDMA->INTTCSTAT;
	if ( tcs & (1UL<<ADO_SSP_RXDMACHANNEL0) ) {
		LPC_GPDMA->INTTCCLEAR = (1UL << ADO_SSP_RXDMACHANNEL0);
		// This was SSP0 finishing its RX Channel.
		ado_sspjob_t *job = &ado_sspjobs[0].job[ado_sspjobs[0].current_job];
		job->ADO_SSP_JobFinished_IRQCallback(job->context, ADO_SSP_JOBDONE, job->rxData, job->rxSize);

		ado_sspjobs[0].jobs_pending--;
		if (ado_sspjobs[0].jobs_pending > 0) {
			// Continue with nextJob
			ado_sspjobs[0].current_job++;
			if (ado_sspjobs[0].current_job == ADO_SSP_MAXJOBS) {
				ado_sspjobs[0].current_job = 0;
			}
			ADO_SSP_InitiateDMA(ADO_SSP0,&ado_sspjobs[0].job[ado_sspjobs[0].current_job]);
		}
	}
	if ( tcs & (1<<ADO_SSP_RXDMACHANNEL1) ) {
		LPC_GPDMA->INTTCCLEAR = (1UL << ADO_SSP_RXDMACHANNEL1);
		// This was SSP1 finishing its RX Channel.
		ado_sspjob_t *job = &ado_sspjobs[1].job[ado_sspjobs[1].current_job];
		job->ADO_SSP_JobFinished_IRQCallback(job->context, ADO_SSP_JOBDONE, job->rxData, job->rxSize);

		ado_sspjobs[1].jobs_pending--;
		if (ado_sspjobs[1].jobs_pending > 0) {
			// Continue with nextJob
			ado_sspjobs[1].current_job++;
			if (ado_sspjobs[1].current_job == ADO_SSP_MAXJOBS) {
				ado_sspjobs[1].current_job = 0;
			}
			ADO_SSP_InitiateDMA(ADO_SSP1,&ado_sspjobs[1].job[ado_sspjobs[1].current_job]);
		}
	}
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
}

// __attribute__((always_inline))		// This gives another 0,5% Performance improvement but it needs 150..200 bytes more prog memory!
void ADO_SSP_InitiateDMA(ado_sspid_t sspId, ado_sspjob_t *newJob) {
	DMA_TransferDescriptor_t *pDmaTd = ado_sspjobs[sspId].dmaTd;

	if (newJob->ADO_SSP_JobActivated_IRQCallback != 0) {
		// This could be used to activate a CS line other than the SSL of the SSP HW Unit.
		newJob->ADO_SSP_JobActivated_IRQCallback(newJob->context);			//TODO: 1st Test it, 2nd where is 'Deactivate' called!?
	}

	// Adjust the rx/tx addresses and length in prepared dma control structures.
	(pDmaTd+0)->ctrl =  0x00009000 | (uint32_t)(newJob->txSize);
	(pDmaTd+1)->ctrl =  0x88009000 | (uint32_t)(newJob->rxSize);
	(pDmaTd+1)->dst = (uint32_t)newJob->rxData;
	(pDmaTd+2)->ctrl =  0x04009000 | (uint32_t)(newJob->txSize);
	(pDmaTd+2)->src = (uint32_t)newJob->txData;
	(pDmaTd+3)->ctrl =  0x00009000 | (uint32_t)(newJob->rxSize);

#ifdef ADO_SSPDMA_BITFLIPSAFE												//TODO: re-test this version after all refactorings finished.....
	// Rewrite the constant part of the DMA-Ctrl structures every time used.
	LPC_SSP_T *pSSP = (LPC_SSP_T *)ADO_SSP_RegBase[sspId];
	(pDmaTd+0)->src = 1;
	(pDmaTd+0)->dst = (uint32_t)&rxDummy;
	(pDmaTd+0)->lli = (uint32_t)(pDmaTd+1);
	//(pDmaTd+0)->ctrl = 0x00009006;
	(pDmaTd+1)->src = (uint32_t)&(pSSP->DR);
	(pDmaTd+1)->lli = 0;
	//(pDmaTd+1)->ctrl = 0x8800900A;
	(pDmaTd+2)->dst = 0;
	(pDmaTd+2)->lli = (uint32_t)(pDmaTd+3);
	//(pDmaTd+2)->ctrl = 0x04009006;
	(pDmaTd+3)->src = (uint32_t)&txDummy;
	(pDmaTd+3)->dst = (uint32_t)&(pSSP->DR);
	(pDmaTd+3)->lli = 0;
	//(pDmaTd+3)->ctrl = 0x0000900A;
#endif


	// TODO: the variants with re-adjusting source and destination 'HW Address' vs 'Channel feature select' should be refactored
	//	( in its own _SGTransfer() routine !???) to avoid this 'mis-alignment' of block 1-2 if only one is used.....
	//  also Test this to work properly with second SSP channel!!!
	//
	if (newJob->txSize > 0) {
		(pDmaTd+1)->src = (uint32_t)&(ADO_SSP_RegBase[sspId]->DR);
		(pDmaTd+3)->dst = (uint32_t)&(ADO_SSP_RegBase[sspId]->DR);
		Chip_GPDMA_SGTransfer(LPC_GPDMA, ADO_SSP_RxDmaChannel[sspId],(pDmaTd+0), GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA);
		Chip_GPDMA_SGTransfer(LPC_GPDMA, ADO_SSP_TxDmaChannel[sspId],(pDmaTd+2), GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA);
	} else {
		// TX block is 0, so we start with second DMA Blocks - only rx part of job
		(pDmaTd+1)->src = 1;
		(pDmaTd+3)->dst = 0;
		Chip_GPDMA_SGTransfer(LPC_GPDMA, ADO_SSP_RxDmaChannel[sspId],(pDmaTd+1), GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA);
		Chip_GPDMA_SGTransfer(LPC_GPDMA, ADO_SSP_TxDmaChannel[sspId],(pDmaTd+3), GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA);
	}
}
