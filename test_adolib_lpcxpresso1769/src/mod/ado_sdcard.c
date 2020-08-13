/*
 * ado_sdcard.c
 *
 *  Created on: 16.05.2020
 *      Author: Robert
 */
#include "ado_sdcard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mod/cli.h>
#include <ado_crc.h>

#include "ado_sspdma.h"

//SD commands, many of these are not used here
typedef enum ado_sdc_cmd_e
{
 ADO_SDC_CMD0_GO_IDLE_STATE   		  =  0,
 ADO_SDC_CMD1_SEND_OP_COND            =  1,
 ADO_SDC_CMD8_SEND_IF_COND			  =  8,
 ADO_SDC_CMD9_SEND_CSD                =  9,
 ADO_SDC_CMD12_STOP_TRANSMISSION      =  12,
 ADO_SDC_CMD13_SEND_STATUS            =  13,
 ADO_SDC_CMD16_SET_BLOCK_LEN          =  16,
 ADO_SDC_CMD17_READ_SINGLE_BLOCK      =  17,
 ADO_SDC_CMD18_READ_MULTIPLE_BLOCKS   =  18,
 ADO_SDC_CMD24_WRITE_SINGLE_BLOCK     =  24,
 ADO_SDC_CMD25_WRITE_MULTIPLE_BLOCKS  =  25,
 ADO_SDC_CMD32_ERASE_BLOCK_START_ADDR =  32,
 ADO_SDC_CMD33_ERASE_BLOCK_END_ADDR   =  33,
 ADO_SDC_CMD38_ERASE_SELECTED_BLOCKS  =  38,
 ADO_SDC_CMD41_SD_SEND_OP_COND		  =  41,   //ACMD41
 ADO_SDC_CMD55_APP_CMD				  =  55,
 ADO_SDC_CMD58_READ_OCR				  =  58,
 ADO_SDC_CMD59_CRC_ON_OFF 			  =  59
} ado_sdc_cmd_t;

typedef enum ado_sdc_status_e
{
	ADO_SDC_CARDSTATUS_UNDEFINED = 0,
	ADO_SDC_CARDSTATUS_INIT_RESET,
	ADO_SDC_CARDSTATUS_INIT_RESET2,
	ADO_SDC_CARDSTATUS_INIT_CMD8,
	ADO_SDC_CARDSTATUS_INIT_ACMD41_1,
	ADO_SDC_CARDSTATUS_INIT_ACMD41_2,
	ADO_SDC_CARDSTATUS_INIT_ACMD41_3,
	ADO_SDC_CARDSTATUS_INIT_READ_OCR,
	ADO_SDC_CARDSTATUS_INITIALIZED,
	ADO_SDC_CARDSTATUS_READ_SBCMD,
	ADO_SDC_CARDSTATUS_READ_SBWAITDATA,
	ADO_SDC_CARDSTATUS_READ_SBWAITDATA2,
	ADO_SDC_CARDSTATUS_READ_SBDATA,
	ADO_SDC_CARDSTATUS_WRITE_SBCMD,
	ADO_SDC_CARDSTATUS_WRITE_SBDATA,
	ADO_SDC_CARDSTATUS_WRITE_BUSYWAIT,
	ADO_SDC_CARDSTATUS_WRITE_CHECKSTATUS,
	ADO_SDC_CARDSTATUS_ERROR = 9999
} ado_sdc_status_t;

typedef enum ado_sdc_cardtype_e
{
	ADO_SDC_CARD_UNKNOWN = 0,
	ADO_SDC_CARD_1XSD,				// V1.xx Card (not accepting CMD8 -> standard capacity
	ADO_SDC_CARD_20SD,				// V2.00 Card standard capacity
	ADO_SDC_CARD_20HCXC,			// V2.00 Card High Capacity HC or ExtendenCapacity XC
} ado_sdc_cardtype_t;


void DMAFinishedIRQ(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize);
void SdcSendCommand(ado_sspid_t bus, ado_sdc_cmd_t cmd, uint32_t jobContext, uint32_t arg);
void SdcReadBlock(ado_sspid_t bus, uint32_t blockNr);

