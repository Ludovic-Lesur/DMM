/*
 * node.c
 *
 *  Created on: 23 jan. 2023
 *      Author: Ludo
 */

#include "node.h"

#include "at.h"
#include "dinfox.h"
#include "lpuart.h"
#include "lvrm.h"
#include "mode.h"
#include "r4s8cr.h"
#include "rtc.h"
#include "uhfm.h"

/*** NODE local macros ***/

#define NODE_STRING_DATA_INDEX_MAX			32
#define NODE_REGISTER_ADDRESS_MAX			64

#define NODE_SIGFOX_PAYLOAD_STARTUP_SIZE	8
#define NODE_SIGFOX_PAYLOAD_SIZE_MAX		12

#define NODE_SIGFOX_PERIOD_SECONDS			300
#define NODE_SIGFOX_LOOP_MAX				100

/*** NODE local structures ***/

typedef enum {
	NODE_PROTOCOL_AT = 0,
#ifdef AM
	NODE_PROTOCOL_R4S8CR,
#endif
	NODE_PROTOCOL_LAST
} NODE_protocol_t;

typedef NODE_status_t (*NODE_read_register_t)(NODE_read_parameters_t* read_params, NODE_read_data_t* read_data, NODE_access_status_t* read_status);
typedef NODE_status_t (*NODE_write_register_t)(NODE_write_parameters_t* write_params, NODE_access_status_t* write_status);
typedef NODE_status_t (*NODE_update_data_t)(NODE_data_update_t* data_update);
typedef NODE_status_t (*NODE_get_sigfox_payload_t)(int32_t* integer_data_value, NODE_sigfox_ul_payload_type_t ul_payload_type, uint8_t* ul_payload, uint8_t* ul_payload_size);

typedef struct {
	NODE_read_register_t read_register;
	NODE_write_register_t write_register;
	NODE_update_data_t update_data;
	NODE_get_sigfox_payload_t get_sigfox_ul_payload;
} NODE_functions_t;

typedef struct {
	char_t* name;
	NODE_protocol_t protocol;
	uint8_t last_register_address;
	uint8_t last_string_data_index;
	STRING_format_t* registers_format;
	NODE_functions_t functions;
} NODE_descriptor_t;

