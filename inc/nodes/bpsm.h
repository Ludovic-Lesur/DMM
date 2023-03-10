/*
 * bpsm.h
 *
 *  Created on: 26 feb. 2023
 *      Author: Ludo
 */

#ifndef __BPSM_H__
#define __BPSM_H__

#include "dinfox.h"
#include "node.h"
#include "string.h"
#include "types.h"

/*** BPSM structures ***/

typedef enum {
	BPSM_REGISTER_VSRC_MV = DINFOX_REGISTER_LAST,
	BPSM_REGISTER_VSTR_MV,
	BPSM_REGISTER_VBKP_MV,
	BPSM_REGISTER_CHARGE_ENABLE,
	BPSM_REGISTER_CHARGE_STATUS,
	BPSM_REGISTER_BACKUP_ENABLE,
	BPSM_REGISTER_LAST,
} BPSM_register_address_t;

typedef enum {
	BPSM_STRING_DATA_INDEX_VSRC_MV = DINFOX_STRING_DATA_INDEX_LAST,
	BPSM_STRING_DATA_INDEX_VSTR_MV,
	BPSM_STRING_DATA_INDEX_VBKP_MV,
	BPSM_STRING_DATA_INDEX_CHARGE_ENABLE,
	BPSM_STRING_DATA_INDEX_CHARGE_STATUS,
	BPSM_STRING_DATA_INDEX_BACKUP_ENABLE,
	BPSM_STRING_DATA_INDEX_LAST,
} BPSM_string_data_index_t;

/*** BPSM macros ***/

#define BPSM_NUMBER_OF_SPECIFIC_REGISTERS	(BPSM_REGISTER_LAST - DINFOX_REGISTER_LAST)
#define BPSM_NUMBER_OF_SPECIFIC_STRING_DATA	(BPSM_STRING_DATA_INDEX_LAST - DINFOX_STRING_DATA_INDEX_LAST)

static const STRING_format_t BPSM_REGISTERS_FORMAT[BPSM_NUMBER_OF_SPECIFIC_REGISTERS] = {
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_DECIMAL,
	STRING_FORMAT_BOOLEAN,
	STRING_FORMAT_BOOLEAN,
	STRING_FORMAT_BOOLEAN
};

/*** BPSM functions ***/

NODE_status_t BPSM_update_data(NODE_data_update_t* data_update);
NODE_status_t BPSM_get_sigfox_ul_payload(int32_t* integer_data_value, NODE_sigfox_ul_payload_type_t ul_payload_type, uint8_t* ul_payload, uint8_t* ul_payload_size);

#endif /* __BPSM_H__ */
