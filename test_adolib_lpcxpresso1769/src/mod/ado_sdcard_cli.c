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

// CLI Callback Prototypes
void SdcReadBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len);
void SdcWriteBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len);
void SdcReadFATBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len);
void SdcReadFATBootSectorBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len);

static int  cardCnt = 0;            // Count of available cards
static void **sdCards;              // Pointers to available sdCards
static void *selectedCard = 0;      // Currently selected card (pointer)

// FAT32 defines
typedef struct partitionInfo_Structure {
    uint8_t         status;             //0x80 - active partition
    uint8_t         headStart;          //starting head
    uint16_t        cylSectStart;       //starting cylinder and sector.
    uint8_t         type;               //partition type
    uint8_t         headEnd;            //ending head of the partition
    uint16_t        cylSectEnd;         //ending cylinder and sector
    uint32_t        firstSector;        //total sectors between MBR & the first sector of the partition
    uint32_t        sectorsTotal;       //size of this partition in sectors
} __attribute__((packed)) partitionInfo_t;

typedef struct MBRinfo_Structure {
    unsigned char    nothing[440];          // ignore, placed here to fill the gap in the structure
    uint32_t         devSig;                // Device Signature (for Windows)
    uint16_t         filler;                // 0x0000
    partitionInfo_t  partitionData[4];      // partition records (16x4)
    uint16_t         signature;             // 0xaa55
} __attribute__((packed))  MBRinfo_t ;

//Structure to access volume boot sector data (its sector 0 on unpartitioned devices or the first sector of a partition)
typedef struct bootSector_S {
    uint8_t jumpBoot[3];
    uint8_t OEMName[8];
    // FAT-BIOS Param Block common part (DOS2.0)
    uint16_t bytesPerSector;             // default: 512
    uint8_t  sectorPerCluster;           // 1,2,4,8,16,...
    uint16_t reservedSectorCount;        // 32 for FAT32 ?
    uint8_t  numberofFATs;               // 2 ('almost always')
    uint16_t rootEntryCount;             // 0 for FAT32
    uint16_t totalSectors_F16;           // 0 for FAT32
    uint8_t mediaType;
    uint16_t FATsize_F16;                // 0 for FAT32
    // DOS3.31 extensions
    uint16_t sectorsPerTrack;
    uint16_t numberofHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors_F32;
    // FAT32 Extended Bios Param Block
    uint32_t FATsize_F32;               // count of sectors occupied by one FAT
    uint16_t extFlags;
    uint16_t FSversion;                 // 0x0000 (defines version 0.0)
    uint32_t rootCluster;               // first cluster of root directory (=2)
    uint16_t FSinfo;                    // sector number of FSinfo structure (=1)
    uint16_t BackupBootSector;
    uint8_t  reserved[12];
    // FAT16 Extended Bios Param Block (offset moved by F32 Extensions)
    uint8_t  driveNumber;
    uint8_t  reserved1;
    uint8_t  bootSignature;
    uint32_t volumeID;
    uint8_t  volumeLabel[11];           // "NO NAME "
    uint8_t  fileSystemType[8];         // "FAT32"
    uint8_t  bootData[420];
    uint16_t signature;                 // 0xaa55
} __attribute__((packed)) bootSector_t;

uint8_t          sdcRwData[520];        // The Client Buffer for Writing and reading to a SDCard. Currently we need 512+3 bytes here (1 token + 2checksum bytes)
uint8_t          sdcEditBlock[512];     // This is the place where we show/dump received block and edit it in RAM for the writeBlock command,

// Variables/Defines to control the CLI output fordistributing and delaying the lines with help of mainloop.
// TODO: -> move to CLI as general feature !?
#define ADO_SDC_DUMPMODE_RAW             0
#define ADO_SDC_DUMPMODE_FAT_BL0         1
#define ADO_SDC_DUMPMODE_FAT_BOOTSECTOR  2

uint16_t         sdcWaitLoops = 0;
uint16_t         sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
uint16_t         sdcDumpLines = 0;
uint32_t         sdcCurRwBlockNr;