typedef union {
	uint8_t frame[NODE_SIGFOX_PAYLOAD_SIZE_MAX];
	struct {
		unsigned node_address : 8;
		unsigned board_id : 8;
		uint8_t node_data[NODE_SIGFOX_PAYLOAD_SIZE_MAX - 2];
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} NODE_sigfox_ul_payload_t;

typedef union {
	uint8_t frame[NODE_SIGFOX_PAYLOAD_STARTUP_SIZE];
	struct {
		unsigned reset_reason : 8;
		unsigned major_version : 8;
		unsigned minor_version : 8;
		unsigned commit_index : 8;
		unsigned commit_id : 28;
		unsigned dirty_flag : 4;
	} __attribute__((scalar_storage_order("big-endian"))) __attribute__((packed));
} NODE_sigfox_payload_startup_t;

typedef struct {
	char_t string_data_name[NODE_STRING_DATA_INDEX_MAX][NODE_STRING_BUFFER_SIZE];
	char_t string_data_value[NODE_STRING_DATA_INDEX_MAX][NODE_STRING_BUFFER_SIZE];
	int32_t registers_value[NODE_REGISTER_ADDRESS_MAX];
} NODE_data_t;

typedef struct {
	NODE_data_t data;
#ifdef AM
	NODE_address_t uhfm_address;
#else
	uint8_t uhfm_connected;
#endif
	// Uplink.
	NODE_sigfox_ul_payload_t sigfox_ul_payload;
	uint8_t sigfox_ul_payload_size;
	uint32_t sigfox_ul_seconds_count;
	uint8_t sigfox_ul_node_list_index;
	NODE_sigfox_ul_payload_type_t sigfox_ul_payload_type_index;
	// Downlink.
	uint8_t sigfox_dl_payload[UHFM_SIGFOX_DL_PAYLOAD_SIZE];
} NODE_context_t;

/*** NODE local global variables ***/

// Note: table is indexed with board ID.
static const NODE_descriptor_t NODES[DINFOX_BOARD_ID_LAST] = {
	{"LVRM", NODE_PROTOCOL_AT, LVRM_REGISTER_LAST, LVRM_STRING_DATA_INDEX_LAST, (STRING_format_t*) LVRM_REGISTERS_FORMAT,
		{&AT_read_register, &AT_write_register, &LVRM_update_data, &LVRM_get_sigfox_ul_payload}
	},
	{"BPSM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"DDRM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"UHFM", NODE_PROTOCOL_AT, UHFM_REGISTER_LAST, UHFM_STRING_DATA_INDEX_LAST, (STRING_format_t*) UHFM_REGISTERS_FORMAT,
		{&AT_read_register, &AT_write_register, &UHFM_update_data, &UHFM_get_sigfox_ul_payload}
	},
	{"GPSM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"SM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"DIM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"RRM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"DMM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
	{"MPMCM", NODE_PROTOCOL_AT, 0, 0, NULL,
		{&AT_read_register, &AT_write_register, NULL, NULL}
	},
#ifdef AM
	{"R4S8CR", NODE_PROTOCOL_R4S8CR, R4S8CR_REGISTER_LAST, R4S8CR_STRING_DATA_INDEX_LAST, (STRING_format_t*) R4S8CR_REGISTERS_FORMAT,
		{&R4S8CR_read_register, &R4S8CR_write_register, &R4S8CR_update_data, &R4S8CR_get_sigfox_ul_payload}},
#endif
};
static NODE_context_t node_ctx;

/*** NODE local functions ***/

/* CHECK NODE POINTER AND BOARD ID.
 * @param:	None.
 * @return:	None.
 */
#define _NODE_check_node_and_board_id(void) { \
	if (node == NULL) { \
		status = NODE_ERROR_NULL_PARAMETER; \
		goto errors; \
	} \
	if ((node -> board_id) >= DINFOX_BOARD_ID_LAST) { \
		status = NODE_ERROR_NOT_SUPPORTED; \
		goto errors; \
	} \
}

/* CHECK FUNCTION POINTER.
 * @param function_name:	Function to check.
 * @return:					None.
 */
#define _NODE_check_function_pointer(function_name) { \
	if ((NODES[node -> board_id].functions.function_name) == NULL) { \
		status = NODE_ERROR_NOT_SUPPORTED; \
		goto errors; \
	} \
}

/* FLUSH ONE LINE OF THE MEASURERMENTS VALUE BUFFER.
 * @param:	None.
 * @return:	None.
 */
static void _NODE_flush_string_data_value(uint8_t string_data_index) {
	// Local variables.
	uint8_t idx = 0;
	// Char loop.
	for (idx=0 ; idx<NODE_STRING_BUFFER_SIZE ; idx++) {
		node_ctx.data.string_data_name[string_data_index][idx] = STRING_CHAR_NULL;
		node_ctx.data.string_data_value[string_data_index][idx] = STRING_CHAR_NULL;
	}
}

/* FLUSH WHOLE DATAS VALUE BUFFER.
 * @param:	None.
 * @return:	None.
 */
void _NODE_flush_all_data_value(void) {
	// Local variables.
	uint8_t idx = 0;
	// Reset string and integer data.
	for (idx=0 ; idx<NODE_STRING_DATA_INDEX_MAX ; idx++) _NODE_flush_string_data_value(idx);
	for (idx=0 ; idx<NODE_REGISTER_ADDRESS_MAX ; idx++) node_ctx.data.registers_value[idx] = 0;
}

/* FLUSH NODES LIST.
 * @param:	None.
 */
void _NODE_flush_list(void) {
	// Local variables.
	uint8_t idx = 0;
	// Reset node list.
	for (idx=0 ; idx<NODES_LIST_SIZE_MAX ; idx++) {
#ifdef AM
		NODES_LIST.list[idx].address = 0xFF;
#endif
		NODES_LIST.list[idx].board_id = DINFOX_BOARD_ID_ERROR;
	}
	NODES_LIST.count = 0;
}

/* SEND NODE DATA THROUGH RADIO.
 * @param node:					Node to monitor by radio.
 * @param sigfox_payload_type:	Type of data to send.
 * @param bidirectional_flag:	Downlink request flag.
 * @return status:				Function execution status.
 */
