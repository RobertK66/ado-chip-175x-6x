/*
 * ado_crc.c
 *
 *  Created on: 08.06.2020
 *      Author: Robert copied from Pegasus code
 */

#include "ado_crc.h"

uint16_t CRC16_0x1021(const uint8_t* data_p, uint16_t length, uint16_t start){
    uint8_t x;
    uint16_t crc = start;

    while (length--){
        x = crc >> 8 ^ (*data_p++);
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

//uint16_t Fletcher16(uint8_t* data, int len){
//    uint16_t sum1 = 0;
//    uint16_t sum2 = 0;
//    int index;
//    for(index=0;index<len;++index){
//        sum1 = (sum1 + data[index]) % 255;
//	sum2 = (sum2 + sum1) % 255;
//    }
//    return (sum2 << 8) | sum1;
//}