void AdoSdcardCliInit(int cardC, void *cardV[]) {
    if (cardC>0) {
        // Lets remember the array of SdCardPointers
        cardCnt = cardC;
        sdCards = cardV;
        // switch to the first card in Array
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
    if (selectedCard != 0) {
        SdcCardinitialize(selectedCard);
    }
}


void SdcReadCmd(int argc, char *argv[]){
    if (argc > 0) {
        sdcCurRwBlockNr = strtol(argv[0], NULL, 0);     //This allows '0x....' hex entry!
    }
    SdcReadBlockAsync(selectedCard, sdcCurRwBlockNr, sdcRwData, SdcReadBlockFinished);
}

void SdcReadBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("Block Nr  %08X read success.\n", blockNr);
        // -> keep data and trigger CLI dump
        memcpy(sdcEditBlock, data, 512);
        sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
        sdcWaitLoops = 1000;
        sdcDumpLines = 16;
    } else {
        // TODO: check for types of errors .....
        printf("Card error....\n");
    }
}

void SdcWriteCmd(int argc, char *argv[]) {
    // Prepare the write data. TODO: move to biniary part of module!
    sdcRwData[0] = 0xFE;                            // Data Block Start token.
    memcpy(sdcRwData + 1, sdcEditBlock, 512);
    uint16_t crc = CRC16_XMODEM(sdcRwData, 512);
    sdcRwData[513] = (uint8_t)(crc >> 8);
    sdcRwData[514] = (uint8_t)crc;

    if (argc > 0) {
        sdcCurRwBlockNr = strtol(argv[0], NULL, 0);        //This allows '0x....' hex entry!
    }
    SdcWriteBlockAsync(selectedCard, sdcCurRwBlockNr, sdcRwData, SdcWriteBlockFinished);
}

void SdcWriteBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("Block Nr  %08X write success.\n", blockNr);
    } else {
        // TODO or any other errors .....
        printf("Card error....\n");
    }
}

void SdcEditCmd(int argc, char *argv[]) {
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

    printf("Block modified.\n");
    // Triger dump of modified data in RAM.
    sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
    sdcWaitLoops = 1000;
    sdcDumpLines = 16;
}

void SdcCheckFatCmd(int argc, char *argv[]) {
    // Read Block 0 and analyse if its a BOOT sector or a Partition Table.
    SdcReadBlockAsync(selectedCard, 0, sdcRwData, SdcReadFATBlockFinished);
}

void SdcReadFATBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("FAT Block %08X read success.\n\n", blockNr);
        // -> trigger dump
        memcpy(sdcEditBlock, data, 512);
        sdcDumpMode = ADO_SDC_DUMPMODE_FAT_BL0;
        sdcWaitLoops = 1000;
        sdcDumpLines = 16;
    } else {
        // TODO or any other errors .....
        printf("Card error.\n");
    }
}

// The according SdcReadBlockAsync is triggered after dump of FAT partition table is finished.
void SdcReadFATBootSectorBlockFinished (sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len) {
    if (result == SDC_RES_SUCCESS) {
        printf("\nFAT Boot Sector %08X read success\n", blockNr);
        // -> trigger dump
        memcpy(sdcEditBlock, data, 512);
        sdcDumpMode = ADO_SDC_DUMPMODE_FAT_BOOTSECTOR;
        sdcWaitLoops = 1000;
        sdcDumpLines = 16;
    } else {
        // TODO or any other errors .....
        printf("Card error.\n");
    }
}

