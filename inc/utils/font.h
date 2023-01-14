/*
 * font.h
 *
 *  Created on: 12 jan. 2023
 *      Author: Ludo
 */

#ifndef __FONT_H__
#define __FONT_H__

/*** FONT macros ***/

#define FONT_ASCII_TABLE_SIZE		128
#define FONT_ASCII_TABLE_OFFSET		32
#define FONT_TABLE_SIZE				(FONT_ASCII_TABLE_SIZE - FONT_ASCII_TABLE_OFFSET)
#define FONT_CHAR_WIDTH_PIXELS		6

/*** FONT characters bitmap definition ***/

#define FONT_CHAR_NULL	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define FONT_CHAR_0		{0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00}
#define FONT_CHAR_1		{0x44, 0x42, 0x7F, 0x40, 0x40, 0x00}
#define FONT_CHAR_2		{0x62, 0x73, 0x59, 0x4B, 0x4E, 0x00}
#define FONT_CHAR_3		{0x22, 0x41, 0x49, 0x6B, 0x3E, 0x00}
#define FONT_CHAR_4		{0x38, 0x2C, 0x26, 0x73, 0x21, 0x00}
#define FONT_CHAR_5		{0x4F, 0x49, 0x49, 0x49, 0x79, 0x00}
#define FONT_CHAR_6		{0x7F, 0x49, 0x49, 0x49, 0x79, 0x00}
#define FONT_CHAR_7		{0x61, 0x31, 0x19, 0x0D, 0x07, 0x00}
#define FONT_CHAR_8		{0x7F, 0x49, 0x49, 0x49, 0x7F, 0x00}
#define FONT_CHAR_9		{0x4F, 0x49, 0x49, 0x49, 0x7F, 0x00}
#define FONT_CHAR_A		{0x7E, 0x09, 0x09, 0x09, 0x7E, 0x00}
#define FONT_CHAR_B		{0x7F, 0x49, 0x49, 0x49, 0x3E, 0x00}
#define FONT_CHAR_C		{0x7F, 0x41, 0x41, 0x41, 0x41, 0x00}
#define FONT_CHAR_D		{0x41, 0x41, 0x7F, 0x41, 0x7F, 0x00}
#define FONT_CHAR_E		{0x7F, 0x49, 0x49, 0x49, 0x41, 0x00}
#define FONT_CHAR_F		{0x7F, 0x09, 0x09, 0x09, 0x01, 0x00}
#define FONT_CHAR_G		{0x7F, 0x41, 0x49, 0x49, 0x79, 0x00}
#define FONT_CHAR_H		{0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00}
#define FONT_CHAR_I		{0x41, 0x41, 0x7F, 0x41, 0x41, 0x00}
#define FONT_CHAR_J		{0x61, 0x41, 0x61, 0x7F, 0x01, 0x00}
#define FONT_CHAR_K		{0x7F, 0x80, 0x1E, 0x33, 0x61, 0x00}
#define FONT_CHAR_L		{0x7F, 0x40, 0x40, 0x40, 0x40, 0x00}
#define FONT_CHAR_M		{0x7F, 0x06, 0x0C, 0x06, 0x7F, 0x00}
#define FONT_CHAR_N		{0x7F, 0x06, 0x1C, 0x30, 0x7F, 0x00}
#define FONT_CHAR_O		{0x7F, 0x41, 0x41, 0x41, 0x7F, 0x00}
#define FONT_CHAR_P		{0x7F, 0x09, 0x09, 0x09, 0x0F, 0x00}
#define FONT_CHAR_Q		{0x7F, 0x41, 0x51, 0x61, 0x7F, 0x00}
#define FONT_CHAR_R		{0x7F, 0x1C, 0x3C, 0x6C, 0x4F, 0x00}
#define FONT_CHAR_S		{0x6F, 0x49, 0x49, 0x49, 0x7B, 0x00}
#define FONT_CHAR_T		{0x01, 0x01, 0x7F, 0x01, 0x01, 0x00}
#define FONT_CHAR_U		{0x7F, 0x40, 0x40, 0x40, 0x7F, 0x00}
#define FONT_CHAR_V		{0x1F, 0x30, 0x60, 0x30, 0x1F, 0x00}
#define FONT_CHAR_W		{0x7F, 0x40, 0x70, 0x40, 0x7F, 0x00}
#define FONT_CHAR_X		{0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}
#define FONT_CHAR_Y		{0x07, 0x0C, 0x78, 0x0C, 0x07, 0x00}
#define FONT_CHAR_Z		{0x71, 0x59, 0x4D, 0x47, 0x43, 0x00}

/*** FONT definition ***/

static const uint8_t FONT[FONT_TABLE_SIZE][FONT_CHAR_WIDTH_PIXELS] = {
	FONT_CHAR_NULL, // 0x20
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL, // 0x2F
	FONT_CHAR_0,
	FONT_CHAR_1,
	FONT_CHAR_2,
	FONT_CHAR_3,
	FONT_CHAR_4,
	FONT_CHAR_5,
	FONT_CHAR_6,
	FONT_CHAR_7,
	FONT_CHAR_8,
	FONT_CHAR_9,
	FONT_CHAR_NULL, // 0x3A
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL,
	FONT_CHAR_NULL, // 0x40
	FONT_CHAR_A,
	FONT_CHAR_B,
	FONT_CHAR_C,
	FONT_CHAR_D,
	FONT_CHAR_E,
	FONT_CHAR_F,
	FONT_CHAR_G,
	FONT_CHAR_H,
	FONT_CHAR_I,
	FONT_CHAR_J,
	FONT_CHAR_K,
	FONT_CHAR_L,
	FONT_CHAR_M,
	FONT_CHAR_N,
	FONT_CHAR_O,
	FONT_CHAR_P,
	FONT_CHAR_Q,
	FONT_CHAR_R,
	FONT_CHAR_S,
	FONT_CHAR_T,
	FONT_CHAR_U,
	FONT_CHAR_V,
	FONT_CHAR_W,
	FONT_CHAR_X,
	FONT_CHAR_Y,
	FONT_CHAR_Z
};

#endif /* __FONT_H__ */