//void SdcPowerupCmd(int argc, char *argv[]);
void SdcInitCmd(int argc, char *argv[]);
void SdcReadCmd(int argc, char *argv[]);
void SdcEditCmd(int argc, char *argv[]);
void SdcWriteCmd(int argc, char *argv[]);
void SdcCheckFatCmd(int argc, char *argv[]);


// SSP command level data
typedef struct ado_sdcars_s {
    ado_sdc_cardtype_t sdcType;             // = ADO_SDC_CARD_UNKNOWN;
    uint8_t          sdcCmdData[10];
    uint8_t          sdcCmdResponse[24];
    ado_sspid_t      sdcBusNr;              // = -1;
    ado_sdc_status_t sdcStatus;             // = ADO_SDC_CARDSTATUS_UNDEFINED;
    ado_sdc_status_t nextStatus;            // used by callback to switch state if SPI command finished its sending.
    bool             sdcCmdPending;         // = false;
    uint16_t         sdcWaitLoops;          // = 0;
    void             (*csHandler)(bool select);
} ado_sdcard_t;


static ado_sdcard_t SdCard[2];

//static uint8_t          sdcCmdData[10];
//static uint8_t          sdcCmdResponse[24];
//static ado_sspid_t 		sdcBusNr = -1;
//static ado_sdc_status_t sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
//static bool				sdcCmdPending = false;
//static uint16_t			sdcWaitLoops = 0;


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

#define ADO_SDC_DUMPMODE_RAW             0
#define ADO_SDC_DUMPMODE_FAT_BL0         1
#define ADO_SDC_DUMPMODE_FAT_BOOTSECTOR  2

extern uint16_t         sdcDumpMode; // = ADO_SDC_DUMPMODE_RAW;
extern uint16_t         sdcDumpLines; // = 0;
extern uint32_t 		sdcCurRwBlockNr;
extern uint8_t 			sdcRwData[520];
extern uint8_t 			sdcEditBlock[512];


//static ado_sdc_cardtype_t sdcType = ADO_SDC_CARD_UNKNOWN;

//
void ActivateCS(uint32_t context) {
    ado_sdcard_t *sdCard = (ado_sdcard_t *)context;
    if (sdCard->csHandler != 0) {
        sdCard->csHandler(true);
    }

//   if (sdcBusNr == ADO_SSP0) {
//       // 0-16
//       Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 16);
//   } else if  (sdcBusNr == ADO_SSP1) {
//       // 0-6
//       Chip_GPIO_SetPinOutLow(LPC_GPIO, 0, 6);
//   }
}
//
//void DeActivateCS() {
//    if (sdcBusNr == ADO_SSP0) {
//       // 0-16
//       Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 16);
//    } else if  (sdcBusNr == ADO_SSP1) {
//       // 0-6
//        Chip_GPIO_SetPinOutHigh(LPC_GPIO, 0, 6);
//   }
//}


void *SdcInit(ado_sspid_t bus,  void(*csHandler)(bool select)) {

	SdCard[bus].sdcBusNr = bus;
	SdCard[bus].csHandler = csHandler;
	SdCard[bus].sdcType = ADO_SDC_CARD_UNKNOWN;
	SdCard[bus].sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
	SdCard[bus].sdcCmdPending = false;
	SdCard[bus].sdcWaitLoops = 0;

//	CliRegisterCommand("sdcPow", SdcPowerupCmd);
//	CliRegisterCommand("sdcInit", SdcInitCmd);
//	CliRegisterCommand("sdcRead", SdcReadCmd);
//	CliRegisterCommand("sdcEdit", SdcEditCmd);
//	CliRegisterCommand("sdcWrite", SdcWriteCmd);
//	CliRegisterCommand("sdcFAT", SdcCheckFatCmd);
	return (void *)&SdCard[bus];
}

