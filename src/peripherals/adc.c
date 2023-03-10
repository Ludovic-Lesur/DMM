/*
 * adc.c
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#include "adc.h"

#include "adc_reg.h"
#include "gpio.h"
#include "lptim.h"
#include "mapping.h"
#include "math.h"
#include "rcc_reg.h"
#include "types.h"

/*** ADC local macros ***/

#define ADC_MEDIAN_FILTER_LENGTH		9
#define ADC_CENTER_AVERAGE_LENGTH		3

#define ADC_FULL_SCALE_12BITS			4095

#define ADC_VREFINT_VOLTAGE_MV			((VREFINT_CAL * VREFINT_VCC_CALIB_MV) / (ADC_FULL_SCALE_12BITS))
#define ADC_VMCU_DEFAULT_MV				3000

#define ADC_TIMEOUT_COUNT				1000000

/*** ADC local structures ***/

typedef enum {
	ADC_CHANNEL_VUSB = 1,
	ADC_CHANNEL_VHMI = 4,
	ADC_CHANNEL_VRS = 6,
	ADC_CHANNEL_VREFINT = 17,
	ADC_CHANNEL_TMCU = 18,
	ADC_CHANNEL_LAST = 19
} ADC_channel_t;

typedef enum {
	ADC_CONVERSION_TYPE_VMCU = 0,
	ADC_CONVERSION_TYPE_VOLTAGE_ATTENUATION,
	ADC_CONVERSION_TYPE_VOLTAGE_AMPLIFICATION,
	ADC_CONVERSION_TYPE_LAST
} ADC_conversion_t;

typedef struct {
	ADC_channel_t channel;
	ADC_conversion_t gain_type;
	uint32_t gain;
} ADC_input_t;

typedef struct {
	uint32_t vrefint_12bits;
	uint32_t data[ADC_DATA_INDEX_LAST];
	int8_t tmcu_degrees;
} ADC_context_t;

/*** ADC local global variables ***/

static const ADC_input_t ADC_INPUTS[ADC_DATA_INDEX_LAST] = {
	{ADC_CHANNEL_VREFINT, ADC_CONVERSION_TYPE_VMCU, 0},
	{ADC_CHANNEL_VUSB, ADC_CONVERSION_TYPE_VOLTAGE_ATTENUATION, 2},
	{ADC_CHANNEL_VRS, ADC_CONVERSION_TYPE_VOLTAGE_ATTENUATION, 10},
	{ADC_CHANNEL_VHMI, ADC_CONVERSION_TYPE_VOLTAGE_ATTENUATION, 2}
};
static ADC_context_t adc_ctx;

/*** ADC local functions ***/

/* PERFORM A SINGLE ADC CONVERSION.
 * @param adc_channel:			Channel to convert.
 * @param adc_result_12bits:	Pointer to 32-bits value that will contain ADC raw result on 12 bits.
 * @return status:				Function execution status.
 */
static ADC_status_t _ADC1_single_conversion(ADC_channel_t adc_channel, uint32_t* adc_result_12bits) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	uint32_t loop_count = 0;
	// Check parameters.
	if (adc_channel >= ADC_CHANNEL_LAST) {
		status = ADC_ERROR_CHANNEL;
		goto errors;
	}
	if (adc_result_12bits == NULL) {
		status = ADC_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Select input channel.
	ADC1 -> CHSELR &= 0xFFF80000; // Reset all bits.
	ADC1 -> CHSELR |= (0b1 << adc_channel);
	// Clear all flags.
	ADC1 -> ISR |= 0x0000089F;
	// Read raw supply voltage.
	ADC1 -> CR |= (0b1 << 2); // ADSTART='1'.
	while (((ADC1 -> ISR) & (0b1 << 2)) == 0) {
		// Wait end of conversion ('EOC='1') or timeout.
		loop_count++;
		if (loop_count > ADC_TIMEOUT_COUNT) {
			status = ADC_ERROR_TIMEOUT;
			goto errors;
		}
	}
	(*adc_result_12bits) = (ADC1 -> DR);
errors:
	return status;
}

/* PERFORM SEVERAL CONVERSIONS FOLLOWED BY A MEDIAN FILTER.
 * @param adc_channel:			Channel to convert.
 * @param adc_result_12bits:	Pointer to 32-bits value that will contain ADC filtered result on 12 bits.
 * @return status:				Function execution status.
 */
static ADC_status_t _ADC1_filtered_conversion(ADC_channel_t adc_channel, uint32_t* adc_result_12bits) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	MATH_status_t math_status = MATH_SUCCESS;
	uint32_t adc_sample_buf[ADC_MEDIAN_FILTER_LENGTH] = {0x00};
	uint8_t idx = 0;
	// Check parameters.
	if (adc_channel >= ADC_CHANNEL_LAST) {
		status = ADC_ERROR_CHANNEL;
		goto errors;
	}
	if (adc_result_12bits == NULL) {
		status = ADC_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Perform all conversions.
	for (idx=0 ; idx<ADC_MEDIAN_FILTER_LENGTH ; idx++) {
		status = _ADC1_single_conversion(adc_channel, &(adc_sample_buf[idx]));
		if (status != ADC_SUCCESS) goto errors;
	}
	// Apply median filter.
	math_status = MATH_median_filter_u32(adc_sample_buf, ADC_MEDIAN_FILTER_LENGTH, ADC_CENTER_AVERAGE_LENGTH, adc_result_12bits);
	MATH_status_check(ADC_ERROR_BASE_MATH);
errors:
	return status;
}

