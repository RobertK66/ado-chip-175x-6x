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

#define ADO_SSP_MAXJOBS 		(13)
#define ADO_SSP_RXDMACHANNEL0	7
#define ADO_SSP_TXDMACHANNEL0	6
#define ADO_SSP_RXDMACHANNEL1	5
#define ADO_SSP_TXDMACHANNEL1	4

typedef enum ado_sspid_e
{
	ADO_SSP0 = 0, ADO_SSP1 = 1
} ado_sspid_t;

typedef enum ado_sspstatus_e
{
	SSP_JOB_STATE_NEW = 0, ADO_SSP_JOBDONE, SSP_JOB_STATE_ERROR, SSP_JOB_BUFFER_OVERFLOW
} ado_sspstatus_t;

#define AdoSSP_FinishedHandler(name) void(*name)(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize)
#define AdoSSP_ActivateHandler(name) void(*name)(uint32_t context)




void ADO_SSP_Init(ado_sspid_t sspId, uint32_t bitRate);
void ADO_SSP_AddJob( uint32_t 	context,
					 ado_sspid_t sspId,
					 uint8_t     *txData,
					 uint8_t     *rxData,
					 uint16_t	bytes_to_send,
					 uint16_t 	bytes_to_read,
					 AdoSSP_FinishedHandler(finished),
					 AdoSSP_ActivateHandler(activate) );


#endif /* ADO_SSPDMA_H_ */
