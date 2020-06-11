/*
 * test_sspdma.c
 *
 *  Created on: 11.06.2020
 *      Author: Robert
 */

#include "test_sspdma.h"

#include <ado_sspdma.h>
#include <stdio.h>

extern uint8_t txDummy;         // White box testing. We access internal Variables of ADO_SSPDMA here.
extern uint8_t rxDummy;         // White box testing. We access internal Variables of ADO_SSPDMA here.


volatile bool jobFinished;
uint8_t txTest[5] = {0x11,0x22,0x33,0x44, 0x55};
uint8_t rxTest[5] = {0xAA,0xBB,0xCC,0xDD, 0xEE};

void TestJobFinished(uint32_t context, ado_sspstatus_t jobStatus, uint8_t *rxData, uint16_t rxSize){
    test_result_t *res = (test_result_t *)context;
    jobFinished = true;

    if (jobStatus != ADO_SSP_JOBDONE) {
        testFailed(res, "Job Error");
        return;
    }

    if (rxSize != 3) {
        testFailed(res, "Wrong RxSize");
        return;
    }

    for (int i=0; i<3; i++) {
        // Our loopback should receive all RXData with content of (modules internal) txDummy byte!
        if (rxData[i] != 0x88) {
            testFailed(res, "Wrong Rx Data");
            return;
        }
    }

    if (rxDummy != 0x55) {
        // The (modules internal) rxDummy is used to 'dump' rx buffer while tx data is sent in fist dma block.
        // So it should hold the last byte of our TX test data.
        testFailed(res, "Wrong Tx Data");
        return ;
    }

    // if test really passed is decided on job callers side.
    //testPassed(res);
}

void ssp_test1(test_result_t *res) {
    // We use the SSP1 here which has no hardware connected at all in our board.
    ADO_SSP_Init(ADO_SSP1, 100000, SSP_CLOCK_MODE3);

    // White box test !
    // We enable loopback on our SSP, so all Tx bytes are fed back to RX buffer by LPC Chip Hardware itself....
    Chip_SSP_EnableLoopBack(LPC_SSP1);
    // We have to adjust the (module internal) Tx dummy byte to recognize received data for the test.
    txDummy = 0x88;
    jobFinished = false;
    ADO_SSP_AddJob((uint32_t)res, ADO_SSP1 , txTest, rxTest, 5 , 3, TestJobFinished, 0);

    uint32_t waitLoops = 1000000;
    while (!jobFinished && (waitLoops-- > 1));
    if (waitLoops == 0) {
        testFailed(res, "Job Timeout!");
    }
    waitLoops = 1000000 - waitLoops;
    printf("SSP Test Job w. 100kHz needed. %d waitloops.\n", waitLoops );
    if (waitLoops > 4000) {
        testFailed(res, "Job duration too long!");
        return;
    }
    testPassed(res);
    txDummy = 0xFF;     // !!!!
}
