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

#include "ado_ssp.h"

#define ADO_SSP_DUMP_RX(device) { uint32_t temp; (void)temp; while ((device->SR & SSP_STAT_RNE) != 0) { temp = device->DR; } }


// from RTOS
//#define configMAX_LIBRARY_INTERRUPT_PRIORITY    ( 5 )
//#define SSP1_INTERRUPT_PRIORITY         (configMAX_LIBRARY_INTERRUPT_PRIORITY + 3)  /* SSP1 (Flash, MPU) */
//#define SSP0_INTERRUPT_PRIORITY         (SSP1_INTERRUPT_PRIORITY + 1)   /* SSP0 (Flash) - should be lower than SSP1 */


typedef struct ssp_job_s
{
	uint8_t *array_to_send;
	uint16_t bytes_to_send;
	uint8_t *array_to_read;
	//uint16_t bytes_to_read;
	uint16_t framesToProcess;

	uint16_t framesProcessed;
	uint16_t rxFrameIdx;
	uint16_t bytes_read;					//TODO. replace redundant values with defines (framesProcesssed / rx IDX ...)
	uint8_t status;
	//bool dir_tx;
	void(*chipSelectHandler)(bool select);
} volatile ssp_job_t;


typedef struct ssp_jobs_s
{
	ssp_job_t job[SSP_MAX_JOBS];
	uint8_t current_job;
//	uint8_t last_job_added;				// TODO: remove. not used any more
	uint8_t jobs_pending;
	bool   ssp_initialized;
	uint16_t ssp_rov_error_counter;
	uint16_t ssp_job_error_counter;
} volatile ssp_jobs_t;

// local/module variables
ssp_jobs_t ssp_jobs[2];

// Prototypes
void AdoSspIrqHandler(LPC_SSP_T *device, uint8_t busNr);
void ssp_init(LPC_SSP_T *device, uint8_t busNr, IRQn_Type irq, uint32_t irqPrio );
void FillTxBuffer(LPC_SSP_T *device, ssp_job_t *job, uint8_t maxPerFill);


// Module Init
void ssp01_init(void)
{
	ssp_jobs[0].ssp_initialized = false;
	ssp_jobs[1].ssp_initialized = false;

	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_SSP0);				//
	Chip_Clock_SetPCLKDiv(SYSCTL_PCLK_SSP0, SYSCTL_CLKDIV_1);		//
	//Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_SSP1);

	ssp_init(LPC_SSP0, SSP_BUS0, SSP0_IRQn, 0);
	//ssp_init(LPC_SSP1, SSP_BUS1, SSP1_IRQn, 0);

	return;
}

// Common init routine used for both SSP buses
void ssp_init(LPC_SSP_T *device, uint8_t busNr, IRQn_Type irq, uint32_t irqPrio ) {

	uint32_t cl = Chip_Clock_GetMainClockRate();


	Chip_SSP_Set_Mode(device, SSP_MODE_MASTER);
	Chip_SSP_SetFormat(device, SSP_BITS_8, SSP_FRAMEFORMAT_SPI, SSP_CLOCK_CPHA0_CPOL0);
//	Chip_SSP_SetClockRate(device, 4, 2);				// SSPClk: 9.6Mhz   	( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (4+1) )
	Chip_SSP_SetClockRate(device, 3, 2);				// SSPClk: 12Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (3+1) )
//	Chip_SSP_SetClockRate(device, 2, 2);				// SSPClk: 16Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (2+1) )
//	Chip_SSP_SetClockRate(device, 1, 2);				// SSPClk: 24Mhz		( With 96Mhz SystemCoreClock -> SSP Clk = 48Mhz / (1+1) )
//	Chip_SSP_SetClockRate(device, 0, 2);				// SSPClk: 48Mhz			- not working with my external wired socket !? ...

	Chip_SSP_DisableLoopBack(device);
	Chip_SSP_Enable(device);

	ADO_SSP_DUMP_RX(device);

	// Enable and clear IRSs
	Chip_SSP_Int_Enable(device, (SSP_INT_RTMO | SSP_INT_ROR | SSP_INT_RXHF ));
	Chip_SSP_ClearIntPending(device,SSP_INTCLR_BITMASK);

	/* Reset buffers to default values */
	ssp_jobs[busNr].current_job = 0;
	ssp_jobs[busNr].jobs_pending = 0;

	//NVIC_SetPriority(irq, irqPrio);
	NVIC_EnableIRQ (irq);

	ssp_jobs[busNr].ssp_job_error_counter = 0;
	ssp_jobs[busNr].ssp_rov_error_counter = 0;
	ssp_jobs[busNr].ssp_initialized = true;
}