/* COMPUTE MCU TEMPERATURE THANKS TO INTERNAL VOLTAGE REFERENCE.
 * @param:			None.
 * @return status:	Function execution status.
 */
static ADC_status_t _ADC1_compute_tmcu(void) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	uint32_t raw_temp_sensor_12bits = 0;
	int32_t raw_temp_calib_mv = 0;
	int32_t temp_calib_degrees = 0;
	// Read raw temperature.
	status = _ADC1_filtered_conversion(ADC_CHANNEL_TMCU, &raw_temp_sensor_12bits);
	if (status != ADC_SUCCESS) goto errors;
	// Compute temperature according to MCU factory calibration (see p.301 and p.847 of RM0377 datasheet).
	raw_temp_calib_mv = ((int32_t) raw_temp_sensor_12bits * adc_ctx.data[ADC_DATA_INDEX_VMCU_MV]) / (TS_VCC_CALIB_MV) - TS_CAL1; // Equivalent raw measure for calibration power supply (VCC_CALIB).
	temp_calib_degrees = raw_temp_calib_mv * ((int32_t) (TS_CAL2_TEMP-TS_CAL1_TEMP));
	temp_calib_degrees = (temp_calib_degrees) / ((int32_t) (TS_CAL2 - TS_CAL1));
	adc_ctx.tmcu_degrees = temp_calib_degrees + TS_CAL1_TEMP;
errors:
	return status;
}

/* COMPUTE ALL ADC CHANNELS.
 * @param:			None.
 * @return status:	Function execution status.
 */
static ADC_status_t _ADC1_compute_all_channels(void) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	uint8_t idx = 0;
	uint32_t voltage_12bits = 0;
	// Channels loop.
	for (idx=0 ; idx<ADC_DATA_INDEX_LAST ; idx++) {
		// Get raw result.
		status = _ADC1_filtered_conversion(ADC_INPUTS[idx].channel, &voltage_12bits);
		if (status != ADC_SUCCESS) goto errors;
		// Update VREFINT.
		if (ADC_INPUTS[idx].channel == ADC_CHANNEL_VREFINT) {
			adc_ctx.vrefint_12bits = voltage_12bits;
		}
		// Convert to mV using VREFINT.
		switch (ADC_INPUTS[idx].gain_type) {
		case ADC_CONVERSION_TYPE_VMCU:
			// Retrieve supply voltage from bandgap result.
			adc_ctx.data[idx] = (VREFINT_CAL * VREFINT_VCC_CALIB_MV) / (adc_ctx.vrefint_12bits);
			break;
		case ADC_CONVERSION_TYPE_VOLTAGE_ATTENUATION:
			adc_ctx.data[idx] = (ADC_VREFINT_VOLTAGE_MV * voltage_12bits * ADC_INPUTS[idx].gain) / (adc_ctx.vrefint_12bits);
			break;
		case ADC_CONVERSION_TYPE_VOLTAGE_AMPLIFICATION:
			adc_ctx.data[idx] = (ADC_VREFINT_VOLTAGE_MV * voltage_12bits) / (adc_ctx.vrefint_12bits * ADC_INPUTS[idx].gain);
			break;
		default:
			status = ADC_ERROR_CONVERSION_TYPE;
			goto errors;
		}
	}
errors:
	return status;
}

/*** ADC functions ***/

/* INIT ADC1 PERIPHERAL.
 * @param:			None.
 * @return status:	Function execution status.
 */
