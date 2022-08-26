/*
 * ado_mram_cli.c
 *
 *  Created on: 10.08.2020
 *      Author: Robert
 */
#include "ado_mram_cli.h"

#include <chip.h>
#include <stdio.h>
#include <stdlib.h>
#include <mod/cli.h>
#include <mod/ado_mram.h>

// prototypes
void ReadMramCmd(int argc, char *argv[]);
void WriteMramCmd(int argc, char *argv[]);
void ReadMramFinished (uint8_t chipIdx, mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);
void WriteMramFinished (uint8_t chipIdx, mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len);


uint8_t  cliData[MRAM_MAX_WRITE_SIZE];

void AdoMramCliInit() {
   // Extra call with invalid chipIdx to register commands -> make something more elegant ;-)
   CliRegisterCommand("mRead", ReadMramCmd);
   CliRegisterCommand("mWrite",WriteMramCmd);
}


void ReadMramCmd(int argc, char *argv[]) {


    // CLI params to binary params
    uint8_t  idx = atoi(argv[0]);
    uint32_t adr = atoi(argv[1]);
    uint32_t len = atoi(argv[2]);
    if (len > MRAM_MAX_READ_SIZE) {
        len = MRAM_MAX_READ_SIZE;
    }
    if (idx >= MRAM_CHIP_CNT) {
       idx = 0;
    }

    //mram_chip_t *mramPtr =  &MramChipStates[idx];

    // Binary Command
    MramReadAsync(idx, adr, cliData, len, ReadMramFinished);
}


void ReadMramFinished (uint8_t chipIdx, mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len) {
    if (result == MRAM_RES_SUCCESS) {
        printf("MRAM read at %04X:\n", adr);
        for (int i=0; i<len; i++ ) {
            printf("%02X ", ((uint8_t*)data)[i]);
            if ((i+1)%8 == 0) {
                printf("   ");
            }
        }
        printf("\n");
    } else {
        printf("Error reading MRAM: %d\n", result);
    }
}

void WriteMramCmd(int argc, char *argv[]) {
    if (argc != 4) {
        printf("uasge: cmd <chipidx> <adr> <databyte> <len> \n" );
        return;
    }

    // CLI params to binary params
    uint8_t  idx = atoi(argv[0]);
    uint32_t adr = atoi(argv[1]);
    uint8_t byte = atoi(argv[2]);
    uint32_t len = atoi(argv[3]);
    if (len > MRAM_MAX_WRITE_SIZE) {
        len = MRAM_MAX_WRITE_SIZE;
    }
    if (idx > MRAM_CHIP_CNT) {
        idx = 0;
    }

    //mram_chip_t *mramPtr =  &MramChipStates[idx];

    for (int i=0;i<len;i++){
        cliData[i] = byte;
    }

    // Binary Command
    MramWriteAsync(idx, adr, cliData, len,  WriteMramFinished);
}

void WriteMramFinished (uint8_t chipIdx, mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len) {
    if (result == MRAM_RES_SUCCESS) {
        printf("%d bytes written to mram at %04X \n", len, adr);
    } else {
        printf("Error reading MRAM: %d\n", result);
    }
}