void SdcMain(void *pCard) {
    ado_sdcard_t *sdCard = (ado_sdcard_t *)pCard;

	if (!sdCard->sdcCmdPending) {
		// This is the sdc state engine reacting to a finished SPI job
		switch (sdCard->sdcStatus) {
		case ADO_SDC_CARDSTATUS_INIT_RESET:
			if (sdCard->sdcCmdResponse[1] == 0x01) {
				// Send CMD8
				SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD8_SEND_IF_COND, ADO_SDC_CARDSTATUS_INIT_CMD8, 0x000001AA);
			} else {
				// Try a 2nd time
				// All my SDHD cards switch to SPI mode with this repeat - regardless of power up sequence!
				SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD0_GO_IDLE_STATE, ADO_SDC_CARDSTATUS_INIT_RESET2, 0);
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_RESET2:
				if (sdCard->sdcCmdResponse[1] == 0x01) {
					// Send CMD8
					SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD8_SEND_IF_COND, ADO_SDC_CARDSTATUS_INIT_CMD8, 0x000001AA);
				} else {
				    // TODO: replace with error event
					printf("CMD GO_IDLE error.\n");
					sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
				}
				break;

		case ADO_SDC_CARDSTATUS_INIT_CMD8:
			if (sdCard->sdcCmdResponse[1] == 0x01) {
			    sdCard->sdcType = ADO_SDC_CARD_20SD;
				// Send CMD55 (-> ACMD41)
				SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD55_APP_CMD, ADO_SDC_CARDSTATUS_INIT_ACMD41_1, 0);
			} else {
			    sdCard->sdcType = ADO_SDC_CARD_1XSD;
				// TODO: init not ready here state engine missing....
			    // TODO: replace with error event
				printf("Sd Card version not implemented yet.\n");
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_1:
			if (sdCard->sdcCmdResponse[1] == 0x01) {
				// Send (CMD55 - ) ACMD41
				if (sdCard->sdcType == ADO_SDC_CARD_1XSD) {
					SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD41_SD_SEND_OP_COND, ADO_SDC_CARDSTATUS_INIT_ACMD41_2, 0);
				} else {
					SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD41_SD_SEND_OP_COND, ADO_SDC_CARDSTATUS_INIT_ACMD41_2, 0x40000000);
				}
			} else {
			    // TODO: replace with error event
				printf("CMD55 rejected\n");
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_2:
			if (sdCard->sdcCmdResponse[1] == 0x01) {
				// Init sequence is not finished yet(idle mode).
				// Lets wait some main loops and then retry the ACMD41
			    sdCard->sdcWaitLoops = 100;
			    sdCard->sdcStatus = ADO_SDC_CARDSTATUS_INIT_ACMD41_3;
			    printf("."); // debug indicator only....
			} else if (sdCard->sdcCmdResponse[1] == 0x00) {
				printf("\n");  // debug indicator only....
				// Send CMD58 to get R3 - OCR Response
				SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD58_READ_OCR, ADO_SDC_CARDSTATUS_INIT_READ_OCR, 0);
			} else {
			    // TODO: replace with error event.
				printf("Errors %02X to ACMD41\n", sdCard->sdcCmdResponse[1]);
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_ACMD41_3:
			if (sdCard->sdcWaitLoops > 0) {
			    sdCard->sdcWaitLoops--;
				if (sdCard->sdcWaitLoops == 0) {
					// Retry the ACMD41
					// Send CMD55 (-> ACMD41)
					SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD55_APP_CMD, ADO_SDC_CARDSTATUS_INIT_ACMD41_1, 0);
				}
			}
			break;

		case ADO_SDC_CARDSTATUS_INIT_READ_OCR:
			if (sdCard->sdcCmdResponse[1] == 0x00) {
				if ((sdCard->sdcType == ADO_SDC_CARD_20SD) &&
					((sdCard->sdcCmdResponse[2] & 0x40) != 0)) {
				    sdCard->sdcType = ADO_SDC_CARD_20HCXC;
				}
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_INITIALIZED;
				printf("Card (type %d) initialized.\n", sdCard->sdcType);  // Debug indicator only!? or success event !?
			} else {
			    // TODO: replace by error event
				printf("Errors %02X reading OCR.\n", sdCard->sdcCmdResponse[1]);
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_READ_SBCMD:
			// TODO: how many bytes can a response be delayed (Spec says 0..8 / 1..8 depending of card Type !?
			if ((sdCard->sdcCmdResponse[1] == 0x00) || (sdCard->sdcCmdResponse[2] == 0x00))  {
				// Lets wait for the start data token.
			    sdCard->sdcCmdPending = true;
			    sdCard->nextStatus = ADO_SDC_CARDSTATUS_READ_SBWAITDATA;
				ADO_SSP_AddJob((uint32_t)(sdCard), sdCard->sdcBusNr, sdCard->sdcCmdData, sdCard->sdcCmdResponse, 0 , 1, DMAFinishedIRQ, ActivateCS);
			} else {
			    // TODO: Signal Error event
				printf("Error %02X %02X with read block command\n",sdCard->sdcCmdResponse[1], sdCard->sdcCmdResponse[2] );
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_READ_SBWAITDATA:
			if (sdCard->sdcCmdResponse[0] == 0xFE) {
				// Now we can read all data bytes including 2byte CRC + 1 byte over-read as always to get the shift register in the card emptied....
			    sdCard->sdcCmdPending = true;
			    sdCard->nextStatus = ADO_SDC_CARDSTATUS_READ_SBDATA;
			    // TOOD: the sdcCmdData is provided by CLI client. lets move this out later....
				ADO_SSP_AddJob((uint32_t)(sdCard),  sdCard->sdcBusNr, sdCard->sdcCmdData, sdcRwData, 0 , 515, DMAFinishedIRQ, ActivateCS);
			} else {
				// Wait some mainloops before asking for data token again.
			    sdCard->sdcStatus = ADO_SDC_CARDSTATUS_READ_SBWAITDATA2;
			    sdCard->sdcWaitLoops = 10;
			}
			break;

		case ADO_SDC_CARDSTATUS_READ_SBWAITDATA2:
			if (sdCard->sdcWaitLoops > 0) {
			    sdCard->sdcWaitLoops--;
				if (sdCard->sdcWaitLoops == 0) {
					// Read 1 byte to check for data token
				    sdCard->sdcCmdPending = true;
				    sdCard->nextStatus = ADO_SDC_CARDSTATUS_READ_SBWAITDATA;
					ADO_SSP_AddJob((uint32_t)(sdCard), sdCard->sdcBusNr, sdCard->sdcCmdData, sdCard->sdcCmdResponse, 0 , 1,DMAFinishedIRQ,ActivateCS);
				}
			}
			break;

		case ADO_SDC_CARDSTATUS_READ_SBDATA:
		{
		    sdCard->sdcStatus = ADO_SDC_CARDSTATUS_INITIALIZED;

		    // TODO: This is already CLI data processiong -> move out to _cli.c somehow - or keep the crc feature in here !?
			uint16_t crc = CRC16_XMODEM(sdcRwData, 514);
			printf("\nData received Block 0x%08X: CRC ", sdcCurRwBlockNr);
			if (0 == crc) {
				printf("OK\n");
				memcpy(sdcEditBlock, sdcRwData, 512);
			    sdcDumpLines = 16;      // Start output
			} else {
				printf("ERROR\n");
			}
			break;
		}

		case ADO_SDC_CARDSTATUS_WRITE_SBCMD:
			if (sdCard->sdcCmdResponse[1] == 0x00) {
				// Now we can write all data bytes including 1 Start token and 2byte CRC. We expect 1 byte data response token....
			    sdCard->sdcCmdPending = true;
			    sdCard->nextStatus = ADO_SDC_CARDSTATUS_WRITE_SBDATA;
				printf("\nw");      // Debug print only
				// TODO: sdcReadWriteData is provided by CLI Client -> move this out later....
				ADO_SSP_AddJob((uint32_t)(sdCard),sdCard->sdcBusNr, sdcRwData, sdCard->sdcCmdResponse, 515 , 3, DMAFinishedIRQ, ActivateCS);
			} else {
			    // TODO: signal error event
				printf("Error %02X %02X with read block command\n",sdCard->sdcCmdResponse[1],sdCard->sdcCmdResponse[2] );
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_WRITE_SBDATA:
			if ((sdCard->sdcCmdResponse[0] & 0x1F) == 0x05) {
				// Data was accepted. now we wait until busy token is off again....
				// Read 1 byte to check for busy token
			    sdCard->sdcCmdPending = true;
			    sdCard->nextStatus = ADO_SDC_CARDSTATUS_WRITE_BUSYWAIT;
				ADO_SSP_AddJob((uint32_t)(sdCard),  sdCard->sdcBusNr,  sdCard->sdcCmdData,  sdCard->sdcCmdResponse, 0 , 1, DMAFinishedIRQ, ActivateCS);
				printf("5");
			} else {
			    // TODO: signal error event
				printf("Error %02X %02X with write data block\n",  sdCard->sdcCmdResponse[0],  sdCard->sdcCmdResponse[1] );
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		case ADO_SDC_CARDSTATUS_WRITE_BUSYWAIT:
			if ( sdCard->sdcCmdResponse[0] == 0x00) {
				// still busy !?
				// Read 1 byte to check for busy token
				// TODO: some mainloop wait here ....
			    sdCard->sdcCmdPending = true;
			    sdCard->nextStatus = ADO_SDC_CARDSTATUS_WRITE_BUSYWAIT;
				printf("o");
				ADO_SSP_AddJob((uint32_t)(sdCard), sdCard->sdcBusNr, sdCard->sdcCmdData, sdCard->sdcCmdResponse, 0 , 1, DMAFinishedIRQ,ActivateCS);
			} else {
				// busy is off now. Lets Check Status
				// Send CMD13
				SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD13_SEND_STATUS, ADO_SDC_CARDSTATUS_WRITE_CHECKSTATUS, 0);
			}
			break;

		case ADO_SDC_CARDSTATUS_WRITE_CHECKSTATUS:
			if (sdCard->sdcCmdResponse[1] == 0x00) {
			    sdCard->sdcStatus = ADO_SDC_CARDSTATUS_INITIALIZED;
				printf("!\n");  // debug only output os signal success event
			} else {
			    //TODO: signal error event
				printf("Error %02X %02X answer to CMD13\n", sdCard->sdcCmdResponse[1], sdCard->sdcCmdResponse[2] );
				sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
			}
			break;

		// Nothing needed here - only count down the main loop timer
		case ADO_SDC_CARDSTATUS_INITIALIZED:
			if (sdCard->sdcWaitLoops > 0) {
			    sdCard->sdcWaitLoops--;
			}
			break;

		default:
			break;
		}

		// Now lets do other stuff in main loop
		if (sdCard->sdcWaitLoops == 0) {
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
                    sdCard->sdcWaitLoops = 3000;
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
                                sdcDumpMode = ADO_SDC_DUMPMODE_FAT_BOOTSECTOR;
                                SdcReadBlock(sdCard->sdcBusNr, bootSector);
                            }
			            }
			        } else {
			            printf("Block 0 does not contain Boot Block marker!\n");
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
                       printf("Block %d contains no Boot Sector!\n");
                   }
			       sdcDumpLines = 0;
			    }
			}
		}
	}
}


