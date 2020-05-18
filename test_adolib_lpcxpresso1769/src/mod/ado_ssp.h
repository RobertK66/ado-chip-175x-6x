/*
 * stc_spi.h
 *
 *  Created on: 07.10.2012
 *      Author: Andi, Robert
 */

#ifndef ADO_SPI_H_
#define ADO_SPI_H_

#include <stdint.h>
#include <chip.h>

/* Max SPI buffer length */
#define SSP_MAX_JOBS (16)

typedef enum ssp_busnr_e
{
	SSP_BUS0 = 0, SSP_BUS1 = 1
} ssp_busnr_t;

enum
{
	SSP_JOB_STATE_DONE = 0, SSP_JOB_STATE_PENDING, SSP_JOB_STATE_ACTIVE, SSP_JOB_STATE_SSP_ERROR, SSP_JOB_STATE_DEVICE_ERROR
};

typedef enum ssp_addjob_ret_e
{ /* Return values for ssp_add_job */
	SSP_JOB_ADDED = 0, SSP_JOB_BUFFER_OVERFLOW, SSP_JOB_MALLOC_FAILED, SSP_JOB_ERROR, SSP_JOB_NOT_INITIALIZED, SSP_WRONG_BUSNR
} ssp_jobdef_ret_t;

void ssp01_init(void);

ssp_jobdef_ret_t ssp_add_job2( ssp_busnr_t busNr,
							   uint8_t *array_to_send,
							   uint16_t bytes_to_send,
							   uint8_t *array_to_store,
							   uint16_t bytes_to_read,
							   uint8_t **job_status,
							   void(*chipSelectHandler)(bool select));

void DumpSspJobs(uint8_t bus);

#endif /* ADO_SPI_H_ */
