/*
 * ado_sdcard.h
 *
 *  Created on: 16.05.2020
 *      Author: Robert
 */

#ifndef MOD_ADO_SDCARD_H_
#define MOD_ADO_SDCARD_H_

#include <ado_sspdma.h>

void SdcInit(ado_sspid_t sspId, void(*csHandler)(bool select));
void SdcMain(ado_sspid_t bus);



#endif /* MOD_ADO_SDCARD_H_ */