void DMAFinishedIRQ(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize) {
    ado_sdcard_t *sdCard = (ado_sdcard_t *)context;

    //DeActivateCS();
    if (sdCard->csHandler != 0) {
        sdCard->csHandler(false);                       // Deselect the cards CS line
    }
	if (jobStatus == ADO_SSP_JOBDONE) {
		sdCard->sdcStatus = sdCard->nextStatus;         // Signal Status switch to mainloop.
	} else {
	    // TODO: Check, if mainloop signals this error as event in this case !?
		sdCard->sdcStatus = ADO_SDC_CARDSTATUS_ERROR;
	}
	sdCard->sdcCmdPending = false;
}


void SdcReadBlock(ado_sspid_t bus, uint32_t blockNr) {
    sdcCurRwBlockNr = blockNr;
    if (SdCard[bus].sdcType == ADO_SDC_CARD_20SD) {
      // SD 2.0 cards take byte addresses as argument
      blockNr = blockNr << 9;
   }
   SdcSendCommand(bus, ADO_SDC_CMD17_READ_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_READ_SBCMD, blockNr);
}

void SdcSendCommand(ado_sspid_t bus, ado_sdc_cmd_t cmd, uint32_t jobContext, uint32_t arg) {
	int responseSize = 3;

	SdCard[bus].sdcCmdData[0] = 0x40 | cmd;
	SdCard[bus].sdcCmdData[1] = (uint8_t)(arg>>24);
	SdCard[bus].sdcCmdData[2] = (uint8_t)(arg>>16);
	SdCard[bus].sdcCmdData[3] = (uint8_t)(arg>>8);
	SdCard[bus].sdcCmdData[4] = (uint8_t)(arg);
	SdCard[bus].sdcCmdData[5] = 0xFF;

	// This 2 cmds need correct crc
	if (cmd == ADO_SDC_CMD0_GO_IDLE_STATE) {
	    SdCard[bus].sdcCmdData[5] = 0x95;
	} else if (cmd == ADO_SDC_CMD8_SEND_IF_COND) {
	    SdCard[bus].sdcCmdData[5] = 0x87;
		responseSize = 7;
	} else if (cmd ==  ADO_SDC_CMD58_READ_OCR) {
		responseSize = 7;
	}  else if (cmd ==  ADO_SDC_CMD13_SEND_STATUS) {
		responseSize = 4;
	}

	SdCard[bus].sdcCmdPending = true;
	SdCard[bus].nextStatus = jobContext;
	ADO_SSP_AddJob((uint32_t)(&SdCard[bus]), SdCard[bus].sdcBusNr, SdCard[bus].sdcCmdData, SdCard[bus].sdcCmdResponse, 6 , responseSize, DMAFinishedIRQ, ActivateCS);
}


