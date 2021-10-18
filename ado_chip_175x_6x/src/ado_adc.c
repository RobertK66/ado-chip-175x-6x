/*
 * ado_adc.c
 *
 *  Created on: 17.10.2021
 *      Author: Andi
 */

#include "stdint.h"
#include "chip_lpc175x_6x.h"
#include "adc_17xx_40xx.h"
#include "stdio.h"
#include "math.h"

#define ADC_SUPPLY_CURRENT_CH 0
#define ADC_SUPPLY_CURRENT_SP_CH 1
#define ADC_TEMPERATURE_CH 2
#define ADC_SUPPLY_VOLTAGE_CH 3

#define ADC_VREF 2.048 // Volt

void read_transmit_sensors();

void AdcInit() {
	//{ 0,23 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Main supply current (Analog Input 0)  */
	//{ 0,24 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Sidpanel supply current (Analog Input 1)  */
	//{ 0,25 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Temperature (Analog Input 2)  */
	//{ 0,26 , IOCON_MODE_INACT | IOCON_FUNC1 },  /* Supply Voltage (Analog Input 3)  */

	ADC_CLOCK_SETUP_T init;
	init.adcRate = 1000;
	init.burstMode = 1;

	Chip_ADC_Init(LPC_ADC, &init);

	Chip_ADC_EnableChannel(LPC_ADC, ADC_SUPPLY_CURRENT_CH, 1);
	Chip_ADC_EnableChannel(LPC_ADC, ADC_SUPPLY_CURRENT_SP_CH, 1);
	Chip_ADC_EnableChannel(LPC_ADC, ADC_TEMPERATURE_CH, 1);
	Chip_ADC_EnableChannel(LPC_ADC, ADC_SUPPLY_VOLTAGE_CH, 1);

	Chip_ADC_SetBurstCmd(LPC_ADC, 1);
	Chip_ADC_SetStartMode(LPC_ADC, 0, 0);

	CliRegisterCommand("adcRead", read_transmit_sensors);
}

void read_transmit_sensors() {

	uint16_t adc_val;
	Chip_ADC_ReadValue(LPC_ADC, ADC_SUPPLY_CURRENT_CH, &adc_val);
	float cur = (adc_val * (ADC_VREF / 4095) - 0.01) / 100 / 0.075;

	Chip_ADC_ReadValue(LPC_ADC, ADC_SUPPLY_CURRENT_SP_CH, &adc_val);
	float cur_sp = (adc_val * (ADC_VREF / 4095) - 0.01) / 100 / 0.075 - 0.0003;

	printf("Currents1: %.2f mA %.2f mA\n", 1000 * cur, 1000 * cur_sp);

	Chip_ADC_ReadValue(LPC_ADC, ADC_TEMPERATURE_CH, &adc_val);
	float temp = adc_val * (4.9 / 3.9 * ADC_VREF / 4095);

	temp = -1481.96 + sqrt(2196200 + (1.8639 - temp) * 257730);

	printf("AN-Temp3: %.3f °C\n", temp);

	Chip_ADC_ReadValue(LPC_ADC, ADC_SUPPLY_VOLTAGE_CH, &adc_val);
	float volt = adc_val * (2.0 * ADC_VREF / 4095);
	printf("Supply: %.3f V\n", volt);

	return;
}