// This is a temporary place to have a mainloop handling the dump feature. Will be moved to CLI Module later on....
void AdoCliMain(void) {
    if (sdcWaitLoops == 0) {
          // Do other stuff in Mainloop -> this is CLI move out later
          if (sdcDumpLines > 0) {
              if (sdcDumpMode == ADO_SDC_DUMPMODE_RAW) {
                    uint8_t *ptr = &sdcEditBlock[32*(16-sdcDumpLines)];
                    char hex[100];
                    char asci[33];
                    for (int i=0;i<32;i++) {
                        char c = *(ptr+i);
                        sprintf(&hex[i*3],"%02X ", c);
                        if (c > 0x20) {
                            asci[i] = c;
                        } else {
                            asci[i] = '.';
                        }
                    }
                    asci[32] = 0;
                    hex[96] = 0;
                    printf("%03X-%08X: %s %s\n", (sdcCurRwBlockNr>>23), (sdcCurRwBlockNr<<9) + 32*(16-sdcDumpLines), hex, asci);
                    sdcWaitLoops = 3000;
                    sdcDumpLines--;
              } else if (sdcDumpMode == ADO_SDC_DUMPMODE_FAT_BL0) {
                  if (sdcEditBlock[0x1fe] == 0x55 && sdcEditBlock[0x1ff] == 0xAA) {
                      if ((sdcEditBlock[0] == 0xE9) || (sdcEditBlock[0] == 0xEB)) {
                          // This looks like a 'Boot Sector' - so its an 'unpartitioned' card.
                          sdcDumpMode = ADO_SDC_DUMPMODE_FAT_BOOTSECTOR;
                          return;     // Skip to other Dumpmode with next call.
                      } else {
                          // This is no Boot Sector.Then it should be a 'Master Boot Record'.
                            MBRinfo_t *p = (MBRinfo_t *)sdcEditBlock;
                            uint32_t  bootSector = 0;
                            printf("MBR-Sign: %04X DevSig: %08X\n", p->signature, p->devSig);
                            for (int i=3; i>-1; i--) {
                                if ( p->partitionData[i].type != 0x00) {
                                    printf("MBR-Part[%d]-Status      : %02X\n", i, p->partitionData[i].status);
                                    printf("MBR-Part[%d]-Type        : %02X\n", i, p->partitionData[i].type);
                                    printf("MBR-Part[%d]-FirstSector : %08X\n", i, p->partitionData[i].firstSector);
                                    printf("MBR-Part[%d]-SectorsTotal: %08X\n", i, p->partitionData[i].sectorsTotal);
                                    printf("MBR-Part[%d]-CHS Start: %02X %04X\n", i, p->partitionData[i].headStart, p->partitionData[i].cylSectStart );
                                    printf("MBR-Part[%d]-CHS End  : %02X %04X\n", i, p->partitionData[i].headEnd, p->partitionData[i].cylSectEnd );
                                    // We use 'lowest partition' boot sector to continue FAT analysis.
                                    bootSector =  p->partitionData[i].firstSector;
                                } else {
                                    printf("MBR-Part[%d]: empty\n", i);
                                }
                            }
                            if (bootSector != 0) {
                                // Trigger read of boot sektor
                                SdcReadBlockAsync(selectedCard, bootSector, sdcRwData, SdcReadFATBootSectorBlockFinished);
                            }
                      }
                  } else {
                      printf("This block does not contain Boot Block marker!\n");
                  }
                  sdcDumpLines = 0;
              } else if (sdcDumpMode == ADO_SDC_DUMPMODE_FAT_BOOTSECTOR) {
                  bootSector_t *p = (bootSector_t *)sdcEditBlock;

                  if (((sdcEditBlock[0] == 0xE9) || (sdcEditBlock[0] == 0xEB)) &&
                       p->signature == 0xAA55) {
                       // This is a valid 'Boot Sector'
                     printf("FS     : %0.8s\n", p->fileSystemType);
                     printf("OEMName: %0.8s\n", p->OEMName);
                     printf("%d Sectors (%d bytes) per cluster\n", p->sectorPerCluster, p->bytesPerSector);

                     printf("hidden : %08X total: %08X\n", p->hiddenSectors, p->totalSectors_F32);
                     printf("res : %04X rootCl: %08X rootEntryCnt: %04X\n", p->reservedSectorCount, p->rootCluster, p->rootEntryCount);
                     printf("NrFAT : %02X FATSize: %08X \n", p->numberofFATs, p->FATsize_F32);

                     printf("-> first Data Sector: %08X\n",  p->hiddenSectors +  p->reservedSectorCount + p->numberofFATs * p->FATsize_F32);

                   } else {
                       printf("This Block contains no Boot Sector!\n");
                   }
                 sdcDumpLines = 0;
              }
          }
      } else {
          sdcWaitLoops--;
      }
}