void SSP1_IRQHandler(void)
{
	AdoSspIrqHandler(LPC_SSP1, SSP_BUS1);
}

void SSP0_IRQHandler(void)
{
	Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4);
	AdoSspIrqHandler(LPC_SSP0, SSP_BUS0);
	Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
}

void AdoSspIrqHandler(LPC_SSP_T *device, ssp_busnr_t busNr) {
	ssp_jobs_t *jobs = &ssp_jobs[busNr];
	ssp_job_t *cur_job = &(jobs->job[jobs->current_job]);

	// Check if there is a valid active job.
	if (cur_job->status != SSP_JOB_STATE_ACTIVE) {
		ADO_SSP_DUMP_RX(device);
		jobs->ssp_job_error_counter++;
		return;
	}

	// Check for RX Overflow. In this case a byte was already lost, so nothing usefull to do here...
	if (Chip_SSP_GetRawIntStatus(device, SSP_INT_ROR)) {
		// Clear Rx Overrun Error bit
		Chip_SSP_ClearIntPending(device, SSP_INT_ROR);
		ADO_SSP_DUMP_RX(device);
		jobs->ssp_rov_error_counter++;
		return;
	}

	uint32_t status;
	uint32_t temp;
	bool     rxEmpty = false;
	uint8_t *readPtr = &cur_job->array_to_read[cur_job->bytes_read];


	for (int i=0;i<8;i++) {
		//Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
		status = device->SR;
		//Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4);
		// RX
		if (status & SSP_STAT_RNE) {
			temp = device->DR;
			if (cur_job->rxFrameIdx++ >= cur_job->bytes_to_send) {
				*readPtr++ = temp;
				//cur_job->bytes_read++;
			}
		} else {
			rxEmpty = true;
		}
		// TX
		if ((status & SSP_STAT_TNF) &&
			(cur_job->framesProcessed < cur_job->framesToProcess)) {

			if (cur_job->framesProcessed < cur_job->bytes_to_send) {
				temp = cur_job->array_to_send[cur_job->framesProcessed];
			} else {
				temp = 0xFF;
			}
			cur_job->framesProcessed++;
			device->DR = temp;
		} else {
			if (rxEmpty) {
				break;
			}
		}
	}

	cur_job->bytes_read += (uint16_t)(readPtr - &cur_job->array_to_read[cur_job->bytes_read]);

	if (cur_job->bytes_read >= (cur_job->framesToProcess - cur_job->bytes_to_send)) {
		/* All bytes read */
		/* Unselect device */
		if (cur_job->chipSelectHandler != 0) {
			uint16_t helper = 0;
			while ((device->SR & SSP_STAT_BSY) && (helper < 100000))
			{
				/* Wait for SSP to finish transmission */
				helper++;
			}
			cur_job->chipSelectHandler(false);
		}

		/* This Job is done, increment to next job and execute if pending */
		cur_job->status = SSP_JOB_STATE_DONE;

		jobs->current_job++;
		jobs->jobs_pending--;

		if (jobs->current_job == SSP_MAX_JOBS)
		{
			jobs->current_job = 0;
		}

		Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 4);
		Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 4);

		//ADO_SSP_DUMP_RX(device);

		/* Check if jobs are pending */
		if (jobs->jobs_pending > 0)
		{
			// select next job
			cur_job = &(jobs->job[jobs->current_job]);

			/* Select device */
			if (cur_job->chipSelectHandler != 0) {
				cur_job->chipSelectHandler(true);
			}
			cur_job->status = SSP_JOB_STATE_ACTIVE;
			/* Fill FIFO */

			FillTxBuffer(device, cur_job, 5);
		}


	}

	//portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
	return;

	/* TODO remessure Max. execution time:  ???? cycles */
	/* TODO remessure Average execution time: ???? cycles */
	/* TODO optimize for performance */
}


void FillTxBuffer(LPC_SSP_T *device, ssp_job_t *job, uint8_t maxPerFill) {
	// The buffer can hold maximum 8 bytes
	uint16_t processEnd;
	if ((job->framesToProcess - job->framesProcessed) >= maxPerFill) {
		processEnd = job->framesProcessed + maxPerFill;
	} else {
		processEnd = job->framesToProcess;
	}

	uint32_t temp;
	// try to fill in as much as possible into TX Buffer
	while (((device->SR & SSP_STAT_TNF)) && (job->framesProcessed < processEnd))
	{
		if (job->framesProcessed < job->bytes_to_send) {
			temp = job->array_to_send[job->framesProcessed];
		} else {
			temp = 0xFF;
		}
		job->framesProcessed++;
		device->DR = temp;
	}
}