NODE_status_t _NODE_radio_send(NODE_t* node, NODE_sigfox_ul_payload_type_t ul_payload_type, uint8_t bidirectional_flag) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint8_t sigfox_payload_specific_size;
	NODE_sigfox_payload_startup_t sigfox_payload_startup;
	UHFM_sigfox_message_t sigfox_message;
	NODE_access_status_t send_status;
	uint8_t idx = 0;
	// Check board ID.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(get_sigfox_ul_payload);
	// Reset payload.
	for (idx=0 ; idx<NODE_SIGFOX_PAYLOAD_SIZE_MAX ; idx++) node_ctx.sigfox_ul_payload.frame[idx] = 0x00;
	node_ctx.sigfox_ul_payload_size = 0;
	// Add board ID and node address.
	node_ctx.sigfox_ul_payload.board_id = (node -> board_id);
#ifdef AM
	node_ctx.sigfox_ul_payload.node_address = (node -> address);
#else
	node_ctx.sigfox_payload.node_address = DINFOX_NODE_ADDRESS_BROADCAST;
#endif
	node_ctx.sigfox_ul_payload_size = 2;
	// Add specific payload.
	switch (ul_payload_type) {
	case NODE_SIGFOX_PAYLOAD_TYPE_STARTUP:
		// Check node protocol.
		if (NODES[node -> board_id].protocol != NODE_PROTOCOL_AT) {
			status = NODE_ERROR_SIGFOX_PAYLOAD_EMPTY;
			goto errors;
		}
		// Build startup payload here since the format is common to all boards.
		sigfox_payload_startup.reset_reason = node_ctx.data.registers_value[DINFOX_REGISTER_RESET_REASON];
		sigfox_payload_startup.major_version = node_ctx.data.registers_value[DINFOX_REGISTER_SW_VERSION_MAJOR];
		sigfox_payload_startup.minor_version = node_ctx.data.registers_value[DINFOX_REGISTER_SW_VERSION_MINOR];
		sigfox_payload_startup.commit_index = node_ctx.data.registers_value[DINFOX_REGISTER_SW_VERSION_COMMIT_INDEX];
		sigfox_payload_startup.commit_id = node_ctx.data.registers_value[DINFOX_REGISTER_SW_VERSION_COMMIT_ID];
		sigfox_payload_startup.dirty_flag = node_ctx.data.registers_value[DINFOX_REGISTER_SW_VERSION_DIRTY_FLAG];
		// Add specific data to global paylaod.
		for (idx=0 ; idx<NODE_SIGFOX_PAYLOAD_STARTUP_SIZE ; idx++) {
			node_ctx.sigfox_ul_payload.node_data[idx] = sigfox_payload_startup.frame[idx];
		}
		node_ctx.sigfox_ul_payload_size += NODE_SIGFOX_PAYLOAD_STARTUP_SIZE;
		break;
	case NODE_SIGFOX_PAYLOAD_TYPE_MONITORING:
	case NODE_SIGFOX_PAYLOAD_TYPE_DATA:
		// Execute function of the corresponding board ID.
		status = NODES[node -> board_id].functions.get_sigfox_ul_payload(node_ctx.data.registers_value, ul_payload_type, node_ctx.sigfox_ul_payload.node_data, &sigfox_payload_specific_size);
		if (status != NODE_SUCCESS) goto errors;
		// Check returned pointer.
		if (sigfox_payload_specific_size == 0) {
			status = NODE_ERROR_SIGFOX_PAYLOAD_EMPTY;
			goto errors;
		}
		node_ctx.sigfox_ul_payload_size += sigfox_payload_specific_size;
		break;
	default:
		status = NODE_ERROR_SIGFOX_PAYLOAD_TYPE;
		goto errors;
	}
	// Check UHFM board availability.
