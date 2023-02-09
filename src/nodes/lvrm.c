/*
 * lvrm.c
 *
 *  Created on: 22 jan. 2023
 *      Author: Ludo
 */

#include "lvrm.h"

#include "dinfox.h"
#include "lbus.h"
#include "mode.h"
#include "node_common.h"
#include "string.h"

/*** LVRM local macros ***/

#define LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE		3
#define LVRM_SIGFOX_PAYLOAD_DATA_SIZE			7

static const char_t* LVRM_STRING_DATA_NAME[LVRM_NUMBER_OF_SPECIFIC_STRING_DATA] = {"VCOM =", "VOUT =", "IOUT =", "RELAY ="};
static const char_t* LVRM_STRING_DATA_UNIT[LVRM_NUMBER_OF_SPECIFIC_STRING_DATA] = {"mV", "mV", "uA", STRING_NULL};

/*** LVRM local structures ***/

typedef union {
	uint8_t frame[LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE];
	struct {
		unsigned vmcu_mv : 16;
		unsigned tmcu_degrees : 8;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} LVRM_sigfox_payload_monitoring_t;

typedef union {
	uint8_t frame[LVRM_SIGFOX_PAYLOAD_DATA_SIZE];
	struct {
		unsigned vcom_mv : 16;
		unsigned vout_mv : 16;
		unsigned iout_ua : 23;
		unsigned out_en : 1;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} LVRM_sigfox_payload_data_t;

/*** LVRM functions ***/

/* RETRIEVE SPECIFIC DATA OF LVRM NODE.
 * @param rs485_address:		RS485 address of the node to update.
 * @param string_data_index:	Node string data index.
 * @param single_string_data:	Pointer to the data string to be filled.
 * @param registers_value:		Registers value table.
 * @return status:				Function execution status.
 */
NODE_status_t LVRM_update_data(NODE_address_t rs485_address, uint8_t string_data_index, NODE_single_string_data_t* single_string_data, int32_t* registers_value) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	STRING_status_t string_status = STRING_SUCCESS;
	NODE_read_parameters_t read_params;
	NODE_read_data_t read_data;
	NODE_access_status_t read_status;
	uint8_t register_address = 0;
	uint8_t buffer_size = 0;
	// Check index.
	if ((string_data_index < DINFOX_STRING_DATA_INDEX_LAST) || (string_data_index >= LVRM_STRING_DATA_INDEX_LAST)) {
		status = NODE_ERROR_STRING_DATA_INDEX;
		goto errors;
	}
	// Convert to register address.
	register_address = (string_data_index + DINFOX_REGISTER_LAST - DINFOX_STRING_DATA_INDEX_LAST);
	// Read parameters.
#ifdef AM
	read_params.node_address = rs485_address;
#endif
	read_params.register_address = register_address;
	read_params.type = NODE_READ_TYPE_VALUE;
	read_params.timeout_ms = LBUS_TIMEOUT_MS;
	read_params.format = STRING_FORMAT_DECIMAL;
	// Read data.
	status = LBUS_read_register(&read_params, &read_data, &read_status);
	if (status != NODE_SUCCESS) goto errors;
	// Add data name.
	NODE_append_string_name((char_t*) LVRM_STRING_DATA_NAME[string_data_index - DINFOX_STRING_DATA_INDEX_LAST]);
	buffer_size = 0;
	// Add data value.
	if (read_status.all == 0) {
		// Update integer data.
		NODE_update_value(register_address, read_data.value);
		// Specific print for relay.
		if (string_data_index == LVRM_STRING_DATA_INDEX_OUT_EN) {
			NODE_append_string_value((read_data.value == 0) ? "OFF" : "ON");
		}
		else {
			NODE_append_string_value(read_data.raw);
		}
		// Add unit.
		NODE_append_string_value((char_t*) LVRM_STRING_DATA_UNIT[string_data_index - DINFOX_STRING_DATA_INDEX_LAST]);
	}
	else {
		NODE_append_string_value(NODE_STRING_DATA_ERROR);
	}
errors:
	return status;
}

/* GET LVRM NODE SIGFOX PAYLOAD.
 * @param integer_data_value:	Pointer to the node registers value.
 * @param sigfox_payload_type:	Sigfox payload type.
 * @param sigfox_payload:		Pointer that will contain the specific sigfox payload of the node.
 * @param sigfox_payload_size:	Pointer to byte that will contain sigfox payload size.
 * @return status:				Function execution status.
 */
NODE_status_t LVRM_get_sigfox_payload(int32_t* integer_data_value, NODE_sigfox_payload_type_t sigfox_payload_type, uint8_t* sigfox_payload, uint8_t* sigfox_payload_size) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	LVRM_sigfox_payload_monitoring_t sigfox_payload_monitoring;
	LVRM_sigfox_payload_data_t sigfox_payload_data;
	uint8_t idx = 0;
	// Check parameters.
	if ((integer_data_value == NULL) || (sigfox_payload == NULL) || (sigfox_payload_size == NULL)) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Check type.
	switch (sigfox_payload_type) {
	case NODE_SIGFOX_PAYLOAD_TYPE_MONITORING:
		// Build monitoring payload.
		sigfox_payload_monitoring.vmcu_mv = integer_data_value[DINFOX_REGISTER_VMCU_MV];
		sigfox_payload_monitoring.tmcu_degrees = integer_data_value[DINFOX_REGISTER_TMCU_DEGREES];
		// Copy payload.
		for (idx=0 ; idx<LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE ; idx++) {
			sigfox_payload[idx] = sigfox_payload_monitoring.frame[idx];
		}
		(*sigfox_payload_size) = LVRM_SIGFOX_PAYLOAD_MONITORING_SIZE;
		break;
	case NODE_SIGFOX_PAYLOAD_TYPE_DATA:
		// Build data payload.
		sigfox_payload_data.vcom_mv = integer_data_value[LVRM_REGISTER_VCOM_MV];
		sigfox_payload_data.vout_mv = integer_data_value[LVRM_REGISTER_VOUT_MV];
		sigfox_payload_data.iout_ua = integer_data_value[LVRM_REGISTER_IOUT_UA];
		sigfox_payload_data.out_en = integer_data_value[LVRM_REGISTER_OUT_EN];
		// Copy payload.
		for (idx=0 ; idx<LVRM_SIGFOX_PAYLOAD_DATA_SIZE ; idx++) {
			sigfox_payload[idx] = sigfox_payload_data.frame[idx];
		}
		(*sigfox_payload_size) = LVRM_SIGFOX_PAYLOAD_DATA_SIZE;
		break;
	default:
		status = NODE_ERROR_SIGFOX_PAYLOAD_TYPE;
		goto errors;
	}
errors:
	return status;
}