ssp_jobdef_ret_t ssp_add_job2( ssp_busnr_t busNr,
							   uint8_t *array_to_send,
							   uint16_t bytes_to_send,
							   uint8_t *array_to_store,
							   uint16_t bytes_to_read,
							   uint8_t **job_status,
							   void(*chipSelectHandler)(bool select)) {
	uint8_t positionNewJob;
	ssp_jobs_t *jobs = &ssp_jobs[busNr];
	LPC_SSP_T *device;
	
	if (busNr == SSP_BUS0) {
		device = LPC_SSP0;
	} else if (busNr == SSP_BUS1) {
		device = LPC_SSP1;
	} else {
		return SSP_WRONG_BUSNR;
	}

	if (jobs->ssp_initialized == false)
	{
		/* SSP is not initialized - return */
		return SSP_JOB_NOT_INITIALIZED;
	}

	if (jobs->jobs_pending >= SSP_MAX_JOBS)
	{
		/* Maximum amount of jobs stored, job can't be added! */
		/* This is possibly caused by a locked interrupt -> remove all jobs and re-init SSP */
		//taskENTER_CRITICAL();
		jobs->ssp_job_error_counter++;
		jobs->jobs_pending = 0; /* Delete jobs */
		ssp01_init(); /* Reinit SSP   make re-init per SSP nr possible here !?*/
		//taskEXIT_CRITICAL();
		return SSP_JOB_BUFFER_OVERFLOW;
	}

	// taskENTER_CRITICAL();		TODO: need for real multithreading.!?!?
	{
		positionNewJob = (jobs->current_job + jobs->jobs_pending) % SSP_MAX_JOBS;
		ssp_job_t *newJob = &(jobs->job[positionNewJob]);

		newJob->framesToProcess = bytes_to_send + bytes_to_read;
		newJob->framesProcessed = 0;
		newJob->rxFrameIdx = 0;
		jobs->job[positionNewJob].array_to_send = array_to_send;
		jobs->job[positionNewJob].bytes_to_send = bytes_to_send;
		//jobs->job[positionNewJob].bytes_sent = 0;
		jobs->job[positionNewJob].array_to_read = array_to_store;
		//jobs->job[positionNewJob].bytes_to_read = bytes_to_read;
		jobs->job[positionNewJob].bytes_read = 0;
		//jobs->job[position].device = chip;
		jobs->job[positionNewJob].chipSelectHandler = chipSelectHandler;
		jobs->job[positionNewJob].status = SSP_JOB_STATE_PENDING;

//		if (bytes_to_send > 0)
//		{
//			/* Job contains transfer and read eventually */
//			jobs->job[positionNewJob].dir_tx = true;
//		}
//		else
//		{
//			/* Job contains readout only - transfer part is skipped */
//			jobs->job[positionNewJob].dir_tx = false;
//		}

		if (jobs->jobs_pending == 0)
		{
			/* if no jobs pending, start the Communication.*/
			/* Select device */
			if (chipSelectHandler != 0) {
				chipSelectHandler(true);
			}
			newJob->status = SSP_JOB_STATE_ACTIVE;
			ADO_SSP_DUMP_RX(device);
			/* Fill FIFO */
			FillTxBuffer(device, newJob,5);
		}

		jobs->jobs_pending++;
	}

	/* Set pointer to job bus_status if necessary */
	if (job_status != NULL)
	{
		*job_status = (uint8_t *) &(jobs->job[positionNewJob].status);
	}

	// taskEXIT_CRITICAL();	TODO needed for real multithreading
	return SSP_JOB_ADDED; /* Job added successfully */
}

//
//void DumpSspJobs(uint8_t bus) {
//	bus = bus & 0x01;
//	printf("sspjobs[%d] cur: %d, pen: %d, stat\n", bus, ssp_jobs[bus].current_job, ssp_jobs[bus].jobs_pending, ssp_jobs[bus].bus_status );
//	for (int i= 0; i<SSP_MAX_JOBS; i++) {
//		printf("%s[%d]: ", ((i==ssp_jobs[bus].current_job)?"c->":"   "), i);
//		printf("st:%d dir:%d tx:%d/%d rx:%d/%d\n", ssp_jobs[bus].job[i].status, ssp_jobs[bus].job[i].dir, ssp_jobs[bus].job[i].bytes_sent,ssp_jobs[bus].job[i].bytes_to_send, ssp_jobs[bus].job[i].bytes_read, ssp_jobs[bus].job[i].bytes_to_read );
//	}
//}
//
