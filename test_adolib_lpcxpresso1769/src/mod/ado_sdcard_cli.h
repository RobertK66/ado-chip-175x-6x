/*
 * ado_sdcard_cli.h
 *
 *  Created on: 13.08.2020
 *      Author: Robert
 */

#ifndef MOD_ADO_SDCARD_CLI_H_
#define MOD_ADO_SDCARD_CLI_H_

void AdoSdcardCliInit(int cardC);
void AdoCliMain(void);  //TODO: move this to CLI module !? Feature to Dump a lot of lines without overstressing the Tx Buffer ....

#endif /* MOD_ADO_SDCARD_CLI_H_ */