//void SdcPowerupCmd(int argc, char *argv[]) {
//	LPC_SSP_T *sspBase = 0;
//	if (sdcBusNr == ADO_SSP0) {
//		sspBase = LPC_SSP0;
//	} else if  (sdcBusNr == ADO_SSP0) {
//		sspBase = LPC_SSP1;
//	}
////  This does not work, because the direct CS is controlled by SSP and can not be easily hold at high without changing the SSP config.....
////  As all our tested cards do switch to SPI Mode when the INIT CMD0 is repeated once, we do not care for Power on yet.....
////	if (sspBase != 0) {
////		uint8_t dummy; (void)dummy;
////		// Make 80 Clock without CS
////		for (int i=0;i<10;i++) {
////			sspBase->DR = 0x55;
////			dummy = sspBase->SR;
////			dummy = sspBase->DR; // Clear Rx buffer.
////		}
////		{ uint32_t temp; (void)temp; while ((sspBase->SR & SSP_STAT_RNE) != 0) { temp = sspBase->DR; } }
////	}
//}



//void SdcInitCmd(int argc, char *argv[]) {
//	LPC_SSP_T *sspBase = 0;
//	ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards
//
//	if (sdcBusNr == ADO_SSP0) {
//		sspBase = LPC_SSP0;
//	} else if  (sdcBusNr == ADO_SSP1) {
//		sspBase = LPC_SSP1;
//	}
//	if (sspBase != 0) {
//	    // Reset module state and initiate the card init sequence
//	    SdCard[sdcBusNr].sdcType = ADO_SDC_CARD_UNKNOWN;
//		SdCard[sdcBusNr].sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
//		SdcSendCommand(sdcBusNr, ADO_SDC_CMD0_GO_IDLE_STATE, ADO_SDC_CARDSTATUS_INIT_RESET, 0);
//	}
//}
//
//
//void SdcReadCmd(int argc, char *argv[]){
//	uint32_t arg = 0;
//	ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards
//
//	if (argc > 0) {
//		//adr = atoi(argv[0]);
//		sdcCurRwBlockNr = strtol(argv[0], NULL, 0);		//This allows '0x....' hex entry!
//	}
//	if (SdCard[sdcBusNr].sdcType == ADO_SDC_CARD_20SD) {
//		// SD Cards take byte addresses as argument
//		arg = sdcCurRwBlockNr << 9;
//	} else {
//		// 2.0 HC or XC take block addressing with fixed 512 byte blocks as argument
//		arg = sdcCurRwBlockNr;
//	}
//
//	if (SdCard[sdcBusNr].sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
//	    sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
//		SdcSendCommand(sdcBusNr, ADO_SDC_CMD17_READ_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_READ_SBCMD, arg);
//	} else {
//		printf("Card not ready to receive commands....\n");
//	}
//}
//
//void SdcWriteCmd(int argc, char *argv[]) {
//    ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards
//
//	sdcRwData[0] = 0xFE;							// Data Block Start token
//	memcpy(sdcRwData + 1, sdcEditBlock, 512);
//	uint16_t crc = CRC16_XMODEM(sdcRwData, 512);
//	sdcRwData[513] = (uint8_t)(crc >> 8);
//	sdcRwData[514] = (uint8_t)crc;
//
//	if (argc > 0) {
//		 sdcCurRwBlockNr = strtol(argv[0], NULL, 0);		//This allows '0x....' hex entry!
//	}
//
//	if (SdCard[sdcBusNr].sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
//	    sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
//		SdcSendCommand(sdcBusNr, ADO_SDC_CMD24_WRITE_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_WRITE_SBCMD, sdcCurRwBlockNr);
//	} else {
//		printf("Card not ready to receive commands....\n");
//	}
//
//}
//
//void SdcEditCmd(int argc, char *argv[]) {
//    ado_sspid_t sdcBusNr = ADO_SSP1; // TODO: allow CLI for both SDCards
//
//    uint16_t adr = 0;
//	char *content = "Test Edit from LPC1769";
//
//	if (argc > 0) {
//		 adr = strtol(argv[0], NULL, 0);		//This allows '0x....' hex entry!
//		 adr &= 0x01FF;
//	}
//
//	if (argc > 1) {
//		content = argv[1];
//	}
//
//	int i = 0;
//	for (uint8_t *ptr = (uint8_t*)(sdcEditBlock + adr); ptr < (uint8_t*)(sdcEditBlock + adr + strlen(content)); ptr++) {
//		if (ptr < (sdcEditBlock + 512)) {
//			*ptr = content[i++];
//		}
//	}
//	printf("Block modified: \n");
//	sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
//	SdCard[sdcBusNr].sdcWaitLoops = 1000;
//	sdcDumpLines = 16;
//}
//
//void SdcCheckFatCmd(int argc, char *argv[]) {
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
//}


