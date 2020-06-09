/*
 * test_crc.c
 *
 *  Created on: 08.06.2020
 *      Author: Robert
 */

#include "test_crc.h"

#include <chip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ado_test.h>
#include <ado_crc.h>

void tcrc_test1(test_result_t *res) {
    char *str = "123456789";

    uint16_t crc = CRC16_CCITT_FALSE((uint8_t*)str,9);
    if (crc != 0x29b1) {
        testFailed(res, "Wrong CRC 1a");
    }

    crc = CRC16_SPI_FUJITSU((uint8_t*)str,9);
    if (crc != 0xe5cc) {
        testFailed(res, "Wrong CRC 1b");
    }

    crc = CRC16_XMODEM((uint8_t*)str,9);
    if (crc != 0x31c3) {
        testFailed(res, "Wrong CRC 1c");
    }
    testPassed(res);
}

void tcrc_test2(test_result_t *res) {
    char *str = "123456789";

    uint16_t crc = CRC16_GENIBUS((uint8_t*)str,9);
    if (crc != 0xd64e) {
        testFailed(res, "Wrong CRC 2a");
    }

    crc = CRC16_GSM((uint8_t*)str,9);
    if (crc != 0xce3c) {
        testFailed(res, "Wrong CRC 2b");
    }
    testPassed(res);
}
