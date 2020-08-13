/*
 * ado_sdcard_cli.c
 *
 *  Created on: 13.08.2020
 *      Author: Robert
 */

#include "ado_sdcard_cli.h"

#include <mod/cli.h>

//void SdcPowerupCmd(int argc, char *argv[]);
extern void SdcInitCmd(int argc, char *argv[]);
extern void SdcReadCmd(int argc, char *argv[]);
extern void SdcEditCmd(int argc, char *argv[]);
extern void SdcWriteCmd(int argc, char *argv[]);
extern void SdcCheckFatCmd(int argc, char *argv[]);


void AdoSdcardCliInit(void) {
    CliRegisterCommand("sdcInit", SdcInitCmd);
    CliRegisterCommand("sdcRead", SdcReadCmd);
    CliRegisterCommand("sdcEdit", SdcEditCmd);
    CliRegisterCommand("sdcWrite", SdcWriteCmd);
    CliRegisterCommand("sdcFAT", SdcCheckFatCmd);
}