void SdcCardinitialize(void *cardPtr){
    // Reset module state and initiate the card init sequence
    ado_sdcard_t *sdCard = (ado_sdcard_t *)cardPtr;
    sdCard->sdcType = ADO_SDC_CARD_UNKNOWN;
    sdCard->sdcStatus = ADO_SDC_CARDSTATUS_UNDEFINED;
    SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD0_GO_IDLE_STATE, ADO_SDC_CARDSTATUS_INIT_RESET, 0);
}

void SdcReadBlockAsync(void *cardPtr, uint32_t blockNr, uint8_t *data,  void (*finishedHandler)(sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len)) {
    ado_sdcard_t *sdCard = (ado_sdcard_t *)cardPtr;
    uint32_t arg = 0;

    if (sdCard->sdcType == ADO_SDC_CARD_20SD) {
          // SD Cards take byte addresses as argument
          arg = blockNr << 9;
      } else {
          // 2.0 HC or XC take block addressing with fixed 512 byte blocks as argument
          arg = blockNr;
      }

      if (sdCard->sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
          sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
          SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD17_READ_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_READ_SBCMD, arg);
      } else {
          finishedHandler(SDC_RES_ERROR, blockNr, 0, 0);
      }
}

void SdcWriteBlockAsync(void *cardPtr, uint32_t blockNr, uint8_t *data, void (*finishedHandler)(sdc_res_t result, uint32_t blockNr, uint8_t *data, uint32_t len)) {
    ado_sdcard_t *sdCard = (ado_sdcard_t *)cardPtr;
    uint32_t arg = 0;

    if (sdCard->sdcType == ADO_SDC_CARD_20SD) {
          // SD Cards take byte addresses as argument
          arg = blockNr << 9;
      } else {
          // 2.0 HC or XC take block addressing with fixed 512 byte blocks as argument
          arg = blockNr;
      }


    if (sdCard->sdcStatus == ADO_SDC_CARDSTATUS_INITIALIZED) {
          sdcDumpMode = ADO_SDC_DUMPMODE_RAW;
          SdcSendCommand(sdCard->sdcBusNr, ADO_SDC_CMD24_WRITE_SINGLE_BLOCK, ADO_SDC_CARDSTATUS_WRITE_SBCMD, arg);
      } else {
          finishedHandler(SDC_RES_ERROR, blockNr, 0, 0);
      }
}
