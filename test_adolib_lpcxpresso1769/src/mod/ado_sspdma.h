/*
 * stc_spi.h
 *
 *  Created on: 07.10.2012
 *      Author: Andi, Robert
 */

#ifndef ADO_SSPDMA_H_
#define ADO_SSPDMA_H_

#include <stdint.h>
#include <chip.h>

#define ADO_SSP_MAXJOBS 		(16)
#define ADO_SSP_RXDMACHANNEL0	1
#define ADO_SSP_RXDMACHANNEL1	3
#define ADO_SSP_TXDMACHANNEL0	2
#define ADO_SSP_TXDMACHANNEL1	4

typedef enum ado_sspid_e
{
	ADO_SSP0 = 0, ADO_SSP1 = 1
} ado_sspid_t;

typedef enum ado_sspstatus_e
{
	SSP_JOB_STATE_DONE = 0, SSP_JOB_STATE_PENDING, SSP_JOB_STATE_ACTIVE, SSP_JOB_STATE_SSP_ERROR, SSP_JOB_STATE_DEVICE_ERROR, SSP_JOB_BUFFER_OVERFLOW		// TODO Check usage....
} ado_sspstatus_t;

//typedef enum ssp_addjob_ret_e
//{ /* Return values for ssp_add_job */
//	SSP_JOB_ADDED = 0, SSP_JOB_BUFFER_OVERFLOW, SSP_JOB_MALLOC_FAILED, SSP_JOB_ERROR, SSP_JOB_NOT_INITIALIZED, SSP_WRONG_BUSNR
//} ssp_jobdef_ret_t;

void ADO_SSP_Init(ado_sspid_t sspId, uint32_t bitRate);

//void ADO_SSP_AddJobSingle();
void ADO_SSP_AddJobScattered(ado_sspid_t sspId, uint8_t *txData, uint8_t *rxData, uint16_t bytes_to_send, uint16_t bytes_to_read, void(*finished)(ado_sspstatus_t s, uint8_t *rx, uint16_t cnt));



//ssp_jobdef_ret_t ssp_add_job2( ssp_busnr_t busNr,
//							   uint8_t *array_to_send,
//							   uint16_t bytes_to_send,
//							   uint8_t *array_to_store,
//							   uint16_t bytes_to_read,
//							   uint8_t **job_status,
//							   void(*chipSelectHandler)(bool select));


#endif /* ADO_SSPDMA_H_ */