#ifdef AM
	if (node_ctx.uhfm_address == DINFOX_NODE_ADDRESS_BROADCAST) {
#else
	if (node_ctx.uhfm_connected == 0) {
#endif
		status = NODE_ERROR_NONE_RADIO_MODULE;
		goto errors;
	}
	// Build Sigfox message structure.
	sigfox_message.ul_payload = (uint8_t*) node_ctx.sigfox_ul_payload.frame;
	sigfox_message.ul_payload_size = node_ctx.sigfox_ul_payload_size;
	sigfox_message.bidirectional_flag = bidirectional_flag;
	sigfox_message.dl_payload = (uint8_t*) node_ctx.sigfox_dl_payload;
	// Send message.
#ifdef AM
	status = UHFM_send_sigfox_message(node_ctx.uhfm_address, &sigfox_message, &send_status);
#else
	status = UHFM_send_sigfox_message(&sigfox_message, &send_status);
#endif
	if (status != NODE_SUCCESS) goto errors;
	// Check send status.
	if (send_status.all != 0) {
		status = NODE_ERROR_SIGFOX_SEND;
	}
errors:
	return status;
}

/*** NODE functions ***/

/* INIT NODE LAYER.
 * @param:	None.
 * @return:	None.
 */
void NODE_init(void) {
	// Reset node list.
	_NODE_flush_list();
	// Init context.
	node_ctx.sigfox_ul_seconds_count = NODE_SIGFOX_PERIOD_SECONDS;
	node_ctx.sigfox_ul_node_list_index = 0;
	node_ctx.sigfox_ul_payload_type_index = 0;
}

/* GET NODE BOARD NAME.
 * @param node:				Node to get name of.
 * @param board_name_ptr:	Pointer to string that will contain board name.
 * @return status:			Function execution status.
 */
NODE_status_t NODE_get_name(NODE_t* node, char_t** board_name_ptr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check board ID.
	_NODE_check_node_and_board_id();
	// Get name of the corresponding board ID.
	(*board_name_ptr) = (char_t*) NODES[node -> board_id].name;
errors:
	return status;
}

/* GET NODE LAST STRING INDEX.
 * @param node:						Node to get name of.
 * @param last_string_data_index:	Pointer to byte that will contain last string index.
 * @return status:					Function execution status.
 */
NODE_status_t NODE_get_last_string_data_index(NODE_t* node, uint8_t* last_string_data_index) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check board ID.
	_NODE_check_node_and_board_id();
	// Get name of the corresponding board ID.
	(*last_string_data_index) = NODES[node -> board_id].last_string_data_index;
errors:
	return status;
}

/* PERFORM A SINGLE NODE MEASUREMENT.
 * @param node:					Node to update.
 * @param string_data_index:	Node string data index.
 * @return status:				Function execution status.
 */
NODE_status_t NODE_update_data(NODE_t* node, uint8_t string_data_index) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_data_update_t data_update;
	// Check board ID.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(update_data);
	// Flush line.
	_NODE_flush_string_data_value(string_data_index);
	// Update pointers.
#ifdef AM
	data_update.node_address = (node -> address);
#endif
	data_update.string_data_index = string_data_index;
	data_update.name_ptr = (char_t*) &(node_ctx.data.string_data_name[string_data_index]);
	data_update.value_ptr = (char_t*) &(node_ctx.data.string_data_value[string_data_index]);
	data_update.registers_value_ptr = (int32_t*) node_ctx.data.registers_value;
	// Check node protocol.
	switch (NODES[node -> board_id].protocol) {
	case NODE_PROTOCOL_AT:
		// Check index to update common or specific data.
		if (string_data_index < DINFOX_STRING_DATA_INDEX_LAST) {
			status = DINFOX_update_data(&data_update);
		}
		else {
			status = NODES[node -> board_id].functions.update_data(&data_update);
		}
		break;
#ifdef AM
	case NODE_PROTOCOL_R4S8CR:
		status = NODES[node -> board_id].functions.update_data(&data_update);
		break;
#endif
	default:
		status = NODE_ERROR_PROTOCOL;
		break;
	}
errors:
	return status;
}

/* PERFORM ALL NODE MEASUREMENTS.
 * @param node:		Node to update.
 * @return status:	Function execution status.
 */
NODE_status_t NODE_update_all_data(NODE_t* node) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint8_t idx = 0;
	// Check board ID.
	_NODE_check_node_and_board_id();
	// Check indexes.
	if ((NODES[node -> board_id].last_string_data_index) == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	// Reset buffers.
	_NODE_flush_all_data_value();
	// String data loop.
	for (idx=0 ; idx<(NODES[node -> board_id].last_string_data_index) ; idx++) {
		status = NODE_update_data(node, idx);
		if (status != NODE_SUCCESS) goto errors;
	}
errors:
	return status;
}