ADC_status_t ADC1_init(void) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	uint8_t idx = 0;
	uint32_t loop_count = 0;
	// Init context.
	adc_ctx.vrefint_12bits = 0;
	for (idx=0 ; idx<ADC_DATA_INDEX_LAST ; idx++) adc_ctx.data[idx] = 0;
	adc_ctx.data[ADC_DATA_INDEX_VMCU_MV] = ADC_VMCU_DEFAULT_MV;
	adc_ctx.tmcu_degrees = 0;
	// Init GPIOs.
	GPIO_configure(&GPIO_MNTR_EN, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_ADC1_IN1, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_ADC1_IN4, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_ADC1_IN6, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	// Init context.
	adc_ctx.vrefint_12bits = 0;
	for (idx=0 ; idx<ADC_DATA_INDEX_LAST ; idx++) adc_ctx.data[idx] = 0;
	adc_ctx.data[ADC_DATA_INDEX_VMCU_MV] = ADC_VMCU_DEFAULT_MV;
	// Enable peripheral clock.
	RCC -> APB2ENR |= (0b1 << 9); // ADCEN='1'.
	// Ensure ADC is disabled.
	if (((ADC1 -> CR) & (0b1 << 0)) != 0) {
		ADC1 -> CR |= (0b1 << 1); // ADDIS='1'.
	}
	// Enable ADC voltage regulator.
	ADC1 -> CR |= (0b1 << 28);
	lptim1_status = LPTIM1_delay_milliseconds(5, LPTIM_DELAY_MODE_ACTIVE);
	LPTIM1_status_check(ADC_ERROR_BASE_LPTIM);
	// ADC configuration.
	ADC1 -> CCR |= (0b1 << 25); // Enable low frequency clock (LFMEN='1').
	ADC1 -> CFGR2 |= (0b11 << 30); // Use PCLK2 as ADCCLK (MSI).
	ADC1 -> SMPR |= (0b111 << 0); // Maximum sampling time.
	// ADC calibration.
	ADC1 -> CR |= (0b1 << 31); // ADCAL='1'.
	while ((((ADC1 -> CR) & (0b1 << 31)) != 0) && (((ADC1 -> ISR) & (0b1 << 11)) == 0)) {
		// Wait until calibration is done or timeout.
		loop_count++;
		if (loop_count > ADC_TIMEOUT_COUNT) {
			status = ADC_ERROR_CALIBRATION;
			break;
		}
	}
errors:
	return status;
}

/* PERFORM INTERNAL ADC MEASUREMENTS.
 * @param:			None.
 * @return status:	Function execution status.
 */
ADC_status_t ADC1_perform_measurements(void) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	uint32_t loop_count = 0;
	uint8_t hmi_on = 0;
	// Enable ADC peripheral.
	ADC1 -> CR |= (0b1 << 0); // ADEN='1'.
	while (((ADC1 -> ISR) & (0b1 << 0)) == 0) {
		// Wait for ADC to be ready (ADRDY='1') or timeout.
		loop_count++;
		if (loop_count > ADC_TIMEOUT_COUNT) {
			status = ADC_ERROR_TIMEOUT;
			goto errors;
		}
	}
	// Enable voltage dividers and HMI power supply.
	GPIO_write(&GPIO_MNTR_EN, 1);
	hmi_on = GPIO_read(&GPIO_HMI_POWER_ENABLE);
	if (hmi_on == 0) {
		GPIO_write(&GPIO_HMI_POWER_ENABLE, 1);
	}
	// Wait voltage dividers stabilization.
	lptim1_status = LPTIM1_delay_milliseconds(100, LPTIM_DELAY_MODE_STOP);
	LPTIM1_status_check(ADC_ERROR_BASE_LPTIM);
	// Wake-up VREFINT and temperature sensor.
	ADC1 -> CCR |= (0b11 << 22); // TSEN='1' and VREFEF='1'.
	lptim1_status = LPTIM1_delay_milliseconds(10, LPTIM_DELAY_MODE_ACTIVE);
	LPTIM1_status_check(ADC_ERROR_BASE_LPTIM);
	// Perform conversions.
	status = _ADC1_compute_all_channels();
	if (status != ADC_SUCCESS) goto errors;
	status = _ADC1_compute_tmcu();
	if (status != ADC_SUCCESS) goto errors;
errors:
	// Switch internal voltage reference off.
	ADC1 -> CCR &= ~(0b11 << 22); // TSEN='0' and VREFEF='0'.
	// Disable voltage dividers and HMI power supply.
	GPIO_write(&GPIO_MNTR_EN, 0);
	if (hmi_on == 0) {
		GPIO_write(&GPIO_HMI_POWER_ENABLE, 0);
	}
	// Disable ADC peripheral.
	ADC1 -> CR |= (0b1 << 1); // ADDIS='1'.
	return status;
}

/* GET ADC DATA.
 * @param data_idx:	Index of the data to retrieve.
 * @param data:		Pointer that will contain ADC data.
 * @return status:	Function execution status.
 */
ADC_status_t ADC1_get_data(ADC_data_index_t data_idx, uint32_t* data) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	// Check parameters.
	if (data_idx >= ADC_DATA_INDEX_LAST) {
		status = ADC_ERROR_DATA_INDEX;
		goto errors;
	}
	if (data == NULL) {
		status = ADC_ERROR_NULL_PARAMETER;
		goto errors;
	}
	(*data) = adc_ctx.data[data_idx];
errors:
	return status;
}

/* GET MCU TEMPERATURE.
 * @param tmcu_degrees:	Pointer to 8-bits value that will contain MCU temperature in degrees (2-complement).
 * @return status:		Function execution status.
 */
ADC_status_t ADC1_get_tmcu(int8_t* tmcu_degrees) {
	// Local variables.
	ADC_status_t status = ADC_SUCCESS;
	// Check parameter.
	if (tmcu_degrees == NULL) {
		status = ADC_ERROR_NULL_PARAMETER;
		goto errors;
	}
	(*tmcu_degrees) = adc_ctx.tmcu_degrees;
errors:
	return status;
}
