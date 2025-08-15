/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * mst_structs.h: MST/BINA data structures.                                *
 *                                                                         *
 * Copyright (c) 2019-2025 by David Korth.                                 *
 * SPDX-License-Identifier: MIT                                            *
 ***************************************************************************/

#ifndef __MST06_MST_STRUCTS_H__
#define __MST06_MST_STRUCTS_H__

#include <stdint.h>
#include "common.h"

#pragma pack(1)

/**
 * MST file header.
 * Currently, only Sonic'06 files are supported.
 *
 * All offsets are relative to the end of the header.
 *
 * Field endianness is determined by the 'endianness' field.
 */
#define BINA_MAGIC 'BINA'
typedef struct PACKED _MST_Header {
	uint32_t file_size;		// [0x000] Total size of the MST file.
	uint32_t doff_tbl_offset;	// [0x004] Start of the differential offset table.
	uint32_t doff_tbl_length;	// [0x008] Differential offset table length.
	uint32_t unk_zero1;		// [0x00C]
	uint32_t unk_zero2;		// [0x010]
	uint16_t unk_zero3;		// [0x014]
	char version;			// [0x016] Version. ('1')
	char endianness;		// [0x017] 'B' for big-endian; 'L' for little-endian.
	uint32_t bina_magic;		// [0x018] 'BINA'
	uint32_t unk_zero4;		// [0x01C]
} MST_Header;
ASSERT_STRUCT(MST_Header, 32);

#pragma pack()

/**
 * WTXT header.
 * Indicates the number of messages in the file.
 *
 * Field endianness is determined by the 'endianness' field
 * in the MST header.
 */
#define WTXT_MAGIC 'WTXT'
typedef struct _WTXT_Header {
	uint32_t magic;			// [0x000] 'WTXT'
	uint32_t msg_tbl_name_offset;	// [0x004] Offset of message table name.
	uint32_t msg_tbl_count;		// [0x008] Number of strings in the message table.
} WTXT_Header;
ASSERT_STRUCT(WTXT_Header, 3*sizeof(uint32_t));

/**
 * Following WTXT_Header is an array of message pointers.
 * Messages are encded as UTF-16BE.
 */
typedef struct _WTXT_MsgPointer {
	uint32_t name_offset;		// [0x000] Offset of message name. (Shift-JIS)
	uint32_t text_offset;		// [0x004] Offset of message text. (UTF-16)
	uint32_t placeholder_offset;	// [0x008] If non-zero, offset of placeholder icon name. (Shift-JIS)
} WTXT_MsgPointer;
ASSERT_STRUCT(WTXT_MsgPointer, 3*sizeof(uint32_t));

#endif /* __MST06_MST_STRUCTS_H__ */