/* UNSTACK NODE DATA FORMATTED AS STRING.
 * @param node:						Node to read.
 * @param string_data_index:		Node string data index.
 * @param string_data_name_ptr:		Pointer to string that will contain next measurement name.
 * @param string_data_value_ptr:	Pointer to string that will contain next measurement value.
 * @return status:					Function execution status.
 */
NODE_status_t NODE_read_string_data(NODE_t* node, uint8_t string_data_index, char_t** string_data_name_ptr, char_t** string_data_value_ptr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	// Check parameters.
	_NODE_check_node_and_board_id();
	// Check index.
	if (NODES[node -> board_id].last_string_data_index == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	if (string_data_index >= (NODES[node -> board_id].last_string_data_index)) { \
		status = NODE_ERROR_STRING_DATA_INDEX;
		goto errors;
	}
	// Update pointers.
	(*string_data_name_ptr) = (char_t*) node_ctx.data.string_data_name[string_data_index];
	(*string_data_value_ptr) = (char_t*) node_ctx.data.string_data_value[string_data_index];
errors:
	return status;
}

/* WRITE NODE DATA.
 * @param node:					Node to write.
 * @param string_data_index:	Node string data index.
 * @param value:				Value to write in corresponding register.
 * @param write_status:			Pointer to the writing operation status.
 * @return status:				Function execution status.
 */
NODE_status_t NODE_write_register(NODE_t* node, uint8_t register_address, int32_t value, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_write_parameters_t write_input;
	// Check node and board ID.
	_NODE_check_node_and_board_id();
	_NODE_check_function_pointer(write_register);
	// Check write status.
	if (write_status == NULL) {
		status = NODE_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Check register address.
	if (NODES[node -> board_id].last_register_address == 0) {
		status = NODE_ERROR_NOT_SUPPORTED;
		goto errors;
	}
	if (register_address >= (NODES[node -> board_id].last_register_address)) {
		status = NODE_ERROR_REGISTER_ADDRESS;
		goto errors;
	}
	// Common write parameters.
#ifdef AM
	write_input.node_address = (node -> address);
#endif
	write_input.value = value;
	write_input.register_address = register_address;
	// Check node protocol.
	switch (NODES[node -> board_id].protocol) {
	case NODE_PROTOCOL_AT:
		// Specific write parameters.
		write_input.timeout_ms = AT_DEFAULT_TIMEOUT_MS;
		write_input.format = (register_address < DINFOX_REGISTER_LAST) ? DINFOX_REGISTERS_FORMAT[register_address] : NODES[node -> board_id].registers_format[register_address - DINFOX_REGISTER_LAST];
		break;
#ifdef AM
	case NODE_PROTOCOL_R4S8CR:
		// Specific write parameters.
		write_input.timeout_ms = R4S8CR_TIMEOUT_MS;
		write_input.format = NODES[node -> board_id].registers_format[register_address];
		break;
#endif
	default:
		status = NODE_ERROR_PROTOCOL;
		break;
	}
	status = NODES[node -> board_id].functions.write_register(&write_input, write_status);
errors:
	return status;
}

/* WRITE NODE DATA.
 * @param node:					Node to write.
 * @param string_data_index:	Node string data index.
 * @param value:				Value to write in corresponding register.
 * @param write_status:			Pointer to the writing operation status.
 * @return status:				Function execution status.
 */
NODE_status_t NODE_write_string_data(NODE_t* node, uint8_t string_data_index, int32_t value, NODE_access_status_t* write_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint8_t register_address = string_data_index;
	// Convert string data index to register.
	if ((NODES[node ->board_id].protocol == NODE_PROTOCOL_AT) && (string_data_index >= DINFOX_STRING_DATA_INDEX_LAST)) {
		register_address = (string_data_index + DINFOX_REGISTER_LAST - DINFOX_STRING_DATA_INDEX_LAST);
	}
	// Write register.
	status = NODE_write_register(node, register_address, value, write_status);
	return status;
}

/* SCAN ALL NODE ON BUS.
 * @param:			None.
 * @return status:	Function executions status.
 */
NODE_status_t NODE_scan(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint8_t nodes_count = 0;
	uint8_t idx = 0;
	// Reset list.
	_NODE_flush_list();
#ifdef AM
	node_ctx.uhfm_address = DINFOX_NODE_ADDRESS_BROADCAST;
#else
	node_ctx.uhfm_connected = 0;
#endif
	// Add master board to the list.
	NODES_LIST.list[0].board_id = DINFOX_BOARD_ID_DMM;
#ifdef AM
	NODES_LIST.list[0].address = DINFOX_NODE_ADDRESS_DMM;
#endif
	NODES_LIST.count++;
	// Scan LBUS nodes.
	status = AT_scan(&(NODES_LIST.list[NODES_LIST.count]), (NODES_LIST_SIZE_MAX - NODES_LIST.count), &nodes_count);
	if (status != NODE_SUCCESS) goto errors;
	// Update count.
	NODES_LIST.count += nodes_count;
	// Search UHFM board in nodes list.
	for (idx=0 ; idx<NODES_LIST.count ; idx++) {
		// Check board ID.
		if (NODES_LIST.list[idx].board_id == DINFOX_BOARD_ID_UHFM) {
#ifdef AM
			node_ctx.uhfm_address = NODES_LIST.list[idx].address;
#else
			node_ctx.uhfm_connected = 1;
#endif
			break;
		}
	}
#ifdef AM
	// Scan R4S8CR nodes.
	status = R4S8CR_scan(&(NODES_LIST.list[NODES_LIST.count]), (NODES_LIST_SIZE_MAX - NODES_LIST.count), &nodes_count);
	if (status != NODE_SUCCESS) goto errors;
	// Update count.
	NODES_LIST.count += nodes_count;
#endif
errors:
	return status;
}

/* MAIN TASK OF NODE LAYER.
 * @param:			None.
 * @return status:	Function execution status.
 */
NODE_status_t NODE_task(void) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	LPUART_status_t lpuart1_status = LPUART_SUCCESS;
	uint32_t loop_count = 0;
	// Increment time.
	node_ctx.sigfox_ul_seconds_count += RTC_WAKEUP_PERIOD_SECONDS;
	// Check Sigfox period.
	if (node_ctx.sigfox_ul_seconds_count >= NODE_SIGFOX_PERIOD_SECONDS) {
		// Reset count.
		node_ctx.sigfox_ul_seconds_count = 0;
		// Turn bus interface on.
		lpuart1_status = LPUART1_power_on();
		LPUART1_status_check(NODE_ERROR_BASE_LPUART);
		// Search next Sigfox message to send.
		do {
			// Update node data.
			status = NODE_update_all_data(&(NODES_LIST.list[node_ctx.sigfox_ul_node_list_index]));
			if (status == NODE_SUCCESS) {
				// Send data through radio.
				status = _NODE_radio_send(&(NODES_LIST.list[node_ctx.sigfox_ul_node_list_index]), node_ctx.sigfox_ul_payload_type_index, 0);
				// Handle all errors except not supported and empty payload.
				if ((status != NODE_SUCCESS) && (status != NODE_ERROR_NOT_SUPPORTED) && (status != NODE_ERROR_SIGFOX_PAYLOAD_EMPTY)) goto errors;
			}
			else {
				// Handle all errors except not supported.
				if (status != NODE_ERROR_NOT_SUPPORTED) goto errors;
			}
			// Increment payload type index.
			node_ctx.sigfox_ul_payload_type_index++;
			if (node_ctx.sigfox_ul_payload_type_index >= NODE_SIGFOX_PAYLOAD_TYPE_LAST) {
				// Switch to next node.
				node_ctx.sigfox_ul_payload_type_index = 0;
				node_ctx.sigfox_ul_node_list_index++;
				if (node_ctx.sigfox_ul_node_list_index >= NODES_LIST.count) {
					// Come back to first node.
					node_ctx.sigfox_ul_node_list_index = 0;
				}
			}
			// Exit if timeout.
			loop_count++;
			if (loop_count > NODE_SIGFOX_LOOP_MAX) {
				status = NODE_ERROR_SIGFOX_LOOP;
				goto errors;
			}

		}
		while (status != NODE_SUCCESS);
	}
errors:
	// Turn bus interface off.
	LPUART1_power_off();
	return status;
}
