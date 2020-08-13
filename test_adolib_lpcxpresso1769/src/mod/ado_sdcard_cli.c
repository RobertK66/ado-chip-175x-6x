/*
 * ado_sdcard_cli.c
 *
 *  Created on: 13.08.2020
 *      Author: Robert
 */

#include "ado_sdcard_cli.h"
#include "ado_sdcard.h"

#include <ado_crc.h>
#include <ado_sspdma.h>
#include <mod/cli.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Command Prototypes
void SdcSwitchCardCmd(int argc, char *argv[]);
void SdcInitCmd(int argc, char *argv[]);
void SdcReadCmd(int argc, char *argv[]);
void SdcEditCmd(int argc, char *argv[]);
void SdcWriteCmd(int argc, char *argv[]);
void SdcCheckFatCmd(int argc, char *argv[]);

static void **sdCards;
static int  cardCnt = 0;
static void *selectedCard = 0;

#define ADO_SDC_DUMPMODE_RAW             0
#define ADO_SDC_DUMPMODE_FAT_BL0         1
#define ADO_SDC_DUMPMODE_FAT_BOOTSECTOR  2

uint16_t         sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
uint16_t         sdcDumpLines = 0;
uint32_t      sdcCurRwBlockNr;
uint8_t           sdcRwData[520];
uint8_t           sdcEditBlock[512];


void AdoSdcardCliInit(int cardC, void *cardV[]) {
    if (cardC>0) {
        cardCnt = cardC;
        sdCards = cardV;
        selectedCard = sdCards[0];
        CliRegisterCommand("sdcCard", SdcSwitchCardCmd);
        CliRegisterCommand("sdcInit", SdcInitCmd);
        CliRegisterCommand("sdcRead", SdcReadCmd);
        CliRegisterCommand("sdcEdit", SdcEditCmd);
        CliRegisterCommand("sdcWrite", SdcWriteCmd);
        CliRegisterCommand("sdcFAT", SdcCheckFatCmd);
    }
}

void SdcSwitchCardCmd(int argc, char *argv[]) {
    if (argc == 1) {
        int cardNr = atoi(argv[0]);
        if ((cardNr >= 0) && (cardNr < cardCnt)) {
            selectedCard = sdCards[cardNr];
            printf("SDCard %d selected.\n", cardNr);
        }
    }
}


void SdcInitCmd(int argc, char *argv[]) {
//    LPC_SSP_T *sspBase = 0;
//    ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards
//
//    if (sdcBusNr == ADO_SSP0) {
//        sspBase = LPC_SSP0;
//    } else if  (sdcBusNr == ADO_SSP1) {
//        sspBase = LPC_SSP1;
//    }
    if (selectedCard != 0) {
        // Reset module state and initiate the card init sequence
//        SdcCardinitialize(selectedCard);
//        SdCard[sdcBusNr].sdcType = ADO_SDC_CARD_UNKNOWN;
//        SdCard[sdcBusNr].sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
//        SdcSendCommand(sdcBusNr, ADO_SDC_CMD0_GO_IDLE_STATE, ADO_SDC_CARDSTATUS_INIT_RESET, 0);
        SdcCardinitialize(selectedCard);
    }
}



void SdcReadBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("Block read success\n");
        // -> trigger dump
    } else {
        // TODO or any other errors .....
        printf("Card not ready to receive commands....\n");
    }
}

void SdcReadCmd(int argc, char *argv[]){
   // uint32_t arg = 0;
   // ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards

    if (argc > 0) {
        //adr = atoi(argv[0]);
        sdcCurRwBlockNr = strtol(argv[0], NULL, 0);     //This allows '0x....' hex entry!
    }

    SdcReadBlockAsync(selectedCard, sdcCurRwBlockNr, sdcRwData, SdcReadBlockFinished);

//    if (SdCard[sdcBusNr].sdcType == ADO_SDC_CARD_20SD) {
//        // SD Cards take byte addresses as argument
//        arg = sdcCurRwBlockNr << 9;
//    } else {
//        // 2.0 HC or XC take block addressing with fixed 512 byte blocks as argument
//        arg = sdcCurRwBlockNr;
//    }
//
//    if (SdCard[sdcBusNr].sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
//        sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
//        SdcSendCommand(sdcBusNr, ADO_SDC_CMD17_READ_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_READ_SBCMD, arg);
//    } else {
//        printf("Card not ready to receive commands....\n");
//    }
}

void SdcWriteBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("Block write success\n");
        // -> trigger dump
    } else {
        // TODO or any other errors .....
        printf("Card not ready to receive commands....\n");
    }
}

void SdcWriteCmd(int argc, char *argv[]) {
   // ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards

    sdcRwData[0] = 0xFE;                            // Data Block Start token
    memcpy(sdcRwData + 1, sdcEditBlock, 512);
    uint16_t crc = CRC16_XMODEM(sdcRwData, 512);
    sdcRwData[513] = (uint8_t)(crc >> 8);
    sdcRwData[514] = (uint8_t)crc;

    if (argc > 0) {
         sdcCurRwBlockNr = strtol(argv[0], NULL, 0);        //This allows '0x....' hex entry!
    }

    SdcWriteBlockAsync(selectedCard, sdcCurRwBlockNr, sdcRwData, SdcWriteBlockFinished);

//
//    if (SdCard[sdcBusNr].sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
//        sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
//        SdcSendCommand(sdcBusNr, ADO_SDC_CMD24_WRITE_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_WRITE_SBCMD, sdcCurRwBlockNr);
//    } else {
//        printf("Card not ready to receive commands....\n");
//    }

}

void SdcEditCmd(int argc, char *argv[]) {
    //ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards

    uint16_t adr = 0;
    char *content = "Test Edit from LPC1769";

    if (argc > 0) {
         adr = strtol(argv[0], NULL, 0);        //This allows '0x....' hex entry!
         adr &= 0x01FF;
    }

    if (argc > 1) {
        content = argv[1];
    }

    int i = 0;
    for (uint8_t *ptr = (uint8_t*)(sdcEditBlock + adr); ptr < (uint8_t*)(sdcEditBlock + adr + strlen(content)); ptr++) {
        if (ptr < (sdcEditBlock + 512)) {
            *ptr = content[i++];
        }
    }
    printf("Block modified: \n");
    sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
    //SdCard[sdcBusNr].sdcWaitLoops = 1000;
    sdcDumpLines = 16;
}

void SdcReadFATBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("FAT Block read success\n");
        // -> trigger dump
    } else {
        // TODO or any other errors .....
        printf("Card not ready to receive commands....\n");
    }
}

void SdcCheckFatCmd(int argc, char *argv[]) {

    SdcReadBlockAsync(selectedCard, 0, sdcRwData, SdcReadFATBlockFinished);

//    ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards
//
//    if (SdCard[sdcBusNr].sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
////        uint32_t arg;
////        sdcCurRwBlockNr = 0;
////        if (sdcType == ADO_SDC_CARD_20SD) {
////            // SD Cards take byte addresses as argument
////            arg = sdcCurRwBlockNr << 9;
////        } else {
////            // 2.0 HC or XC take block addressing with fixed 512 byte blocks as argument
////            arg = sdcCurRwBlockNr;
////        }
//        sdcDumpMode = ADO_SDC_DUMPMODE_FAT_BL0;
//        SdcReadBlock(sdcBusNr, 0);
//    } else {
//        printf("Card not ready to receive commands....\n");
//    }
}

